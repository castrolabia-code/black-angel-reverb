// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#include "HybridReverb.h"

#include <cmath>

namespace ba
{
void HybridReverb::prepare (double sampleRate, int maxBlockSize)
{
    fs = sampleRate;
    maxBlock = std::max (1, maxBlockSize);

    preDelay.prepare (fs);
    early.prepare (fs);
    diffusion.prepare (fs);
    fdn.prepare (fs);
    conv.prepare (fs, kSubBlock);
    eq.prepare (fs);
    stereo.prepare (fs);

    for (auto* s : { &earlyGain, &lateGain, &algoGain, &convGain, &wetGain })
        s->reset (fs, 0.05);
    dryMix.reset (fs, 0.02);
    wetMix.reset (fs, 0.02);

    setParameters (params);
    for (auto* s : { &earlyGain, &lateGain, &algoGain, &convGain, &wetGain, &dryMix, &wetMix })
        s->setImmediate (s->getTarget());
    preDelay.snap();
    stereo.snap();
    reset();
}

void HybridReverb::reset()
{
    preDelay.reset();
    early.reset();
    diffusion.reset();
    fdn.reset();
    conv.reset();
    eq.reset();
    algoIdle = convIdle = false;
}

void HybridReverb::setParameters (const ReverbParams& p) noexcept
{
    params = p;
    preDelay.setDelayMs (p.predelayMs);
    early.setParameters (p.size, p.shape, p.damping);
    diffusion.setParameters (p.size, p.diffusion);

    FDNReverb::Settings s;
    s.decaySeconds = std::min (std::max (p.decaySeconds, 0.1f), 30.0f);
    s.size = p.size;
    s.shape = p.shape;
    s.damping = p.damping;
    s.lowDecayMult = p.lowDecayMult;
    s.highDecayMult = p.highDecayMult;
    s.lowXoverHz = p.xoverLowHz;
    s.highXoverHz = p.xoverHighHz;
    s.lfoHz = p.lfoHz;
    s.modDepth = p.modDepth;
    s.modMode = p.modMode;
    s.freeze = p.freeze;
    s.infinite = p.infinite;
    fdn.setSettings (s);

    eq.setSettings (p.eq);
    stereo.setWidth (p.width);

    earlyGain.setTarget (dbToGain (p.earlyDb));
    lateGain.setTarget (dbToGain (p.lateDb));
    wetGain.setTarget (dbToGain (p.wetDb));

    float a = 1.0f, c = 0.0f;
    if (p.engine == EngineMode::Convolution) { a = 0.0f; c = 1.0f; }
    else if (p.engine == EngineMode::Hybrid)
    {
        const float b = std::min (std::max (p.blend, 0.0f), 1.0f);
        a = std::cos (b * 0.5f * kPi);
        c = std::sin (b * 0.5f * kPi);
    }
    algoGain.setTarget (a * dbToGain (p.algoDb));
    convGain.setTarget (c * dbToGain (p.irDb));

    if (p.routing == RoutingMode::Aux)
    {
        dryMix.setTarget (0.0f);
        wetMix.setTarget (1.0f);
    }
    else
    {
        const float m = std::min (std::max (p.mix, 0.0f), 1.0f);
        dryMix.setTarget (m <= 0.0f ? 1.0f : std::cos (m * 0.5f * kPi));
        wetMix.setTarget (m <= 0.0f ? 0.0f : std::sin (m * 0.5f * kPi));
    }
}

void HybridReverb::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
{
    DenormalGuard noDenormals;
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (kSubBlock, numSamples - done);
        processChunk (inL, inR, outL, outR, done, n);
        done += n;
    }
}

void HybridReverb::processChunk (const float* inL, const float* inR, float* outL, float* outR, int offset, int n) noexcept
{
    // Engines that are fully faded out are skipped (no CPU) and restart clean.
    const bool algoOff = algoGain.getTarget() == 0.0f && ! algoGain.isSmoothing();
    const bool convOff = convGain.getTarget() == 0.0f && ! convGain.isSmoothing();
    if (algoIdle && ! algoOff) { early.reset(); diffusion.reset(); fdn.reset(); }
    if (convIdle && ! convOff) conv.reset();
    algoIdle = algoOff;
    convIdle = convOff;

    if (! algoOff)
    {
        early.beginBlock (n);
        fdn.beginBlock();
    }
    eq.beginBlock (n);

    float wl[kSubBlock], wr[kSubBlock];

    for (int i = 0; i < n; ++i)
    {
        float l = inL[offset + i], r = inR[offset + i];
        preDelay.process (l, r);
        convInL[i] = l;
        convInR[i] = r;

        float al = 0.0f, ar = 0.0f;
        if (! algoOff)
        {
            float el, er;
            early.process (l, r, el, er);
            float dl = l, dr = r;
            diffusion.process (dl, dr);
            float ll, lr;
            fdn.process (dl, dr, ll, lr);
            const float eg = earlyGain.next(), lg = lateGain.next();
            const float ag = algoGain.next();
            al = (el * eg + ll * lg) * ag;
            ar = (er * eg + lr * lg) * ag;
        }
        else
        {
            earlyGain.next(); lateGain.next(); algoGain.next();
        }
        wl[i] = al;
        wr[i] = ar;
    }

    if (! convOff)
    {
        conv.process (convInL, convInR, convOutL, convOutR, n);
        for (int i = 0; i < n; ++i)
        {
            const float g = convGain.next();
            wl[i] += convOutL[i] * g;
            wr[i] += convOutR[i] * g;
        }
    }
    else
    {
        convGain.skip (n);
    }

    for (int i = 0; i < n; ++i)
    {
        float l = wl[i], r = wr[i];
        eq.process (l, r);
        stereo.process (l, r);
        const float g = wetGain.next();
        l = safetyLimit (l * g);
        r = safetyLimit (r * g);
        wl[i] = l;
        wr[i] = r;

        const float dm = dryMix.next(), wm = wetMix.next();
        const float dl = inL[offset + i], dr = inR[offset + i];
        outL[offset + i] = dl * dm + l * wm;
        outR[offset + i] = dr * dm + r * wm;
    }
}
} // namespace ba
