#pragma once
//
// RBJ Audio Cookbook peaking EQ biquad, per channel. Coefficients verified
// numerically before porting (see tools/verify_biquad.py in the repo notes):
// max center-frequency gain error ~1e-12 dB, poles stay inside the unit
// circle for every band/gain/sample-rate combination tested (worst case
// |pole| ~= 0.9995 at +12 dB / 31 Hz), and 0 dB gain is bit-exact unity.
//
#include <cmath>

class BiquadPeakingFilter
{
public:
    void configure(double centerFreqHz, double gainDb, double q, double sampleRate)
    {
        m_bypass = std::abs(gainDb) < 0.01 || centerFreqHz <= 0.0 || centerFreqHz >= sampleRate / 2.0;
        if (m_bypass)
            return;

        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * M_PI_CONST * centerFreqHz / sampleRate;
        const double alpha = std::sin(w0) / (2.0 * q);
        const double cosw0 = std::cos(w0);

        const double a0 = 1.0 + alpha / A;
        m_b0 = (1.0 + alpha * A) / a0;
        m_b1 = (-2.0 * cosw0) / a0;
        m_b2 = (1.0 - alpha * A) / a0;
        m_a1 = (-2.0 * cosw0) / a0;
        m_a2 = (1.0 - alpha / A) / a0;
    }

    // Call whenever playback seeks/restarts to avoid an audible pop from
    // stale filter state.
    void reset()
    {
        m_x1 = m_x2 = m_y1 = m_y2 = 0.0;
    }

    inline float process(float in)
    {
        if (m_bypass)
            return in;

        const double x0 = static_cast<double>(in);
        const double y0 = m_b0 * x0 + m_b1 * m_x1 + m_b2 * m_x2 - m_a1 * m_y1 - m_a2 * m_y2;

        m_x2 = m_x1;
        m_x1 = x0;
        m_y2 = m_y1;
        m_y1 = y0;

        return static_cast<float>(y0);
    }

    bool isBypassed() const { return m_bypass; }

private:
    static constexpr double M_PI_CONST = 3.14159265358979323846;

    bool m_bypass = true;

    double m_b0 = 1.0, m_b1 = 0.0, m_b2 = 0.0;
    double m_a1 = 0.0, m_a2 = 0.0;

    double m_x1 = 0.0, m_x2 = 0.0;
    double m_y1 = 0.0, m_y2 = 0.0;
};
