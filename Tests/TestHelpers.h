// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include "DSP/HybridReverb.h"
#include "DSP/IRSynth.h"

#include <cmath>
#include <vector>
#include <algorithm>

namespace test
{
struct Stereo
{
    std::vector<float> l, r;
    explicit Stereo (size_t n = 0) : l (n, 0.0f), r (n, 0.0f) {}
    size_t size() const { return l.size(); }
};

inline Stereo noise (size_t n, float amp, uint32_t seed = 1)
{
    Stereo s (n);
    ba::Rng rng (seed);
    for (size_t i = 0; i < n; ++i) { s.l[i] = amp * rng.bip(); s.r[i] = amp * rng.bip(); }
    return s;
}

inline Stereo impulse (size_t n, float amp)
{
    Stereo s (n);
    s.l[0] = s.r[0] = amp;
    return s;
}

/** Renders through the reverb with a fixed or variable host block size. */
inline Stereo render (ba::HybridReverb& rv, const Stereo& in, int blockSize, bool variable = false)
{
    Stereo out (in.size());
    size_t pos = 0;
    ba::Rng rng (77);
    while (pos < in.size())
    {
        int n = variable ? 1 + (int) (rng.uni() * (float) blockSize) : blockSize;
        n = (int) std::min<size_t> ((size_t) n, in.size() - pos);
        rv.process (in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
        pos += (size_t) n;
    }
    return out;
}

inline ba::ReverbParams wetOnly()
{
    ba::ReverbParams p;
    p.routing = ba::RoutingMode::Aux;
    p.predelayMs = 0.0f;
    p.eq.lowCutOn = false;
    p.eq.highCutOn = false;
    return p;
}

/** T60 from Schroeder backward integration, fitted between -5 and -25 dB. */
inline double measureT60 (const Stereo& ir, double fs)
{
    const size_t n = ir.size();
    std::vector<double> edc (n);
    double acc = 0.0;
    for (size_t i = n; i-- > 0;)
    {
        acc += (double) ir.l[i] * ir.l[i] + (double) ir.r[i] * ir.r[i];
        edc[i] = acc;
    }
    if (acc <= 0.0) return 0.0;
    const double total = edc[0];
    double t5 = -1.0, t25 = -1.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double db = 10.0 * std::log10 (edc[i] / total + 1.0e-30);
        if (t5 < 0.0 && db <= -5.0) t5 = (double) i / fs;
        if (t25 < 0.0 && db <= -25.0) { t25 = (double) i / fs; break; }
    }
    if (t5 < 0.0 || t25 < 0.0) return 0.0;
    return 3.0 * (t25 - t5);
}

inline bool allFinite (const Stereo& s)
{
    for (size_t i = 0; i < s.size(); ++i)
        if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) return false;
    return true;
}

inline float peak (const Stereo& s, size_t from = 0, size_t to = SIZE_MAX)
{
    float p = 0.0f;
    to = std::min (to, s.size());
    for (size_t i = from; i < to; ++i) p = std::max ({ p, std::fabs (s.l[i]), std::fabs (s.r[i]) });
    return p;
}

inline double rms (const Stereo& s, size_t from = 0, size_t to = SIZE_MAX)
{
    to = std::min (to, s.size());
    double e = 0.0;
    for (size_t i = from; i < to; ++i) e += (double) s.l[i] * s.l[i] + (double) s.r[i] * s.r[i];
    return std::sqrt (e / (2.0 * (double) std::max<size_t> (1, to - from)));
}

inline double diffRmsDb (const Stereo& a, const Stereo& b)
{
    double e = 0.0, ref = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        const double dl = (double) a.l[i] - b.l[i], dr = (double) a.r[i] - b.r[i];
        e += dl * dl + dr * dr;
        ref += (double) a.l[i] * a.l[i] + (double) a.r[i] * a.r[i];
    }
    return 10.0 * std::log10 ((e + 1.0e-30) / (ref + 1.0e-30));
}
} // namespace test
