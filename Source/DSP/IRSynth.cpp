// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#include "IRSynth.h"
#include "Util/DspUtil.h"
#include "Util/Filters.h"

#include <algorithm>
#include <cmath>

namespace ba
{
const std::array<IRModel, IRSynth::kNumFactoryIRs>& IRSynth::factoryModels()
{
    //   name                category        rt60  lowM  highM gap  erSpan taps erLvl build width bright  kind
    static const std::array<IRModel, kNumFactoryIRs> models { {
        { "Small Room",       "Small Spaces",   0.45f, 1.1f, 0.6f,  2.0f,  18.0f, 14, 0.9f, 10.0f, 0.70f, 12000.0f, 0 },
        { "Tiled Bathroom",   "Small Spaces",   0.90f, 0.9f, 0.9f,  1.5f,  14.0f, 18, 1.1f,  6.0f, 0.60f, 15000.0f, 0 },
        { "Wood Closet",      "Small Spaces",   0.25f, 1.2f, 0.4f,  1.0f,   8.0f, 10, 1.0f,  4.0f, 0.50f,  8000.0f, 0 },
        { "Studio A",         "Studios",        0.60f, 1.0f, 0.7f,  3.0f,  25.0f, 16, 0.8f, 15.0f, 0.80f, 14000.0f, 0 },
        { "Studio B Live",    "Studios",        0.95f, 1.0f, 0.8f,  4.0f,  32.0f, 20, 0.8f, 20.0f, 0.85f, 15000.0f, 0 },
        { "Drum Room",        "Studios",        0.80f, 1.3f, 0.7f,  3.5f,  28.0f, 22, 1.0f, 12.0f, 0.75f, 13000.0f, 0 },
        { "Vocal Booth A",    "Vocal Rooms",    0.35f, 1.0f, 0.5f,  1.5f,  10.0f, 10, 0.7f,  6.0f, 0.60f, 11000.0f, 0 },
        { "Vocal Booth B",    "Vocal Rooms",    0.50f, 0.9f, 0.6f,  2.0f,  14.0f, 12, 0.7f,  8.0f, 0.70f, 12500.0f, 0 },
        { "Chamber",          "Vocal Rooms",    1.40f, 0.9f, 0.8f,  6.0f,  40.0f, 24, 0.7f, 25.0f, 0.90f, 14000.0f, 0 },
        { "Concert Hall",     "Concert Halls",  2.20f, 1.2f, 0.6f, 14.0f,  70.0f, 28, 0.6f, 60.0f, 0.95f, 13000.0f, 0 },
        { "Chamber Hall",     "Concert Halls",  1.70f, 1.1f, 0.7f, 10.0f,  55.0f, 24, 0.6f, 45.0f, 0.90f, 14000.0f, 0 },
        { "Opera House",      "Concert Halls",  1.50f, 1.3f, 0.5f, 18.0f,  80.0f, 26, 0.5f, 70.0f, 0.95f, 11000.0f, 0 },
        { "Grand Auditorium", "Auditoriums",    3.00f, 1.2f, 0.6f, 22.0f,  95.0f, 30, 0.5f, 90.0f, 1.00f, 12000.0f, 0 },
        { "Arena",            "Auditoriums",    3.50f, 1.1f, 0.5f, 35.0f, 120.0f, 24, 0.6f, 110.0f, 1.00f, 10000.0f, 0 },
        { "Theater",          "Auditoriums",    1.30f, 1.1f, 0.6f, 12.0f,  60.0f, 22, 0.6f, 50.0f, 0.90f, 12000.0f, 0 },
        { "Stone Chapel",     "Churches",       2.80f, 1.0f, 0.7f, 12.0f,  60.0f, 26, 0.7f, 55.0f, 0.95f, 12000.0f, 0 },
        { "Cathedral",        "Churches",       6.50f, 1.3f, 0.5f, 30.0f, 140.0f, 32, 0.5f, 160.0f, 1.00f, 10000.0f, 0 },
        { "Basilica",         "Churches",       8.50f, 1.4f, 0.4f, 40.0f, 160.0f, 32, 0.4f, 200.0f, 1.00f,  9000.0f, 0 },
        { "Cinematic Stage",  "Cinematic",      4.20f, 1.4f, 0.5f, 25.0f, 110.0f, 28, 0.5f, 120.0f, 1.00f, 11000.0f, 0 },
        { "Dark Tunnel",      "Cinematic",      2.60f, 1.6f, 0.3f,  8.0f,  50.0f, 30, 0.9f, 30.0f, 0.70f,  6000.0f, 0 },
        { "Canyon",           "Cinematic",      5.00f, 1.1f, 0.6f, 80.0f, 400.0f, 12, 1.2f, 250.0f, 1.00f,  9000.0f, 0 },
        { "Reverse Swell",    "Experimental",   2.00f, 1.0f, 0.8f,  0.0f,  20.0f,  8, 0.3f, 10.0f, 1.00f, 14000.0f, 1 },
        { "Gated Plate",      "Experimental",   1.80f, 1.0f, 0.9f,  2.0f,  10.0f, 16, 0.6f,  5.0f, 0.90f, 16000.0f, 2 },
        { "Infinite Bloom",   "Experimental",  12.00f, 1.0f, 0.7f, 20.0f,  80.0f, 20, 0.3f, 900.0f, 1.00f, 12000.0f, 3 },
    } };
    return models;
}

IRBuffer IRSynth::generateFactory (int index, double sampleRate, float stretch, float decayScale)
{
    const auto& models = factoryModels();
    index = std::min (std::max (index, 0), kNumFactoryIRs - 1);
    return generate (models[(size_t) index], sampleRate, stretch, decayScale, 0xA11CE000u + (uint32_t) index * 7919u);
}

IRBuffer IRSynth::generate (const IRModel& m, double fs, float stretch, float decayScale, uint32_t seed)
{
    stretch = std::min (std::max (stretch, 0.5f), 2.0f);
    decayScale = std::min (std::max (decayScale, 0.1f), 1.0f);
    const float rt = m.rt60 * stretch * decayScale;
    const float gap = m.gapMs * 0.001f * stretch;
    const float span = m.erSpanMs * 0.001f * stretch;
    const float build = std::max (0.002f, m.buildMs * 0.001f * stretch);

    float lengthSec = gap + span + rt * 1.1f + 0.05f;
    if (m.kind == 2) lengthSec = gap + 0.35f * stretch;
    lengthSec = std::min (lengthSec, 20.0f);
    const int len = std::max (64, (int) (lengthSec * fs));

    IRBuffer ir;
    ir.sampleRate = fs;
    ir.channels.assign (2, std::vector<float> ((size_t) len, 0.0f));

    Rng rng (seed != 0 ? seed : 0x1234567u);
    std::vector<float> common ((size_t) len), own ((size_t) len);

    const float t60Low = rt * m.lowMult, t60High = std::max (0.05f, rt * m.highMult);
    const float wMix = std::min (std::max (m.width, 0.0f), 1.0f);
    const float cA = std::sqrt (1.0f - wMix * wMix), cB = wMix; // cA^2 + cB^2 = 1

    for (size_t i = 0; i < (size_t) len; ++i)
        common[i] = rng.gauss();

    for (int ch = 0; ch < 2; ++ch)
    {
        for (size_t i = 0; i < (size_t) len; ++i)
            own[i] = rng.gauss();

        // Three-band noise with independent decay per band.
        SVF lowLP, highHP;
        lowLP.setup (SVF::Type::LowPass, fs, 300.0f, 0.707f, 0.0f);
        highHP.setup (SVF::Type::HighPass, fs, 3500.0f, 0.707f, 0.0f);
        auto& out = ir.channels[(size_t) ch];
        const float kLow = -6.9078f / (t60Low * (float) fs);
        const float kMid = -6.9078f / (rt * (float) fs);
        const float kHigh = -6.9078f / (t60High * (float) fs);
        const int start = (int) (gap * fs);
        for (int i = start; i < len; ++i)
        {
            const float n = cA * common[(size_t) i] + cB * own[(size_t) i];
            const float lo = lowLP.process (n);
            const float hi = highHP.process (n);
            const float mid = n - lo - hi;
            const float t = (float) (i - start);
            float env;
            if (m.kind == 2)
                env = t < 0.28f * stretch * (float) fs ? 1.0f : std::exp (-t / (0.01f * (float) fs));
            else
                env = 1.0f;
            const float onset = 1.0f - std::exp (-t / (build * (float) fs));
            const float tail = lo * std::exp (kLow * t) + mid * std::exp (kMid * t) + hi * std::exp (kHigh * t);
            out[(size_t) i] = tail * onset * env;
        }

        if (m.kind == 1)
            std::reverse (out.begin() + start, out.end());

        // Early reflections: discrete taps, slightly different per channel.
        float tailEnergy = 0.0f;
        for (int i = start; i < std::min (len, start + (int) (0.05 * fs)); ++i)
            tailEnergy += out[(size_t) i] * out[(size_t) i];
        const float erAmp = m.erLevel * std::sqrt (std::max (tailEnergy, 1.0e-6f) / (float) std::max (1, m.erTaps)) * 2.0f;
        for (int k = 0; k < m.erTaps; ++k)
        {
            const float u = ((float) k + rng.uni()) / (float) m.erTaps;
            const float tSec = gap + span * u * u;
            const int idx = (int) (tSec * fs);
            if (idx < 0 || idx >= len) continue;
            const float sgn = rng.uni() < 0.5f ? -1.0f : 1.0f;
            const float atten = std::pow (1.0f - 0.7f * u, 1.5f);
            const float placed = m.kind == 1 ? (float) (len - 1 - idx + start) : (float) idx;
            out[(size_t) std::min (len - 1, (int) placed)] += sgn * atten * erAmp * (0.8f + 0.4f * rng.uni());
        }

        // Overall tone.
        SVF tone;
        tone.setup (SVF::Type::LowPass, fs, std::min (m.brightnessHz, (float) (0.45 * fs)), 0.6f, 0.0f);
        for (auto& v : out)
            v = tone.process (v);

        // Short fade-out of the last 10 ms.
        const int fade = std::min (len, (int) (0.01 * fs));
        for (int i = 0; i < fade; ++i)
            out[(size_t) (len - 1 - i)] *= (float) i / (float) fade;
    }

    normalizeIR (ir);
    return ir;
}
} // namespace ba
