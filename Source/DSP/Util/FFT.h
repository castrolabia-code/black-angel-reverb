// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include <vector>
#include <cmath>

namespace ba
{
struct Cpx { float re = 0.0f, im = 0.0f; };

/** Iterative radix-2 complex FFT. prepare() allocates; transforms do not. */
class FFT
{
public:
    void prepare (int size)
    {
        n = size;
        log2n = 0;
        while ((1 << log2n) < n) ++log2n;
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < log2n; ++b)
                if (i & (1 << b)) r |= 1 << (log2n - 1 - b);
            rev[(size_t) i] = r;
        }
        twiddle.resize ((size_t) (n / 2));
        for (int k = 0; k < n / 2; ++k)
        {
            const double a = -2.0 * 3.14159265358979323846 * k / n;
            twiddle[(size_t) k] = { (float) std::cos (a), (float) std::sin (a) };
        }
    }

    int size() const noexcept { return n; }

    /** In-place. Inverse is unscaled (caller multiplies by 1/n). */
    void transform (Cpx* x, bool inverse) const noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const int r = rev[(size_t) i];
            if (i < r) { Cpx t = x[i]; x[i] = x[r]; x[r] = t; }
        }
        const float sgn = inverse ? -1.0f : 1.0f;
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
            {
                for (int j = 0; j < half; ++j)
                {
                    const Cpx w = twiddle[(size_t) (j * step)];
                    const float wr = w.re, wi = sgn * w.im;
                    Cpx& a = x[i + j];
                    Cpx& b = x[i + j + half];
                    const float vr = b.re * wr - b.im * wi;
                    const float vi = b.re * wi + b.im * wr;
                    b.re = a.re - vr; b.im = a.im - vi;
                    a.re += vr;       a.im += vi;
                }
            }
        }
    }

private:
    int n = 0, log2n = 0;
    std::vector<int> rev;
    std::vector<Cpx> twiddle;
};
} // namespace ba
