// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_message.hpp>
#include "TestHelpers.h"

#include <cstdio>

TEST_CASE ("Measured T60 matches the DECAY control within 10 %", "[algorithmic][decay]")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        for (float decay : { 0.5f, 1.0f, 2.0f, 5.0f, 10.0f })
        {
            ba::HybridReverb rv;
            auto p = test::wetOnly();
            p.decaySeconds = decay;
            p.damping = 0.0f;
            p.modDepth = 0.0f;
            p.earlyDb = -100.0f;
            rv.setParameters (p);
            rv.prepare (fs, 512);
            const auto in = test::impulse ((size_t) (fs * (decay * 1.2 + 0.5)), 0.1f);
            const auto out = test::render (rv, in, 512);
            const double t60 = test::measureT60 (out, fs);
            INFO ("fs " << fs << " decay " << decay << " measured " << t60);
            std::printf ("[decay] fs %6.0f  set %5.2f s  measured %6.3f s  (%+.1f %%)\n",
                         fs, (double) decay, t60, 100.0 * (t60 / decay - 1.0));
            REQUIRE (t60 > decay * 0.9);
            REQUIRE (t60 < decay * 1.1);
        }
    }
}

TEST_CASE ("Damping shortens the high-frequency decay only", "[algorithmic][damping]")
{
    const double fs = 48000.0;
    auto measureBand = [fs] (float damping, bool high)
    {
        ba::HybridReverb rv;
        auto p = test::wetOnly();
        p.decaySeconds = 3.0f;
        p.damping = damping;
        p.modDepth = 0.0f;
        p.earlyDb = -100.0f;
        if (high) { p.eq.lowCutOn = true; p.eq.lowCutHz = 6000.0f; p.eq.lowCut24 = true; }
        else      { p.eq.highCutOn = true; p.eq.highCutHz = 500.0f; p.eq.highCut24 = true; }
        rv.setParameters (p);
        rv.prepare (fs, 256);
        const auto out = test::render (rv, test::impulse ((size_t) (fs * 4.0), 0.1f), 256);
        return test::measureT60 (out, fs);
    };
    const double lowNoDamp = measureBand (0.0f, false), lowDamp = measureBand (1.0f, false);
    const double highNoDamp = measureBand (0.0f, true), highDamp = measureBand (1.0f, true);
    std::printf ("[damping] low band %.2f -> %.2f s, high band %.2f -> %.2f s\n", lowNoDamp, lowDamp, highNoDamp, highDamp);
    REQUIRE (std::fabs (lowDamp - lowNoDamp) < 0.35 * lowNoDamp);
    REQUIRE (highDamp < 0.4 * highNoDamp);
}

TEST_CASE ("Freeze holds the tail and Infinite stays bounded", "[algorithmic][stability]")
{
    const double fs = 48000.0;
    SECTION ("Freeze: energy stays within 1 dB for 2 minutes")
    {
        ba::HybridReverb rv;
        auto p = test::wetOnly();
        p.decaySeconds = 3.0f;
        rv.setParameters (p);
        rv.prepare (fs, 512);
        auto in = test::noise ((size_t) (fs * 125.0), 0.3f, 4);
        for (size_t i = (size_t) (fs * 2.0); i < in.size(); ++i) in.l[i] = in.r[i] = 0.0f;

        test::Stereo out (in.size());
        for (size_t pos = 0; pos < in.size(); pos += 512)
        {
            if (pos >= (size_t) (fs * 1.9)) { p.freeze = true; rv.setParameters (p); }
            const int n = (int) std::min<size_t> (512, in.size() - pos);
            rv.process (in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
        }
        REQUIRE (test::allFinite (out));
        const double first = test::rms (out, (size_t) (fs * 5), (size_t) (fs * 15));
        const double last = test::rms (out, (size_t) (fs * 115), (size_t) (fs * 125));
        const double driftDb = 20.0 * std::log10 (last / first);
        std::printf ("[freeze] tail level drift over 110 s: %+.2f dB\n", driftDb);
        REQUIRE (first > 0.01);
        REQUIRE (std::fabs (driftDb) < 1.0);
    }
    SECTION ("Infinite with constant loud input: bounded, finite, below 0 dBFS")
    {
        ba::HybridReverb rv;
        auto p = test::wetOnly();
        p.infinite = true;
        p.decaySeconds = 30.0f;
        rv.setParameters (p);
        rv.prepare (fs, 512);
        const auto in = test::noise ((size_t) (fs * 60.0), 1.0f, 8);
        const auto out = test::render (rv, in, 512);
        REQUIRE (test::allFinite (out));
        std::printf ("[infinite] peak %.3f, rms last 10 s %.3f\n", (double) test::peak (out),
                     test::rms (out, (size_t) (fs * 50)));
        REQUIRE (test::peak (out) <= 0.967f);
    }
}
