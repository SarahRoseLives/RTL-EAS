// Minimal in-place iterative radix-2 FFT, used only for coarse peak finding
// (auto channel offset) and diagnostics.
#pragma once

#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

inline void fftRadix2(std::vector<std::complex<double>>& a, bool inverse = false)
{
    size_t n = a.size();
    if (n < 2)
    {
        return;
    }

    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
        {
            j ^= bit;
        }
        j ^= bit;
        if (i < j)
        {
            std::swap(a[i], a[j]);
        }
    }

    for (size_t len = 2; len <= n; len <<= 1)
    {
        double ang = 2.0 * M_PI / (double)len * (inverse ? 1.0 : -1.0);
        std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                std::complex<double> u = a[i + k];
                std::complex<double> v = a[i + k + len / 2] * w;
                a[i + k]               = u + v;
                a[i + k + len / 2]     = u - v;
                w *= wl;
            }
        }
    }
}
