#include "Visualizer.h"
#include "AudioEngine.h"
#include "FFT.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

QStringList Visualizer::styleNames()
{
    return {
        QStringLiteral("Bars"),
        QStringLiteral("Mirrored Bars"),
        QStringLiteral("Wave"),
        QStringLiteral("Line Spectrum"),
        QStringLiteral("Circular"),
        QStringLiteral("Dots"),
        QStringLiteral("VU Meter"),
        QStringLiteral("Particles"),
        QStringLiteral("Brick Box"),
    };
}

Visualizer::Style Visualizer::styleFromName(const QString &name)
{
    const QStringList names = styleNames();
    const int idx = names.indexOf(name);
    return static_cast<Style>(idx >= 0 ? idx : 0);
}

QString Visualizer::styleToName(Style s)
{
    const QStringList names = styleNames();
    const int idx = static_cast<int>(s);
    return (idx >= 0 && idx < names.size()) ? names[idx] : names[0];
}

QStringList Visualizer::colorSchemeNames()
{
    return {
        QStringLiteral("Purple"),
        QStringLiteral("Ocean"),
        QStringLiteral("Sunset"),
        QStringLiteral("Neon Green"),
        QStringLiteral("Hot Pink"),
        QStringLiteral("Cyan"),
        QStringLiteral("Fire"),
        QStringLiteral("Gold"),
    };
}

Visualizer::ColorScheme Visualizer::colorSchemeFromName(const QString &name)
{
    const QStringList names = colorSchemeNames();
    const int idx = names.indexOf(name);
    return static_cast<ColorScheme>(idx >= 0 ? idx : 0);
}

QString Visualizer::colorSchemeToName(ColorScheme s)
{
    const QStringList names = colorSchemeNames();
    const int idx = static_cast<int>(s);
    return (idx >= 0 && idx < names.size()) ? names[idx] : names[0];
}

Visualizer::Visualizer(AudioEngine *engine, QWidget *parent)
    : QWidget(parent), m_engine(engine)
{
    setMinimumHeight(80);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    applyColorScheme();

    m_timer.setInterval(33); // ~30 fps
    connect(&m_timer, &QTimer::timeout, this, &Visualizer::onTick);
}

void Visualizer::setStyle(Style s)
{
    m_style = s;
    update();
}

void Visualizer::setColorScheme(ColorScheme s)
{
    m_colorScheme = s;
    applyColorScheme();
    update();
}

void Visualizer::applyColorScheme()
{
    switch (m_colorScheme) {
    case ColorScheme::Purple:
        m_primaryColor = QColor(0x6c, 0x5c, 0xe7);
        m_secondaryColor = QColor(0xa2, 0x9b, 0xfe);
        break;
    case ColorScheme::Ocean:
        m_primaryColor = QColor(0x0f, 0x4c, 0x81);
        m_secondaryColor = QColor(0x38, 0xb6, 0xff);
        break;
    case ColorScheme::Sunset:
        m_primaryColor = QColor(0xe1, 0x4e, 0x3b);
        m_secondaryColor = QColor(0xff, 0xb3, 0x47);
        break;
    case ColorScheme::NeonGreen:
        m_primaryColor = QColor(0x0c, 0x8a, 0x4a);
        m_secondaryColor = QColor(0x39, 0xff, 0x8f);
        break;
    case ColorScheme::HotPink:
        m_primaryColor = QColor(0xc7, 0x2a, 0x7c);
        m_secondaryColor = QColor(0xff, 0x6f, 0xd8);
        break;
    case ColorScheme::Cyan:
        m_primaryColor = QColor(0x0e, 0x7c, 0x86);
        m_secondaryColor = QColor(0x4d, 0xf0, 0xff);
        break;
    case ColorScheme::Fire:
        m_primaryColor = QColor(0x9a, 0x1d, 0x0e);
        m_secondaryColor = QColor(0xff, 0x8a, 0x1e);
        break;
    case ColorScheme::Gold:
        m_primaryColor = QColor(0x8a, 0x6a, 0x0e);
        m_secondaryColor = QColor(0xff, 0xd7, 0x66);
        break;
    }
}

void Visualizer::setActive(bool active)
{
    if (active && !m_timer.isActive())
        m_timer.start();
    else if (!active && m_timer.isActive())
        m_timer.stop();
}

void Visualizer::onTick()
{
    computeSpectrum();
    update();
}

void Visualizer::ensureBandEdges()
{
    const double sr = m_engine ? m_engine->sampleRate() : 44100.0;
    if (sr == m_bandEdgesForSampleRate)
        return;
    m_bandEdgesForSampleRate = sr;

    // 40 Hz..16 kHz (clamped to Nyquist for low sample rates), not
    // 20 Hz..sr/2: real program material carries almost nothing below
    // ~40 Hz or above ~16 kHz (typical mp3/streaming low-pass rolloff),
    // so the old 20..22050 Hz range spent several of the 32 log-spaced
    // bands on frequencies that are always silent - they rendered as
    // flat, invisible ("dead black") bars regardless of the track. This
    // range concentrates all 32 bands in the part of the spectrum that
    // actually carries energy, so the display reads as full width instead
    // of empty at the edges. (Paired with kFftSize=4096 in Visualizer.h,
    // which fixes the other half of the same symptom: coarser FFT bins
    // couldn't resolve the narrowest low bands at all.)
    const double minF = 40.0;
    const double maxF = std::max(minF + 1.0, std::min(sr / 2.0, 16000.0));
    for (int i = 0; i <= kNumBands; ++i) {
        const double t = static_cast<double>(i) / kNumBands;
        m_bandEdgesHz[static_cast<size_t>(i)] = static_cast<float>(minF * std::pow(maxF / minF, t));
    }
}

void Visualizer::computeSpectrum()
{
    ensureBandEdges();

    std::vector<float> raw = m_engine ? m_engine->recentMonoSamples(kFftSize) : std::vector<float>(kFftSize, 0.0f);
    m_waveform = raw; // keep a copy for Wave/VU styles before windowing mutates a working copy

    std::vector<float> windowed = raw;
    fft::applyHannWindow(windowed);

    std::vector<fft::Complex> spectrum(kFftSize);
    for (int i = 0; i < kFftSize; ++i)
        spectrum[static_cast<size_t>(i)] = fft::Complex(windowed[static_cast<size_t>(i)], 0.0f);
    fft::transform(spectrum);

    const double sr = m_engine ? m_engine->sampleRate() : 44100.0;
    const int usableBins = kFftSize / 2;

    std::array<float, kNumBands> raw01{};
    raw01.fill(0.0f);

    for (int bin = 1; bin < usableBins; ++bin) {
        const double freq = bin * sr / kFftSize;
        // Find which band this bin belongs to (linear scan over 32 bands is cheap).
        for (int b = 0; b < kNumBands; ++b) {
            if (freq >= m_bandEdgesHz[static_cast<size_t>(b)] && freq < m_bandEdgesHz[static_cast<size_t>(b) + 1]) {
                const float mag = std::abs(spectrum[static_cast<size_t>(bin)]) / (kFftSize / 2.0f);
                raw01[static_cast<size_t>(b)] = std::max(raw01[static_cast<size_t>(b)], mag);
                break;
            }
        }
    }

    for (int b = 0; b < kNumBands; ++b) {
        // Convert to dB and normalize a -60..0 dB range to 0..1 before smoothing.
        const float mag = raw01[static_cast<size_t>(b)];
        const float db = 20.0f * std::log10(std::max(mag, 1e-6f));
        const float norm = std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);

        float &smoothed = m_bandMagnitudes[static_cast<size_t>(b)];
        if (norm > smoothed)
            smoothed = smoothed * 0.4f + norm * 0.6f; // rise fast
        else
            smoothed = smoothed * 0.82f + norm * 0.18f; // fall slow

        // Classic hardware-analyzer peak-hold cap: jumps up instantly with
        // the band, then falls back down slowly on its own regardless of
        // what the band does next - used by BrickBox.
        float &peak = m_bandPeaks[static_cast<size_t>(b)];
        if (smoothed >= peak)
            peak = smoothed;
        else
            peak = std::max(0.0f, peak - 0.014f);
    }
}

void Visualizer::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient bgGrad(0, 0, 0, height());
    bgGrad.setColorAt(0.0, QColor(0x20, 0x21, 0x30));
    bgGrad.setColorAt(1.0, QColor(0x14, 0x15, 0x1f));
    p.fillRect(rect(), bgGrad);

    switch (m_style) {
    case Style::Bars:          drawBars(p, false); break;
    case Style::MirroredBars:  drawBars(p, true); break;
    case Style::Wave:          drawWave(p); break;
    case Style::LineSpectrum:  drawLineSpectrum(p); break;
    case Style::Circular:      drawCircular(p); break;
    case Style::Dots:          drawDots(p); break;
    case Style::VuMeter:       drawVuMeter(p); break;
    case Style::Particles:     drawParticles(p); break;
    case Style::BrickBox:      drawBrickBox(p); break;
    }
}

void Visualizer::drawBars(QPainter &p, bool mirrored)
{
    const int w = width();
    const int h = height();
    const double barW = static_cast<double>(w) / kNumBands;

    for (int b = 0; b < kNumBands; ++b) {
        const float v = m_bandMagnitudes[static_cast<size_t>(b)];
        const double barH = v * (mirrored ? h * 0.48 : h * 0.95);
        const double x = b * barW + barW * 0.15;
        const double bw = barW * 0.7;

        QLinearGradient grad(0, h, 0, 0);
        grad.setColorAt(0.0, m_primaryColor);
        grad.setColorAt(1.0, m_secondaryColor);

        if (mirrored) {
            const double midY = h / 2.0;
            p.fillRect(QRectF(x, midY - barH, bw, barH), grad);
            p.fillRect(QRectF(x, midY, bw, barH), grad);
        } else {
            const QRectF barRect(x, h - barH, bw, barH);
            p.fillRect(barRect, grad);
            // Soft highlight cap for a glassier look.
            if (barH > 3.0) {
                QColor cap = m_secondaryColor;
                cap.setAlpha(200);
                p.fillRect(QRectF(x, h - barH, bw, 2.0), cap);
            }
        }
    }
}

void Visualizer::drawWave(QPainter &p)
{
    const int w = width();
    const int h = height();
    if (m_waveform.empty())
        return;

    QPainterPath path;
    const int n = static_cast<int>(m_waveform.size());
    for (int i = 0; i < n; ++i) {
        const double x = static_cast<double>(i) / (n - 1) * w;
        const double y = h / 2.0 - m_waveform[static_cast<size_t>(i)] * h * 0.45;
        if (i == 0)
            path.moveTo(x, y);
        else
            path.lineTo(x, y);
    }

    QPen pen(m_secondaryColor, 2.2);
    p.setPen(pen);
    p.drawPath(path);
}

void Visualizer::drawLineSpectrum(QPainter &p)
{
    const int w = width();
    const int h = height();

    QPainterPath path;
    for (int b = 0; b < kNumBands; ++b) {
        const double x = (b + 0.5) / kNumBands * w;
        const double y = h - m_bandMagnitudes[static_cast<size_t>(b)] * h * 0.9;
        if (b == 0)
            path.moveTo(x, y);
        else
            path.lineTo(x, y);
    }

    QPainterPath fillPath = path;
    fillPath.lineTo(w, h);
    fillPath.lineTo(0, h);
    fillPath.closeSubpath();

    QLinearGradient grad(0, 0, 0, h);
    grad.setColorAt(0.0, QColor(m_primaryColor.red(), m_primaryColor.green(), m_primaryColor.blue(), 140));
    grad.setColorAt(1.0, QColor(m_primaryColor.red(), m_primaryColor.green(), m_primaryColor.blue(), 10));
    p.fillPath(fillPath, grad);

    p.setPen(QPen(m_secondaryColor, 2.2));
    p.drawPath(path);
}

void Visualizer::drawCircular(QPainter &p)
{
    const int w = width();
    const int h = height();
    const double cx = w / 2.0;
    const double cy = h / 2.0;
    const double baseR = std::min(w, h) * 0.22;
    const double maxExtra = std::min(w, h) * 0.28;

    p.translate(cx, cy);
    for (int b = 0; b < kNumBands; ++b) {
        const double angle = (2.0 * M_PI * b) / kNumBands;
        const float v = m_bandMagnitudes[static_cast<size_t>(b)];
        const double r0 = baseR;
        const double r1 = baseR + v * maxExtra;

        const double x0 = std::cos(angle) * r0;
        const double y0 = std::sin(angle) * r0;
        const double x1 = std::cos(angle) * r1;
        const double y1 = std::sin(angle) * r1;

        QLinearGradient lineGrad(x0, y0, x1, y1);
        lineGrad.setColorAt(0.0, m_primaryColor);
        lineGrad.setColorAt(1.0, m_secondaryColor);
        QPen pen(QBrush(lineGrad), 3.0, Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        p.drawLine(QPointF(x0, y0), QPointF(x1, y1));

        p.setPen(Qt::NoPen);
        p.setBrush(m_secondaryColor);
        p.drawEllipse(QPointF(x1, y1), 2.2, 2.2);
    }
    p.resetTransform();
}

void Visualizer::drawDots(QPainter &p)
{
    const int w = width();
    const int h = height();
    const double colW = static_cast<double>(w) / kNumBands;
    const int maxDots = 12;
    const double dotSpacing = static_cast<double>(h) * 0.9 / maxDots;
    const double dotR = std::min(colW * 0.28, dotSpacing * 0.35);

    p.setPen(Qt::NoPen);
    for (int b = 0; b < kNumBands; ++b) {
        const float v = m_bandMagnitudes[static_cast<size_t>(b)];
        const int lit = static_cast<int>(v * maxDots + 0.5f);
        const double cx = (b + 0.5) * colW;
        for (int d = 0; d < lit; ++d) {
            const double cy = h - 6 - d * dotSpacing;
            const double t = static_cast<double>(d) / maxDots;
            QColor c = (t > 0.8) ? QColor(0xe5, 0x5b, 0x6c) : (t > 0.55 ? m_secondaryColor : m_primaryColor);
            p.setBrush(c);
            p.drawEllipse(QPointF(cx, cy), dotR, dotR);
        }
    }
}

void Visualizer::drawVuMeter(QPainter &p)
{
    const int w = width();
    const int h = height();

    float peak = 0.0f;
    for (float s : m_waveform)
        peak = std::max(peak, std::abs(s));
    for (float v : m_bandMagnitudes)
        peak = std::max(peak, v * 0.6f); // blend a little spectral energy in

    const int segments = 24;
    const double gap = 3.0;
    const double segW = (w - gap * (segments - 1)) / segments;
    const int lit = static_cast<int>(peak * segments + 0.5f);

    for (int i = 0; i < segments; ++i) {
        const double x = i * (segW + gap);
        QColor c(0x33, 0x34, 0x4a);
        if (i < lit) {
            const double t = static_cast<double>(i) / segments;
            c = (t > 0.85) ? QColor(0xe5, 0x5b, 0x6c) : (t > 0.6 ? QColor(0xf0, 0xb4, 0x29) : m_primaryColor);
        }
        p.fillRect(QRectF(x, h * 0.15, segW, h * 0.7), c);
    }
}

void Visualizer::drawParticles(QPainter &p)
{
    const int w = width();
    const int h = height();

    float energy = 0.0f;
    for (float v : m_bandMagnitudes)
        energy += v;
    energy /= kNumBands;

    // Spawn new particles proportional to current energy. A wider spawn
    // budget (and a higher cap) keeps the doubled-height widget from
    // looking sparse at low energy.
    const int spawnCount = static_cast<int>(energy * 10) + 1;
    for (int i = 0; i < spawnCount && m_particles.size() < 600; ++i) {
        Particle particle;
        particle.x = static_cast<float>(QRandomGenerator::global()->bounded(w));
        particle.y = static_cast<float>(h);
        particle.vx = (QRandomGenerator::global()->bounded(200) - 100) / 100.0f * 0.6f;
        particle.vy = -(1.5f + energy * 6.0f + QRandomGenerator::global()->bounded(100) / 100.0f);
        particle.life = 1.0f;
        m_particles.push_back(particle);
    }

    p.setPen(Qt::NoPen);
    for (auto &particle : m_particles) {
        particle.x += particle.vx;
        particle.y += particle.vy;
        particle.life -= 0.014f;
        if (particle.life <= 0.0f || particle.y < 0)
            continue;

        const float alpha = std::clamp(particle.life, 0.0f, 1.0f);
        const double r = 2.2 + energy * 4.0;

        // Soft radial glow behind the solid core for a prettier, less
        // "bare dot" look.
        QRadialGradient glow(QPointF(particle.x, particle.y), r * 3.0);
        QColor glowColor = m_secondaryColor;
        glowColor.setAlphaF(alpha * 0.35f);
        glow.setColorAt(0.0, glowColor);
        QColor glowEdge = m_secondaryColor;
        glowEdge.setAlphaF(0.0f);
        glow.setColorAt(1.0, glowEdge);
        p.setBrush(glow);
        p.drawEllipse(QPointF(particle.x, particle.y), r * 3.0, r * 3.0);

        QColor core = m_secondaryColor;
        core.setAlphaF(alpha);
        p.setBrush(core);
        p.drawEllipse(QPointF(particle.x, particle.y), r, r);
    }

    m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(),
                                      [w, h](const Particle &pt) {
                                          return pt.life <= 0.0f || pt.y < 0 || pt.y > h ||
                                                 pt.x < -20 || pt.x > w + 20;
                                      }),
                       m_particles.end());
}

void Visualizer::drawBrickBox(QPainter &p)
{
    // Classic hardware-EQ "brick" analyzer: each band is a stack of
    // discrete blocks (not a smooth bar), colored low-to-high like a
    // level meter, plus a bright falling peak-hold brick riding above the
    // lit stack - distinct from Dots (circles, no peak-hold) and VU Meter
    // (single overall bar, not per-band).
    const int w = width();
    const int h = height();
    const double colW = static_cast<double>(w) / kNumBands;

    const int rows = 16;
    const double marginTop = h * 0.06;
    const double marginBottom = h * 0.06;
    const double gridH = std::max(1.0, h - marginTop - marginBottom);
    const double rowGap = 2.0;
    const double brickH = std::max(1.0, (gridH - rowGap * (rows - 1)) / rows);
    const double colGap = std::max(1.0, colW * 0.15);
    const double brickW = std::max(1.0, colW - colGap);

    for (int b = 0; b < kNumBands; ++b) {
        const float v = m_bandMagnitudes[static_cast<size_t>(b)];
        const float peak = m_bandPeaks[static_cast<size_t>(b)];
        const int lit = std::clamp(static_cast<int>(v * rows + 0.5f), 0, rows);
        const int peakRow = std::clamp(static_cast<int>(peak * rows), 0, rows - 1);

        const double x = b * colW + colGap * 0.5;

        for (int r = 0; r < rows; ++r) {
            const double y = h - marginBottom - (r + 1) * brickH - r * rowGap;
            const double t = static_cast<double>(r) / rows;

            QColor c;
            if (r < lit) {
                c = (t > 0.82) ? QColor(0xe5, 0x5b, 0x6c)
                                : (t > 0.5 ? m_secondaryColor : m_primaryColor);
            } else {
                c = QColor(0x2c, 0x2d, 0x3d); // unlit brick, dim well
            }
            p.fillRect(QRectF(x, y, brickW, brickH), c);
        }

        // Falling peak-hold brick, drawn brighter than any lit brick below it.
        if (peakRow >= lit && peak > 0.01f) {
            const double py = h - marginBottom - (peakRow + 1) * brickH - peakRow * rowGap;
            p.fillRect(QRectF(x, py, brickW, brickH), m_secondaryColor.lighter(170));
        }
    }
}
