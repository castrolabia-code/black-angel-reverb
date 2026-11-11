// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include "Util/Filters.h"

namespace ba
{
struct EQSettings
{
    bool lowCutOn = true;    float lowCutHz = 120.0f;   bool lowCut24 = false;
    bool lowShelfOn = false; float lowShelfHz = 250.0f; float lowShelfDb = 0.0f;
    bool peak1On = false;    float peak1Hz = 500.0f;    float peak1Db = 0.0f; float peak1Q = 1.0f;
    bool peak2On = false;    float peak2Hz = 3000.0f;   float peak2Db = 0.0f; float peak2Q = 1.0f;
    bool highShelfOn = false;float highShelfHz = 6000.0f; float highShelfDb = 0.0f;
    bool highCutOn = true;   float highCutHz = 12000.0f; bool highCut24 = false;
};

/** Six-band EQ for the wet signal: low cut (12/24 dB/oct), low shelf,
    two bells, high shelf, high cut (12/24 dB/oct). Frequencies, gains and Q
    glide (~30 ms); bands switch with a short fade. magnitudeDb() evaluates
    the exact digital response for the GUI curve. */
class WetEQ
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto* s : { &lowCutMix, &highCutMix, &lowSlope, &highSlope })
            s->reset (fs, 0.02);
        snapToTargets = true;
        reset();
    }

    void reset()
    {
        for (auto& ch : f)
            for (auto& flt : ch)
                flt.reset();
    }

    void setSettings (const EQSettings& s) noexcept
    {
        target = s;
        lowCutMix.setTarget (s.lowCutOn ? 1.0f : 0.0f);
        highCutMix.setTarget (s.highCutOn ? 1.0f : 0.0f);
        lowSlope.setTarget (s.lowCut24 ? 1.0f : 0.0f);
        highSlope.setTarget (s.highCut24 ? 1.0f : 0.0f);
    }

    void beginBlock (int numSamples) noexcept
    {
        const float a = snapToTargets ? 1.0f : 1.0f - std::exp (-(float) numSamples / (0.03f * (float) fs));
        if (snapToTargets)
        {
            lowCutMix.setImmediate (lowCutMix.getTarget());
            highCutMix.setImmediate (highCutMix.getTarget());
            lowSlope.setImmediate (lowSlope.getTarget());
            highSlope.setImmediate (highSlope.getTarget());
        }
        snapToTargets = false;

        auto glideLog = [a] (float& v, float tgt) { v = std::exp (std::log (v) + a * (std::log (tgt) - std::log (v))); };
        auto glide = [a] (float& v, float tgt) { v += a * (tgt - v); };

        glideLog (cur.lowCutHz, target.lowCutHz);
        glideLog (cur.highCutHz, target.highCutHz);
        glideLog (cur.lowShelfHz, target.lowShelfHz);
        glideLog (cur.peak1Hz, target.peak1Hz);
        glideLog (cur.peak2Hz, target.peak2Hz);
        glideLog (cur.highShelfHz, target.highShelfHz);
        glideLog (cur.peak1Q, target.peak1Q);
        glideLog (cur.peak2Q, target.peak2Q);
        glide (cur.lowShelfDb, target.lowShelfOn ? target.lowShelfDb : 0.0f);
        glide (cur.peak1Db, target.peak1On ? target.peak1Db : 0.0f);
        glide (cur.peak2Db, target.peak2On ? target.peak2Db : 0.0f);
        glide (cur.highShelfDb, target.highShelfOn ? target.highShelfDb : 0.0f);

        for (auto& ch : f)
        {
            ch[0].setup (SVF::Type::HighPass, fs, cur.lowCutHz, 0.7071f, 0.0f);
            ch[1].setup (SVF::Type::HighPass, fs, cur.lowCutHz, 0.7071f, 0.0f);
            ch[2].setup (SVF::Type::LowShelf, fs, cur.lowShelfHz, 0.7071f, cur.lowShelfDb);
            ch[3].setup (SVF::Type::Bell, fs, cur.peak1Hz, cur.peak1Q, cur.peak1Db);
            ch[4].setup (SVF::Type::Bell, fs, cur.peak2Hz, cur.peak2Q, cur.peak2Db);
            ch[5].setup (SVF::Type::HighShelf, fs, cur.highShelfHz, 0.7071f, cur.highShelfDb);
            ch[6].setup (SVF::Type::LowPass, fs, cur.highCutHz, 0.7071f, 0.0f);
            ch[7].setup (SVF::Type::LowPass, fs, cur.highCutHz, 0.7071f, 0.0f);
        }
    }

    void process (float& l, float& r) noexcept
    {
        const float lcm = lowCutMix.next(), hcm = highCutMix.next();
        const float ls = lowSlope.next(), hs = highSlope.next();
        l = channel (f[0], l, lcm, hcm, ls, hs);
        r = channel (f[1], r, lcm, hcm, ls, hs);
    }

    static double magnitudeDb (const EQSettings& s, double hz, double fs)
    {
        double mag = 1.0;
        SVF t;
        if (s.lowCutOn)
        {
            t.setup (SVF::Type::HighPass, fs, s.lowCutHz, 0.7071f, 0.0f);
            const double m = t.magnitude (hz, fs);
            mag *= s.lowCut24 ? m * m : m;
        }
        if (s.lowShelfOn)  { t.setup (SVF::Type::LowShelf, fs, s.lowShelfHz, 0.7071f, s.lowShelfDb); mag *= t.magnitude (hz, fs); }
        if (s.peak1On)     { t.setup (SVF::Type::Bell, fs, s.peak1Hz, s.peak1Q, s.peak1Db); mag *= t.magnitude (hz, fs); }
        if (s.peak2On)     { t.setup (SVF::Type::Bell, fs, s.peak2Hz, s.peak2Q, s.peak2Db); mag *= t.magnitude (hz, fs); }
        if (s.highShelfOn) { t.setup (SVF::Type::HighShelf, fs, s.highShelfHz, 0.7071f, s.highShelfDb); mag *= t.magnitude (hz, fs); }
        if (s.highCutOn)
        {
            t.setup (SVF::Type::LowPass, fs, s.highCutHz, 0.7071f, 0.0f);
            const double m = t.magnitude (hz, fs);
            mag *= s.highCut24 ? m * m : m;
        }
        return 20.0 * std::log10 (std::max (mag, 1.0e-9));
    }

private:
    static float channel (SVF* fl, float x, float lcm, float hcm, float ls, float hs) noexcept
    {
        const float h1 = fl[0].process (x);
        const float h2 = fl[1].process (h1);
        const float hp = h1 + ls * (h2 - h1);
        x += lcm * (hp - x);
        x = fl[2].process (x);
        x = fl[3].process (x);
        x = fl[4].process (x);
        x = fl[5].process (x);
        const float l1 = fl[6].process (x);
        const float l2 = fl[7].process (l1);
        const float lp = l1 + hs * (l2 - l1);
        return x + hcm * (lp - x);
    }

    double fs = 48000.0;
    SVF f[2][8];
    EQSettings target, cur;
    LinearSmoother lowCutMix, highCutMix, lowSlope, highSlope;
    bool snapToTargets = true;
};
} // namespace ba
