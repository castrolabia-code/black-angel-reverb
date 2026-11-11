// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include "Util/DelayLine.h"
#include "Util/DspUtil.h"

namespace ba
{
/** Stereo pre-delay. Changing the time cross-fades between two read heads
    (30 ms) instead of sliding the head, so there is no pitch glide or click. */
class PreDelay
{
public:
    static constexpr float kMaxMs = 500.0f;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const int maxSamples = (int) (fs * (kMaxMs + 10.0f) / 1000.0f) + 4;
        lineL.prepare (maxSamples);
        lineR.prepare (maxSamples);
        fadeLength = std::max (1, (int) (0.03 * fs));
        reset();
    }

    void reset()
    {
        lineL.clear(); lineR.clear();
        currentDelay = nextDelay = targetDelay;
        fadePos = -1;
    }

    void setDelayMs (float ms) noexcept
    {
        ms = std::min (std::max (ms, 0.0f), kMaxMs);
        targetDelay = std::max (1, (int) std::lround (ms * 0.001 * fs));
    }

    /** Snap to the target without a fade (use after prepare / preset load while silent). */
    void snap() noexcept { currentDelay = nextDelay = targetDelay; fadePos = -1; }

    void process (float& l, float& r) noexcept
    {
        lineL.push (l);
        lineR.push (r);

        if (fadePos < 0 && targetDelay != currentDelay)
        {
            nextDelay = targetDelay;
            fadePos = 0;
        }

        if (fadePos < 0)
        {
            l = lineL.at (currentDelay);
            r = lineR.at (currentDelay);
            return;
        }

        const float t = (float) fadePos / (float) fadeLength;
        const float gOld = 1.0f - t, gNew = t;
        l = gOld * lineL.at (currentDelay) + gNew * lineL.at (nextDelay);
        r = gOld * lineR.at (currentDelay) + gNew * lineR.at (nextDelay);
        if (++fadePos >= fadeLength)
        {
            currentDelay = nextDelay;
            fadePos = -1;
        }
    }

private:
    DelayLine lineL, lineR;
    double fs = 48000.0;
    int targetDelay = 1, currentDelay = 1, nextDelay = 1;
    int fadePos = -1, fadeLength = 1440;
};
} // namespace ba
