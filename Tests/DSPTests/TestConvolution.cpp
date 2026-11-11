// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_message.hpp>
#include "TestHelpers.h"

#include <chrono>
#include <cstdio>

namespace
{
ba::IRBuffer randomIR (int channels, int length, uint32_t seed)
{
    ba::IRBuffer ir;
    ir.sampleRate = 48000.0;
    ba::Rng rng (seed);
    for (int c = 0; c < channels; ++c)
    {
        std::vector<float> h ((size_t) length);
        for (int i = 0; i < length; ++i)
            h[(size_t) i] = rng.gauss() * std::exp (-3.0f * (float) i / (float) length);
        ir.channels.push_back (std::move (h));
    }
    ba::normalizeIR (ir);
    return ir;
}

/** Exact time-domain reference with the same routing as ConvolutionInstance. */
test::Stereo directConvolution (const ba::IRBuffer& ir, const test::Stereo& x)
{
    test::Stereo y (x.size());
    struct Route { int in, out, ch; };
    std::vector<Route> routes;
    const int nch = ir.numChannels();
    if (nch == 1) routes = { { 0, 0, 0 }, { 1, 1, 0 } };
    else if (nch == 2) routes = { { 0, 0, 0 }, { 1, 1, 1 } };
    else routes = { { 0, 0, 0 }, { 0, 1, 1 }, { 1, 0, 2 }, { 1, 1, 3 } };
    for (const auto& r : routes)
    {
        const auto& h = ir.channels[(size_t) r.ch];
        const auto& in = r.in == 0 ? x.l : x.r;
        auto& out = r.out == 0 ? y.l : y.r;
        for (size_t t = 0; t < x.size(); ++t)
        {
            double s = 0.0;
            const size_t kmax = std::min (h.size(), t + 1);
            for (size_t k = 0; k < kmax; ++k)
                s += (double) h[k] * in[t - k];
            out[t] += (float) s;
        }
    }
    return y;
}
} // namespace

TEST_CASE ("Partitioned convolution equals direct convolution (zero latency)", "[convolution]")
{
    struct Case { int channels, length; };
    for (const Case c : { Case { 1, 100 }, Case { 2, 5000 }, Case { 1, 24000 }, Case { 2, 24000 }, Case { 4, 24000 } })
    {
        const auto ir = randomIR (c.channels, c.length, 11u + (uint32_t) c.length);
        const auto x = test::noise (26000, 0.5f, 5);
        const auto ref = directConvolution (ir, x);

        ba::ConvolutionInstance inst;
        inst.build (ir, 48000.0);
        test::Stereo y (x.size());
        size_t pos = 0;
        ba::Rng rng (9);
        while (pos < x.size())
        {
            const int n = (int) std::min<size_t> (1 + (size_t) (rng.uni() * 700.0f), x.size() - pos);
            inst.process (x.l.data() + pos, x.r.data() + pos, y.l.data() + pos, y.r.data() + pos, n);
            pos += (size_t) n;
        }
        const double errDb = test::diffRmsDb (ref, y);
        float maxAbs = 0.0f;
        for (size_t i = 0; i < x.size(); ++i)
            maxAbs = std::max ({ maxAbs, std::fabs (ref.l[i] - y.l[i]), std::fabs (ref.r[i] - y.r[i]) });
        INFO ("channels " << c.channels << " length " << c.length << " error " << errDb << " dB, max abs " << maxAbs);
        std::printf ("[convolution] %d ch, %d taps: error vs direct = %.1f dB (max abs %.2e)\n",
                     c.channels, c.length, errDb, (double) maxAbs);
        REQUIRE (errDb < -100.0);
    }
}

TEST_CASE ("Convolution engine swaps IRs with a fade and stays finite", "[convolution]")
{
    ba::ConvolutionEngine eng;
    eng.prepare (48000.0, 512);
    const auto x = test::noise (48000 * 2, 0.5f, 2);
    test::Stereo y (x.size());

    auto a = std::make_unique<ba::ConvolutionInstance>();
    a->build (ba::IRSynth::generateFactory (3, 48000.0), 48000.0);
    eng.submit (std::move (a));

    for (size_t pos = 0; pos < x.size(); pos += 512)
    {
        if (pos == 48000 - 512 * 10)
        {
            auto b = std::make_unique<ba::ConvolutionInstance>();
            b->build (ba::IRSynth::generateFactory (16, 48000.0), 48000.0);
            eng.submit (std::move (b));
        }
        const int n = (int) std::min<size_t> (512, x.size() - pos);
        eng.process (x.l.data() + pos, x.r.data() + pos, y.l.data() + pos, y.r.data() + pos, n);
        eng.collectGarbage();
    }
    REQUIRE (test::allFinite (y));
    REQUIRE (test::rms (y, 24000, 40000) > 0.05);
    REQUIRE (test::rms (y, 60000) > 0.05);
}

TEST_CASE ("All 24 factory IRs are valid and decay as designed", "[convolution][ir]")
{
    const auto& models = ba::IRSynth::factoryModels();
    for (int i = 0; i < ba::IRSynth::kNumFactoryIRs; ++i)
    {
        const auto ir = ba::IRSynth::generateFactory (i, 48000.0);
        REQUIRE (ir.numChannels() == 2);
        test::Stereo s ((size_t) ir.length());
        s.l = ir.channels[0];
        s.r = ir.channels[1];
        REQUIRE (test::allFinite (s));
        double e = 0.0;
        for (float v : s.l) e += (double) v * v;
        REQUIRE (e <= 1.0001);
        REQUIRE (e > 0.5);
        if (models[(size_t) i].kind == 0)
        {
            const double t60 = test::measureT60 (s, 48000.0);
            const double expected = models[(size_t) i].rt60;
            INFO (models[(size_t) i].name << ": T60 " << t60 << " s, expected about " << expected);
            REQUIRE (t60 > expected * 0.6);
            REQUIRE (t60 < expected * 1.3);
        }
    }
}
