// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include "PreDelay.h"
#include "EarlyReflections.h"
#include "Diffusion.h"
#include "FDNReverb.h"
#include "ConvolutionEngine.h"
#include "WetEQ.h"
#include "StereoProcessor.h"


namespace ba
{
enum class EngineMode { Algorithmic = 0, Convolution = 1, Hybrid = 2 };
enum class RoutingMode { Insert = 0, Aux = 1 };

/** Every value the DSP needs, in plain units. Filled by the plugin from its
    parameters once per block; tests fill it directly. */
struct ReverbParams
{
    float predelayMs = 20.0f;
    float decaySeconds = 2.2f;
    float damping = 0.35f;      // 0..1
    float size = 0.5f;          // 0..1
    float width = 1.0f;         // 0..2
    float lfoHz = 0.6f;
    float modDepth = 0.25f;     // 0..1
    ModMode modMode = ModMode::Smooth;
    float diffusion = 0.7f;     // 0..1
    float shape = 0.45f;        // 0..1
    float earlyDb = -6.0f;
    float lateDb = 0.0f;
    float mix = 0.3f;           // 0..1 (Insert)
    float wetDb = 0.0f;
    RoutingMode routing = RoutingMode::Insert;
    EngineMode engine = EngineMode::Algorithmic;
    float blend = 0.5f;         // 0 = all algorithmic, 1 = all IR (Hybrid)
    float algoDb = 0.0f;
    float irDb = 0.0f;
    EQSettings eq;
    bool freeze = false;
    bool infinite = false;
    float lowDecayMult = 1.0f;
    float highDecayMult = 1.0f;
    float xoverLowHz = 250.0f;
    float xoverHighHz = 4000.0f;
};

/** The whole wet path plus the dry/wet stage. The dry signal is never
    processed: at Mix 0 % (Insert) the output equals the input bit for bit. */
class HybridReverb
{
public:
    static constexpr int kSubBlock = 32;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setParameters (const ReverbParams& p) noexcept;
    /** in and out may point to the same buffers. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

    ConvolutionEngine& getConvolution() noexcept { return conv; }
    int getLatencySamples() const noexcept { return 0; }
    double getSampleRate() const noexcept { return fs; }

private:
    void processChunk (const float* inL, const float* inR, float* outL, float* outR, int offset, int n) noexcept;

    double fs = 48000.0;
    int maxBlock = 0;
    ReverbParams params;

    PreDelay preDelay;
    EarlyReflections early;
    Diffusion diffusion;
    FDNReverb fdn;
    ConvolutionEngine conv;
    WetEQ eq;
    StereoProcessor stereo;

    LinearSmoother earlyGain, lateGain, algoGain, convGain, wetGain, dryMix, wetMix;
    bool algoIdle = false, convIdle = false;
    float convInL[kSubBlock] {}, convInR[kSubBlock] {}, convOutL[kSubBlock] {}, convOutR[kSubBlock] {};
};
} // namespace ba
