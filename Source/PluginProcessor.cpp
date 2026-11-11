// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/IRSynth.h"

namespace pid = ba::params::id;

//==============================================================================
void IRLoaderThread::buildNow()
{
    const juce::ScopedLock sl (buildLock);
    const int index = reqIndex.load();
    const float stretch = reqStretch.load(), decay = reqDecay.load();
    const double rate = reqRate.load();
    if (rate <= 0.0)
        return;
    if (index == builtIndex && stretch == builtStretch && decay == builtDecay && rate == builtRate)
        return;

    auto inst = std::make_unique<ba::ConvolutionInstance>();
    inst->build (ba::IRSynth::generateFactory (index, rate, stretch, decay), rate);
    engine.submit (std::move (inst));
    builtIndex = index;
    builtStretch = stretch;
    builtDecay = decay;
    builtRate = rate;
}

void IRLoaderThread::run()
{
    while (! threadShouldExit())
    {
        wait (100);
        if (threadShouldExit())
            break;
        buildNow();
        engine.collectGarbage();
    }
}

//==============================================================================
BlackAngelProcessor::BlackAngelProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "BlackAngelState", ba::params::createLayout())
{
    for (auto* p : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            params[withId->paramID] = apvts.getRawParameterValue (withId->paramID);
    bypassParam = apvts.getParameter (pid::bypass);
    irLoader.startThread (juce::Thread::Priority::low);
}

BlackAngelProcessor::~BlackAngelProcessor()
{
    irLoader.stopThread (4000);
}

bool BlackAngelProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo())
        return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono())
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::stereo() && sc != juce::AudioChannelSet::mono())
            return false;
    }
    return true;
}

void BlackAngelProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    const int maxBlock = std::max (samplesPerBlock, 32);
    reverb.setParameters (readParameters());
    reverb.prepare (sampleRate, maxBlock);
    dryCopy.setSize (2, maxBlock, false, true, true);
    bypassMix.reset (sampleRate, 0.02);
    bypassMix.setCurrentAndTargetValue (value (pid::bypass) > 0.5f ? 0.0f : 1.0f);

    irLoader.request ((int) value (pid::irIndex), value (pid::irStretch), value (pid::irDecay), sampleRate);
    irLoader.forceRebuild();
    irLoader.buildNow();
    setLatencySamples (reverb.getLatencySamples());
}

void BlackAngelProcessor::releaseResources() {}

ba::ReverbParams BlackAngelProcessor::readParameters()
{
    auto db = [] (float v) { return v <= -59.9f ? -100.0f : v; };
    ba::ReverbParams p;

    p.predelayMs = value (pid::predelay);
    const int sync = (int) value (pid::predelaySync);
    if (sync > 0)
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    if (*b > 1.0) bpm = *b;
        p.predelayMs = (float) std::min (500.0, 60000.0 / bpm * ba::params::syncQuarterNotes (sync));
    }
    p.decaySeconds = value (pid::decay);
    p.damping = value (pid::damping);
    p.size = value (pid::size);
    p.width = value (pid::width);
    p.lfoHz = value (pid::lfoSpeed);
    p.modDepth = value (pid::modDepth);
    p.modMode = (ba::ModMode) (int) value (pid::modMode);
    p.diffusion = value (pid::diffusion);
    p.shape = value (pid::shape);
    p.earlyDb = db (value (pid::earlyLevel));
    p.lateDb = db (value (pid::lateLevel));
    p.mix = value (pid::mix);
    p.wetDb = value (pid::wetLevel);
    p.routing = (ba::RoutingMode) (int) value (pid::routingMode);
    p.engine = (ba::EngineMode) (int) value (pid::engineMode);
    p.blend = value (pid::blend);
    p.algoDb = db (value (pid::algoLevel));
    p.irDb = db (value (pid::irLevel));

    auto& e = p.eq;
    e.lowCutOn = value (pid::lowCutOn) > 0.5f;
    e.lowCutHz = value (pid::lowCut);
    e.lowCut24 = value (pid::lowCutSlope) > 0.5f;
    e.highCutOn = value (pid::highCutOn) > 0.5f;
    e.highCutHz = value (pid::highCut);
    e.highCut24 = value (pid::highCutSlope) > 0.5f;
    e.lowShelfOn = value (pid::lowShelfOn) > 0.5f;
    e.lowShelfHz = value (pid::lowShelfF);
    e.lowShelfDb = value (pid::lowShelfG);
    e.peak1On = value (pid::peak1On) > 0.5f;
    e.peak1Hz = value (pid::peak1F);
    e.peak1Db = value (pid::peak1G);
    e.peak1Q = value (pid::peak1Q);
    e.peak2On = value (pid::peak2On) > 0.5f;
    e.peak2Hz = value (pid::peak2F);
    e.peak2Db = value (pid::peak2G);
    e.peak2Q = value (pid::peak2Q);
    e.highShelfOn = value (pid::highShelfOn) > 0.5f;
    e.highShelfHz = value (pid::highShelfF);
    e.highShelfDb = value (pid::highShelfG);

    p.freeze = value (pid::freeze) > 0.5f;
    p.infinite = value (pid::infinite) > 0.5f;
    return p;
}

void BlackAngelProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0)
        return;

    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int inChannels = mainIn.getNumChannels();

    irLoader.request ((int) value (pid::irIndex), value (pid::irStretch), value (pid::irDecay), currentRate);
    reverb.setParameters (readParameters());
    bypassMix.setTargetValue (value (pid::bypass) > 0.5f ? 0.0f : 1.0f);

    float* outL = mainOut.getWritePointer (0);
    float* outR = mainOut.getWritePointer (1);
    const float* inL = inChannels > 0 ? mainIn.getReadPointer (0) : outL;
    if (inChannels == 1)
        mainOut.copyFrom (1, 0, inL, n); // mono in: duplicate before processing in place
    const float* inR = inChannels > 1 ? mainIn.getReadPointer (1) : outR;

    // Hosts can send larger blocks than announced: process in safe chunks.
    const int maxChunk = dryCopy.getNumSamples();
    for (int start = 0; start < n; start += maxChunk)
    {
        const int len = std::min (maxChunk, n - start);
        const bool bypassActive = bypassMix.isSmoothing() || bypassMix.getTargetValue() < 1.0f;
        if (bypassActive)
        {
            dryCopy.copyFrom (0, 0, inL + start, len);
            dryCopy.copyFrom (1, 0, inR + start, len);
        }
        reverb.process (inL + start, inR + start, outL + start, outR + start, len);
        if (bypassActive)
        {
            for (int i = 0; i < len; ++i)
            {
                const float g = bypassMix.getNextValue();
                outL[start + i] = dryCopy.getSample (0, i) + g * (outL[start + i] - dryCopy.getSample (0, i));
                outR[start + i] = dryCopy.getSample (1, i) + g * (outR[start + i] - dryCopy.getSample (1, i));
            }
        }
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        if (ch >= mainOut.getNumChannels())
            buffer.clear (ch, 0, n);
}

//==============================================================================
void BlackAngelProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", 1, nullptr);
    state.setProperty ("pluginVersion", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BlackAngelProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* BlackAngelProcessor::createEditor()
{
    return new BlackAngelEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BlackAngelProcessor();
}
