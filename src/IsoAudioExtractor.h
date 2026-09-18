#pragma once
//
// Reads audio tracks out of a ".iso" file added via Playlist's "Add ISO..."
// button and converts each one to a standalone .flac file the playlist can
// then load like any other track. Two source formats are recognized, by
// content (never by file extension - a renamed/misnamed .iso should still
// work, same philosophy as CoverArtExtractor's magic-byte sniffing):
//
//  - SACD ISO (a "ScarletBook" disc image): the 2-channel ("TWOCHTOC")
//    audio area's track table is read directly out of the SACD's own
//    on-disc structures, and each track's raw DSD audio is demuxed sector
//    by sector, repackaged into a temporary .dsf file (see DsfWriter.h),
//    decoded back down to PCM through this app's own already-proven
//    QAudioDecoder/FFmpeg pipeline (the same one AudioEngine uses for
//    ordinary .dsf playback), and finally encoded to FLAC (see
//    FlacEncoder.h). DST-compressed tracks (SACD's own lossless codec on
//    top of DSD) are detected and reported as unsupported rather than
//    silently producing garbage or silence - decoding DST correctly would
//    need porting a large, intricate reference decoder this project has no
//    way to validate without real compressed test material, which isn't a
//    risk worth taking for a feature whose whole point is producing
//    trustworthy lossless output. Multichannel SACD audio (the "MULCHTOC"
//    area) is not read at all; only the 2-channel area is, since that's
//    what a normal stereo listening setup wants and it sidesteps ever
//    having to pick a surround-to-stereo downmix.
//
//  - A raw CD-DA (audio CD) image: no filesystem, no SACD signature, just
//    44.1kHz/16-bit/stereo PCM samples back to back. Split into tracks via
//    a same-name ".cue" sheet sitting next to the .iso if one exists
//    (INDEX 01 timestamps only - pre-gaps are folded into the previous
//    track, the usual convention); with no .cue, the whole image becomes
//    one track.
//
// The SACD on-disc structures this reads were verified field-by-field
// against sacd-ripper's own reference implementation (scarletbook.h /
// scarletbook_read.c) - see this feature's accompanying source-controlled
// notes/commit message for the exact fields and byte offsets relied on.
//
#include <QString>
#include <QStringList>

#include <cstdint>
#include <functional>
#include <vector>

namespace IsoAudioExtractor {

enum class SourceKind {
    SacdRawDsd, // SACD 2-channel area, uncompressed DSD - convertible
    SacdDst,    // SACD 2-channel area, DST-compressed - NOT convertible (reported, not attempted)
    CddaPcm,    // raw CD-DA image (whole-file or one .cue-defined track)
};

struct TrackInfo
{
    int number = 0;      // 1-based
    QString title;       // "Track NN" when nothing more specific is known
    SourceKind kind = SourceKind::CddaPcm;

    // SACD addressing (kind != CddaPcm): absolute LSNs (2048-byte logical
    // sectors) straight from the disc's own track table.
    int64_t startLsn = 0;
    int64_t lengthLsn = 0;

    // CDDA addressing (kind == CddaPcm): byte range into the .iso itself.
    int64_t startByte = 0;
    int64_t lengthByte = 0;
};

struct OpenResult
{
    bool ok = false;
    QString errorMessage; // set when !ok
    QString formatLabel;  // e.g. "SACD ISO (2-channel)" / "CD-DA image", for UI display
    std::vector<TrackInfo> tracks;
};

// Sniffs isoPath and enumerates its tracks; reads only table-of-contents /
// header data, no audio.
OpenResult open(const QString &isoPath);

struct ExtractResult
{
    bool ok = false;
    QString errorMessage;
    // True when this track stopped early because isCancelled() (see
    // extractTrackToFlac below) returned true, as opposed to a genuine
    // error - callers should treat the two very differently (a cancelled
    // track isn't a failure to report to the user, just the user's own
    // request taking effect).
    bool cancelled = false;
};

// Coarse phase of one track's extraction, reported through
// extractTrackToFlac()'s onProgress callback below so a caller (see
// IsoImportWorker.h) can show something more informative than a single
// per-track progress step - a whole SACD track can take a genuinely
// noticeable amount of wall-clock time (reading tens of thousands of
// sectors, then decoding, then encoding), not just an instant.
enum class ProgressPhase {
    ReadingSacdAudio, // demuxing raw DSD out of the SACD's audio sectors
    WritingDsf,        // packaging the demuxed DSD into the temporary .dsf (fast; percent is not meaningful)
    DecodingDsd,       // running that .dsf through QAudioDecoder to get PCM (percent is not meaningful)
    ReadingCdda,       // reading the raw PCM byte range for a CD-DA track (fast; percent is not meaningful)
    EncodingFlac,      // FlacEncoder actually writing the output file
};

// Extracts and converts one track (from a TrackInfo returned by open() on
// the same isoPath) straight to a new .flac file at outFlacPath (created,
// overwriting any existing file). tempDir is used for the SACD path's
// intermediate .dsf file, which is deleted again before this returns.
// A TrackInfo with kind == SacdDst always fails with a clear message - see
// the header comment above for why DST is out of scope rather than
// attempted.
//
// onProgress, when given, is called synchronously on the caller's own
// thread (this function does no threading of its own) with a phase and a
// 0-100 percent within that phase. Percent is only meaningful for
// ReadingSacdAudio and EncodingFlac (the two phases whose cost scales with
// how much of the track is left, and so can report incremental progress);
// the other phases are comparatively quick and are just reported once at
// 0 then once at 100 as start/end markers.
using ProgressCallback = std::function<void(ProgressPhase phase, int percent)>;

// isCancelled, when given, is polled at every reasonable opportunity
// throughout extraction - between phases, inside the SACD sector-demux
// loop, inside the QAudioDecoder wait, and inside FlacEncoder::encode()
// itself (see its own isCancelled parameter) - so a cancellation request
// takes effect within roughly one loop iteration's worth of work, not
// only once the whole track finishes on its own. Called synchronously on
// the caller's own thread, same as onProgress.
using CancelCheck = std::function<bool()>;

ExtractResult extractTrackToFlac(const QString &isoPath, const TrackInfo &track,
                                  const QString &outFlacPath, const QString &tempDir,
                                  const ProgressCallback &onProgress = nullptr,
                                  const CancelCheck &isCancelled = nullptr);

} // namespace IsoAudioExtractor
