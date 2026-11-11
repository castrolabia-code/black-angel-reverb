// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include <vector>
#include <algorithm>

namespace ba
{
/** Power-of-two circular delay line.
    Delays are measured relative to the NEXT sample to be pushed:
    read (d) returns the sample pushed d calls ago (d >= 1).
    Fractional reads use 4-point Hermite interpolation and need d >= 2. */
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 8)
            size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void clear() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); }
    int capacity() const noexcept { return mask - 6; }

    void push (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    float at (int d) const noexcept { return buffer[(size_t) ((writePos - d) & mask)]; }

    float readFrac (float d) const noexcept
    {
        const int di = (int) d;
        const float f = d - (float) di;
        const float xm1 = at (di - 1), x0 = at (di), x1 = at (di + 1), x2 = at (di + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    /** First-order all-pass (Thiran) fractional read: unity magnitude at all
        frequencies, so a modulated line inside a feedback loop loses no
        energy. state is the caller's one-sample memory. Needs d >= 2. */
    float readAllpass (float d, float& state) const noexcept
    {
        int di = (int) d;
        float f = d - (float) di;
        if (f < 0.1f) { f += 1.0f; --di; } // keep the coefficient well-conditioned
        const float eta = (1.0f - f) / (1.0f + f);
        const float y = at (di + 1) + eta * (at (di) - state);
        state = y;
        return y;
    }

private:
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
};
} // namespace ba
