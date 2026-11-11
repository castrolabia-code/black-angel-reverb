// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include "Util/DspUtil.h"

namespace ba
{
enum class ModMode { Smooth = 0, Chorus = 1, Random = 2 };

/** Bank of N decorrelated LFOs used to modulate delay-line read positions.
    Outputs are in samples. Smooth = sines with spread phases and rates,
    Chorus = deeper sines, Random = band-limited noise (smoothed random walk). */
template <int N>
class LfoBank
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        Rng rng (0x5EED1234u);
        for (int i = 0; i < N; ++i)
        {
            phase[i] = (float) i / (float) N + 0.13f * rng.uni();
            rateSpread[i] = 0.8f + 0.4f * rng.uni();
            randCur[i] = randTarget[i] = 0.0f;
            randState[i] = rng.nextU() | 1u;
        }
        depthSmoother.reset (fs, 0.05);
        depthSmoother.setImmediate (0.0f);
    }

    void setParameters (float rateHz, float depth01, ModMode m) noexcept
    {
        rate = std::min (std::max (rateHz, 0.01f), 10.0f);
        mode = m;
        const float base = 12.0f * (float) (fs / 48000.0);
        const float modeScale = mode == ModMode::Chorus ? 2.5f : (mode == ModMode::Random ? 1.5f : 1.0f);
        depthSmoother.setTarget (std::min (std::max (depth01, 0.0f), 1.0f) * base * modeScale);
    }

    float maxDepthSamples() const noexcept { return 12.0f * 2.5f * (float) (fs / 48000.0) + 2.0f; }

    /** Advances all LFOs by one sample and writes the offsets (in samples). */
    void tick (float* out) noexcept
    {
        const float depth = depthSmoother.next();
        const float inc = rate / (float) fs;
        for (int i = 0; i < N; ++i)
        {
            phase[i] += inc * rateSpread[i];
            if (phase[i] >= 1.0f)
            {
                phase[i] -= 1.0f;
                Rng r (randState[i]);
                randTarget[i] = r.bip();
                randState[i] = r.s;
            }
            float v;
            if (mode == ModMode::Random)
            {
                // Glide toward a new random target each cycle (smooth, no steps).
                randCur[i] += (randTarget[i] - randCur[i]) * std::min (1.0f, 4.0f * inc * rateSpread[i]);
                v = randCur[i];
            }
            else
            {
                v = std::sin (2.0f * kPi * phase[i]);
            }
            out[i] = depth * v;
        }
    }

private:
    double fs = 48000.0;
    float rate = 0.6f;
    ModMode mode = ModMode::Smooth;
    float phase[(size_t) N] {}, rateSpread[(size_t) N] {}, randCur[(size_t) N] {}, randTarget[(size_t) N] {};
    uint32_t randState[(size_t) N] {};
    LinearSmoother depthSmoother;
};
} // namespace ba
