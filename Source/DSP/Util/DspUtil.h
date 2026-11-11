// KB REVERB - BLACK ANGEL
// Copyright (C) 2026 KB Audio. Licensed under the GNU AGPLv3 (see LICENSE).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
  #include <xmmintrin.h>
  #define BA_HAS_SSE 1
#else
  #define BA_HAS_SSE 0
#endif

namespace ba
{
constexpr float kPi = 3.14159265358979323846f;
constexpr double kPiD = 3.14159265358979323846;

inline float dbToGain (float db) noexcept { return db <= -100.0f ? 0.0f : std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept  { return g <= 1.0e-5f ? -100.0f : 20.0f * std::log10 (g); }

/** Linear ramp towards a target over a fixed time. Allocation free. */
class LinearSmoother
{
public:
    void reset (double sampleRate, double rampSeconds) noexcept
    {
        rampLength = std::max (1, (int) std::lround (sampleRate * rampSeconds));
        remaining = 0;
        current = target;
    }
    void setImmediate (float v) noexcept { current = target = v; remaining = 0; }
    void setTarget (float v) noexcept
    {
        if (v == target)
            return;
        target = v;
        remaining = rampLength;
        step = (target - current) / (float) rampLength;
    }
    float next() noexcept
    {
        if (remaining > 0)
        {
            current += step;
            if (--remaining == 0)
                current = target;
        }
        return current;
    }
    void skip (int n) noexcept
    {
        if (remaining <= 0) return;
        if (n >= remaining) { current = target; remaining = 0; }
        else                { current += step * (float) n; remaining -= n; }
    }
    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept  { return target; }
    bool isSmoothing() const noexcept { return remaining > 0; }

private:
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int remaining = 0, rampLength = 1;
};

/** Small deterministic xorshift generator (no allocation, reproducible). */
struct Rng
{
    explicit Rng (uint32_t seed = 0x9E3779B9u) noexcept : s (seed != 0 ? seed : 1u) {}
    uint32_t nextU() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() noexcept { return (float) (nextU() >> 8) * (1.0f / 16777216.0f); }
    float bip() noexcept { return uni() * 2.0f - 1.0f; }
    float gauss() noexcept
    {
        const float u1 = std::max (uni(), 1.0e-7f), u2 = uni();
        return std::sqrt (-2.0f * std::log (u1)) * std::cos (2.0f * kPi * u2);
    }
    uint32_t s;
};

/** RAII flush-to-zero / denormals-are-zero for the current thread. */
class DenormalGuard
{
public:
#if BA_HAS_SSE
    DenormalGuard() noexcept : old (_mm_getcsr()) { _mm_setcsr (old | 0x8040u); }
    ~DenormalGuard() noexcept { _mm_setcsr (old); }
private:
    unsigned int old;
#else
    DenormalGuard() noexcept = default;
#endif
};

/** Transparent below -1.4 dBFS, soft ceiling at -0.3 dBFS. Safety only. */
inline float safetyLimit (float x) noexcept
{
    constexpr float knee = 0.85f, range = 0.1166f;
    const float a = std::fabs (x);
    if (a <= knee) return x;
    const float y = knee + range * std::tanh ((a - knee) / range);
    return x < 0.0f ? -y : y;
}

/** Sign of row r of a Sylvester Hadamard matrix at column c. */
inline float hadamardSign (int r, int c) noexcept
{
    unsigned v = (unsigned) (r & c);
    int parity = 0;
    while (v) { parity ^= 1; v &= v - 1; }
    return parity ? -1.0f : 1.0f;
}
} // namespace ba
