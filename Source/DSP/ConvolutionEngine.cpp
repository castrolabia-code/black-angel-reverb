// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#include "ConvolutionEngine.h"
#include "Util/DspUtil.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ba
{
//==============================================================================
void normalizeIR (IRBuffer& ir)
{
    const int nch = ir.numChannels();
    if (nch == 0) return;
    // Energy reaching each output.
    double e[2] = { 0.0, 0.0 };
    for (int c = 0; c < nch; ++c)
    {
        const int out = nch == 4 ? (c & 1) : std::min (c, 1);
        for (float v : ir.channels[(size_t) c])
            e[out] += (double) v * v;
    }
    if (nch == 1) e[1] = e[0];
    const double m = std::max (e[0], e[1]);
    if (m <= 1.0e-20) return;
    const float g = (float) (1.0 / std::sqrt (m));
    for (auto& ch : ir.channels)
        for (auto& v : ch)
            v *= g;
}

//==============================================================================
void UPOLSStage::build (int blockLength, bool spreadWork, const std::vector<Path>& paths)
{
    L = blockLength;
    Nfft = 2 * L;
    bins = L + 1;
    spread = spreadWork;
    fft.prepare (Nfft);

    size_t maxLen = 0;
    for (const auto& p : paths)
        maxLen = std::max (maxLen, p.ir.size());
    numParts = (int) ((maxLen + (size_t) L - 1) / (size_t) L);

    pathData.clear();
    inputUsed[0] = inputUsed[1] = false;
    std::vector<Cpx> tmp ((size_t) Nfft);
    for (const auto& p : paths)
    {
        PathData pd;
        pd.input = p.input;
        pd.output = p.output;
        inputUsed[p.input] = true;
        pd.kernel.assign ((size_t) numParts * (size_t) bins, Cpx {});
        pd.nonZero.assign ((size_t) numParts, 0);
        for (int q = 0; q < numParts; ++q)
        {
            std::fill (tmp.begin(), tmp.end(), Cpx {});
            bool any = false;
            for (int i = 0; i < L; ++i)
            {
                const size_t idx = (size_t) q * (size_t) L + (size_t) i;
                const float v = idx < p.ir.size() ? p.ir[idx] : 0.0f;
                tmp[(size_t) i].re = v;
                any = any || v != 0.0f;
            }
            pd.nonZero[(size_t) q] = any ? 1 : 0;
            if (! any) continue;
            fft.transform (tmp.data(), false);
            std::copy (tmp.begin(), tmp.begin() + bins, pd.kernel.begin() + (ptrdiff_t) ((size_t) q * (size_t) bins));
        }
        pathData.push_back (std::move (pd));
    }

    for (int c = 0; c < 2; ++c)
    {
        fdl[c].assign (inputUsed[c] ? (size_t) numParts * (size_t) bins : 0, Cpx {});
        inAccum[c].assign ((size_t) L, 0.0f);
        timeBuf[c].assign ((size_t) Nfft, 0.0f);
        outReady[c].assign ((size_t) L, 0.0f);
        outNext[c].assign ((size_t) L, 0.0f);
        acc[c].assign ((size_t) bins, Cpx {});
    }
    work.assign ((size_t) Nfft, Cpx {});
    unitsTotal = 2 + (int64_t) numParts * (int64_t) pathData.size() + 2;
    reset();
}

void UPOLSStage::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        std::fill (fdl[c].begin(), fdl[c].end(), Cpx {});
        std::fill (inAccum[c].begin(), inAccum[c].end(), 0.0f);
        std::fill (timeBuf[c].begin(), timeBuf[c].end(), 0.0f);
        std::fill (outReady[c].begin(), outReady[c].end(), 0.0f);
        std::fill (outNext[c].begin(), outNext[c].end(), 0.0f);
        std::fill (acc[c].begin(), acc[c].end(), Cpx {});
    }
    fdlHead = 0;
    pos = 0;
    unitsDone = 0;
    jobActive = false;
}

void UPOLSStage::startJob() noexcept
{
    for (int c = 0; c < 2; ++c)
    {
        std::memmove (timeBuf[c].data(), timeBuf[c].data() + L, sizeof (float) * (size_t) L);
        std::memcpy (timeBuf[c].data() + L, inAccum[c].data(), sizeof (float) * (size_t) L);
        std::fill (acc[c].begin(), acc[c].end(), Cpx {});
    }
    fdlHead = (fdlHead + 1) % numParts;
    unitsDone = 0;
    jobActive = true;
}

void UPOLSStage::doUnits (int64_t target) noexcept
{
    target = std::min (target, unitsTotal);
    const int64_t macUnits = (int64_t) numParts * (int64_t) pathData.size();
    while (unitsDone < target)
    {
        const int64_t u = unitsDone++;
        if (u < 2)
        {
            const int c = (int) u;
            if (! inputUsed[c]) continue;
            for (int i = 0; i < Nfft; ++i)
                work[(size_t) i] = { timeBuf[c][(size_t) i], 0.0f };
            fft.transform (work.data(), false);
            std::copy (work.begin(), work.begin() + bins, fdl[c].begin() + (ptrdiff_t) ((size_t) fdlHead * (size_t) bins));
        }
        else if (u < 2 + macUnits)
        {
            const int64_t m = u - 2;
            const auto& pd = pathData[(size_t) (m / numParts)];
            const int q = (int) (m % numParts);
            if (! pd.nonZero[(size_t) q]) continue;
            const int slot = (fdlHead - q + numParts) % numParts;
            const Cpx* X = fdl[pd.input].data() + (size_t) slot * (size_t) bins;
            const Cpx* H = pd.kernel.data() + (size_t) q * (size_t) bins;
            Cpx* Y = acc[pd.output].data();
            for (int k = 0; k < bins; ++k)
            {
                Y[k].re += X[k].re * H[k].re - X[k].im * H[k].im;
                Y[k].im += X[k].re * H[k].im + X[k].im * H[k].re;
            }
        }
        else
        {
            const int o = (int) (u - 2 - macUnits);
            const Cpx* Y = acc[o].data();
            for (int k = 0; k < bins; ++k)
                work[(size_t) k] = Y[k];
            for (int k = 1; k < L; ++k)
                work[(size_t) (Nfft - k)] = { Y[k].re, -Y[k].im };
            fft.transform (work.data(), true);
            const float scale = 1.0f / (float) Nfft;
            for (int i = 0; i < L; ++i)
                outNext[o][(size_t) i] = work[(size_t) (L + i)].re * scale;
        }
    }
}

void UPOLSStage::process (const float* const* in, float* const* out, int numSamples) noexcept
{
    if (numParts == 0) return;
    int done = 0;
    while (done < numSamples)
    {
        const int chunk = std::min (numSamples - done, L - pos);
        for (int c = 0; c < 2; ++c)
            std::memcpy (inAccum[c].data() + pos, in[c] + done, sizeof (float) * (size_t) chunk);
        for (int o = 0; o < 2; ++o)
        {
            const float* src = outReady[o].data() + pos;
            float* dst = out[o] + done;
            for (int i = 0; i < chunk; ++i)
                dst[i] += src[i];
        }
        pos += chunk;
        done += chunk;

        if (spread && jobActive && pos < L)
            doUnits (unitsTotal * (int64_t) pos / (int64_t) L);

        if (pos == L)
        {
            pos = 0;
            if (jobActive)
            {
                doUnits (unitsTotal);
                std::swap (outReady[0], outNext[0]);
                std::swap (outReady[1], outNext[1]);
                jobActive = false;
            }
            startJob();
            if (! spread)
            {
                doUnits (unitsTotal);
                std::swap (outReady[0], outNext[0]);
                std::swap (outReady[1], outNext[1]);
                jobActive = false;
            }
        }
    }
}

//==============================================================================
void ConvolutionInstance::build (const IRBuffer& ir, double sampleRate)
{
    fs = sampleRate;
    irLength = ir.length();
    const int nch = ir.numChannels();

    struct Route { int in, out, ch; };
    std::vector<Route> routes;
    if (nch == 1)      routes = { { 0, 0, 0 }, { 1, 1, 0 } };
    else if (nch == 2) routes = { { 0, 0, 0 }, { 1, 1, 1 } };
    else if (nch >= 4) routes = { { 0, 0, 0 }, { 0, 1, 1 }, { 1, 0, 2 }, { 1, 1, 3 } };

    direct.clear();
    std::vector<UPOLSStage::Path> headPaths, tailPaths;
    for (const auto& r : routes)
    {
        const auto& h = ir.channels[(size_t) r.ch];
        const int len = (int) h.size();

        DirectPath dp {};
        dp.input = r.in;
        dp.output = r.out;
        for (int k = 0; k < kDirect; ++k)
            dp.taps[k] = k < len ? h[(size_t) k] : 0.0f;
        direct.push_back (dp);

        if (len > kDirect)
        {
            // Head stage (latency 128): g[n] = h[n + 128], covers [128, 16384).
            UPOLSStage::Path p { r.in, r.out, {} };
            const int end = std::min (len, kHeadEnd);
            p.ir.assign (h.begin() + kDirect, h.begin() + end);
            headPaths.push_back (std::move (p));
        }
        if (len > kHeadEnd)
        {
            // Tail stage (latency 2 * 4096): g[n] = h[n + 8192] for n >= 8192.
            UPOLSStage::Path p { r.in, r.out, {} };
            const int offset = 2 * kTailBlock;
            p.ir.assign ((size_t) (len - offset), 0.0f);
            for (int n = kHeadEnd - offset; n < len - offset; ++n)
                p.ir[(size_t) n] = h[(size_t) (n + offset)];
            tailPaths.push_back (std::move (p));
        }
    }

    head = UPOLSStage();
    tail = UPOLSStage();
    if (! headPaths.empty()) head.build (kHeadBlock, false, headPaths);
    if (! tailPaths.empty()) tail.build (kTailBlock, true, tailPaths);
    reset();
}

void ConvolutionInstance::reset() noexcept
{
    std::memset (hist, 0, sizeof (hist));
    histPos = 0;
    head.reset();
    tail.reset();
}

void ConvolutionInstance::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
{
    const float* in[2] = { inL, inR };
    float* out[2] = { outL, outR };
    for (int i = 0; i < numSamples; ++i)
    {
        hist[0][histPos] = hist[0][histPos + kDirect] = inL[i];
        hist[1][histPos] = hist[1][histPos + kDirect] = inR[i];
        float y[2] = { 0.0f, 0.0f };
        for (const auto& dp : direct)
        {
            const float* x = &hist[dp.input][histPos + kDirect]; // x[0] = newest
            float s = 0.0f;
            for (int k = 0; k < kDirect; ++k)
                s += dp.taps[k] * *(x - k);
            y[dp.output] += s;
        }
        outL[i] = y[0];
        outR[i] = y[1];
        histPos = (histPos + 1) % kDirect;
    }
    head.process (in, out, numSamples);
    tail.process (in, out, numSamples);
}

//==============================================================================
ConvolutionEngine::~ConvolutionEngine()
{
    delete pending.exchange (nullptr);
    delete current;
    delete fading;
    delete overflow;
    collectGarbage();
}

void ConvolutionEngine::prepare (double sampleRate, int maxBlockSize)
{
    delete pending.exchange (nullptr);
    delete current;  current = nullptr;
    delete fading;   fading = nullptr;
    delete overflow; overflow = nullptr;
    collectGarbage();
    fs = sampleRate;
    maxBlock = std::max (1, maxBlockSize);
    scratchL.assign ((size_t) maxBlock, 0.0f);
    scratchR.assign ((size_t) maxBlock, 0.0f);
    fadeLength = std::max (1, (int) (0.05 * fs));
    fadePos = 0;
}

void ConvolutionEngine::reset() noexcept
{
    if (current) current->reset();
    if (fading) { retire (fading); fading = nullptr; }
}

void ConvolutionEngine::submit (std::unique_ptr<ConvolutionInstance> instance)
{
    delete pending.exchange (instance.release());
}

void ConvolutionEngine::collectGarbage()
{
    for (auto& slot : trash)
        delete slot.exchange (nullptr);
}

void ConvolutionEngine::retire (ConvolutionInstance* p) noexcept
{
    if (p == nullptr) return;
    for (auto& slot : trash)
    {
        ConvolutionInstance* expected = nullptr;
        if (slot.compare_exchange_strong (expected, p))
            return;
    }
    // All slots busy (loader thread has not collected yet): keep it one more round.
    if (overflow == nullptr) overflow = p;
}

void ConvolutionEngine::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
{
    if (overflow != nullptr)
    {
        auto* p = overflow;
        overflow = nullptr;
        retire (p);
    }

    if (auto* p = pending.exchange (nullptr))
    {
        if (p->getSampleRate() != fs)
        {
            retire (p);
        }
        else
        {
            if (fading) retire (fading);
            fading = current;
            current = p;
            fadePos = 0;
        }
    }

    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (numSamples - done, maxBlock);
        float* oL = outL + done;
        float* oR = outR + done;

        if (current) current->process (inL + done, inR + done, oL, oR, n);
        else { std::fill (oL, oL + n, 0.0f); std::fill (oR, oR + n, 0.0f); }

        if (fading)
        {
            fading->process (inL + done, inR + done, scratchL.data(), scratchR.data(), n);
            for (int i = 0; i < n; ++i)
            {
                const float t = std::min (1.0f, (float) (fadePos + i) / (float) fadeLength);
                const float gIn = std::sin (t * 0.5f * kPi), gOut = std::cos (t * 0.5f * kPi);
                oL[i] = oL[i] * gIn + scratchL[(size_t) i] * gOut;
                oR[i] = oR[i] * gIn + scratchR[(size_t) i] * gOut;
            }
            fadePos += n;
            if (fadePos >= fadeLength)
            {
                retire (fading);
                fading = nullptr;
            }
        }
        else if (current && fadePos < fadeLength)
        {
            // First IR after silence: fade in.
            for (int i = 0; i < n; ++i)
            {
                const float t = std::min (1.0f, (float) (fadePos + i) / (float) fadeLength);
                const float g = std::sin (t * 0.5f * kPi);
                oL[i] *= g; oR[i] *= g;
            }
            fadePos += n;
        }
        done += n;
    }
}
} // namespace ba
