// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/HybridReverb.h"
#include "Parameters/ParameterLayout.h"

#include <atomic>

/** Builds convolution instances off the audio thread whenever the selected
    IR, its stretch/decay or the sample rate changes, and frees old ones. */
class IRLoaderThread : public juce::Thread
{
public:
    explicit IRLoaderThread (ba::ConvolutionEngine& e) : juce::Thread ("BlackAngel IR loader"), engine (e) {}
    ~IRLoaderThread() override { stopThread (4000); }

    /** Cheap when nothing changed (called from the audio thread every block). */
    void request (int index, float stretch, float decay, double sampleRate) noexcept
    {
        if (reqIndex.load() == index && reqStretch.load() == stretch
            && reqDecay.load() == decay && reqRate.load() == sampleRate)
            return;
        reqIndex.store (index);
        reqStretch.store (stretch);
        reqDecay.store (decay);
        reqRate.store (sampleRate);
        notify();
    }
    void forceRebuild() { const juce::ScopedLock sl (buildLock); builtRate = 0.0; }

    /** Synchronous build, used by prepareToPlay so offline renders start with an IR. */
    void buildNow();

    void run() override;

private:
    ba::ConvolutionEngine& engine;
    std::atomic<int> reqIndex { 3 };
    std::atomic<float> reqStretch { 1.0f }, reqDecay { 1.0f };
    std::atomic<double> reqRate { 0.0 };
    int builtIndex = -1;
    float builtStretch = 0.0f, builtDecay = 0.0f;
    double builtRate = 0.0;
    juce::CriticalSection buildLock;
};

class BlackAngelProcessor : public juce::AudioProcessor
{
public:
    BlackAngelProcessor();
    ~BlackAngelProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

private:
    ba::ReverbParams readParameters();
    float value (const char* pid) const { return params.at (pid)->load(); }

    std::map<juce::String, std::atomic<float>*> params;
    juce::AudioProcessorParameter* bypassParam = nullptr;

    ba::HybridReverb reverb;
    IRLoaderThread irLoader { reverb.getConvolution() };
    juce::AudioBuffer<float> dryCopy;
    juce::LinearSmoothedValue<float> bypassMix;
    double currentRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlackAngelProcessor)
};
