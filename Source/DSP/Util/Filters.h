// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include "DspUtil.h"
#include <complex>

namespace ba
{
/** One-pole low-pass. */
class OnePoleLP
{
public:
    void setCutoff (double fs, float hz) noexcept
    {
        hz = std::min (hz, (float) (0.49 * fs));
        a = 1.0f - std::exp (-2.0f * kPi * hz / (float) fs);
    }
    void setCoefficient (float coeff) noexcept { a = coeff; }
    float process (float x) noexcept { z += a * (x - z); return z; }
    void reset() noexcept { z = 0.0f; }
private:
    float a = 1.0f, z = 0.0f;
};

/** First-order shelving filter (bilinear, prewarped). Magnitude is monotonic
    between its DC and Nyquist gains, which keeps feedback loops bounded. */
class FirstOrderShelf
{
public:
    /** tanHalf = tan (pi * fc / fs). lowShelf: DC gain = gain, HF = 1.
        highShelf: DC = 1, HF gain = gain. */
    void set (float tanHalf, float gain, bool lowShelf) noexcept
    {
        const float K = 1.0f / tanHalf;
        const float r = std::sqrt (std::max (gain, 1.0e-6f));
        float nb0, nb1, na0, na1;
        if (lowShelf)
        {   // H(s) = (s + r) / (s + 1/r)
            nb0 = K + r;        nb1 = r - K;
            na0 = K + 1.0f / r; na1 = 1.0f / r - K;
        }
        else
        {   // H(s) = (r s + 1) / (s / r + 1)
            nb0 = r * K + 1.0f; nb1 = 1.0f - r * K;
            na0 = K / r + 1.0f; na1 = 1.0f - K / r;
        }
        b0 = nb0 / na0; b1 = nb1 / na0; a1 = na1 / na0;
    }
    float process (float x) noexcept
    {
        const float y = b0 * x + b1 * x1 - a1 * y1;
        x1 = x; y1 = y;
        return y;
    }
    void reset() noexcept { x1 = y1 = 0.0f; }
private:
    float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f, x1 = 0.0f, y1 = 0.0f;
};

/** Topology-preserving state variable filter (A. Simper). Stable under
    fast coefficient modulation. magnitude() returns the exact digital
    response, so the GUI curve matches what the audio path does. */
class SVF
{
public:
    enum class Type { LowPass, HighPass, BandPass, Bell, LowShelf, HighShelf };

    void setup (Type type, double fs, float hz, float q, float gainDb) noexcept
    {
        hz = clampHz (hz, fs);
        q = std::max (q, 0.025f);
        const double A = std::pow (10.0, gainDb / 40.0);
        double gg = std::tan (kPiD * hz / fs);
        double kk = 1.0 / q;
        switch (type)
        {
            case Type::LowPass:   m0 = 0.0; m1 = 0.0;  m2 = 1.0;  break;
            case Type::HighPass:  m0 = 1.0; m1 = -kk;  m2 = -1.0; break;
            case Type::BandPass:  m0 = 0.0; m1 = kk;   m2 = 0.0;  break; // unity peak
            case Type::Bell:      kk = 1.0 / (q * A); m0 = 1.0; m1 = kk * (A * A - 1.0); m2 = 0.0; break;
            case Type::LowShelf:  gg /= std::sqrt (A); m0 = 1.0; m1 = kk * (A - 1.0); m2 = A * A - 1.0; break;
            case Type::HighShelf: gg *= std::sqrt (A); m0 = A * A; m1 = kk * (1.0 - A) * A; m2 = 1.0 - A * A; break;
        }
        g = gg; k = kk;
        a1 = (float) (1.0 / (1.0 + g * (g + k)));
        a2 = (float) (g * a1);
        a3 = (float) (g * a2);
        fm0 = (float) m0; fm1 = (float) m1; fm2 = (float) m2;
    }

    float process (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return fm0 * x + fm1 * v1 + fm2 * v2;
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    double magnitude (double hz, double fs) const noexcept
    {
        const double w = std::tan (kPiD * std::min (hz, 0.4999 * fs) / fs) / g;
        const std::complex<double> s (0.0, w);
        const std::complex<double> D = s * s + k * s + 1.0;
        return std::abs (m0 + m1 * s / D + m2 / D);
    }

    static float clampHz (float hz, double fs) noexcept
    {
        return std::min (std::max (hz, 5.0f), (float) (0.48 * fs));
    }

private:
    double g = 0.1, k = 1.0, m0 = 1.0, m1 = 0.0, m2 = 0.0;
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, fm0 = 1.0f, fm1 = 0.0f, fm2 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};
} // namespace ba
