#include "DsfWriter.h"

#include <QFile>

namespace {

void appendU32LE(QByteArray &out, uint32_t v)
{
    out.append(static_cast<char>(v & 0xFF));
    out.append(static_cast<char>((v >> 8) & 0xFF));
    out.append(static_cast<char>((v >> 16) & 0xFF));
    out.append(static_cast<char>((v >> 24) & 0xFF));
}

void appendU64LE(QByteArray &out, uint64_t v)
{
    for (int i = 0; i < 8; ++i)
        out.append(static_cast<char>((v >> (8 * i)) & 0xFF));
}

// Reverses the bits within a single byte (0b10110000 -> 0b00001101).
// DSF stores each 1-bit DSD sample LSB-first; SACD's native bitstream (like
// DSDIFF/DFF) is MSB-first - this is what bridges the two. A small
// branch-free bit-twiddle rather than a 256-entry lookup table: equally
// correct, no table to transcribe (and risk transcribing) from a reference
// implementation.
uint8_t reverseBits(uint8_t b)
{
    b = static_cast<uint8_t>(((b & 0xF0) >> 4) | ((b & 0x0F) << 4));
    b = static_cast<uint8_t>(((b & 0xCC) >> 2) | ((b & 0x33) << 2));
    b = static_cast<uint8_t>(((b & 0xAA) >> 1) | ((b & 0x55) << 1));
    return b;
}

} // namespace

namespace DsfWriter {

bool writeFromInterleavedFrames(const QString &outPath,
                                 const std::vector<uint8_t> &interleavedFrames,
                                 int channelCount,
                                 int bytesPerChannelPerFrame,
                                 double sampleRate,
                                 QString *errorMessage)
{
    auto fail = [&](const QString &msg) {
        if (errorMessage)
            *errorMessage = msg;
        return false;
    };

    if (channelCount != 1 && channelCount != 2)
        return fail(QStringLiteral("DsfWriter: only mono/stereo is supported."));
    if (bytesPerChannelPerFrame <= 0)
        return fail(QStringLiteral("DsfWriter: invalid bytesPerChannelPerFrame."));

    const size_t frameBytes = static_cast<size_t>(channelCount) * static_cast<size_t>(bytesPerChannelPerFrame);
    if (frameBytes == 0 || interleavedFrames.size() % frameBytes != 0)
        return fail(QStringLiteral("DsfWriter: input is not a whole number of complete frames."));
    const size_t frameCount = interleavedFrames.size() / frameBytes;

    // De-interleave every frame's round-robin bytes into per-channel
    // contiguous, bit-reversed blocks, writing channel 0's block then
    // channel 1's block (etc.) per frame, back to back - this is exactly
    // what a DSF "data" chunk with block_size_per_channel ==
    // bytesPerChannelPerFrame expects.
    QByteArray audioData;
    audioData.resize(static_cast<int>(interleavedFrames.size()));
    {
        char *out = audioData.data();
        for (size_t f = 0; f < frameCount; ++f) {
            const uint8_t *src = interleavedFrames.data() + f * frameBytes;
            for (int c = 0; c < channelCount; ++c) {
                char *dst = out + f * frameBytes + static_cast<size_t>(c) * bytesPerChannelPerFrame;
                for (int i = 0; i < bytesPerChannelPerFrame; ++i)
                    dst[i] = static_cast<char>(reverseBits(src[static_cast<size_t>(i) * channelCount + c]));
            }
        }
    }

    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(QStringLiteral("Could not create \"%1\".").arg(outPath));

    QByteArray header;

    // DSD chunk (28 bytes).
    header.append("DSD ", 4);
    appendU64LE(header, 28); // chunk_data_size: includes this chunk's own header
    const qint64 totalFileSizePos = header.size();
    appendU64LE(header, 0); // total_file_size: patched in below once known
    appendU64LE(header, 0); // metadata_offset: 0, no ID3 footer

    // fmt chunk (52 bytes).
    header.append("fmt ", 4);
    appendU64LE(header, 52);
    appendU32LE(header, 1); // version
    appendU32LE(header, 0); // format_id: 0 = raw DSD (not DST-compressed)
    appendU32LE(header, static_cast<uint32_t>(channelCount)); // channel_type: 1=mono, 2=stereo (values match channel_count for these two cases)
    appendU32LE(header, static_cast<uint32_t>(channelCount));
    appendU32LE(header, static_cast<uint32_t>(sampleRate + 0.5));
    appendU32LE(header, 1); // bits_per_sample: 1 (DSD)
    appendU64LE(header, static_cast<uint64_t>(bytesPerChannelPerFrame) * frameCount * 8); // sample_count: 1-bit samples per channel
    appendU32LE(header, static_cast<uint32_t>(bytesPerChannelPerFrame)); // block_size_per_channel
    appendU32LE(header, 0); // reserved

    // data chunk header (12 bytes) + payload.
    header.append("data", 4);
    appendU64LE(header, 12 + static_cast<uint64_t>(audioData.size()));

    const qint64 headerSize = header.size();
    const qint64 totalFileSize = headerSize + audioData.size();
    // Patch total_file_size now that it's known (little-endian u64 at totalFileSizePos).
    {
        uint64_t v = static_cast<uint64_t>(totalFileSize);
        for (int i = 0; i < 8; ++i) {
            header[static_cast<int>(totalFileSizePos) + i] = static_cast<char>(v & 0xFF);
            v >>= 8;
        }
    }

    if (file.write(header) != header.size())
        return fail(QStringLiteral("Write failed while writing the DSF header to \"%1\".").arg(outPath));
    if (file.write(audioData) != audioData.size())
        return fail(QStringLiteral("Write failed while writing DSD audio data to \"%1\".").arg(outPath));

    file.close();
    return true;
}

} // namespace DsfWriter
