#pragma once
//
// Minimal, from-scratch, spec-conformant FLAC encoder (RFC 9639 "Free
// Lossless Audio Codec"). Written for the ISO/SACD "Add ISO" import feature
// (see IsoAudioExtractor.h) so that feature has zero dependency on an
// external encoder process or a third-party encoder library - everything
// needed to produce a valid, decodable .flac file lives in this one
// self-contained translation unit, matching this project's existing house
// style of hand-rolled DSP (FFT.h, BiquadFilter.h) rather than pulling in
// a big dependency for something the app can reasonably implement itself.
//
// Scope, deliberately kept modest to bound the risk of an unverifiable
// implementation (there is no reference FLAC encoder or bit-exact test
// vector available in this environment to diff against - correctness here
// rests entirely on a careful reading of RFC 9639):
//   - Fixed block size stream, 4096 samples/block (last block shorter).
//   - Per channel, independently: a CONSTANT subframe when the whole block
//     is one repeated value, otherwise the best of the five fixed
//     predictors (orders 0-4, see RFC 9639 9.2.5) - not the adaptive/
//     general LPC ("linear predictor") subframe type real encoders also
//     use, and not stereo decorrelation (mid-side/left-side/side-right).
//     Every frame ends up smaller than raw PCM (Rice-coded residuals, not
//     verbatim), just not as small as `flac -8` would manage. The order is
//     picked by sum-of-abs-residual (the same cheap proxy libFLAC's own
//     estimate_best_order() uses), not by exhaustively Rice-coding every
//     candidate order and comparing - see sumAbsResidual()'s comment in
//     the .cpp for why that's not a quality trade-off, only a speed one.
//   - One Rice partition per subframe (partition order 0), 4-bit Rice
//     parameter chosen by an estimate-and-refine search (see chooseRiceK())
//     rather than exhaustively trying all 15 valid values - not the
//     partition-order search a full encoder performs, but no less *valid*:
//     any parameter in range still decodes losslessly, this just settles
//     for whichever cross-section of the block is cheapest under a single
//     shared parameter rather than letting different partitions of one
//     block use different parameters.
//   - Supports exactly what the ISO importer needs: mono or stereo, 16 or
//     24 bits per sample, 44100 or 88200 Hz.
//   - STREAMINFO's MD5 of the unencoded audio IS computed (via
//     QCryptographicHash), so a decoder that checks it will find it
//     correct; STREAMINFO's min/max frame size is left as "not known" (0)
//     since that would need a first pass over the encoded output.
//
#include <QString>

#include <cstdint>
#include <functional>
#include <vector>

namespace FlacEncoder {

// Encodes one fully-buffered, interleaved PCM signal to a new .flac file at
// outPath (overwritten if it already exists). `interleavedSamples` holds
// one int32_t per sample, containing the true signed value at
// `bitsPerSample` resolution (e.g. -32768..32767 for 16-bit) - not a
// left-shifted/normalized value - interleaved channel-major (all channels'
// sample 0, then all channels' sample 1, ...), matching the layout
// AudioEngine's own PCM buffer already uses elsewhere in this app.
//
// channelCount: 1 (mono) or 2 (stereo) only - the only cases the ISO
// importer produces. bitsPerSample: 16 or 24 only. sampleRate: 44100 or
// 88200 only (both have a dedicated 4-bit code in the FLAC frame header,
// so no "uncommon sample rate" escape is needed).
//
// Returns false (and, if given, an error message) on an unsupported
// parameter combination or a file I/O failure; the partially-written file
// is left in place in the latter case (the caller already knows the whole
// import failed and will report/skip the track, same as any other
// mid-batch I/O error).
// onProgress, when given, is called synchronously on the caller's own
// thread (never from a background thread of this function's own making -
// there is none) each time encoding progress crosses another whole
// percent, with 0 and 100 both guaranteed to be reported exactly once.
bool encode(const QString &outPath,
            const std::vector<int32_t> &interleavedSamples,
            int sampleRate,
            int channelCount,
            int bitsPerSample,
            QString *errorMessage = nullptr,
            const std::function<void(int percent)> &onProgress = nullptr);

} // namespace FlacEncoder
