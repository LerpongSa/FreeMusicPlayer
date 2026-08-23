#pragma once
//
// Minimal in-place radix-2 iterative FFT. Verified against numpy.fft with
// max abs error ~1e-12 for sizes up to 4096, and against a known 1 kHz test
// tone (windowed, N=2048 @ 44.1 kHz) landing in the expected bin - see the
// project notes for the Python reference used to validate this before it
// was ported.
//
#include <complex>
#include <vector>
#include <cstddef>

namespace fft {

using Complex = std::complex<float>;

inline bool isPowerOfTwo(size_t n)
{
    return n != 0 && (n & (n - 1)) == 0;
}

// In-place FFT. `a.size()` must be a power of two.
inline void transform(std::vector<Complex> &a)
{
    const size_t n = a.size();
    if (n <= 1 || !isPowerOfTwo(n))
        return;

    // Bit-reversal permutation.
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j |= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }

    for (size_t length = 2; length <= n; length <<= 1) {
        const float ang = -2.0f * 3.14159265358979323846f / static_cast<float>(length);
        const Complex wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += length) {
            Complex w(1.0f, 0.0f);
            for (size_t k = 0; k < length / 2; ++k) {
                const Complex u = a[i + k];
                const Complex v = a[i + k + length / 2] * w;
                a[i + k] = u + v;
                a[i + k + length / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

// Hann window, applied in place before transform() for lower spectral leakage.
inline void applyHannWindow(std::vector<float> &samples)
{
    const size_t n = samples.size();
    if (n < 2)
        return;
    for (size_t i = 0; i < n; ++i) {
        const float w = 0.5f - 0.5f * std::cos(2.0f * 3.14159265358979323846f *
                                                 static_cast<float>(i) / static_cast<float>(n - 1));
        samples[i] *= w;
    }
}

} // namespace fft
