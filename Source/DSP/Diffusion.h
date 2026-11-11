// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include "Util/DelayLine.h"
#include "Util/DspUtil.h"

namespace ba
{
/** Input diffuser: 4 Schroeder all-pass stages per channel with mutually
    prime lengths. DIFFUSION sets the all-pass gain (0 .. 0.75; above 0.75
    the sound turns metallic). SIZE scales the lengths with a smooth glide. */
class Diffusion
{
public:
    static constexpr int kStages = 4;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < kStages; ++s)
            {
                lines[c][s].prepare ((int) (fs * baseMs[c][s] * 0.001 * 1.25) + 8);
                delay[c][s].reset (fs, 0.4);
            }
        setParameters (0.5f, 0.7f);
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < kStages; ++s)
                delay[c][s].setImmediate (delay[c][s].getTarget());
        gainSmoother.reset (fs, 0.05);
        gainSmoother.setImmediate (gainTarget);
        reset();
    }

    void reset()
    {
        for (auto& ch : lines)
            for (auto& l : ch)
                l.clear();
    }

    void setParameters (float size01, float diffusion01) noexcept
    {
        const float scale = 0.4f + 0.8f * std::min (std::max (size01, 0.0f), 1.0f);
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < kStages; ++s)
                delay[c][s].setTarget (std::max (2.0f, baseMs[c][s] * 0.001f * (float) fs * scale));
        gainTarget = 0.75f * std::min (std::max (diffusion01, 0.0f), 1.0f);
        gainSmoother.setTarget (gainTarget);
    }

    void process (float& l, float& r) noexcept
    {
        const float g = gainSmoother.next();
        l = chain (0, l, g);
        r = chain (1, r, g);
    }

private:
    float chain (int c, float x, float g) noexcept
    {
        for (int s = 0; s < kStages; ++s)
        {
            const float d = lines[c][s].readFrac (delay[c][s].next());
            const float v = x + g * d;
            x = d - g * v;
            lines[c][s].push (v);
        }
        return x;
    }

    static constexpr float baseMs[2][kStages] = { { 4.771f, 3.595f, 12.73f, 9.307f },
                                                  { 4.937f, 3.411f, 13.11f, 8.873f } };
    DelayLine lines[2][kStages];
    LinearSmoother delay[2][kStages];
    LinearSmoother gainSmoother;
    float gainTarget = 0.5f;
    double fs = 48000.0;
};
} // namespace ba
