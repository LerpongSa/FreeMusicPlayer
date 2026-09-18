#include "FlacEncoder.h"

#include <QFile>
#include <QCryptographicHash>

#include <algorithm>
#include <array>
#include <cmath>

// All bit-layout decisions below follow RFC 9639 "Free Lossless Audio
// Codec" section 8 (metadata) and section 9 (frame structure) - see
// FlacEncoder.h for the header comment summarizing which subset of the
// format this encoder actually produces.

namespace {

// ---------------------------------------------------------------------------
// MSB-first bit packing. FLAC's bitstream packs fields MSB-first and lets
// them straddle byte boundaries freely (only the very first subframe of a
// frame is guaranteed byte-aligned, by construction of the frame header
// fields all summing to a whole number of bytes) - this one accumulator
// handles both the byte-aligned header fields and the bit-level subframe
// data uniformly, since a byte-aligned write is just the same operation
// with numBits a multiple of 8.
// ---------------------------------------------------------------------------
class BitWriter
{
public:
    void writeBits(uint32_t value, int numBits)
    {
        for (int i = numBits - 1; i >= 0; --i) {
            m_curByte = static_cast<uint8_t>((m_curByte << 1) | ((value >> i) & 1u));
            if (++m_curBits == 8) {
                m_bytes.push_back(static_cast<char>(m_curByte));
                m_curByte = 0;
                m_curBits = 0;
            }
        }
    }

    // Rice unary prefix: q zero bits then a terminating 1 bit. Chunked into
    // 32-bit writeBits() calls rather than one bit at a time so a
    // pathological (mis-chosen) q can't turn into millions of individual
    // one-bit calls - can't happen with this encoder's own k search (see
    // riceCostForK's comment), but a defensive bound costs nothing.
    void writeUnary(uint32_t q)
    {
        while (q >= 32) {
            writeBits(0, 32);
            q -= 32;
        }
        if (q > 0)
            writeBits(0, static_cast<int>(q));
        writeBits(1, 1);
    }

    // Pads with zero bits to the next byte boundary. A no-op if already
    // aligned.
    void align()
    {
        if (m_curBits > 0) {
            m_curByte = static_cast<uint8_t>(m_curByte << (8 - m_curBits));
            m_bytes.push_back(static_cast<char>(m_curByte));
            m_curByte = 0;
            m_curBits = 0;
        }
    }

    const QByteArray &bytes() const { return m_bytes; }

private:
    QByteArray m_bytes;
    uint32_t m_curByte = 0;
    int m_curBits = 0;
};

// CRC-8, poly x^8+x^2+x^1+x^0 (0x07), init 0, MSB-first, not reflected -
// RFC 9639 9.1.8. Covers a frame header up to (not including) its own CRC
// byte.
uint8_t crc8(const char *data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; ++i) {
        crc ^= static_cast<uint8_t>(data[i]);
        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07) : static_cast<uint8_t>(crc << 1);
    }
    return crc;
}

// CRC-16, poly x^16+x^15+x^2+x^0 (0x8005), init 0, MSB-first, not reflected
// - RFC 9639 9.3. Covers a whole frame (sync code through the padding
// before this CRC).
uint16_t crc16(const char *data, int len)
{
    uint16_t crc = 0;
    for (int i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(static_cast<uint8_t>(data[i])) << 8;
        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x8005) : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

// UTF-8-like coded number (RFC 9639 9.1.5, Table 18) - extended to 6 bytes
// / 31 bits, which is all a frame number ever needs here (this encoder
// only ever writes fixed-block-size streams, so the coded number is always
// a frame index, never a sample index). A 3-hour album at the 4096-sample
// blocks this encoder always uses is under 120,000 frames, so the 3-byte
// form covers every real case; the wider forms are implemented anyway so
// nothing silently truncates on an implausibly long track.
void writeCodedNumber(BitWriter &bw, uint32_t number)
{
    if (number < 0x80) {
        bw.writeBits(number, 8);
    } else if (number < 0x800) {
        bw.writeBits(0xC0u | (number >> 6), 8);
        bw.writeBits(0x80u | (number & 0x3Fu), 8);
    } else if (number < 0x10000) {
        bw.writeBits(0xE0u | (number >> 12), 8);
        bw.writeBits(0x80u | ((number >> 6) & 0x3Fu), 8);
        bw.writeBits(0x80u | (number & 0x3Fu), 8);
    } else if (number < 0x200000) {
        bw.writeBits(0xF0u | (number >> 18), 8);
        bw.writeBits(0x80u | ((number >> 12) & 0x3Fu), 8);
        bw.writeBits(0x80u | ((number >> 6) & 0x3Fu), 8);
        bw.writeBits(0x80u | (number & 0x3Fu), 8);
    } else if (number < 0x4000000) {
        bw.writeBits(0xF8u | (number >> 24), 8);
        bw.writeBits(0x80u | ((number >> 18) & 0x3Fu), 8);
        bw.writeBits(0x80u | ((number >> 12) & 0x3Fu), 8);
        bw.writeBits(0x80u | ((number >> 6) & 0x3Fu), 8);
        bw.writeBits(0x80u | (number & 0x3Fu), 8);
    } else { // up to 31 bits
        bw.writeBits(0xFCu | (number >> 30), 8);
        bw.writeBits(0x80u | ((number >> 24) & 0x3Fu), 8);
        bw.writeBits(0x80u | ((number >> 18) & 0x3Fu), 8);
        bw.writeBits(0x80u | ((number >> 12) & 0x3Fu), 8);
        bw.writeBits(0x80u | ((number >> 6) & 0x3Fu), 8);
        bw.writeBits(0x80u | (number & 0x3Fu), 8);
    }
}

// Zigzag-folds a signed residual to unsigned (RFC 9639 9.2.7.2): "for
// positive numbers, the number doubled; for negative numbers, the number
// multiplied by -2 minus 1". Every fixed-predictor residual used here fits
// comfortably in 32 bits (see the header comment), so this plain int32
// zigzag - not the wider intermediate computeFixedResidual() uses - is
// exact.
uint32_t zigzag(int32_t v)
{
    return (static_cast<uint32_t>(v) << 1) ^ static_cast<uint32_t>(v >> 31);
}

// Exact bit cost of Rice-coding `residuals` with parameter k, as one
// partition (unary quotient + terminator + k-bit remainder per sample).
// k is only ever tried in 0..14 (the 15 values a 4-bit Rice parameter can
// hold; 0b1111 is the escape code this encoder never emits), which bounds
// the unary part of any single residual to at most a few thousand zero
// bits even for a pathological (near full-scale) residual - nothing close
// to the "unbounded" blowup an unrestricted k search could produce.
uint64_t riceCostForK(const std::vector<int32_t> &residuals, int k)
{
    uint64_t bits = 0;
    for (int32_t r : residuals)
        bits += (zigzag(r) >> k) + 1 + static_cast<uint32_t>(k);
    return bits;
}

// Sum of zigzag-folded residual magnitudes - a cheap proxy for a fixed
// predictor order's eventual Rice-coded size (real encoders, including
// libFLAC's own estimate_best_order(), pick the predictor order this way
// rather than running a full Rice-parameter search per candidate order:
// Rice code length is a monotonic function of residual magnitude, so this
// reliably lands on the same order the full search would, at an O(n)
// pass instead of an O(15n) one per candidate).
uint64_t sumAbsResidual(const std::vector<int32_t> &residuals)
{
    uint64_t sum = 0;
    for (int32_t r : residuals)
        sum += zigzag(r);
    return sum;
}

// Picks the Rice parameter for `residuals`. Rice-coded size is convex
// (unimodal) in k, and for a Laplacian-like residual distribution (real
// audio prediction residuals) the optimum is well approximated by
// floor(log2(mean folded residual)) - so the true optimum is essentially
// always within +-1 of that estimate. Checking only the estimate and its
// two neighbors exactly (instead of an earlier version of this encoder's
// exhaustive sweep over all 15 valid values) gets the same answer for a
// fraction of the cost.
int chooseRiceK(const std::vector<int32_t> &residuals, uint64_t sumAbs)
{
    const uint64_t mean = residuals.empty() ? 0 : sumAbs / residuals.size();
    int estimate = 0;
    while ((1ull << (estimate + 1)) <= mean + 1 && estimate < 14)
        ++estimate;

    int bestK = 0;
    uint64_t bestCost = UINT64_MAX;
    for (int k = std::max(0, estimate - 1); k <= std::min(14, estimate + 1); ++k) {
        const uint64_t cost = riceCostForK(residuals, k);
        if (cost < bestCost) {
            bestCost = cost;
            bestK = k;
        }
    }
    return bestK;
}

// Warm-up samples bypass prediction entirely (RFC 9639 9.2.5), so fixed
// predictor order `order` only produces a residual for samples order..n-1.
// Predictions are computed in int64 to give headroom against the highest
// (order 4, coefficients up to 6x) predictor overshooting on a worst-case
// 24-bit input before the residual (guaranteed small enough to fit 32 bits
// for any real audio) is narrowed back to int32.
void computeFixedResidual(const int32_t *x, int blockSize, int order, std::vector<int32_t> &out)
{
    out.resize(static_cast<size_t>(blockSize - order));
    for (int n = order; n < blockSize; ++n) {
        int64_t pred = 0;
        switch (order) {
        case 0: pred = 0; break;
        case 1: pred = x[n - 1]; break;
        case 2: pred = 2LL * x[n - 1] - x[n - 2]; break;
        case 3: pred = 3LL * x[n - 1] - 3LL * x[n - 2] + x[n - 3]; break;
        case 4: pred = 4LL * x[n - 1] - 6LL * x[n - 2] + 4LL * x[n - 3] - x[n - 4]; break;
        default: break;
        }
        out[static_cast<size_t>(n - order)] = static_cast<int32_t>(static_cast<int64_t>(x[n]) - pred);
    }
}

// Writes `count` samples starting at x[0], each `bits` bits, signed two's
// complement, MSB-first - the "unencoded sample" building block RFC 9639
// reuses for constant subframes, warm-up samples and verbatim subframes
// (9.2.3).
void writeRawSamples(BitWriter &bw, const int32_t *x, int count, int bits)
{
    const uint32_t mask = (bits >= 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
    for (int i = 0; i < count; ++i)
        bw.writeBits(static_cast<uint32_t>(x[i]) & mask, bits);
}

// Encodes one channel's worth of one block: CONSTANT if every sample is
// identical, otherwise the cheapest of fixed predictor orders 0..4 (each
// with its own best single-partition Rice parameter). Returns the encoded
// bit cost purely for the caller's own bookkeeping/logging - the samples
// are written directly into `bw`.
void writeSubframe(BitWriter &bw, const int32_t *x, int blockSize, int bitsPerSample)
{
    bool constant = true;
    for (int i = 1; i < blockSize && constant; ++i)
        constant = (x[i] == x[0]);

    if (constant) {
        bw.writeBits(0b000000, 7); // subframe header: "must be 0" bit + type=CONSTANT(0)
        bw.writeBits(0, 1);        // wasted-bits-per-sample flag: none used
        writeRawSamples(bw, x, 1, bitsPerSample);
        return;
    }

    const int maxOrder = std::min(4, blockSize - 1);
    int bestOrder = 0;
    uint64_t bestSumAbs = UINT64_MAX;
    std::vector<int32_t> residual;

    // Pass 1 (cheap): settle the predictor order via sumAbsResidual - one
    // O(n) pass per candidate order, no Rice search yet (see its comment).
    for (int order = 0; order <= maxOrder; ++order) {
        computeFixedResidual(x, blockSize, order, residual);
        const uint64_t sumAbs = sumAbsResidual(residual);
        if (sumAbs < bestSumAbs) {
            bestSumAbs = sumAbs;
            bestOrder = order;
        }
    }

    // Pass 2: only now, for the one order that won, work out its actual
    // best Rice parameter (chooseRiceK's narrowed 3-value search, not an
    // exhaustive one).
    computeFixedResidual(x, blockSize, bestOrder, residual);
    const int bestK = chooseRiceK(residual, bestSumAbs);

    bw.writeBits(0b001000u | static_cast<uint32_t>(bestOrder), 7); // "must be 0" bit + type=FIXED(order)
    bw.writeBits(0, 1);                                            // wasted-bits-per-sample flag: none used
    writeRawSamples(bw, x, bestOrder, bitsPerSample);
    bw.writeBits(0b00, 2);   // coded residual: 4-bit Rice parameters
    bw.writeBits(0b0000, 4); // partition order 0 - a single partition covering the whole block
    bw.writeBits(static_cast<uint32_t>(bestK), 4);
    const uint32_t riceMask = (1u << bestK) - 1u;
    for (int32_t r : residual) {
        const uint32_t folded = zigzag(r);
        bw.writeUnary(folded >> bestK);
        bw.writeBits(folded & riceMask, bestK);
    }
}

int blockSizeCode(int blockSize, bool *uncommon)
{
    *uncommon = false;
    switch (blockSize) {
    case 192: return 0b0001;
    case 576: return 0b0010;
    case 1152: return 0b0011;
    case 2304: return 0b0100;
    case 4608: return 0b0101;
    case 256: return 0b1000;
    case 512: return 0b1001;
    case 1024: return 0b1010;
    case 2048: return 0b1011;
    case 4096: return 0b1100;
    case 8192: return 0b1101;
    case 16384: return 0b1110;
    case 32768: return 0b1111;
    default:
        *uncommon = true;
        return (blockSize <= 256) ? 0b0110 : 0b0111;
    }
}

// Encodes one whole frame (header, one subframe per channel, padding,
// footer CRC) into a fresh BitWriter and returns its bytes ready to
// append to the output file. A fresh BitWriter per frame means the CRCs
// below can just hash bw.bytes() directly - no need to track/slice a
// running cross-frame byte offset.
QByteArray encodeFrame(const int32_t *interleaved, int blockSize, uint32_t frameNumber,
                        int sampleRate, int channelCount, int bitsPerSample)
{
    BitWriter bw;
    bw.writeBits(0xFF, 8); // sync byte 1
    bw.writeBits(0xF8, 8); // sync byte 2: reserved(0) + blocking strategy(0=fixed block size)

    bool uncommonBlockSize = false;
    const int bsCode = blockSizeCode(blockSize, &uncommonBlockSize);
    const int srCode = (sampleRate == 44100) ? 0b1001 : (sampleRate == 88200) ? 0b0001 : 0b0000;
    bw.writeBits(static_cast<uint32_t>((bsCode << 4) | srCode), 8);

    const int chCode = (channelCount == 2) ? 0b0001 : 0b0000; // stereo / mono, the only cases this encoder supports
    const int depthCode = (bitsPerSample == 16) ? 0b100 : (bitsPerSample == 24) ? 0b110 : 0b000;
    bw.writeBits(static_cast<uint32_t>((chCode << 4) | (depthCode << 1)), 8); // low bit: reserved(0)

    writeCodedNumber(bw, frameNumber);

    if (uncommonBlockSize) {
        if (bsCode == 0b0110)
            bw.writeBits(static_cast<uint32_t>(blockSize - 1), 8);
        else
            bw.writeBits(static_cast<uint32_t>(blockSize - 1), 16);
    }

    const uint8_t headerCrc = crc8(bw.bytes().constData(), bw.bytes().size());
    bw.writeBits(headerCrc, 8);

    // De-interleave into per-channel subframes. blockSize is at most 4096
    // (this encoder's fixed block size), so a small stack-ish buffer per
    // channel via std::vector is cheap and avoids re-deriving strides
    // inline in writeSubframe().
    std::vector<int32_t> chanBuf(static_cast<size_t>(blockSize));
    for (int c = 0; c < channelCount; ++c) {
        for (int i = 0; i < blockSize; ++i)
            chanBuf[static_cast<size_t>(i)] = interleaved[i * channelCount + c];
        writeSubframe(bw, chanBuf.data(), blockSize, bitsPerSample);
    }

    bw.align();

    const uint16_t footerCrc = crc16(bw.bytes().constData(), bw.bytes().size());
    bw.writeBits(footerCrc >> 8, 8);
    bw.writeBits(footerCrc & 0xFF, 8);

    return bw.bytes();
}

} // namespace

namespace FlacEncoder {

bool encode(const QString &outPath,
            const std::vector<int32_t> &interleavedSamples,
            int sampleRate,
            int channelCount,
            int bitsPerSample,
            QString *errorMessage,
            const std::function<void(int percent)> &onProgress)
{
    auto fail = [&](const QString &msg) {
        if (errorMessage)
            *errorMessage = msg;
        return false;
    };

    if (channelCount != 1 && channelCount != 2)
        return fail(QStringLiteral("FlacEncoder: only mono/stereo is supported."));
    if (bitsPerSample != 16 && bitsPerSample != 24)
        return fail(QStringLiteral("FlacEncoder: only 16 or 24 bits per sample is supported."));
    if (sampleRate != 44100 && sampleRate != 88200)
        return fail(QStringLiteral("FlacEncoder: only 44100 or 88200 Hz is supported."));
    if (interleavedSamples.empty() || channelCount == 0
        || interleavedSamples.size() % static_cast<size_t>(channelCount) != 0) {
        return fail(QStringLiteral("FlacEncoder: empty or non-interleaved sample buffer."));
    }

    const uint64_t totalSamplesPerChannel = interleavedSamples.size() / static_cast<size_t>(channelCount);

    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(QStringLiteral("Could not create \"%1\".").arg(outPath));

    // MD5 of the unencoded audio (RFC 9639 8.2): all channels' samples
    // interleaved, signed little-endian, byte-aligned. 16 and 24 bits are
    // both already a whole number of bytes, so no sign-extension padding
    // (the spec's rule for e.g. 6-bit samples) is needed here.
    const int bytesPerSample = bitsPerSample / 8;
    QByteArray md5Input;
    md5Input.resize(static_cast<int>(interleavedSamples.size()) * bytesPerSample);
    {
        char *p = md5Input.data();
        for (int32_t s : interleavedSamples) {
            for (int b = 0; b < bytesPerSample; ++b) {
                *p++ = static_cast<char>((s >> (8 * b)) & 0xFF);
            }
        }
    }
    const QByteArray md5 = QCryptographicHash::hash(md5Input, QCryptographicHash::Md5);

    // ---- STREAMINFO -----------------------------------------------------
    {
        BitWriter bw;
        bw.writeBits(0x66, 8); // 'f'
        bw.writeBits(0x4C, 8); // 'L'
        bw.writeBits(0x61, 8); // 'a'
        bw.writeBits(0x43, 8); // 'C'

        bw.writeBits(1, 1);       // last-metadata-block flag: this is the only block
        bw.writeBits(0, 7);       // block type 0 = STREAMINFO
        bw.writeBits(34, 24);     // block length in bytes (fixed STREAMINFO payload size)

        bw.writeBits(4096, 16);   // min block size
        bw.writeBits(4096, 16);   // max block size
        bw.writeBits(0, 24);      // min frame size: not known
        bw.writeBits(0, 24);      // max frame size: not known
        bw.writeBits(static_cast<uint32_t>(sampleRate), 20);
        bw.writeBits(static_cast<uint32_t>(channelCount - 1), 3);
        bw.writeBits(static_cast<uint32_t>(bitsPerSample - 1), 5);
        // u(36) total interchannel samples, split across a writeBits(32) call max -
        // write high 4 bits then low 32 bits since writeBits takes an int numBits
        // and a 32-bit value; totalSamplesPerChannel comfortably fits 36 bits for
        // any real track (2^36 samples at 88.2kHz is decades of audio).
        bw.writeBits(static_cast<uint32_t>((totalSamplesPerChannel >> 32) & 0xF), 4);
        bw.writeBits(static_cast<uint32_t>(totalSamplesPerChannel & 0xFFFFFFFFu), 32);

        for (int i = 0; i < 16; ++i)
            bw.writeBits(static_cast<uint8_t>(md5.at(i)), 8);

        const QByteArray streamInfoBytes = bw.bytes();
        if (file.write(streamInfoBytes) != streamInfoBytes.size())
            return fail(QStringLiteral("Write failed while writing STREAMINFO to \"%1\".").arg(outPath));
    }

    // ---- Frames -----------------------------------------------------------
    constexpr int kBlockSize = 4096;
    uint32_t frameNumber = 0;
    int lastReportedPercent = -1;
    if (onProgress)
        onProgress(0); // guarantee a 0% callback even for a very short (single-frame) track
    for (uint64_t pos = 0; pos < totalSamplesPerChannel; pos += kBlockSize) {
        const int thisBlockSize =
            static_cast<int>(std::min<uint64_t>(kBlockSize, totalSamplesPerChannel - pos));
        const int32_t *framePtr =
            interleavedSamples.data() + pos * static_cast<uint64_t>(channelCount);

        const QByteArray frameBytes =
            encodeFrame(framePtr, thisBlockSize, frameNumber, sampleRate, channelCount, bitsPerSample);
        if (file.write(frameBytes) != frameBytes.size())
            return fail(QStringLiteral("Write failed while encoding a frame to \"%1\".").arg(outPath));

        ++frameNumber;

        if (onProgress) {
            const int pct = static_cast<int>((pos * 100) / totalSamplesPerChannel);
            if (pct != lastReportedPercent) {
                lastReportedPercent = pct;
                onProgress(pct);
            }
        }
    }
    if (onProgress && lastReportedPercent != 100)
        onProgress(100);

    file.close();
    return true;
}

} // namespace FlacEncoder
