// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include "Util/FFT.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace ba
{
/** Impulse response in memory. 1 channel = mono, 2 = stereo (L->L, R->R),
    4 = true stereo (LL, LR, RL, RR = input->output). */
struct IRBuffer
{
    double sampleRate = 48000.0;
    std::vector<std::vector<float>> channels;

    int numChannels() const noexcept { return (int) channels.size(); }
    int length() const noexcept { return channels.empty() ? 0 : (int) channels[0].size(); }
};

/** Scales an IR so the loudest output has unit energy (consistent loudness). */
void normalizeIR (IRBuffer& ir);

/** One uniformly partitioned overlap-save stage (UPOLS, frequency-domain
    delay line). With spread = true the work for each block is distributed
    across the following block, so the CPU cost per audio callback stays flat;
    the stage latency is then 2 * blockLength instead of blockLength. */
class UPOLSStage
{
public:
    struct Path { int input, output; std::vector<float> ir; };

    void build (int blockLength, bool spreadWork, const std::vector<Path>& paths);
    void reset();
    /** Adds the stage output to out[0], out[1]. */
    void process (const float* const* in, float* const* out, int numSamples) noexcept;
    bool isActive() const noexcept { return numParts > 0; }
    int latency() const noexcept { return spread ? 2 * L : L; }

private:
    void doUnits (int64_t target) noexcept;
    void startJob() noexcept;

    int L = 0, Nfft = 0, bins = 0, numParts = 0;
    bool spread = false;
    FFT fft;
    struct PathData { int input, output; std::vector<Cpx> kernel; std::vector<uint8_t> nonZero; };
    std::vector<PathData> pathData;
    std::vector<Cpx> fdl[2];      // numParts * bins per input
    std::vector<float> inAccum[2], timeBuf[2], outReady[2], outNext[2];
    std::vector<Cpx> acc[2], work;
    int fdlHead = 0, pos = 0;
    int64_t unitsDone = 0, unitsTotal = 0;
    bool jobActive = false, inputUsed[2] { false, false };
};

/** A complete zero-latency convolver for one IR: a 128-tap direct FIR head,
    a 128-sample UPOLS body up to 16384 samples and a 4096-sample UPOLS tail
    whose work is spread over time. Built entirely off the audio thread. */
class ConvolutionInstance
{
public:
    static constexpr int kDirect = 128;
    static constexpr int kHeadBlock = 128;
    static constexpr int kHeadEnd = 16384;
    static constexpr int kTailBlock = 4096;

    /** ir must already be at sampleRate. Allocates. */
    void build (const IRBuffer& ir, double sampleRate);
    void reset() noexcept;
    /** Overwrites outL / outR. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

    double getSampleRate() const noexcept { return fs; }
    int getIRLength() const noexcept { return irLength; }

private:
    double fs = 48000.0;
    int irLength = 0;
    struct DirectPath { int input, output; float taps[kDirect]; };
    std::vector<DirectPath> direct;
    float hist[2][2 * kDirect] {};
    int histPos = 0;
    UPOLSStage head, tail;
};

/** Real-time side of the convolution: swaps in new instances with a 50 ms
    equal-power cross-fade and hands old ones back for deletion off the audio
    thread. submit() and collectGarbage() are for the loader thread only. */
class ConvolutionEngine
{
public:
    ConvolutionEngine() = default;
    ~ConvolutionEngine();
    ConvolutionEngine (const ConvolutionEngine&) = delete;
    ConvolutionEngine& operator= (const ConvolutionEngine&) = delete;

    /** Not real-time safe. Drops the current IR (it was built for the old rate). */
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void submit (std::unique_ptr<ConvolutionInstance> instance);
    void collectGarbage();

    /** Overwrites outL / outR. Real-time safe. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

    bool hasIR() const noexcept { return current != nullptr; }
    double getSampleRate() const noexcept { return fs; }

private:
    void retire (ConvolutionInstance* p) noexcept;

    static constexpr int kTrashSlots = 16;
    std::atomic<ConvolutionInstance*> pending { nullptr };
    std::atomic<ConvolutionInstance*> trash[kTrashSlots] {};
    ConvolutionInstance* current = nullptr;
    ConvolutionInstance* fading = nullptr;
    ConvolutionInstance* overflow = nullptr;
    std::vector<float> scratchL, scratchR;
    double fs = 48000.0;
    int maxBlock = 0, fadePos = 0, fadeLength = 2400;
};
} // namespace ba
