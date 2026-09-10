#include "AudioEngine.h"

#include <QAudioBuffer>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QUrl>
#include <QMutexLocker>
#include <QDebug>

#include <algorithm>
#include <cstring>
#include <limits>

// ---------------------------------------------------------------------------
// PcmIODevice: pull-mode source for QAudioSink. No signals/slots needed, so
// no Q_OBJECT and no moc involvement - just virtual overrides.
// ---------------------------------------------------------------------------
class AudioEngine::PcmIODevice : public QIODevice
{
public:
    explicit PcmIODevice(AudioEngine *engine, QObject *parent = nullptr)
        : QIODevice(parent), m_engine(engine)
    {
    }

    bool isSequential() const override { return true; }

    qint64 bytesAvailable() const override
    {
        // Effectively unlimited from the sink's point of view; we always
        // have data or silence to hand back until the track truly ends.
        return std::numeric_limits<qint64>::max() / 4;
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        return m_engine->pullAudio(data, maxSize);
    }

    qint64 writeData(const char *, qint64) override { return -1; }

private:
    AudioEngine *m_engine;
};

// ---------------------------------------------------------------------------

namespace {

float sampleToFloat(QAudioFormat::SampleFormat fmt, const void *ptr)
{
    switch (fmt) {
    case QAudioFormat::UInt8:
        return (static_cast<int>(*static_cast<const quint8 *>(ptr)) - 128) / 128.0f;
    case QAudioFormat::Int16:
        return *static_cast<const qint16 *>(ptr) / 32768.0f;
    case QAudioFormat::Int32:
        return *static_cast<const qint32 *>(ptr) / 2147483648.0f;
    case QAudioFormat::Float:
        return *static_cast<const float *>(ptr);
    default:
        return 0.0f;
    }
}

int bytesPerSample(QAudioFormat::SampleFormat fmt)
{
    switch (fmt) {
    case QAudioFormat::UInt8: return 1;
    case QAudioFormat::Int16: return 2;
    case QAudioFormat::Int32: return 4;
    case QAudioFormat::Float: return 4;
    default: return 0;
    }
}

// ---------------------------------------------------------------------------
// ALAC (Apple Lossless) container sniffing.
//
// ALAC audio is carried inside an MP4/M4A wrapper - the exact same
// container, and usually the exact same ".m4a" extension, that lossy AAC
// uses - so the file extension alone can't tell the two apart. Playback
// needs nothing extra (Qt Multimedia's FFmpeg backend already decodes
// ALAC); this sniff exists purely so the on-screen format line can say
// "ALAC" instead of a generic "M4A", which matters for a player whose
// point is lossless audio. Detection walks straight to the codec's
// four-char id in the container's sample-description table and never
// touches the audio data.
// ---------------------------------------------------------------------------

quint32 beUint32(const uchar *p)
{
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
}

// Locates a direct child box named `name` within the byte range
// [start, end) of an open MP4 file, returning its payload range. Copes
// with the 64-bit extended-size header (32-bit size field == 1) and with
// the "to end of parent" form (size field == 0) so a large 'mdat' laid
// out before 'moov' doesn't throw the walk off.
bool findMp4Child(QFile &f, qint64 start, qint64 end, const char *name,
                  qint64 &payloadStart, qint64 &payloadEnd)
{
    qint64 pos = start;
    while (pos + 8 <= end) {
        if (!f.seek(pos))
            return false;
        const QByteArray header = f.read(8);
        if (header.size() < 8)
            return false;
        const uchar *h = reinterpret_cast<const uchar *>(header.constData());
        qint64 boxSize = beUint32(h);
        qint64 headerLen = 8;
        if (boxSize == 1) {
            const QByteArray ext = f.read(8);
            if (ext.size() < 8)
                return false;
            const uchar *e = reinterpret_cast<const uchar *>(ext.constData());
            boxSize = (static_cast<qint64>(beUint32(e)) << 32) | beUint32(e + 4);
            headerLen = 16;
        } else if (boxSize == 0) {
            boxSize = end - pos;
        }
        if (boxSize < headerLen || pos + boxSize > end)
            return false;
        if (std::memcmp(header.constData() + 4, name, 4) == 0) {
            payloadStart = pos + headerLen;
            payloadEnd = pos + boxSize;
            return true;
        }
        pos += boxSize;
    }
    return false;
}

bool mp4ContainsAlac(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const qint64 fileSize = f.size();

    qint64 s = 0, e = fileSize;
    // moov > trak > mdia > minf > stbl > stsd, then an 'alac' sample entry.
    for (const char *box : {"moov", "trak", "mdia", "minf", "stbl", "stsd"}) {
        if (!findMp4Child(f, s, e, box, s, e))
            return false;
    }
    // 'stsd' payload is a 4-byte version/flags word + 4-byte entry count
    // before the sample-entry boxes begin.
    qint64 entryStart = 0, entryEnd = 0;
    return findMp4Child(f, s + 8, e, "alac", entryStart, entryEnd);
}

// Core Audio Format: an 8-byte file header ("caff" + version/flags) then
// chunks of a 4-byte type + 8-byte big-endian int64 size. The 'desc'
// chunk's mFormatID (4 bytes at offset 8, past the 8-byte mSampleRate
// double) is 'alac' for an Apple Lossless stream.
bool cafContainsAlac(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    if (f.read(4) != QByteArrayLiteral("caff"))
        return false;

    qint64 pos = 8;
    const qint64 fileSize = f.size();
    while (pos + 12 <= fileSize) {
        if (!f.seek(pos))
            return false;
        const QByteArray header = f.read(12);
        if (header.size() < 12)
            return false;
        const uchar *sz = reinterpret_cast<const uchar *>(header.constData()) + 4;
        const qint64 chunkSize = (static_cast<qint64>(beUint32(sz)) << 32) | beUint32(sz + 4);
        if (chunkSize < 0)
            return false;
        if (std::memcmp(header.constData(), "desc", 4) == 0) {
            if (!f.seek(pos + 12 + 8))
                return false;
            return f.read(4) == QByteArrayLiteral("alac");
        }
        pos += 12 + chunkSize;
    }
    return false;
}

} // namespace

AudioEngine::AudioEngine(QObject *parent)
    : QObject(parent)
{
    m_decoder = std::make_unique<QAudioDecoder>(this);

    connect(m_decoder.get(), &QAudioDecoder::bufferReady, this, &AudioEngine::onDecoderBufferReady);
    connect(m_decoder.get(), &QAudioDecoder::finished, this, &AudioEngine::onDecoderFinished);
    connect(m_decoder.get(), QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error),
            this, &AudioEngine::onDecoderError);
    connect(m_decoder.get(), &QAudioDecoder::durationChanged, this, &AudioEngine::onDecoderDurationChanged);

    connect(this, &AudioEngine::audioThreadReachedEnd, this, &AudioEngine::handlePlaybackEnded,
            Qt::QueuedConnection);

    m_positionTimer.setInterval(100);
    connect(&m_positionTimer, &QTimer::timeout, this, &AudioEngine::emitPositionTick);
}

AudioEngine::~AudioEngine()
{
    if (m_sink)
        m_sink->stop();
}

void AudioEngine::resetForNewTrack()
{
    if (m_sink) {
        m_sink->stop();
        m_sink.reset();
    }
    m_ioDevice.reset();

    m_pcm.clear();
    m_pcm.shrink_to_fit();
    m_channelCount = 2;
    m_sampleRate = 44100.0;
    m_totalFrames = 0;
    m_reservedFrames = 0;
    m_formatEstablished = false;
    m_ready = false;
    m_endSignaled = false;
    m_frameCursor.store(0, std::memory_order_relaxed);
    m_durationMs = 0;
    m_equalizer.resetState();
    m_positionTimer.stop();
}

void AudioEngine::loadFile(const QString &path, bool autoPlay)
{
    resetForNewTrack();

    m_currentFilePath = path;
    m_pendingAutoPlay = autoPlay;
    m_fileSizeBytes = QFileInfo(path).size();
    m_containerHint = QFileInfo(path).suffix().toUpper();

    // ".m4a"/".m4b"/".alac" all mean "MP4 wrapper" and could hold either
    // lossless ALAC or lossy AAC; ".caf" likewise. Peek at the container's
    // codec id so the format line reflects what's actually inside. Nothing
    // about decoding changes - QAudioDecoder handles ALAC either way.
    if (m_containerHint == QLatin1String("M4A") || m_containerHint == QLatin1String("M4B")
        || m_containerHint == QLatin1String("MP4") || m_containerHint == QLatin1String("ALAC")) {
        if (mp4ContainsAlac(path))
            m_containerHint = QStringLiteral("ALAC");
    } else if (m_containerHint == QLatin1String("CAF")) {
        if (cafContainsAlac(path))
            m_containerHint = QStringLiteral("ALAC");
    }

    setState(State::Loading);

    // DSF (DSD Stream File) decodes through FFmpeg's dsd_lsbf/dsd_msbf
    // decoder, which demodulates DSD's ~2.8/5.6MHz 1-bit stream down to PCM
    // by a fixed decimation factor - the result is still an unusually high
    // PCM rate (e.g. 352.8kHz for ordinary DSD64), well beyond what most
    // Windows audio devices/drivers can open a WASAPI stream at. When that
    // happens, QAudioSink::start() doesn't fail or error out - it just never
    // actually pulls any data, so playback silently does nothing: no sound,
    // no error dialog, and the position never advances even though the
    // Play/Pause button flips correctly (confirmed via user report and
    // screenshot, 2026-08-29 - title/duration/format line all read back
    // correctly, so decoding itself succeeded; only output failed).
    // Fixed by asking the decoder itself (via setAudioFormat(), backed by
    // the already-bundled swresample-5.dll) to resample DSD down to a
    // conventional high-res PCM rate before we ever construct QAudioSink,
    // instead of exposing DSD's native decimated rate to the audio device.
    // A default-constructed (invalid) QAudioFormat means "no preference" -
    // every other container keeps decoding at its own native rate exactly
    // as before. This MUST be reset on every load (not just set once for
    // DSF), since m_decoder is one shared instance reused for every track -
    // otherwise a forced format from a previous DSF track would silently
    // carry over and force-resample the next, unrelated track too.
    QAudioFormat requestedFormat;
    if (m_containerHint == QLatin1String("DSF")) {
        requestedFormat.setSampleRate(88200); // exact /32 of DSD64's 2,822,400Hz - safely within every common device's range
        requestedFormat.setChannelCount(2);
        requestedFormat.setSampleFormat(QAudioFormat::Float);
    }
    m_decoder->setAudioFormat(requestedFormat);

    m_decoder->setSource(QUrl::fromLocalFile(path));
    m_decoder->start();
}

void AudioEngine::onDecoderDurationChanged(qint64 durationMs)
{
    if (durationMs > 0) {
        m_durationMs = durationMs;
        emit durationChanged(m_durationMs);
        updateFormatDescription(); // refresh kbps now that duration is known, not just on trackLoaded
    }
}

void AudioEngine::finalizeFormatOnFirstBuffer(const QAudioBuffer &buffer)
{
    const QAudioFormat fmt = buffer.format();
    m_sampleRate = fmt.sampleRate() > 0 ? fmt.sampleRate() : 44100.0;
    m_channelCount = std::max(1, fmt.channelCount());

    // Reserve once, from the decoder's reported duration, so the growing
    // buffer doesn't repeatedly reallocate+copy across ~14k buffers for a
    // typical 5-minute track (worst case without this: hundreds of GB of
    // cumulative memcpy across a decode).
    qint64 estMs = m_decoder->duration();
    if (estMs <= 0)
        estMs = 5 * 60 * 1000; // unknown duration: reserve for a 5-minute track, vector still grows if wrong
    m_reservedFrames = static_cast<qint64>((estMs / 1000.0) * m_sampleRate) + m_sampleRate;
    m_pcm.reserve(static_cast<size_t>(m_reservedFrames * m_channelCount));

    m_equalizer.setSampleRate(m_sampleRate);
    m_equalizer.setChannelCount(m_channelCount);

    m_formatEstablished = true;
    updateFormatDescription();
}

void AudioEngine::appendDecodedBuffer(const QAudioBuffer &buffer)
{
    if (!buffer.isValid() || buffer.frameCount() <= 0)
        return;

    if (!m_formatEstablished)
        finalizeFormatOnFirstBuffer(buffer);

    const QAudioFormat fmt = buffer.format();
    const int channels = std::max(1, fmt.channelCount());
    const int bps = bytesPerSample(fmt.sampleFormat());
    if (bps == 0)
        return; // unknown/unsupported sample format for this buffer

    // QAudioBuffer's non-template constData() is private in Qt 6.11 - only
    // the templated accessor is public API.
    const auto *base = buffer.constData<quint8>();
    const int frameCount = buffer.frameCount();

    const size_t writeStart = m_pcm.size();
    m_pcm.resize(writeStart + static_cast<size_t>(frameCount) * static_cast<size_t>(m_channelCount));

    for (int f = 0; f < frameCount; ++f) {
        for (int c = 0; c < m_channelCount; ++c) {
            float v = 0.0f;
            if (c < channels) {
                const quint8 *samplePtr = base + (static_cast<size_t>(f) * channels + c) * bps;
                v = sampleToFloat(fmt.sampleFormat(), samplePtr);
            } else if (channels == 1) {
                // Mono source, engine configured stereo: duplicate the one channel.
                const quint8 *samplePtr = base + static_cast<size_t>(f) * channels * bps;
                v = sampleToFloat(fmt.sampleFormat(), samplePtr);
            }
            m_pcm[writeStart + static_cast<size_t>(f) * m_channelCount + c] = v;
        }
    }

    m_totalFrames = static_cast<qint64>(m_pcm.size() / m_channelCount);

    if (m_decoder->duration() > 0) {
        const qint64 decodedMs = static_cast<qint64>((m_totalFrames / m_sampleRate) * 1000.0);
        const int pct = static_cast<int>(std::clamp<qint64>(decodedMs * 100 / std::max<qint64>(1, m_decoder->duration()), 0, 100));
        emit decodingProgress(pct);
    }
}

void AudioEngine::onDecoderBufferReady()
{
    m_drainScheduled = false;

    int processed = 0;
    while (m_decoder->bufferAvailable() && processed++ < kMaxBuffersPerPass) {
        QAudioBuffer buffer = m_decoder->read();
        appendDecodedBuffer(buffer);
    }

    if (m_decoder->bufferAvailable() && !m_drainScheduled) {
        m_drainScheduled = true;
        QTimer::singleShot(0, this, &AudioEngine::onDecoderBufferReady);
    }
}

void AudioEngine::onDecoderFinished()
{
    // Drain anything left.
    while (m_decoder->bufferAvailable()) {
        QAudioBuffer buffer = m_decoder->read();
        appendDecodedBuffer(buffer);
    }

    // Release the decoder's hold on the file now that everything has been
    // copied into m_pcm - playback from here on reads only from that
    // in-memory buffer via QAudioSink, never from the decoder again. On
    // Windows, the Media Foundation backend otherwise keeps an exclusive-ish
    // read handle open on the source for as long as it stays set, which
    // blocks anything else (e.g. saving an edited tag, see TagEditor) from
    // opening the "currently loaded" file for writing - "Access is denied"
    // even though decoding itself finished long ago.
    m_decoder->setSource(QUrl());

    if (m_totalFrames <= 0) {
        emit errorOccurred(tr("No audio data could be decoded from this file."));
        setState(State::Stopped);
        return;
    }

    m_ready = true;
    m_durationMs = static_cast<qint64>((m_totalFrames / m_sampleRate) * 1000.0);
    emit durationChanged(m_durationMs);
    emit decodingProgress(100);
    emit trackLoaded(m_durationMs);
    updateFormatDescription();

    startPlaybackDevice();

    if (m_pendingAutoPlay) {
        m_pendingAutoPlay = false;
        // Defer the actual start-playing call by one event-loop turn instead
        // of calling play() synchronously right here. Confirmed via user
        // report (2026-08-22): calling play() - which calls
        // m_sink->start(m_ioDevice.get()) - immediately within the same call
        // stack as the sink's own construction a few lines above in
        // startPlaybackDevice() reliably left the track "loaded" (title,
        // duration and the 0:00 position all showed up correctly) but never
        // actually audible - the transport stayed on the Play icon and
        // position never advanced, both for every auto-advance to the next
        // track AND for the manual Next/Previous buttons (both go through
        // this same loadFile(path, true) -> onDecoderFinished() path).
        // Pressing Play manually afterward always worked instantly, because
        // by then the sink had already been alive for at least one full
        // event-loop turn. QTimer::singleShot(0, ...) buys exactly that one
        // turn - the same "let the event loop breathe" pattern already used
        // by onDecoderBufferReady()'s drain loop above. Safe against a fast
        // double-advance in the meantime: play() re-checks m_ready and
        // m_sink, so if a newer loadFile() already reset both by the time
        // this fires, it correctly no-ops (or re-arms m_pendingAutoPlay)
        // instead of resuming stale state.
        QTimer::singleShot(0, this, &AudioEngine::play);
    } else {
        setState(State::Paused);
    }
}

void AudioEngine::onDecoderError(QAudioDecoder::Error error)
{
    if (error == QAudioDecoder::NoError)
        return;
    emit errorOccurred(m_decoder->errorString().isEmpty()
                            ? tr("Could not decode this audio file.")
                            : m_decoder->errorString());
    setState(State::Stopped);
}

void AudioEngine::startPlaybackDevice()
{
    QAudioFormat fmt;
    fmt.setSampleRate(static_cast<int>(m_sampleRate));
    fmt.setChannelCount(m_channelCount);
    fmt.setSampleFormat(QAudioFormat::Float);

    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!device.isFormatSupported(fmt)) {
        // Fall back to the device's preferred format's rate/layout is a
        // bigger change than this app needs to make; most desktop audio
        // stacks (WASAPI shared mode, PulseAudio, CoreAudio) accept
        // arbitrary sample rates and resample internally. We proceed with
        // the requested format and surface a warning if construction fails.
        qWarning() << "Default output device does not natively advertise support for" << fmt
                   << "- attempting anyway.";
    }

    m_sink = std::make_unique<QAudioSink>(device, fmt, this);
    m_sink->setVolume(m_muted ? 0.0 : m_volumePercent / 100.0);

    connect(m_sink.get(), &QAudioSink::stateChanged, this, [this](QAudio::State s) {
        if (s == QAudio::IdleState && m_state == State::Playing) {
            // Sink ran dry - our own end-of-track signal (emitted from
            // pullAudio) is the authoritative source of truth, so this is
            // just a safety net; nothing to do here.
        } else if (s == QAudio::StoppedState && m_sink && m_sink->error() != QAudio::NoError) {
            emit errorOccurred(tr("Audio output error."));
        }
    });

    m_ioDevice = std::make_unique<PcmIODevice>(this);
    m_ioDevice->open(QIODevice::ReadOnly);
}

qint64 AudioEngine::pullAudio(char *data, qint64 maxSize)
{
    const int bytesPerFrame = m_channelCount * static_cast<int>(sizeof(float));
    if (bytesPerFrame <= 0)
        return 0;

    const qint64 framesRequested = maxSize / bytesPerFrame;
    if (framesRequested <= 0)
        return 0;

    const qint64 frame = m_frameCursor.load(std::memory_order_relaxed);
    const qint64 framesAvailable = std::max<qint64>(0, m_totalFrames - frame);
    const qint64 framesToCopy = std::min(framesRequested, framesAvailable);

    auto *out = reinterpret_cast<float *>(data);

    if (framesToCopy > 0) {
        // Locked once for the whole block, not per sample: the UI thread
        // can mutate EQ coefficients/delay-line state concurrently
        // (applyPreset/setBandGain/resetState), so processSample() must
        // only ever run while this is held.
        QMutexLocker eqLock(&m_equalizer.mutex());
        const float *src = m_pcm.data() + frame * m_channelCount;
        for (qint64 f = 0; f < framesToCopy; ++f) {
            for (int c = 0; c < m_channelCount; ++c) {
                float s = src[f * m_channelCount + c];
                s = m_equalizer.processSample(c, s);
                out[f * m_channelCount + c] = m_muted ? 0.0f : s;
            }
        }
    }

    for (qint64 f = framesToCopy; f < framesRequested; ++f)
        for (int c = 0; c < m_channelCount; ++c)
            out[f * m_channelCount + c] = 0.0f;

    m_frameCursor.store(frame + framesToCopy, std::memory_order_relaxed);

    if (framesToCopy < framesRequested && (frame + framesToCopy) >= m_totalFrames && !m_endSignaled) {
        m_endSignaled = true;
        emit audioThreadReachedEnd();
    }

    return framesRequested * bytesPerFrame;
}

void AudioEngine::handlePlaybackEnded()
{
    m_positionTimer.stop();
    setState(State::Stopped);
    emit positionChanged(m_durationMs);
    emit playbackFinished();
}

void AudioEngine::play()
{
    if (!m_ready) {
        m_pendingAutoPlay = true; // still decoding; play as soon as it's ready
        return;
    }
    if (!m_sink)
        return;

    if (m_state == State::Stopped && m_frameCursor.load() >= m_totalFrames) {
        m_frameCursor.store(0, std::memory_order_relaxed);
        m_equalizer.resetState();
        m_endSignaled = false;
    }

    m_sink->start(m_ioDevice.get());
    m_positionTimer.start();
    setState(State::Playing);
}

void AudioEngine::pause()
{
    if (!m_sink)
        return;
    m_sink->stop();
    m_positionTimer.stop();
    setState(State::Paused);
}

void AudioEngine::stop()
{
    if (m_sink)
        m_sink->stop();
    m_positionTimer.stop();
    m_frameCursor.store(0, std::memory_order_relaxed);
    m_equalizer.resetState();
    m_endSignaled = false;
    setState(State::Stopped);
    emit positionChanged(0);
}

void AudioEngine::togglePlayPause()
{
    if (m_state == State::Playing)
        pause();
    else
        play();
}

void AudioEngine::seek(qint64 positionMs)
{
    if (!m_ready || m_sampleRate <= 0.0)
        return;

    qint64 frame = static_cast<qint64>((positionMs / 1000.0) * m_sampleRate);
    frame = std::clamp<qint64>(frame, 0, m_totalFrames);

    const bool wasPlaying = (m_state == State::Playing);
    if (m_sink)
        m_sink->stop();

    m_frameCursor.store(frame, std::memory_order_relaxed);
    m_equalizer.resetState(); // avoid an audible pop from stale filter state
    m_endSignaled = false;

    emit positionChanged(positionMs);

    if (wasPlaying && m_sink) {
        m_sink->start(m_ioDevice.get());
    }
}

void AudioEngine::setVolume(int volumePercent)
{
    m_volumePercent = std::clamp(volumePercent, 0, 100);
    if (m_sink && !m_muted)
        m_sink->setVolume(m_volumePercent / 100.0);
}

void AudioEngine::setMuted(bool muted)
{
    m_muted = muted;
    if (m_sink)
        m_sink->setVolume(m_muted ? 0.0 : m_volumePercent / 100.0);
}

qint64 AudioEngine::positionMs() const
{
    if (m_sampleRate <= 0.0)
        return 0;
    const qint64 frame = m_frameCursor.load(std::memory_order_relaxed);
    return static_cast<qint64>((frame / m_sampleRate) * 1000.0);
}

void AudioEngine::emitPositionTick()
{
    emit positionChanged(positionMs());
}

void AudioEngine::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged(s);
}

void AudioEngine::updateFormatDescription()
{
    if (!m_formatEstablished)
        return;

    QString kbpsStr;
    if (m_fileSizeBytes > 0 && m_durationMs > 0) {
        const double kbps = (m_fileSizeBytes * 8.0 / 1000.0) / (m_durationMs / 1000.0);
        kbpsStr = QString::number(static_cast<int>(kbps + 0.5)) + " kbps";
    } else if (m_fileSizeBytes > 0 && m_decoder->duration() > 0) {
        const double kbps = (m_fileSizeBytes * 8.0 / 1000.0) / (m_decoder->duration() / 1000.0);
        kbpsStr = QString::number(static_cast<int>(kbps + 0.5)) + " kbps";
    }

    const QString khz = QString::number(m_sampleRate / 1000.0, 'f', 1) + " kHz";
    const QString ch = m_channelCount >= 2 ? tr("Stereo") : tr("Mono");
    const double mb = m_fileSizeBytes / (1024.0 * 1024.0);

    QStringList parts;
    if (!m_containerHint.isEmpty())
        parts << m_containerHint;
    if (!kbpsStr.isEmpty())
        parts << kbpsStr;
    parts << khz << ch;
    if (m_fileSizeBytes > 0)
        parts << QString::number(mb, 'f', 1) + " MB";

    emit formatDescriptionChanged(parts.join(" · "));
}

std::vector<float> AudioEngine::recentMonoSamples(int count) const
{
    std::vector<float> out(static_cast<size_t>(std::max(0, count)), 0.0f);
    if (!m_ready || count <= 0 || m_channelCount <= 0 || m_pcm.empty())
        return out;

    const qint64 frame = m_frameCursor.load(std::memory_order_relaxed);
    const qint64 start = frame - count;

    for (int i = 0; i < count; ++i) {
        const qint64 srcFrame = start + i;
        if (srcFrame < 0 || srcFrame >= m_totalFrames)
            continue;
        const float *base = m_pcm.data() + srcFrame * m_channelCount;
        float sum = 0.0f;
        for (int c = 0; c < m_channelCount; ++c)
            sum += base[c];
        out[static_cast<size_t>(i)] = sum / static_cast<float>(m_channelCount);
    }
    return out;
}
