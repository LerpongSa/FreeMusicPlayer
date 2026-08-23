#pragma once
//
// Standard 10-band graphic equalizer (31 Hz - 16 kHz octave-ish spacing),
// with 8 built-in presets plus a "Custom" slot the user can dial in and
// have persisted. Runs one BiquadPeakingFilter per band per channel;
// AudioEngine calls process() per-sample from the audio thread.
//
#include "BiquadFilter.h"

#include <QString>
#include <QVector>
#include <QMutex>
#include <array>
#include <vector>

class Equalizer
{
public:
    static constexpr int kBandCount = 10;

    struct Preset
    {
        QString name;
        std::array<double, kBandCount> gainsDb;
    };

    Equalizer();

    static const std::array<double, kBandCount> &bandFrequencies();
    static const QVector<Preset> &builtinPresets(); // 8 defaults, see .cpp

    void setSampleRate(double sampleRate);
    void setChannelCount(int channels);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // Applies a named built-in preset ("Flat", "Pop", ...). Returns false if
    // the name is not one of builtinPresets().
    bool applyPreset(const QString &name);

    // Sets an arbitrary custom curve (marks current preset as "Custom").
    void setCustomGains(const std::array<double, kBandCount> &gainsDb);
    void setBandGain(int band, double gainDb);

    QString currentPresetName() const { return m_currentPresetName; }
    std::array<double, kBandCount> currentGains() const { return m_gainsDb; }

    // Resets filter delay lines (call on seek/track change to avoid pops).
    void resetState();

    // Coefficients and filter delay-line state are written from the UI
    // thread (applyPreset/setBandGain/setCustomGains/resetState) and read
    // *and* written from the audio-pull thread (processSample, which
    // advances each biquad's delay line every call). AudioEngine::pullAudio
    // takes this lock once per audio block - not per sample, since that
    // would be needless overhead - and calls processSample() only while
    // holding it; every UI-thread mutator below takes it too.
    QMutex &mutex() { return m_mutex; }

    // Processes one interleaved-channel sample in place. Caller MUST hold
    // mutex() for the duration of the block this is used in.
    inline float processSample(int channel, float sample)
    {
        if (!m_enabled || channel < 0 || channel >= static_cast<int>(m_filters.size()))
            return sample;
        float out = sample;
        for (auto &band : m_filters[channel])
            out = band.process(out);
        // Soft clamp to avoid harsh digital clipping when several bands boost together.
        if (out > 1.0f) out = 1.0f;
        if (out < -1.0f) out = -1.0f;
        return out;
    }

private:
    void rebuildFilters();

    bool m_enabled = true;
    double m_sampleRate = 44100.0;
    int m_channelCount = 2;
    QString m_currentPresetName = "Flat";
    std::array<double, kBandCount> m_gainsDb{};

    // [channel][band]
    std::vector<std::array<BiquadPeakingFilter, kBandCount>> m_filters;

    QMutex m_mutex;
};
