#pragma once
//
// Spectrum/waveform visualizer widget. Pulls raw (pre-EQ) mono samples from
// AudioEngine on a timer, runs a windowed FFT, buckets the magnitude
// spectrum into log-spaced bands, and paints one of 15 selectable render
// styles (9 original + 6 added 2026-08-30) as a set of selectable presets
// rather than a fixed look.
//
// Visualizing pre-EQ audio (not the post-EQ signal actually sent to the
// speakers) is a deliberate simplification: it avoids running a second
// filter chain in lockstep with the playback one just for display, at the
// cost of the visualizer not reflecting EQ boosts/cuts exactly. Documented
// in README.md.
//
#include <QWidget>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QColor>

#include <array>
#include <deque>
#include <vector>

class AudioEngine;

class Visualizer : public QWidget
{
    Q_OBJECT

public:
    enum class Style {
        Bars,
        MirroredBars,
        Wave,
        LineSpectrum,
        Circular,
        Dots,
        VuMeter,
        Particles,
        BrickBox,
        // Added 2026-08-30 (6 more render styles):
        Spectrogram,
        Spiral,
        Ribbon,
        Orbit,
        Tunnel,
        Sunburst,
    };

    static constexpr int kStyleCount = 15;

    static QStringList styleNames();               // display names, in enum order
    static Style styleFromName(const QString &name);
    static QString styleToName(Style s);

    // Color palette, independent of Style - 15 render styles above, and 17
    // color schemes here, mixed freely (any style x any scheme).
    enum class ColorScheme {
        Purple,
        Ocean,
        Sunset,
        NeonGreen,
        HotPink,
        Cyan,
        Fire,
        Gold,
        // Added 2026-08-30 (8 more color schemes):
        Emerald,
        Lavender,
        Coral,
        Ice,
        Crimson,
        Amber,
        Midnight,
        Lime,
        // Added 2026-09-10: not a primary/secondary pair like every scheme
        // above but a full hue sweep - each band gets its own color across
        // the spectrum. applyColorScheme() sets m_rainbow for it; the
        // per-band draw code routes through bandColor() which returns an
        // HSV hue instead of a primary/secondary blend when it's set.
        Rainbow,
    };

    static constexpr int kColorSchemeCount = 17;

    static QStringList colorSchemeNames();
    static ColorScheme colorSchemeFromName(const QString &name);
    static QString colorSchemeToName(ColorScheme s);

    explicit Visualizer(AudioEngine *engine, QWidget *parent = nullptr);

    void setStyle(Style s);
    Style style() const { return m_style; }

    void setColorScheme(ColorScheme s);
    ColorScheme colorScheme() const { return m_colorScheme; }

    // Stops/starts the redraw timer; call with false when playback isn't
    // active so the widget doesn't burn CPU animating silence.
    void setActive(bool active);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onTick();

private:
    // Power of two required by fft::transform. 4096 (not 1024) is
    // deliberate: at 1024 samples/44.1kHz the FFT bin spacing is ~43 Hz,
    // but the log-spaced low bands below roughly 200 Hz are each narrower
    // than that in real Hz - several of the lowest bars could never
    // receive a single bin and always rendered as flat/invisible ("dead
    // black" bars on the left of Bars/BrickBox/etc, reported 2026-08-22).
    // 4096 samples gives ~10.8 Hz bins, narrow enough that ensureBandEdges()'s
    // adjusted minF/maxF range (see .cpp) reliably puts at least one bin in
    // every band. Costs ~93ms of window latency (was ~23ms) - imperceptible
    // for a visualizer, and fft::transform is validated up to N=4096 (see
    // FFT.h's header comment).
    static constexpr int kFftSize = 4096;
    static constexpr int kNumBands = 32;

    void computeSpectrum();
    void ensureBandEdges();
    void applyColorScheme();

    // Base color for a spectrum position t in 0..1 (usually band/kNumBands).
    // Normal schemes: the primary->secondary blend at t. Rainbow scheme
    // (m_rainbow): a hue sweep, red at the low end to violet at the high
    // end, so every band paints a different color. Every style's per-band
    // color goes through here so one branch covers all 15.
    QColor bandColor(float t) const;

    void drawBars(QPainter &p, bool mirrored);
    void drawWave(QPainter &p);
    void drawLineSpectrum(QPainter &p);
    void drawCircular(QPainter &p);
    void drawDots(QPainter &p);
    void drawVuMeter(QPainter &p);
    void drawParticles(QPainter &p);
    void drawBrickBox(QPainter &p);
    void drawSpectrogram(QPainter &p);
    void drawSpiral(QPainter &p);
    void drawRibbon(QPainter &p);
    void drawOrbit(QPainter &p);
    void drawTunnel(QPainter &p);
    void drawSunburst(QPainter &p);

    AudioEngine *m_engine;
    QTimer m_timer;
    Style m_style = Style::Bars;

    std::array<float, kNumBands> m_bandMagnitudes{};   // smoothed, 0..1
    std::array<float, kNumBands> m_bandPeaks{};        // slow-falling peak-hold caps, for BrickBox / Bars
    std::array<float, kNumBands + 1> m_bandEdgesHz{};  // log-spaced, computed once per sample rate
    double m_bandEdgesForSampleRate = 0.0;

    std::vector<float> m_waveform; // most recent raw samples, for Wave/VU styles

    // VU Meter ballistics (a real analog VU integrates over ~300 ms and has
    // a separate fast peak needle). Advanced once per painted frame from
    // m_waveform's RMS and peak. Persisted so the meter eases instead of
    // snapping, and so the peak-hold marker can fall slowly on its own.
    float m_vuRms = 0.0f;       // slow "VU" bar, 0..1 linear
    float m_vuPeak = 0.0f;      // fast "PEAK" bar, instant attack / slow release
    float m_vuPeakHold = 0.0f;  // highest recent m_vuPeak, decays gradually
    float m_vuClip = 0.0f;      // 1.0 when the frame clipped, decays to 0

    struct Particle { float x, y, vx, vy, life; };
    std::vector<Particle> m_particles;

    // Spectrogram: rolling history of band-magnitude columns, newest last.
    // Capped at kSpectrogramColumns regardless of paint width so the
    // scroll rate is time-based, not pixel-width-based; drawSpectrogram()
    // stretches whatever history exists across the current widget width.
    static constexpr int kSpectrogramColumns = 96;
    std::deque<std::array<float, kNumBands>> m_spectrogramHistory;

    // Orbit: slow constant rotation, advanced once per tick.
    double m_orbitPhase = 0.0;

    // Tunnel: outward-expanding rings, spawned faster when bass is louder.
    struct TunnelRing { float radius, life; };
    std::vector<TunnelRing> m_tunnelRings;
    double m_tunnelSpawnAccum = 0.0;

    ColorScheme m_colorScheme = ColorScheme::Purple;
    QColor m_primaryColor{0x6c, 0x5c, 0xe7};   // updated by applyColorScheme()
    QColor m_secondaryColor{0x85, 0x78, 0xf0}; // updated by applyColorScheme()
    bool m_rainbow = false;                    // ColorScheme::Rainbow; set by applyColorScheme()
};
