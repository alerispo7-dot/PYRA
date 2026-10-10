#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace pyra
{
constexpr float kPi    = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float fastTanh (float x) noexcept
{
    x = std::clamp (x, -4.5f, 4.5f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float midiToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) * (1.0f / 12.0f)); }

/** Small fast PRNG (xorshift32). */
struct Rng
{
    uint32_t s = 0x12345678u;
    void seed (uint32_t v) noexcept { s = v ? v : 0x9e3779b9u; }
    uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // 0..1
    float bi() noexcept  { return uni() * 2.0f - 1.0f; }                           // -1..1
};

/** Envelope / curve shaping. c in [-1,1]; 0 = linear, >0 = fast start, <0 = slow start. */
inline float curveShape (float t, float c) noexcept
{
    if (std::abs (c) < 0.01f) return t;
    const float s = c * 7.0f;
    return (1.0f - std::exp (-s * t)) / (1.0f - std::exp (-s));
}

inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

inline void panGains (float pan, float& l, float& r) noexcept
{
    // equal-power, pan in [-1,1]
    const float a = (pan + 1.0f) * 0.25f * kPi;
    l = std::cos (a) * 1.41421356f;
    r = std::sin (a) * 1.41421356f;
    l = std::min (l, 1.0f);
    r = std::min (r, 1.0f);
}

} // namespace pyra
