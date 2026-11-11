// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include "Util/DelayLine.h"
#include "Util/Filters.h"
#include "Modulation.h"

namespace ba
{
/** 16-line feedback delay network.
    - Orthogonal Hadamard feedback matrix (fast Walsh-Hadamard, lossless).
    - Per-line gain from the requested T60: g = 10^(-3 d / (T60 fs)), so the
      measured decay matches the DECAY value.
    - Per-line 3-band absorption: first-order low and high shelves set the
      low and high T60 (damping, multiband decay multipliers).
    - 8 of the 16 lines are modulated (cubic interpolation) to break up
      metallic resonances.
    - Freeze / Infinite: the decay exponent fades to zero (lossless loop).
    - Energy leveler: bounds the tank energy (Infinite, Shimmer, huge decays). */
class FDNReverb
{
public:
    static constexpr int N = 16;

    struct Settings
    {
        float decaySeconds = 2.2f;
        float size = 0.5f;          // 0..1
        float shape = 0.45f;        // 0..1, stretches line spread for halls
        float damping = 0.35f;      // 0..1
        float lowDecayMult = 1.0f;  // 0.25..2
        float highDecayMult = 1.0f; // 0.25..2
        float lowXoverHz = 250.0f;
        float highXoverHz = 4000.0f;
        float lfoHz = 0.6f;
        float modDepth = 0.25f;
        ModMode modMode = ModMode::Smooth;
        bool freeze = false;
        bool infinite = false;
    };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int i = 0; i < N; ++i)
        {
            // Rows of a Hadamard matrix: mutually orthogonal input/output taps.
            inSignL[i] = hadamardSign (3, i);
            inSignR[i] = hadamardSign (5, i);
            outSignL[i] = hadamardSign (6, i);
            outSignR[i] = hadamardSign (9, i);
        }
        energyCoeff = (float) (1.0 / (0.5 * fs));
        lfo.prepare (fs);
        const float maxDelay = (float) (maxBaseMs * 0.001 * fs * maxScale) + lfo.maxDepthSamples() + 8.0f;
        for (int i = 0; i < N; ++i)
        {
            lines[i].prepare ((int) maxDelay + 4);
            delaySmooth[i].reset (fs, 0.4);
        }
        freezeSmooth.reset (fs, 0.03);
        inputSmooth.reset (fs, 0.03);
        setSettings (settings);
        for (int i = 0; i < N; ++i)
            delaySmooth[i].setImmediate (delaySmooth[i].getTarget());
        freezeSmooth.setImmediate (freezeSmooth.getTarget());
        inputSmooth.setImmediate (inputSmooth.getTarget());
        reset();
    }

    void reset()
    {
        for (int i = 0; i < N; ++i)
        {
            lines[i].clear();
            apState[i] = 0.0f;
            lowShelf[i].reset();
            highShelf[i].reset();
        }
        energy = 0.0f;
        levelGain = 1.0f;
        freezeRef = -1.0f;
        frozenSamples = 0;
    }

    void setSettings (const Settings& s) noexcept
    {
        settings = s;
        const float scale = sizeScale (s.size, s.shape);
        for (int i = 0; i < N; ++i)
            // Integer lengths: unmodulated lines read without interpolation loss.
            delaySmooth[i].setTarget (std::max (4.0f, std::round (baseMs[i] * 0.001f * (float) fs * scale)));
        const bool lossless = s.freeze || s.infinite;
        freezeSmooth.setTarget (lossless ? 1.0f : 0.0f);
        inputSmooth.setTarget (s.freeze ? 0.0f : 1.0f);
        lfo.setParameters (s.lfoHz, s.modDepth, s.modMode);
        tanLow = std::tan (kPi * std::min (s.lowXoverHz, (float) (0.45 * fs)) / (float) fs);
        tanHigh = std::tan (kPi * std::min (s.highXoverHz, (float) (0.45 * fs)) / (float) fs);
    }

    /** Recompute per-line gains and shelves. Call once per internal sub-block. */
    void beginBlock() noexcept
    {
        const float lossless = freezeSmooth.getCurrent();
        const float e = 1.0f - lossless; // decay exponent scale (0 = infinite)
        const float t60Mid = std::max (settings.decaySeconds, 0.05f);
        const float damp = std::min (std::max (settings.damping, 0.0f), 1.0f);
        const float t60Low = t60Mid * settings.lowDecayMult;
        const float t60High = std::max (0.03f, t60Mid * (1.0f - 0.9f * damp) * settings.highDecayMult);
        const float invFs = 1.0f / (float) fs;
        for (int i = 0; i < N; ++i)
        {
            const float d = delaySmooth[i].getCurrent();
            const float k = -3.0f * d * invFs * e;
            const float gMid = std::pow (10.0f, k / t60Mid);
            const float gLow = std::pow (10.0f, k / t60Low);
            const float gHigh = std::pow (10.0f, k / t60High);
            midGain[i] = std::min (gMid, 1.0f);
            lowShelf[i].set (tanLow, gLow / gMid, true);
            highShelf[i].set (tanHigh, gHigh / gMid, false);
        }
    }

    /** One sample. extraIn (optional, may be null) is injected into the first
        four lines after the matrix (used by Shimmer). tap returns the mono
        sum of those four lines for the shimmer pitch shifter. */
    void process (float inL, float inR, float& outL, float& outR,
                  const float* extraIn = nullptr, float* tap = nullptr) noexcept
    {
        float mod[N];
        lfo.tick (mod);
        freezeSmooth.next();
        const float inGain = inputSmooth.next() * 0.25f;

        float r[N], a[N];
        float e = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const float base = delaySmooth[i].next();
            if (i & 1)
                r[i] = lines[i].readAllpass (std::max (base + mod[i], 3.0f), apState[i]);
            else
                r[i] = lines[i].readFrac (std::max (base, 2.0f));
            e += r[i] * r[i];
        }

        // Energy leveler: only acts when tank RMS per line exceeds ~-9 dBFS.
        // While frozen it also holds the energy captured at the freeze moment,
        // compensating the tiny loss of the modulated (interpolated) reads.
        energy += energyCoeff * (e * (1.0f / N) - energy);
        const float limit = energy > kEnergyLimit ? std::sqrt (kEnergyLimit / energy) : 1.0f;
        float target = limit;
        const bool frozen = settings.freeze && freezeSmooth.getCurrent() >= 0.999f;
        if (frozen)
        {
            // Capture the reference once the energy meter has settled (1 s).
            if (freezeRef < 0.0f && ++frozenSamples >= (int) fs)
                freezeRef = std::min (energy, kEnergyLimit);
            if (freezeRef >= 0.0f)
            {
                const float hold = std::sqrt (freezeRef / std::max (energy, 1.0e-12f));
                target = std::min (hold, 1.0005f);
                if (energy > kEnergyLimit) target = std::min (target, limit);
            }
        }
        else
        {
            freezeRef = -1.0f;
            frozenSamples = 0;
        }
        levelGain += (target < levelGain ? 0.002f : 0.0002f) * (target - levelGain);

        for (int i = 0; i < N; ++i)
            a[i] = highShelf[i].process (lowShelf[i].process (r[i])) * midGain[i] * levelGain;

        // Fast Walsh-Hadamard transform (orthonormal after the 1/4 scale).
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float x = a[j], y = a[j + h];
                    a[j] = x + y;
                    a[j + h] = x - y;
                }

        float sumL = 0.0f, sumR = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            float v = a[i] * 0.25f + inGain * (inL * inSignL[i] + inR * inSignR[i]);
            if (extraIn != nullptr && i < 4)
                v += *extraIn * 0.5f;
            lines[i].push (v);
            sumL += r[i] * outSignL[i];
            sumR += r[i] * outSignR[i];
        }
        if (tap != nullptr)
            *tap = 0.5f * (r[0] - r[1] + r[2] - r[3]);

        outL = sumL * kOutScale;
        outR = sumR * kOutScale;
    }

    float getLevelerGain() const noexcept { return levelGain; }

private:
    static float sizeScale (float size, float shape) noexcept
    {
        size = std::min (std::max (size, 0.0f), 1.0f);
        shape = std::min (std::max (shape, 0.0f), 1.0f);
        return (0.15f + 0.85f * size) * (0.9f + 0.4f * shape);
    }

    static constexpr float maxBaseMs = 141.9f;
    static constexpr float maxScale = 1.3f;
    static constexpr float kEnergyLimit = 0.125f; // per-line mean square (~ -9 dBFS RMS)
    static constexpr float kOutScale = 0.35f;
    static constexpr float baseMs[N] = { 23.1f, 27.7f, 31.3f, 36.7f, 41.1f, 45.3f, 50.9f, 56.3f,
                                         62.9f, 69.7f, 78.1f, 86.9f, 97.3f, 109.1f, 123.7f, 141.9f };

    float inSignL[N] {}, inSignR[N] {}, outSignL[N] {}, outSignR[N] {};

    DelayLine lines[N];
    LinearSmoother delaySmooth[N];
    FirstOrderShelf lowShelf[N], highShelf[N];
    float midGain[N] {}, apState[N] {};
    LfoBank<N> lfo;
    LinearSmoother freezeSmooth, inputSmooth;
    Settings settings;
    double fs = 48000.0;
    float tanLow = 0.016f, tanHigh = 0.27f;
    float energy = 0.0f, levelGain = 1.0f, freezeRef = -1.0f;
    int frozenSamples = 0;
    float energyCoeff = 1.0e-5f;
};
} // namespace ba
