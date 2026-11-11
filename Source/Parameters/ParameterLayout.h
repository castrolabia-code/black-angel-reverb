// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ba::params
{
/** Stable parameter IDs. Never rename an ID that has shipped: saved DAW
    sessions and presets refer to them. */
namespace id
{
    inline constexpr const char* predelay     = "predelay";
    inline constexpr const char* predelaySync = "predelaySync";
    inline constexpr const char* decay        = "decay";
    inline constexpr const char* damping      = "damping";
    inline constexpr const char* size         = "size";
    inline constexpr const char* width        = "width";
    inline constexpr const char* lfoSpeed     = "lfoSpeed";
    inline constexpr const char* modDepth     = "modDepth";
    inline constexpr const char* modMode      = "modMode";
    inline constexpr const char* diffusion    = "diffusion";
    inline constexpr const char* shape        = "shape";
    inline constexpr const char* earlyLevel   = "earlyLevel";
    inline constexpr const char* lateLevel    = "lateLevel";
    inline constexpr const char* mix          = "mix";
    inline constexpr const char* wetLevel     = "wetLevel";
    inline constexpr const char* routingMode  = "routingMode";
    inline constexpr const char* engineMode   = "engineMode";
    inline constexpr const char* blend        = "blend";
    inline constexpr const char* algoLevel    = "algoLevel";
    inline constexpr const char* irLevel      = "irLevel";
    inline constexpr const char* irIndex      = "irIndex";
    inline constexpr const char* irStretch    = "irStretch";
    inline constexpr const char* irDecay      = "irDecay";
    inline constexpr const char* lowCutOn     = "lowCutOn";
    inline constexpr const char* lowCut       = "lowCut";
    inline constexpr const char* lowCutSlope  = "lowCutSlope";
    inline constexpr const char* highCutOn    = "highCutOn";
    inline constexpr const char* highCut      = "highCut";
    inline constexpr const char* highCutSlope = "highCutSlope";
    inline constexpr const char* lowShelfOn   = "lowShelfOn";
    inline constexpr const char* lowShelfF    = "lowShelfF";
    inline constexpr const char* lowShelfG    = "lowShelfG";
    inline constexpr const char* peak1On      = "peak1On";
    inline constexpr const char* peak1F       = "peak1F";
    inline constexpr const char* peak1G       = "peak1G";
    inline constexpr const char* peak1Q       = "peak1Q";
    inline constexpr const char* peak2On      = "peak2On";
    inline constexpr const char* peak2F       = "peak2F";
    inline constexpr const char* peak2G       = "peak2G";
    inline constexpr const char* peak2Q       = "peak2Q";
    inline constexpr const char* highShelfOn  = "highShelfOn";
    inline constexpr const char* highShelfF   = "highShelfF";
    inline constexpr const char* highShelfG   = "highShelfG";
    inline constexpr const char* freeze       = "freeze";
    inline constexpr const char* infinite     = "infinite";
    inline constexpr const char* bypass       = "bypass";
}

/** Tempo-synced pre-delay choices, in quarter notes (index 0 = Off). */
const juce::StringArray& syncNames();
double syncQuarterNotes (int index);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
} // namespace ba::params
