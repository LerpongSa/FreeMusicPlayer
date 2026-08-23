#pragma once
//
// Playback pipeline: QAudioDecoder decodes the whole track into an
// in-memory interleaved float PCM buffer, then a pull-mode QIODevice feeds
// QAudioSink, running every sample through the Equalizer on the way out.
//
// Why decode-to-memory instead of QMediaPlayer: QMediaPlayer exposes no
// insertion point for an audio effect, so there is no way to place an EQ
// in its pipeline. The trade-off (documented for the user in README.md):
// track changes get a short decode pause, memory use is roughly 10 MB per
// minute of audio, and QAudioDecoder cannot seek - which is exactly why we
// decode fully up front and seek instantly inside memory afterwards.
// Play is only enabled once decoding finishes, which sidesteps the whole
// class of "reading past what's decoded so far" race conditions.
//
#include "Equalizer.h"

#include <QObject>
#include <QAudioDecoder>
#include <QAudioSink>
#include <QAudioFormat>
#include <QString>
#include <QTimer>
#include <QMutex>
#include <QVector>

#include <atomic>
#include <memory>
#include <vector>

class AudioEngine : public QObject
{
    Q_OBJECT

public:
    enum class State { Stopped, Loading, Playing, Paused };
    Q_ENUM(State)

    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine() override;

    Equalizer &equalizer() { return m_equalizer; }
    const Equalizer &equalizer() const { return m_equalizer; }

    void loadFile(const QString &path, bool autoPlay);

    void play();
    void pause();
    void stop();
    void togglePlayPause();

    // ms
    void seek(qint64 positionMs);

    void setVolume(int volumePercent); // 0-100
    int volume() const { return m_volumePercent; }

    void setMuted(bool muted);
    bool isMuted() const { return m_muted; }

    State state() const { return m_state; }
    qint64 durationMs() const { return m_durationMs; }
    qint64 positionMs() const;

    QString currentFilePath() const { return m_currentFilePath; }

    // Read-only access for the visualizer: downmixed mono samples of the
    // decoded (pre-EQ) source, `count` samples ending at the current play
    // position, zero-padded at the start of the track. Safe to call from
    // the UI thread at any time - the PCM buffer is immutable once decoding
    // has finished, which is the only state play back is allowed to start from.
    std::vector<float> recentMonoSamples(int count) const;
    double sampleRate() const { return m_sampleRate; }

signals:
    void stateChanged(AudioEngine::State state);
    void trackLoaded(qint64 durationMs);
    void durationChanged(qint64 durationMs);
    void positionChanged(qint64 positionMs);
    void playbackFinished();
    void errorOccurred(QString message);
    void decodingProgress(int percent);
    void formatDescriptionChanged(QString text); // "MP3 - 320 kbps - 44.1 kHz - Stereo"

    // Internal: emitted from the audio-pull thread inside pullAudio(). Never
    // connect to this from outside AudioEngine. A signal emission is
    // thread-safe by design and, connected with Qt::QueuedConnection to a
    // slot on this object (which lives on the UI thread), is the standard
    // way to hop back onto the UI thread - safer here than
    // QMetaObject::invokeMethod with a bare member-function pointer, which
    // does not accept that form, or a raw QTimer::singleShot call made from
    // a thread that isn't running a full app event loop.
    void audioThreadReachedEnd();

private slots:
    void onDecoderBufferReady();
    void onDecoderFinished();
    void onDecoderError(QAudioDecoder::Error error);
    void onDecoderDurationChanged(qint64 durationMs);
    void handlePlaybackEnded();
    void emitPositionTick();

private:
    // Nested class, defined out-of-line in AudioEngine.cpp (no Q_OBJECT
    // needed - it has no signals/slots). No explicit "friend" needed:
    // nested classes have access to the enclosing class's private members
    // since C++11, which is what lets its readData() call pullAudio().
    class PcmIODevice;

    void resetForNewTrack();
    void appendDecodedBuffer(const class QAudioBuffer &buffer);
    void finalizeFormatOnFirstBuffer(const class QAudioBuffer &buffer);
    void startPlaybackDevice();
    void updateFormatDescription();
    void setState(State s);

    qint64 pullAudio(char *data, qint64 maxSize); // called from the audio thread

    std::unique_ptr<QAudioDecoder> m_decoder;
    std::unique_ptr<QAudioSink> m_sink;
    std::unique_ptr<PcmIODevice> m_ioDevice;
    QTimer m_positionTimer;

    Equalizer m_equalizer;

    // Decoded PCM. Grown only during decode (main thread); read-only once
    // m_ready is true, which is also the only state playback may begin in.
    std::vector<float> m_pcm; // interleaved
    int m_channelCount = 2;
    double m_sampleRate = 44100.0;
    qint64 m_totalFrames = 0;
    qint64 m_reservedFrames = 0;

    bool m_formatEstablished = false;
    bool m_ready = false;
    bool m_pendingAutoPlay = false;

    // Written from the audio-pull thread in pullAudio(), read/reset from
    // the UI thread (seek(), play()) - plain bool would be a data race.
    std::atomic<bool> m_endSignaled{false};

    std::atomic<qint64> m_frameCursor{0};

    State m_state = State::Stopped;
    QString m_currentFilePath;
    qint64 m_durationMs = 0;

    int m_volumePercent = 70;

    // Read from the audio-pull thread in pullAudio() (to silence output),
    // written from the UI thread in setMuted() - needs to be atomic for
    // the same reason m_endSignaled is.
    std::atomic<bool> m_muted{false};

    qint64 m_fileSizeBytes = 0;
    QString m_containerHint;

    static constexpr int kMaxBuffersPerPass = 8;
    bool m_drainScheduled = false;
};
