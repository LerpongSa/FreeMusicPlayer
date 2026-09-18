#pragma once
//
// Minimal DSF (Sony/Philips "DSD Stream File") writer, used by
// IsoAudioExtractor.cpp to package raw DSD audio demuxed from an SACD ISO
// into a container this app's existing Qt Multimedia (FFmpeg) decoder can
// already open - the same .dsf decode path already used for standalone
// .dsf playback (see AudioEngine.cpp's DSF handling). Writing a small
// container ourselves, then handing it to the already-proven decoder, is
// deliberately chosen over hand-rolling a DSD-to-PCM decimation filter:
// FFmpeg's dsd decoder + swresample already do that correctly (and is
// exactly what this app's own .dsf playback relies on), so reusing it here
// is much lower-risk than a from-scratch decimator this project has no way
// to validate against a reference.
//
// Field layout verified against sacd-ripper's own dsf.h/dsf.c (the
// reference SACD-ISO-to-DSF extractor); see IsoAudioExtractor.cpp's header
// comment for how the source byte layout this writer consumes is derived.
//
#include <QString>

#include <cstdint>
#include <vector>

namespace DsfWriter {

// `interleavedFrames` holds one or more complete "frames" of raw DSD data,
// concatenated back-to-back, where each frame is
// channelCount * bytesPerChannelPerFrame bytes and is byte-interleaved
// round-robin across channels: byte 0 = channel 0's first byte, byte 1 =
// channel 1's first byte (if stereo), ..., byte channelCount = channel 0's
// second byte, and so on for the whole frame. This is exactly the layout
// an SACD "audio sector" demux naturally produces (see
// IsoAudioExtractor.cpp) - the caller does not need to de-interleave
// anything itself.
//
// This writer de-interleaves into per-channel contiguous blocks as it
// writes (channel 0's bytesPerChannelPerFrame bytes, then channel 1's,
// repeated per frame) and bit-reverses every byte, since DSF stores each
// sample's bit LSB-first while SACD's native DSD bit order (like DSDIFF)
// is MSB-first - skipping this reversal would silently produce noise, not
// a decode error, so getting it right matters more than almost anything
// else in this file.
//
// block_size_per_channel is declared in the file as
// bytesPerChannelPerFrame itself rather than DSF's more common fixed 4096,
// so no cross-frame carry buffering is needed (every SACD frame maps to
// exactly one whole DSF block per channel) - verified against this app's
// own QAudioDecoder via test_flac_roundtrip.cpp-style round-trip testing
// that a non-4096 block size decodes correctly.
bool writeFromInterleavedFrames(const QString &outPath,
                                 const std::vector<uint8_t> &interleavedFrames,
                                 int channelCount,
                                 int bytesPerChannelPerFrame,
                                 double sampleRate,
                                 QString *errorMessage = nullptr);

} // namespace DsfWriter
