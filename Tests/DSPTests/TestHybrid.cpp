// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_message.hpp>
#include "TestHelpers.h"

#include <chrono>
#include <cstdio>
#include <functional>
#include <string>

namespace
{
ba::ReverbParams randomParams (ba::Rng& rng)
{
    ba::ReverbParams p;
    p.predelayMs = rng.uni() * 500.0f;
    p.decaySeconds = 0.1f + rng.uni() * 29.9f;
    p.damping = rng.uni();
    p.size = rng.uni();
    p.width = rng.uni() * 2.0f;
    p.lfoHz = 0.01f + rng.uni() * 9.99f;
    p.modDepth = rng.uni();
    p.modMode = (ba::ModMode) (int) (rng.uni() * 2.99f);
    p.diffusion = rng.uni();
    p.shape = rng.uni();
    p.earlyDb = -30.0f + rng.uni() * 36.0f;
    p.lateDb = -30.0f + rng.uni() * 36.0f;
    p.mix = rng.uni();
    p.wetDb = -24.0f + rng.uni() * 36.0f;
    p.engine = (ba::EngineMode) (int) (rng.uni() * 2.99f);
    p.blend = rng.uni();
    p.eq.lowShelfOn = rng.uni() < 0.5f;  p.eq.lowShelfDb = rng.bip() * 18.0f;
    p.eq.peak1On = rng.uni() < 0.5f;     p.eq.peak1Db = rng.bip() * 18.0f; p.eq.peak1Q = 0.1f + rng.uni() * 9.9f;
    p.eq.peak2On = rng.uni() < 0.5f;     p.eq.peak2Db = rng.bip() * 18.0f; p.eq.peak2Hz = 40.0f + rng.uni() * 17000.0f;
    p.eq.highShelfOn = rng.uni() < 0.5f; p.eq.highShelfDb = rng.bip() * 18.0f;
    p.eq.lowCutHz = 20.0f + rng.uni() * 1980.0f;
    p.eq.highCutHz = 1000.0f + rng.uni() * 19000.0f;
    p.freeze = rng.uni() < 0.1f;
    p.infinite = rng.uni() < 0.1f;
    return p;
}

void loadIR (ba::HybridReverb& rv, int index)
{
    auto inst = std::make_unique<ba::ConvolutionInstance>();
    inst->build (ba::IRSynth::generateFactory (index, rv.getSampleRate()), rv.getSampleRate());
    rv.getConvolution().submit (std::move (inst));
}
} // namespace

TEST_CASE ("Dry signal is untouched at Mix 0 % (Insert)", "[hybrid][dry]")
{
    ba::Rng rng (21);
    for (int trial = 0; trial < 5; ++trial)
    {
        ba::HybridReverb rv;
        auto p = randomParams (rng);
        p.routing = ba::RoutingMode::Insert;
        p.mix = 0.0f;
        rv.setParameters (p);
        rv.prepare (48000.0, 512);
        loadIR (rv, trial);
        const auto in = test::noise (48000, 0.8f, 30u + (uint32_t) trial);
        const auto out = test::render (rv, in, 512);
        bool identical = true;
        for (size_t i = 0; i < in.size(); ++i)
            identical = identical && out.l[i] == in.l[i] && out.r[i] == in.r[i];
        REQUIRE (identical);
    }
}

TEST_CASE ("Random settings at every sample rate: no NaN/Inf, no clipping", "[hybrid][safety]")
{
    ba::Rng rng (99);
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        for (int trial = 0; trial < 4; ++trial)
        {
            ba::HybridReverb rv;
            auto p = randomParams (rng);
            p.routing = ba::RoutingMode::Aux;
            rv.setParameters (p);
            rv.prepare (fs, 1024);
            loadIR (rv, (int) (rng.uni() * 23.99f));
            // 0 dBFS noise, then impulses, then silence.
            auto in = test::noise ((size_t) (fs * 3.0), 1.0f, 40u + (uint32_t) trial);
            for (size_t i = (size_t) (fs * 2.0); i < in.size(); ++i)
                in.l[i] = in.r[i] = (i % 9000 == 0) ? 1.0f : 0.0f;
            const auto out = test::render (rv, in, 1024, true);
            INFO ("fs " << fs << " trial " << trial);
            REQUIRE (test::allFinite (out));
            REQUIRE (test::peak (out) <= 0.967f);
        }
    }
}

TEST_CASE ("Output does not depend on the host block size", "[hybrid][blocks]")
{
    auto run = [] (int block, bool variable)
    {
        ba::HybridReverb rv;
        ba::ReverbParams p;
        p.engine = ba::EngineMode::Hybrid;
        p.mix = 0.5f;
        rv.setParameters (p);
        rv.prepare (48000.0, 2048);
        loadIR (rv, 9);
        return test::render (rv, test::noise (48000 * 2, 0.5f, 3), block, variable);
    };
    const auto ref = run (32, false);
    for (int b : { 1, 7, 64, 441, 512, 2048 })
    {
        const auto out = run (b, false);
        INFO ("block " << b << " diff " << test::diffRmsDb (ref, out) << " dB");
        REQUIRE (test::diffRmsDb (ref, out) < -90.0);
    }
    REQUIRE (test::diffRmsDb (ref, run (900, true)) < -90.0);
}

TEST_CASE ("Every DSP control changes the sound", "[hybrid][controls]")
{
    const double fs = 48000.0;
    auto renderWith = [fs] (const ba::ReverbParams& p)
    {
        ba::HybridReverb rv;
        rv.setParameters (p);
        rv.prepare (fs, 512);
        loadIR (rv, 9);
        auto in = test::noise ((size_t) (fs * 1.5), 0.3f, 12);
        for (size_t i = (size_t) (fs * 0.5); i < in.size(); ++i) in.l[i] = in.r[i] = 0.0f;
        return test::render (rv, in, 512);
    };

    ba::ReverbParams base;
    base.engine = ba::EngineMode::Hybrid;
    base.mix = 0.5f;
    base.eq.lowShelfOn = base.eq.peak1On = base.eq.peak2On = base.eq.highShelfOn = true;
    const auto ref = renderWith (base);

    struct Ctl { std::string name; std::function<void (ba::ReverbParams&)> set; };
    const std::vector<Ctl> controls {
        { "predelay",   [] (auto& p) { p.predelayMs = 200.0f; } },
        { "decay",      [] (auto& p) { p.decaySeconds = 8.0f; } },
        { "damping",    [] (auto& p) { p.damping = 1.0f; } },
        { "size",       [] (auto& p) { p.size = 1.0f; } },
        { "width",      [] (auto& p) { p.width = 0.0f; } },
        { "lfoSpeed",   [] (auto& p) { p.lfoHz = 8.0f; p.modDepth = 1.0f; } },
        { "modulation", [] (auto& p) { p.modDepth = 1.0f; } },
        { "modMode",    [] (auto& p) { p.modMode = ba::ModMode::Random; } },
        { "diffusion",  [] (auto& p) { p.diffusion = 0.0f; } },
        { "shape",      [] (auto& p) { p.shape = 1.0f; } },
        { "early",      [] (auto& p) { p.earlyDb = 6.0f; } },
        { "late",       [] (auto& p) { p.lateDb = -24.0f; } },
        { "mix",        [] (auto& p) { p.mix = 1.0f; } },
        { "wetLevel",   [] (auto& p) { p.wetDb = -12.0f; } },
        { "routing",    [] (auto& p) { p.routing = ba::RoutingMode::Aux; } },
        { "engine",     [] (auto& p) { p.engine = ba::EngineMode::Convolution; } },
        { "blend",      [] (auto& p) { p.blend = 1.0f; } },
        { "algoLevel",  [] (auto& p) { p.algoDb = -24.0f; } },
        { "irLevel",    [] (auto& p) { p.irDb = -24.0f; } },
        { "lowCut",     [] (auto& p) { p.eq.lowCutHz = 1000.0f; } },
        { "lowCutSlope",[] (auto& p) { p.eq.lowCut24 = true; } },
        { "lowCutOn",   [] (auto& p) { p.eq.lowCutOn = false; } },
        { "highCut",    [] (auto& p) { p.eq.highCutHz = 2000.0f; } },
        { "highCutSlope",[] (auto& p) { p.eq.highCut24 = true; } },
        { "highCutOn",  [] (auto& p) { p.eq.highCutOn = false; } },
        { "lowShelf",   [] (auto& p) { p.eq.lowShelfDb = 12.0f; } },
        { "lowShelfHz", [] (auto& p) { p.eq.lowShelfDb = 12.0f; p.eq.lowShelfHz = 800.0f; } },
        { "peak1",      [] (auto& p) { p.eq.peak1Db = 12.0f; } },
        { "peak1Hz",    [] (auto& p) { p.eq.peak1Db = 12.0f; p.eq.peak1Hz = 2000.0f; } },
        { "peak1Q",     [] (auto& p) { p.eq.peak1Db = 12.0f; p.eq.peak1Q = 8.0f; } },
        { "peak2",      [] (auto& p) { p.eq.peak2Db = 12.0f; } },
        { "peak2Hz",    [] (auto& p) { p.eq.peak2Db = 12.0f; p.eq.peak2Hz = 8000.0f; } },
        { "peak2Q",     [] (auto& p) { p.eq.peak2Db = 12.0f; p.eq.peak2Q = 8.0f; } },
        { "highShelf",  [] (auto& p) { p.eq.highShelfDb = -12.0f; } },
        { "highShelfHz",[] (auto& p) { p.eq.highShelfDb = -12.0f; p.eq.highShelfHz = 2000.0f; } },
        { "freeze",     [] (auto& p) { p.freeze = true; } },
        { "infinite",   [] (auto& p) { p.infinite = true; } },
    };

    for (const auto& c : controls)
    {
        auto p = base;
        c.set (p);
        if (c.name == "lowShelfHz" || c.name == "peak1Hz" || c.name == "peak1Q"
            || c.name == "peak2Hz" || c.name == "peak2Q" || c.name == "highShelfHz")
        {
            // Compare against the same gain at the default frequency / Q.
            auto q = base;
            if (c.name.rfind ("lowShelf", 0) == 0) q.eq.lowShelfDb = 12.0f;
            if (c.name.rfind ("peak1", 0) == 0)    q.eq.peak1Db = 12.0f;
            if (c.name.rfind ("peak2", 0) == 0)    q.eq.peak2Db = 12.0f;
            if (c.name.rfind ("highShelf", 0) == 0) q.eq.highShelfDb = -12.0f;
            const double d = test::diffRmsDb (renderWith (q), renderWith (p));
            INFO (c.name << ": " << d << " dB");
            REQUIRE (d > -60.0);
            continue;
        }
        const double d = test::diffRmsDb (ref, renderWith (p));
        INFO (c.name << ": " << d << " dB");
        REQUIRE (d > -60.0);
    }
}

TEST_CASE ("Parameter jumps do not click", "[hybrid][clicks]")
{
    const double fs = 48000.0;
    struct Jump { const char* name; std::function<void (ba::ReverbParams&)> set; };
    const std::vector<Jump> jumps {
        { "predelay", [] (auto& p) { p.predelayMs = 300.0f; } },
        { "size",     [] (auto& p) { p.size = 1.0f; } },
        { "decay",    [] (auto& p) { p.decaySeconds = 10.0f; } },
        { "mix",      [] (auto& p) { p.mix = 1.0f; } },
        { "shape",    [] (auto& p) { p.shape = 1.0f; } },
        { "engine",   [] (auto& p) { p.engine = ba::EngineMode::Convolution; } },
        { "lowCut",   [] (auto& p) { p.eq.lowCutHz = 1500.0f; } },
        { "freeze",   [] (auto& p) { p.freeze = true; } },
    };
    for (const auto& j : jumps)
    {
        ba::HybridReverb rv;
        ba::ReverbParams p;
        p.mix = 0.3f;
        rv.setParameters (p);
        rv.prepare (fs, 256);
        loadIR (rv, 9);
        const size_t n = (size_t) (fs * 3.0), jumpAt = (size_t) (fs * 2.0);
        test::Stereo in (n), out (n);
        for (size_t i = 0; i < n; ++i)
            in.l[i] = in.r[i] = 0.25f * std::sin (2.0f * ba::kPi * 220.0f * (float) i / (float) fs);
        for (size_t pos = 0; pos < n; pos += 256)
        {
            if (pos == jumpAt) { j.set (p); rv.setParameters (p); }
            const int bn = (int) std::min<size_t> (256, n - pos);
            rv.process (in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos, bn);
        }
        auto maxStep = [&out] (size_t a, size_t b)
        {
            float m = 0.0f;
            for (size_t i = a + 1; i < b; ++i)
                m = std::max ({ m, std::fabs (out.l[i] - out.l[i - 1]), std::fabs (out.r[i] - out.r[i - 1]) });
            return m;
        };
        const float before = maxStep ((size_t) (fs * 1.0), jumpAt);
        const float after = maxStep (jumpAt, jumpAt + (size_t) (fs * 0.2));
        std::printf ("[clicks] %-9s max step before %.4f, after jump %.4f (x%.2f)\n", j.name,
                     (double) before, (double) after, (double) (after / before));
        INFO (j.name);
        REQUIRE (after < before * 2.0f);
    }
}

TEST_CASE ("CPU cost per second of stereo audio at 48 kHz", "[hybrid][cpu]")
{
    const double fs = 48000.0;
    for (auto mode : { ba::EngineMode::Algorithmic, ba::EngineMode::Hybrid })
    {
        ba::HybridReverb rv;
        ba::ReverbParams p;
        p.engine = mode;
        rv.setParameters (p);
        rv.prepare (fs, 128);
        if (mode == ba::EngineMode::Hybrid)
        {
            auto inst = std::make_unique<ba::ConvolutionInstance>();
            auto ir = ba::IRSynth::generateFactory (16, fs); // Cathedral, about 7 s
            inst->build (ir, fs);
            rv.getConvolution().submit (std::move (inst));
        }
        const auto in = test::noise ((size_t) (fs * 10.0), 0.3f, 6);
        test::Stereo out (in.size());
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t pos = 0; pos < in.size(); pos += 128)
            rv.process (in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos,
                        (int) std::min<size_t> (128, in.size() - pos));
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double load = sec / 10.0 * 100.0;
        std::printf ("[cpu] %s: %.2f %% of one core (block 128)\n",
                     mode == ba::EngineMode::Algorithmic ? "Algorithmic" : "Hybrid (7 s IR)", load);
        REQUIRE (test::allFinite (out));
#ifndef BA_SANITIZED
        REQUIRE (load < 25.0); // generous bound for shared CI machines
#endif
    }
}
