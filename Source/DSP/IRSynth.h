// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include "ConvolutionEngine.h"
#include <array>

namespace ba
{
/** Description of one synthetic space. All factory IRs are generated from
    these models (image-like early reflections + frequency-dependent
    stochastic tail). They are original syntheses, not recordings. */
struct IRModel
{
    const char* name;
    const char* category;
    float rt60;          // mid-band T60 in seconds
    float lowMult;       // low-band T60 multiplier
    float highMult;      // high-band T60 multiplier
    float gapMs;         // direct-to-first-reflection gap
    float erSpanMs;      // duration of the early reflection cluster
    int erTaps;          // number of discrete reflections
    float erLevel;       // early level relative to tail
    float buildMs;       // tail build-up time
    float width;         // 0 = mono-correlated, 1 = fully decorrelated
    float brightnessHz;  // overall low-pass
    int kind;            // 0 normal, 1 reverse swell, 2 gated, 3 bloom
};

class IRSynth
{
public:
    static constexpr int kNumFactoryIRs = 24;
    static const std::array<IRModel, kNumFactoryIRs>& factoryModels();

    /** Generates a stereo IR at sampleRate. stretch scales all times
        (0.5..2), decayScale scales the T60 (0.1..1). Not real-time safe. */
    static IRBuffer generate (const IRModel& model, double sampleRate,
                              float stretch = 1.0f, float decayScale = 1.0f,
                              uint32_t seed = 0);
    static IRBuffer generateFactory (int index, double sampleRate,
                                     float stretch = 1.0f, float decayScale = 1.0f);
};
} // namespace ba
