#include "IsoAudioExtractor.h"
#include "DsfWriter.h"
#include "FlacEncoder.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QAudioDecoder>
#include <QAudioBuffer>
#include <QEventLoop>
#include <QUrl>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

// ---------------------------------------------------------------------------
// Shared low-level helpers
// ---------------------------------------------------------------------------

constexpr int kLsnSize = 2048;                 // SACD_LSN_SIZE
constexpr int kFrameSize64 = 4704;             // SACD_FRAME_RATE=75, FRAME_SIZE_64 = 588*64/8
constexpr int kMaxPacketSize = 2045;
constexpr int kDataTypeAudio = 2;
constexpr int kMasterTocLsn = 510;             // START_OF_MASTER_TOC

uint32_t beU32(const uchar *p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

uint16_t beU16(const uchar *p)
{
    return uint16_t((uint16_t(p[0]) << 8) | p[1]);
}

bool readLsn(QFile &f, int64_t lsn, int64_t count, QByteArray &out)
{
    if (!f.seek(lsn * kLsnSize))
        return false;
    out = f.read(count * kLsnSize);
    return out.size() == count * kLsnSize;
}

// ---------------------------------------------------------------------------
// SACD: Master TOC / Area TOC / track list (SACDTRL1)
//
// Byte offsets below are computed from sacd-ripper's own packed C structs
// (master_toc_t / area_toc_t / area_tracklist_offset_t in scarletbook.h) -
// every field is ATTRIBUTE_PACKED (no compiler padding), so these offsets
// are exact sums of the preceding fields' sizes.
// ---------------------------------------------------------------------------

struct SacdArea
{
    int64_t tocStartLsn = 0;
    int64_t tocSizeLsn = 0;
    int channelCount = 0;
    int trackCount = 0;
    int64_t areaTrackStartLsn = 0; // whole area's own start/end, for the sanity check below
    int64_t areaTrackEndLsn = 0;
};

bool readMasterToc(QFile &iso, SacdArea &twoChArea, QString *error)
{
    QByteArray sector;
    // A short read here (including a file too small to even contain LSN
    // 510) is treated the same as a signature mismatch, not an error: a
    // small raw CD-DA image is exactly that "too small for an SACD Master
    // TOC" case, and it's a perfectly valid file for the CD-DA fallback
    // below to pick up - only a *found* SACDMTOC signature followed by a
    // structural problem is a hard failure (see readAreaToc/readTrackList).
    if (!readLsn(iso, kMasterTocLsn, 1, sector))
        return false;
    if (sector.left(8) != QByteArray("SACDMTOC", 8))
        return false; // not a SACD ISO - not an error, just "no" for the caller to fall back on

    const uchar *d = reinterpret_cast<const uchar *>(sector.constData());
    const uint32_t area1Toc1Start = beU32(d + 64); // area_1_toc_1_start (2-channel area)
    const uint32_t area1TocSize = beU16(d + 84);   // area_1_toc_size, in LSNs

    if (area1Toc1Start == 0) {
        if (error) *error = QStringLiteral("This SACD ISO has no 2-channel audio area.");
        return false;
    }
    twoChArea.tocStartLsn = area1Toc1Start;
    twoChArea.tocSizeLsn = area1TocSize;
    return true;
}

bool readAreaToc(QFile &iso, SacdArea &area, QString *error)
{
    QByteArray sector;
    if (!readLsn(iso, area.tocStartLsn, 1, sector)) {
        if (error) *error = QStringLiteral("Could not read the Area TOC sector.");
        return false;
    }
    if (sector.left(8) != QByteArray("TWOCHTOC", 8)) {
        if (error) *error = QStringLiteral("2-channel Area TOC signature not found.");
        return false;
    }
    const uchar *d = reinterpret_cast<const uchar *>(sector.constData());
    area.channelCount = d[32];
    area.trackCount = d[69];
    area.areaTrackStartLsn = beU32(d + 72);
    area.areaTrackEndLsn = beU32(d + 76);

    if (area.channelCount != 2) {
        if (error) *error = QStringLiteral("2-channel area does not actually report 2 channels (got %1).").arg(area.channelCount);
        return false;
    }
    if (area.trackCount <= 0 || area.trackCount > 255) {
        if (error) *error = QStringLiteral("Implausible track count (%1) in Area TOC.").arg(area.trackCount);
        return false;
    }
    return true;
}

// Scans the "extra tables" that follow the area_toc_t sector (SACDText,
// SACD_IGL, SACD_ACC, SACDTRL1, SACDTRL2 - each self-identified by an
// 8-byte ASCII id at the start of its first LSN) looking for SACDTRL1,
// which holds the absolute start/length LSN of every track. Unrecognized
// ids end the scan, exactly like sacd-ripper's own reader.
bool readTrackList(QFile &iso, const SacdArea &area, std::vector<int64_t> &startLsn,
                    std::vector<int64_t> &lengthLsn)
{
    int64_t p = area.tocStartLsn + 1;
    const int64_t areaEnd = area.tocStartLsn + std::max<int64_t>(area.tocSizeLsn, 1);

    while (p < areaEnd) {
        QByteArray sector;
        if (!readLsn(iso, p, 1, sector))
            break;
        const QByteArray id = sector.left(8);
        if (id == "SACDText") {
            p += 1;
        } else if (id == "SACD_IGL") {
            p += 2;
        } else if (id == "SACD_ACC") {
            p += 32;
        } else if (id == "SACDTRL2") {
            p += 1;
        } else if (id == "SACDTRL1") {
            const uchar *d = reinterpret_cast<const uchar *>(sector.constData());
            startLsn.resize(static_cast<size_t>(area.trackCount));
            lengthLsn.resize(static_cast<size_t>(area.trackCount));
            for (int i = 0; i < area.trackCount; ++i) {
                startLsn[static_cast<size_t>(i)] = beU32(d + 8 + i * 4);
                lengthLsn[static_cast<size_t>(i)] = beU32(d + 8 + 255 * 4 + i * 4);
            }
            return true;
        } else {
            break;
        }
    }
    return false;
}

// Demuxes one track's raw "audio sectors" into a flat, byte-interleaved
// (round-robin across channels) DSD payload ready for DsfWriter - see
// DsfWriter.h's header comment for the exact byte layout this produces.
// Mirrors scarletbook_process_frames()'s per-sector packet walk field for
// field (see IsoAudioExtractor.h's header comment); the one behavioral
// difference is that a packet whose declared length would run past the
// sector is treated as sector corruption and the rest of that sector is
// abandoned, rather than trusting the length blindly - a defensive
// addition this project can afford to make since, unlike the reference
// ripper, it has no physical drive read-retry logic backing it up.
struct DemuxResult
{
    std::vector<uint8_t> rawDsd; // concatenated complete, uncompressed frames
    bool sawDst = false;
    bool sawRawDsd = false;
};

DemuxResult demuxTrackAudio(QFile &iso, int64_t startLsn, int64_t lengthLsn, int areaChannelCount,
                             const std::function<void(int percent)> &onProgress = nullptr)
{
    DemuxResult result;
    std::vector<uint8_t> curFrame;
    bool frameStarted = false;
    bool frameIsDst = false;
    int lastReportedPercent = -1;

    auto finalizeFrame = [&]() {
        if (frameStarted) {
            if (frameIsDst) {
                if (!curFrame.empty())
                    result.sawDst = true;
            } else if (static_cast<int>(curFrame.size()) == areaChannelCount * kFrameSize64) {
                result.rawDsd.insert(result.rawDsd.end(), curFrame.begin(), curFrame.end());
                result.sawRawDsd = true;
            }
            // A frame whose finished size doesn't match exactly is silently
            // dropped, same as the reference implementation's own check.
        }
        curFrame.clear();
        frameStarted = false;
    };

    if (onProgress)
        onProgress(0); // guarantee a 0% callback even for a very short track

    // Sectors are read strictly sequentially here (i increases by 1 every
    // iteration, never seeking backward), but the loop originally fetched
    // one 2048-byte sector per readLsn() call - one QFile::seek()+read()
    // round trip per sector, so a multi-minute SACD track (tens to
    // hundreds of thousands of sectors) paid that call overhead that many
    // times over. Pulling kReadChunkLsns sectors at once into an in-memory
    // chunk and handing out pointers into it cuts that by the same factor
    // with no change to the parsing below - only how the bytes get into
    // memory changes.
    constexpr int64_t kReadChunkLsns = 2048; // 4 MiB at a time
    QByteArray chunkBuf;
    int64_t chunkStartLsn = -1;
    int64_t chunkLsnCount = 0;

    for (int64_t i = 0; i < lengthLsn; ++i) {
        if (onProgress && lengthLsn > 0) {
            const int pct = static_cast<int>((i * 100) / lengthLsn);
            if (pct != lastReportedPercent) {
                lastReportedPercent = pct;
                onProgress(pct);
            }
        }
        const int64_t lsn = startLsn + i;
        if (chunkStartLsn < 0 || lsn < chunkStartLsn || lsn >= chunkStartLsn + chunkLsnCount) {
            if (!iso.seek(lsn * kLsnSize))
                break;
            chunkBuf = iso.read(kReadChunkLsns * kLsnSize);
            chunkStartLsn = lsn;
            chunkLsnCount = chunkBuf.size() / kLsnSize;
            if (chunkLsnCount == 0)
                break; // short/failed read - nothing more to demux
        }
        const uchar *s = reinterpret_cast<const uchar *>(chunkBuf.constData())
            + (lsn - chunkStartLsn) * kLsnSize;
        int pos = 0;

        const uint8_t header = s[pos++];
        const bool dstEncoded = (header & 1) != 0;
        const int frameInfoCount = (header >> 2) & 7;
        const int packetInfoCount = (header >> 5) & 7;
        if (packetInfoCount > 7 || pos + packetInfoCount * 2 > kLsnSize)
            continue;

        struct Packet { bool frameStart; int dataType; int length; };
        std::array<Packet, 7> packets{};
        for (int k = 0; k < packetInfoCount; ++k) {
            const uint8_t b0 = s[pos];
            const uint8_t b1 = s[pos + 1];
            pos += 2;
            packets[static_cast<size_t>(k)] = {
                (b0 & 0x80) != 0,
                (b0 >> 3) & 7,
                ((b0 & 7) << 8) | b1,
            };
        }

        // Frame-info entries' contents (timecode, and - DST only - channel
        // bits/sector count) aren't needed: this importer only ever accepts
        // raw-DSD frames whose channel count it already knows from the Area
        // TOC, and DST frames are never decoded, only counted. Still have to
        // skip over them correctly to reach the packet payloads that follow.
        const int frameInfoEntrySize = dstEncoded ? 4 : 3;
        pos += frameInfoCount * frameInfoEntrySize;
        if (pos > kLsnSize)
            continue;

        for (int k = 0; k < packetInfoCount; ++k) {
            const Packet &pk = packets[static_cast<size_t>(k)];
            if (pos + pk.length > kLsnSize)
                break; // corrupt packet length - abandon the rest of this sector
            if (pk.length <= kMaxPacketSize && pk.dataType == kDataTypeAudio) {
                if (pk.frameStart) {
                    finalizeFrame();
                    frameStarted = true;
                    frameIsDst = dstEncoded;
                }
                if (frameStarted)
                    curFrame.insert(curFrame.end(), s + pos, s + pos + pk.length);
            }
            pos += pk.length;
        }
    }
    finalizeFrame();
    if (onProgress && lastReportedPercent != 100)
        onProgress(100);
    return result;
}

// ---------------------------------------------------------------------------
// CD-DA (raw audio CD image) + optional sibling .cue
// ---------------------------------------------------------------------------

constexpr int kCddaBytesPerFrame = 2352; // one CD sector's worth of 44.1kHz/16-bit/stereo PCM
constexpr int kCddaFramesPerSecond = 75;

// Parses "mm:ss:ff" into a byte offset from the start of the image.
bool parseCueTimecode(const QString &s, int64_t *byteOffset)
{
    const QRegularExpression re(QStringLiteral("^(\\d+):(\\d{2}):(\\d{2})$"));
    const QRegularExpressionMatch m = re.match(s.trimmed());
    if (!m.hasMatch())
        return false;
    const int64_t mm = m.captured(1).toLongLong();
    const int64_t ss = m.captured(2).toLongLong();
    const int64_t ff = m.captured(3).toLongLong();
    *byteOffset = ((mm * 60 + ss) * kCddaFramesPerSecond + ff) * kCddaBytesPerFrame;
    return true;
}

// Minimal .cue parser: only what's needed to split a raw CDDA image into
// tracks - TRACK NN AUDIO headers and each track's INDEX 01 (the audible
// start; INDEX 00 pre-gaps are folded into the previous track, the usual
// convention burning/ripping tools already follow). Ignores everything
// else (REM comments, CATALOG, PERFORMER/TITLE text, non-AUDIO tracks).
std::vector<IsoAudioExtractor::TrackInfo> parseCueSheet(const QString &cuePath, int64_t isoSize)
{
    std::vector<IsoAudioExtractor::TrackInfo> tracks;
    QFile f(cuePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return tracks;

    const QRegularExpression trackRe(QStringLiteral("^\\s*TRACK\\s+(\\d+)\\s+AUDIO"),
                                      QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression indexRe(QStringLiteral("^\\s*INDEX\\s+01\\s+(\\S+)"),
                                      QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression titleRe(QStringLiteral("^\\s*TITLE\\s+\"(.*)\""),
                                      QRegularExpression::CaseInsensitiveOption);

    int currentTrackNumber = 0;
    QString currentTitle;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine());
        if (const auto m = trackRe.match(line); m.hasMatch()) {
            currentTrackNumber = m.captured(1).toInt();
            currentTitle.clear();
        } else if (const auto m = titleRe.match(line); m.hasMatch() && currentTrackNumber > 0) {
            // A TITLE line belonging to the TRACK block we're currently in
            // (a global album TITLE before the first TRACK line, if any,
            // is simply skipped - currentTrackNumber is still 0 there).
            currentTitle = m.captured(1);
        } else if (const auto m = indexRe.match(line); m.hasMatch() && currentTrackNumber > 0) {
            int64_t byteOffset = 0;
            if (parseCueTimecode(m.captured(1), &byteOffset)) {
                IsoAudioExtractor::TrackInfo t;
                t.number = currentTrackNumber;
                t.title = currentTitle.isEmpty() ? QStringLiteral("Track %1").arg(currentTrackNumber, 2, 10, QChar('0'))
                                                  : currentTitle;
                t.kind = IsoAudioExtractor::SourceKind::CddaPcm;
                t.startByte = byteOffset;
                tracks.push_back(t);
                currentTrackNumber = 0; // one INDEX 01 per TRACK block is all this parser uses
            }
        }
    }

    std::sort(tracks.begin(), tracks.end(),
              [](const auto &a, const auto &b) { return a.startByte < b.startByte; });
    for (size_t i = 0; i < tracks.size(); ++i) {
        const int64_t end = (i + 1 < tracks.size()) ? tracks[i + 1].startByte : isoSize;
        tracks[i].lengthByte = std::max<int64_t>(0, end - tracks[i].startByte);
    }
    // Drop anything the .cue claims starts beyond the actual file - a
    // mismatched .cue/.iso pair (wrong file, edited after ripping, etc.)
    // shouldn't produce a bogus zero-length or out-of-range track.
    tracks.erase(std::remove_if(tracks.begin(), tracks.end(),
                                 [&](const auto &t) { return t.startByte >= isoSize || t.lengthByte <= 0; }),
                 tracks.end());
    return tracks;
}

// ---------------------------------------------------------------------------
// PCM -> FLAC via this app's own decoder (SACD path) or directly (CDDA path)
// ---------------------------------------------------------------------------

// Synchronously decodes an audio file (here, always the temporary .dsf
// this importer just wrote) to interleaved PCM, requesting a specific
// output format up front exactly the way AudioEngine.cpp's own DSF
// handling does - see its comment on why that's needed for DSD sources
// specifically (native DSD rates are far outside what most PCM consumers,
// decoders included, handle predictably without an explicit target
// format).
bool decodeToInt32Pcm(const QString &path, int targetSampleRate, int targetChannels,
                      std::vector<int32_t> &outInterleaved, int outBitsPerSample, QString *error)
{
    QAudioDecoder decoder;
    QAudioFormat fmt;
    fmt.setSampleRate(targetSampleRate);
    fmt.setChannelCount(targetChannels);
    fmt.setSampleFormat(QAudioFormat::Float);
    decoder.setAudioFormat(fmt);
    decoder.setSource(QUrl::fromLocalFile(path));

    bool decodeError = false;
    QString decodeErrorString;
    const double scale = (outBitsPerSample == 24) ? 8388607.0 : 32767.0;
    const int32_t clampLo = (outBitsPerSample == 24) ? -8388608 : -32768;
    const int32_t clampHi = (outBitsPerSample == 24) ? 8388607 : 32767;

    QEventLoop loop;
    QObject::connect(&decoder, &QAudioDecoder::bufferReady, &decoder, [&]() {
        QAudioBuffer buf = decoder.read();
        if (!buf.isValid() || buf.format().sampleFormat() != QAudioFormat::Float)
            return;
        const float *data = buf.constData<float>();
        const int n = buf.frameCount() * buf.format().channelCount();
        outInterleaved.reserve(outInterleaved.size() + static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            const double v = static_cast<double>(data[i]) * scale;
            const int32_t s = static_cast<int32_t>(std::lround(std::clamp(v, static_cast<double>(clampLo),
                                                                           static_cast<double>(clampHi))));
            outInterleaved.push_back(s);
        }
    });
    QObject::connect(&decoder, &QAudioDecoder::finished, &loop, &QEventLoop::quit);
    QObject::connect(&decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error), &decoder,
                      [&](QAudioDecoder::Error) {
                          decodeError = true;
                          decodeErrorString = decoder.errorString();
                          loop.quit();
                      });
    decoder.start();
    loop.exec();

    if (decodeError) {
        if (error) *error = decodeErrorString.isEmpty()
                                 ? QStringLiteral("Could not decode the extracted DSD audio.")
                                 : decodeErrorString;
        return false;
    }
    return true;
}

} // namespace

namespace IsoAudioExtractor {

OpenResult open(const QString &isoPath)
{
    OpenResult result;
    QFile iso(isoPath);
    if (!iso.open(QIODevice::ReadOnly)) {
        result.errorMessage = QStringLiteral("Could not open \"%1\".").arg(isoPath);
        return result;
    }

    // --- Try SACD first -----------------------------------------------------
    SacdArea area;
    QString sacdError;
    if (readMasterToc(iso, area, &sacdError)) {
        if (!readAreaToc(iso, area, &sacdError)) {
            result.errorMessage = sacdError;
            return result;
        }
        std::vector<int64_t> startLsn, lengthLsn;
        if (!readTrackList(iso, area, startLsn, lengthLsn)) {
            result.errorMessage = QStringLiteral("Could not find the SACD track list (SACDTRL1).");
            return result;
        }

        result.formatLabel = QStringLiteral("SACD ISO (2-channel)");
        for (int i = 0; i < area.trackCount; ++i) {
            TrackInfo t;
            t.number = i + 1;
            t.title = QStringLiteral("Track %1").arg(t.number, 2, 10, QChar('0'));
            t.startLsn = startLsn[static_cast<size_t>(i)];
            t.lengthLsn = lengthLsn[static_cast<size_t>(i)];
            // The actual DST-vs-raw-DSD determination happens per-sector
            // during extraction (see demuxTrackAudio) - the on-disc frame
            // format bit lives in the audio data itself, not the track
            // table, so kind is provisionally SacdRawDsd here and corrected
            // (or the track is rejected) once extraction actually runs.
            t.kind = SourceKind::SacdRawDsd;
            result.tracks.push_back(t);
        }
        result.ok = !result.tracks.empty();
        if (!result.ok)
            result.errorMessage = QStringLiteral("No tracks found in the SACD 2-channel area.");
        return result;
    }
    // Not a SACD ISO (readMasterToc returned false without an error message
    // for "wrong signature", or with one for "found the signature but
    // something after it didn't parse" - only the latter is worth bailing
    // out for; a plain signature mismatch just means "try CD-DA instead").
    if (!sacdError.isEmpty()) {
        result.errorMessage = sacdError;
        return result;
    }

    // --- Fall back to raw CD-DA ---------------------------------------------
    const int64_t isoSize = iso.size();
    if (isoSize < kCddaBytesPerFrame) {
        result.errorMessage = QStringLiteral("This file is too small to be a CD-DA image and isn't a "
                                              "recognized SACD ISO.");
        return result;
    }

    const QFileInfo fi(isoPath);
    const QString cuePath = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".cue"));
    std::vector<TrackInfo> cueTracks;
    if (QFile::exists(cuePath))
        cueTracks = parseCueSheet(cuePath, isoSize);

    result.formatLabel = QStringLiteral("CD-DA image") + (cueTracks.empty() ? QString() : QStringLiteral(" (+.cue)"));
    if (!cueTracks.empty()) {
        result.tracks = std::move(cueTracks);
    } else {
        TrackInfo t;
        t.number = 1;
        t.title = fi.completeBaseName();
        t.kind = SourceKind::CddaPcm;
        t.startByte = 0;
        t.lengthByte = isoSize - (isoSize % 4); // trim to a whole 16-bit-stereo sample
        result.tracks.push_back(t);
    }
    result.ok = true;
    return result;
}

ExtractResult extractTrackToFlac(const QString &isoPath, const TrackInfo &track,
                                  const QString &outFlacPath, const QString &tempDir,
                                  const ProgressCallback &onProgress)
{
    ExtractResult result;

    if (track.kind == SourceKind::SacdDst) {
        result.errorMessage = QStringLiteral("This track is DST-compressed, which this app cannot decode.");
        return result;
    }

    QFile iso(isoPath);
    if (!iso.open(QIODevice::ReadOnly)) {
        result.errorMessage = QStringLiteral("Could not open \"%1\".").arg(isoPath);
        return result;
    }

    if (track.kind == SourceKind::CddaPcm) {
        if (onProgress)
            onProgress(ProgressPhase::ReadingCdda, 0);
        if (!iso.seek(track.startByte)) {
            result.errorMessage = QStringLiteral("Could not seek to track %1's data.").arg(track.number);
            return result;
        }
        const QByteArray raw = iso.read(track.lengthByte);
        if (raw.size() < 4) {
            result.errorMessage = QStringLiteral("Track %1 has no audio data.").arg(track.number);
            return result;
        }
        const int sampleCount = raw.size() / 2;
        std::vector<int32_t> pcm(static_cast<size_t>(sampleCount));
        const uchar *d = reinterpret_cast<const uchar *>(raw.constData());
        for (int i = 0; i < sampleCount; ++i) {
            const int16_t s = static_cast<int16_t>(d[i * 2] | (d[i * 2 + 1] << 8)); // little-endian
            pcm[static_cast<size_t>(i)] = s;
        }
        if (onProgress)
            onProgress(ProgressPhase::ReadingCdda, 100);

        QString encodeError;
        const auto encodeProgress = onProgress
            ? std::function<void(int)>([&](int p) { onProgress(ProgressPhase::EncodingFlac, p); })
            : std::function<void(int)>();
        if (!FlacEncoder::encode(outFlacPath, pcm, 44100, 2, 16, &encodeError, encodeProgress)) {
            result.errorMessage = encodeError;
            return result;
        }
        result.ok = true;
        return result;
    }

    // --- SACD raw-DSD path ---------------------------------------------------
    const auto demuxProgress = onProgress
        ? std::function<void(int)>([&](int p) { onProgress(ProgressPhase::ReadingSacdAudio, p); })
        : std::function<void(int)>();
    const DemuxResult demux =
        demuxTrackAudio(iso, track.startLsn, track.lengthLsn, /*areaChannelCount=*/2, demuxProgress);
    if (demux.sawDst && !demux.sawRawDsd) {
        result.errorMessage = QStringLiteral("Track %1 is DST-compressed, which this app cannot decode.")
                                   .arg(track.number);
        return result;
    }
    if (demux.rawDsd.empty()) {
        result.errorMessage = QStringLiteral("No decodable audio found for track %1.").arg(track.number);
        return result;
    }

    if (onProgress)
        onProgress(ProgressPhase::WritingDsf, 0);
    const QString tempDsfPath = QDir(tempDir).filePath(
        QStringLiteral("iso_import_track%1.dsf").arg(track.number, 2, 10, QChar('0')));
    QString dsfError;
    if (!DsfWriter::writeFromInterleavedFrames(tempDsfPath, demux.rawDsd, 2, kFrameSize64,
                                                2822400.0, &dsfError)) {
        result.errorMessage = dsfError;
        return result;
    }
    if (onProgress)
        onProgress(ProgressPhase::WritingDsf, 100);

    if (onProgress)
        onProgress(ProgressPhase::DecodingDsd, 0);
    std::vector<int32_t> pcm;
    QString decodeError;
    const bool decoded = decodeToInt32Pcm(tempDsfPath, 88200, 2, pcm, 24, &decodeError);
    QFile::remove(tempDsfPath);

    if (!decoded) {
        result.errorMessage = decodeError;
        return result;
    }
    if (pcm.empty()) {
        result.errorMessage = QStringLiteral("Decoding track %1 produced no audio.").arg(track.number);
        return result;
    }
    if (onProgress)
        onProgress(ProgressPhase::DecodingDsd, 100);

    QString encodeError;
    const auto encodeProgress = onProgress
        ? std::function<void(int)>([&](int p) { onProgress(ProgressPhase::EncodingFlac, p); })
        : std::function<void(int)>();
    if (!FlacEncoder::encode(outFlacPath, pcm, 88200, 2, 24, &encodeError, encodeProgress)) {
        result.errorMessage = encodeError;
        return result;
    }
    result.ok = true;
    return result;
}

} // namespace IsoAudioExtractor
