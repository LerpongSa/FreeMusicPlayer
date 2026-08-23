#include "Equalizer.h"

#include <QMutexLocker>

#include <algorithm>

namespace {
constexpr double kQ = 1.0; // one octave-ish bandwidth per band, matches band spacing
}

Equalizer::Equalizer()
{
    m_gainsDb.fill(0.0);
    setChannelCount(2);
    rebuildFilters();
}

const std::array<double, Equalizer::kBandCount> &Equalizer::bandFrequencies()
{
    static const std::array<double, kBandCount> kFreqs = {
        31.0, 62.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0
    };
    return kFreqs;
}

const QVector<Equalizer::Preset> &Equalizer::builtinPresets()
{
    // 8 defaults, as requested ("EQ - default 7-10 items"). Gains in dB,
    // one per band in bandFrequencies() order.
    static const QVector<Preset> kPresets = {
        { "Flat",         { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
        { "Pop",          { -1, 2, 4, 4, 1, -1, -2, -2, -1, -1 } },
        { "Rock",         { 4, 3, 2, 0, -1, -1, 0, 2, 3, 4 } },
        { "Jazz",         { 2, 1, 0, 1, -1, -1, 0, 1, 2, 3 } },
        { "Classical",    { 3, 2, 1, 0, 0, 0, -1, -1, 0, 2 } },
        { "Bass Boost",   { 6, 5, 4, 2, 0, 0, 0, 0, 0, 0 } },
        { "Treble Boost", { 0, 0, 0, 0, 0, 1, 2, 4, 5, 6 } },
        { "Vocal Boost",  { -2, -2, -1, 2, 4, 4, 2, 0, -1, -2 } },
    };
    return kPresets;
}

// Note: rebuildFilters() below is a private helper that mutates m_filters
// without locking anything itself - every public entry point that touches
// m_filters (directly or via rebuildFilters/resetState) takes m_mutex once
// for its whole body and then calls the private helper directly. QMutex is
// non-recursive, so rebuildFilters() must never lock again internally, or
// every one of these callers would deadlock on itself.

void Equalizer::setSampleRate(double sampleRate)
{
    if (sampleRate <= 0.0 || sampleRate == m_sampleRate)
        return;
    QMutexLocker locker(&m_mutex);
    m_sampleRate = sampleRate;
    rebuildFilters();
}

void Equalizer::setChannelCount(int channels)
{
    channels = std::max(1, channels);
    if (channels == m_channelCount && static_cast<int>(m_filters.size()) == channels)
        return;
    QMutexLocker locker(&m_mutex);
    m_channelCount = channels;
    m_filters.assign(static_cast<size_t>(m_channelCount), std::array<BiquadPeakingFilter, kBandCount>{});
    rebuildFilters();
}

void Equalizer::setEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_enabled = enabled;
}

bool Equalizer::applyPreset(const QString &name)
{
    for (const Preset &p : builtinPresets()) {
        if (p.name.compare(name, Qt::CaseInsensitive) == 0) {
            QMutexLocker locker(&m_mutex);
            m_gainsDb = p.gainsDb;
            m_currentPresetName = p.name;
            rebuildFilters();
            return true;
        }
    }
    return false;
}

void Equalizer::setCustomGains(const std::array<double, kBandCount> &gainsDb)
{
    QMutexLocker locker(&m_mutex);
    m_gainsDb = gainsDb;
    m_currentPresetName = "Custom";
    rebuildFilters();
}

void Equalizer::setBandGain(int band, double gainDb)
{
    if (band < 0 || band >= kBandCount)
        return;
    QMutexLocker locker(&m_mutex);
    m_gainsDb[static_cast<size_t>(band)] = gainDb;
    m_currentPresetName = "Custom";
    rebuildFilters();
}

void Equalizer::resetState()
{
    QMutexLocker locker(&m_mutex);
    for (auto &channelBands : m_filters)
        for (auto &band : channelBands)
            band.reset();
}

void Equalizer::rebuildFilters()
{
    const auto &freqs = bandFrequencies();
    for (auto &channelBands : m_filters) {
        for (int b = 0; b < kBandCount; ++b)
            channelBands[static_cast<size_t>(b)].configure(freqs[static_cast<size_t>(b)],
                                                             m_gainsDb[static_cast<size_t>(b)],
                                                             kQ, m_sampleRate);
    }
}
