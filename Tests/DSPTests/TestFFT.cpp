// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#include <catch2/catch_test_macros.hpp>
#include "DSP/Util/FFT.h"
#include "DSP/Util/DspUtil.h"

#include <cmath>
#include <vector>

TEST_CASE ("FFT matches a direct DFT and round-trips", "[fft]")
{
    for (int n : { 8, 64, 256, 8192 })
    {
        ba::FFT fft;
        fft.prepare (n);
        ba::Rng rng (3);
        std::vector<ba::Cpx> x ((size_t) n), y;
        for (auto& v : x) v = { rng.bip(), rng.bip() };
        y = x;
        fft.transform (y.data(), false);

        if (n <= 256)
        {
            double maxErr = 0.0;
            for (int k = 0; k < n; ++k)
            {
                double re = 0.0, im = 0.0;
                for (int t = 0; t < n; ++t)
                {
                    const double a = -2.0 * ba::kPiD * k * t / n;
                    re += x[(size_t) t].re * std::cos (a) - x[(size_t) t].im * std::sin (a);
                    im += x[(size_t) t].re * std::sin (a) + x[(size_t) t].im * std::cos (a);
                }
                maxErr = std::max (maxErr, std::hypot (re - y[(size_t) k].re, im - y[(size_t) k].im));
            }
            REQUIRE (maxErr < 1.0e-3 * n);
        }

        fft.transform (y.data(), true);
        double maxErr = 0.0;
        for (int i = 0; i < n; ++i)
            maxErr = std::max (maxErr, (double) std::hypot (y[(size_t) i].re / n - x[(size_t) i].re,
                                                           y[(size_t) i].im / n - x[(size_t) i].im));
        REQUIRE (maxErr < 1.0e-5);
    }
}
