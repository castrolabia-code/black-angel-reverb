// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include "Util/DelayLine.h"
#include "Util/Filters.h"
#include <array>

namespace ba
{
/** Multitap early reflections: 24 taps per channel. SHAPE moves continuously
    through six geometries (Studio, Room, Plate, Chamber, Hall, Cathedral),
    SIZE scales the tap times, DAMPING darkens the reflections. Tap times glide
    smoothly when SIZE/SHAPE move, so automation does not click. */
class EarlyReflections
{
public:
    static constexpr int kTaps = 24;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const int maxSamples = (int) (fs * 0.36) + 8; // 160 ms * 1.25 size scale + jitter headroom
        lineL.prepare (maxSamples);
        lineR.prepare (maxSamples);
        maxTap = (float) (maxSamples - 4);
        buildRandomTables();
        computeTargets();
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < kTaps; ++k)
            {
                cur[c][k] = tgt[c][k];
                gain[c][k] = tgtGain[c][k];
            }
        reset();
    }

    void reset()
    {
        lineL.clear(); lineR.clear();
        lpL.reset(); lpR.reset();
    }

    void setParameters (float size01, float shape01, float damping01) noexcept
    {
        size = size01; shape = shape01;
        const float hz = 18000.0f * (1.0f - 0.8f * damping01);
        lpL.setCutoff (fs, hz);
        lpR.setCutoff (fs, hz);
    }

    /** Called once per internal sub-block (before processing its samples). */
    void beginBlock (int numSamples) noexcept
    {
        computeTargets();
        const float coeff = 1.0f - std::exp (-(float) numSamples / (0.08f * (float) fs));
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < kTaps; ++k)
            {
                start[c][k] = cur[c][k];
                cur[c][k] += coeff * (tgt[c][k] - cur[c][k]);
                step[c][k] = (cur[c][k] - start[c][k]) / (float) numSamples;
                gain[c][k] += coeff * (tgtGain[c][k] - gain[c][k]);
            }
        blockIndex = 0;
    }

    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        lineL.push (inL);
        lineR.push (inR);
        const float fi = (float) blockIndex++;
        float sl = 0.0f, sr = 0.0f;
        for (int k = 0; k < kTaps; ++k)
        {
            sl += gain[0][k] * lineL.readFrac (start[0][k] + step[0][k] * fi);
            sr += gain[1][k] * lineR.readFrac (start[1][k] + step[1][k] * fi);
        }
        outL = lpL.process (sl);
        outR = lpR.process (sr);
    }

private:
    struct Anchor { float startMs, spanMs, curve; };

    void buildRandomTables()
    {
        Rng rng (0xB1AC4A6Eu);
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < kTaps; ++k)
            {
                jitter[c][k] = rng.bip() * 0.4f;
                sign[c][k] = rng.uni() < 0.5f ? -1.0f : 1.0f;
                level[c][k] = 0.8f + 0.4f * rng.uni();
            }
    }

    void computeTargets() noexcept
    {
        static constexpr std::array<Anchor, 6> anchors { {
            { 1.5f,  18.0f, 1.4f },  // Studio
            { 2.5f,  35.0f, 1.3f },  // Room
            { 0.5f,  14.0f, 1.0f },  // Plate
            { 4.0f,  50.0f, 1.2f },  // Chamber
            { 8.0f,  80.0f, 1.5f },  // Hall
            { 14.0f, 120.0f, 1.7f }  // Cathedral
        } };
        const float pos = std::min (std::max (shape, 0.0f), 1.0f) * 5.0f;
        const int i0 = std::min ((int) pos, 4);
        const float t = pos - (float) i0;
        const Anchor& a = anchors[(size_t) i0];
        const Anchor& b = anchors[(size_t) i0 + 1];
        const float startMs = a.startMs + t * (b.startMs - a.startMs);
        const float spanMs  = a.spanMs  + t * (b.spanMs  - a.spanMs);
        const float curve   = a.curve   + t * (b.curve   - a.curve);
        const float scale = 0.35f + 0.9f * std::min (std::max (size, 0.0f), 1.0f);

        for (int c = 0; c < 2; ++c)
        {
            float energy = 0.0f;
            for (int k = 0; k < kTaps; ++k)
            {
                float u = ((float) k + 0.5f + jitter[c][k]) / (float) kTaps;
                u = std::min (std::max (u, 0.0f), 1.0f);
                const float ms = (startMs + spanMs * std::pow (u, curve)) * scale;
                tgt[c][k] = std::min (std::max (2.0f, ms * 0.001f * (float) fs + 1.0f), maxTap);
                const float g = std::pow (1.0f - 0.85f * u, 1.5f) * level[c][k] * sign[c][k];
                tgtGain[c][k] = g;
                energy += g * g;
            }
            const float norm = std::sqrt (0.5f / std::max (energy, 1.0e-9f));
            for (int k = 0; k < kTaps; ++k)
                tgtGain[c][k] *= norm;
        }
    }

    DelayLine lineL, lineR;
    OnePoleLP lpL, lpR;
    double fs = 48000.0;
    float size = 0.5f, shape = 0.45f, maxTap = 1000.0f;
    float jitter[2][kTaps] {}, sign[2][kTaps] {}, level[2][kTaps] {};
    float tgt[2][kTaps] {}, tgtGain[2][kTaps] {};
    float cur[2][kTaps] {}, start[2][kTaps] {}, step[2][kTaps] {}, gain[2][kTaps] {};
    int blockIndex = 0;
};
} // namespace ba
