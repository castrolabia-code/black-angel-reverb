// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#include "ParameterLayout.h"
#include "DSP/IRSynth.h"

namespace ba::params
{
namespace
{
constexpr int kVersion = 1;

using Range = juce::NormalisableRange<float>;

Range logRange (float lo, float hi)
{
    return Range (lo, hi,
                  [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                  [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                  [] (float, float, float v) { return v; });
}

juce::String fmt (float v, int decimals, const char* unit)
{
    return juce::String (v, decimals) + unit;
}

juce::String hzText (float v, int)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                        : juce::String (juce::roundToInt (v)) + " Hz";
}

juce::String dbText (float v, int)
{
    return v <= -59.9f ? juce::String ("-inf dB") : juce::String (v, 1) + " dB";
}

juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

std::unique_ptr<juce::AudioParameterFloat> flt (const char* pid, const char* name, Range r, float def,
                                                 juce::String (*text) (float, int), const char* label = "")
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid, kVersion }, name, r, def,
        juce::AudioParameterFloatAttributes().withLabel (label).withStringFromValueFunction (text));
}

std::unique_ptr<juce::AudioParameterBool> bln (const char* pid, const char* name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { pid, kVersion }, name, def);
}

std::unique_ptr<juce::AudioParameterChoice> chc (const char* pid, const char* name, const juce::StringArray& items, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pid, kVersion }, name, items, def);
}
} // namespace

const juce::StringArray& syncNames()
{
    static const juce::StringArray names { "Off", "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/16D",
                                           "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2" };
    return names;
}

double syncQuarterNotes (int index)
{
    static const double q[] { 0.0, 1.0 / 16.0, 1.0 / 12.0, 1.0 / 8.0, 1.0 / 6.0, 1.0 / 4.0, 3.0 / 8.0,
                              1.0 / 3.0, 1.0 / 2.0, 3.0 / 4.0, 2.0 / 3.0, 1.0, 1.5, 4.0 / 3.0, 2.0 };
    return index >= 0 && index < (int) std::size (q) ? q[index] : 0.0;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    Range predelayRange (0.0f, 500.0f);
    predelayRange.setSkewForCentre (80.0f);

    auto ms = [] (float v, int) { return fmt (v, v < 10.0f ? 1 : 0, " ms"); };
    auto sec = [] (float v, int) { return fmt (v, v < 10.0f ? 2 : 1, " s"); };
    auto hz2 = [] (float v, int) { return fmt (v, 2, " Hz"); };
    auto q = [] (float v, int) { return fmt (v, 2, ""); };

    layout.add (flt (id::predelay, "Predelay", predelayRange, 20.0f, ms, "ms"));
    layout.add (chc (id::predelaySync, "Predelay Sync", syncNames(), 0));
    layout.add (flt (id::decay, "Decay", logRange (0.1f, 30.0f), 2.2f, sec, "s"));
    layout.add (flt (id::damping, "Damping", Range (0.0f, 1.0f), 0.35f, pctText));
    layout.add (flt (id::size, "Size", Range (0.0f, 1.0f), 0.5f, pctText));
    layout.add (flt (id::width, "Width", Range (0.0f, 2.0f), 1.0f, pctText));
    layout.add (flt (id::lfoSpeed, "LFO Speed", logRange (0.01f, 10.0f), 0.6f, hz2, "Hz"));
    layout.add (flt (id::modDepth, "Modulation", Range (0.0f, 1.0f), 0.25f, pctText));
    layout.add (chc (id::modMode, "Mod Mode", { "Smooth", "Chorus", "Random" }, 0));
    layout.add (flt (id::diffusion, "Diffusion", Range (0.0f, 1.0f), 0.7f, pctText));
    layout.add (flt (id::shape, "Shape", Range (0.0f, 1.0f), 0.45f, pctText));
    layout.add (flt (id::earlyLevel, "Early", Range (-60.0f, 6.0f, 0.0f, 2.5f), -6.0f, dbText, "dB"));
    layout.add (flt (id::lateLevel, "Late", Range (-60.0f, 6.0f, 0.0f, 2.5f), 0.0f, dbText, "dB"));
    layout.add (flt (id::mix, "Dry/Wet", Range (0.0f, 1.0f), 0.3f, pctText));
    layout.add (flt (id::wetLevel, "Wet Output", Range (-24.0f, 12.0f), 0.0f, dbText, "dB"));
    layout.add (chc (id::routingMode, "Insert / Aux", { "Insert", "Aux" }, 0));
    layout.add (chc (id::engineMode, "Engine", { "Algorithmic", "Convolution", "Hybrid" }, 0));
    layout.add (flt (id::blend, "Algo/IR Blend", Range (0.0f, 1.0f), 0.5f, pctText));
    layout.add (flt (id::algoLevel, "Algo Level", Range (-60.0f, 6.0f, 0.0f, 2.5f), 0.0f, dbText, "dB"));
    layout.add (flt (id::irLevel, "IR Level", Range (-60.0f, 6.0f, 0.0f, 2.5f), 0.0f, dbText, "dB"));

    juce::StringArray irNames;
    for (const auto& m : IRSynth::factoryModels())
        irNames.add (m.name);
    layout.add (chc (id::irIndex, "IR", irNames, 3));
    layout.add (flt (id::irStretch, "IR Stretch", Range (0.5f, 2.0f), 1.0f, pctText));
    layout.add (flt (id::irDecay, "IR Decay", Range (0.1f, 1.0f), 1.0f, pctText));

    layout.add (bln (id::lowCutOn, "Low Cut On", true));
    layout.add (flt (id::lowCut, "Low Cut", logRange (20.0f, 2000.0f), 120.0f, hzText, "Hz"));
    layout.add (chc (id::lowCutSlope, "Low Cut Slope", { "12 dB/oct", "24 dB/oct" }, 0));
    layout.add (bln (id::highCutOn, "High Cut On", true));
    layout.add (flt (id::highCut, "High Cut", logRange (1000.0f, 20000.0f), 12000.0f, hzText, "Hz"));
    layout.add (chc (id::highCutSlope, "High Cut Slope", { "12 dB/oct", "24 dB/oct" }, 0));
    layout.add (bln (id::lowShelfOn, "Low Shelf On", false));
    layout.add (flt (id::lowShelfF, "Low Shelf Freq", logRange (40.0f, 1000.0f), 250.0f, hzText, "Hz"));
    layout.add (flt (id::lowShelfG, "Low Shelf Gain", Range (-18.0f, 18.0f), 0.0f, dbText, "dB"));
    layout.add (bln (id::peak1On, "Param 1 On", false));
    layout.add (flt (id::peak1F, "Param 1 Freq", logRange (40.0f, 18000.0f), 500.0f, hzText, "Hz"));
    layout.add (flt (id::peak1G, "Param 1 Gain", Range (-18.0f, 18.0f), 0.0f, dbText, "dB"));
    layout.add (flt (id::peak1Q, "Param 1 Q", logRange (0.1f, 10.0f), 1.0f, q));
    layout.add (bln (id::peak2On, "Param 2 On", false));
    layout.add (flt (id::peak2F, "Param 2 Freq", logRange (40.0f, 18000.0f), 3000.0f, hzText, "Hz"));
    layout.add (flt (id::peak2G, "Param 2 Gain", Range (-18.0f, 18.0f), 0.0f, dbText, "dB"));
    layout.add (flt (id::peak2Q, "Param 2 Q", logRange (0.1f, 10.0f), 1.0f, q));
    layout.add (bln (id::highShelfOn, "High Shelf On", false));
    layout.add (flt (id::highShelfF, "High Shelf Freq", logRange (1000.0f, 16000.0f), 6000.0f, hzText, "Hz"));
    layout.add (flt (id::highShelfG, "High Shelf Gain", Range (-18.0f, 18.0f), 0.0f, dbText, "dB"));

    layout.add (bln (id::freeze, "Freeze", false));
    layout.add (bln (id::infinite, "Infinite", false));
    layout.add (bln (id::bypass, "Bypass", false));
    return layout;
}
} // namespace ba::params
