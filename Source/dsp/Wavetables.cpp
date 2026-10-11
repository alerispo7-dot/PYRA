#include "Wavetables.h"
#include <complex>
#include <functional>
#include <cmath>
#include <map>

namespace pyra
{
namespace
{
    constexpr int N = Wavetables::kBaseLen;
    constexpr int H = N / 2;               // highest harmonic bin
    constexpr double twoPi = 6.283185307179586;

    using cd = std::complex<double>;

    // Iterative radix-2 FFT (in place). inverse => conjugated twiddles, no scaling.
    void fft (std::vector<cd>& a, bool inverse)
    {
        const int n = (int) a.size();
        for (int i = 1, j = 0; i < n; ++i)
        {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap (a[(size_t) i], a[(size_t) j]);
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const double ang = twoPi / len * (inverse ? 1.0 : -1.0);
            const cd wl (std::cos (ang), std::sin (ang));
            for (int i = 0; i < n; i += len)
            {
                cd w (1.0, 0.0);
                for (int k = 0; k < len / 2; ++k)
                {
                    const cd u = a[(size_t) (i + k)];
                    const cd v = a[(size_t) (i + k + len / 2)] * w;
                    a[(size_t) (i + k)] = u + v;
                    a[(size_t) (i + k + len / 2)] = u - v;
                    w *= wl;
                }
            }
        }
    }

    using TimeFn = std::function<double (double x, double t, int frame, int frames)>;
    using SpecFn = std::function<void (double t, int frame, int frames, std::vector<double>& amp, std::vector<double>& phase)>;

    struct Def
    {
        const char* name;
        int frames;
        TimeFn timeFn;
        SpecFn specFn;
        const char* category = nullptr;
    };

    double saw (double x)    { return 2.0 * std::fmod (x + 0.5, 1.0) - 1.0; }
    double frac (double x)   { return x - std::floor (x); }

    // Resonance curve used for formant-style spectra
    double peak (double f, double centre, double bw) { const double d = (f - centre) / bw; return 1.0 / (1.0 + d * d); }

    struct Vowel { double f1, f2, f3; };
    const Vowel vowels[] = { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 280, 2250, 2900 }, { 450, 800, 2830 }, { 325, 700, 2530 } };

    Vowel vowelAt (double t)
    {
        const double p = juce::jlimit (0.0, 3.999, t * 4.0);
        const int i = (int) p; const double f = p - i;
        auto& a = vowels[i]; auto& b = vowels[i + 1];
        return { a.f1 + (b.f1 - a.f1) * f, a.f2 + (b.f2 - a.f2) * f, a.f3 + (b.f3 - a.f3) * f };
    }

    uint32_t hashRand (uint32_t x)
    {
        x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
        return x;
    }
    double rnd (uint32_t a, uint32_t b) { return (hashRand (a * 9781u + b * 6271u + 17u) & 0xffffff) / (double) 0xffffff; }

    std::vector<Def> definitions()
    {
        std::vector<Def> d;

        d.push_back ({ "Basic Shapes", 4, [] (double x, double, int f, int) {
            switch (f) {
                case 0:  return std::sin (twoPi * x);
                case 1:  return x < 0.25 ? 4 * x : (x < 0.75 ? 2 - 4 * x : 4 * x - 4);
                case 2:  return saw (x);
                default: return x < 0.5 ? 1.0 : -1.0; } }, {} });

        d.push_back ({ "Sine", 1, [] (double x, double, int, int) { return std::sin (twoPi * x); }, {} });
        d.push_back ({ "Saw", 1, [] (double x, double, int, int) { return saw (x); }, {} });

        d.push_back ({ "Square PWM", 16, [] (double x, double t, int, int) {
            const double pw = 0.5 - t * 0.46; return x < pw ? 1.0 : -1.0; }, {} });

        d.push_back ({ "Tri > Saw", 16, [] (double x, double t, int, int) {
            const double p = 0.5 + t * 0.495;
            return x < p ? -1.0 + 2.0 * x / p : 1.0 - 2.0 * (x - p) / (1.0 - p); }, {} });

        d.push_back ({ "Analog Saw", 8, {}, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = (1.0 / n) * std::exp (-n * t * 0.06) * (1.0 + 0.08 * t * ((n & 1) ? 1 : -1)); ph[n] = 0; } } });

        d.push_back ({ "Hard Sync", 16, [] (double x, double t, int, int) {
            const double r = 1.0 + t * 7.0; return saw (frac (x * r)) * (1.0 - 0.15 * x); }, {} });

        d.push_back ({ "Harmonic Sweep", 16, {}, [] (double t, int, int, auto& a, auto& ph) {
            const double count = 1.0 + std::pow (t, 2.0) * 95.0;
            for (int n = 1; n <= H; ++n) { a[n] = juce::jlimit (0.0, 1.0, count - n + 1.0) / std::sqrt ((double) n); ph[n] = 0; } } });

        d.push_back ({ "Odd Harmonics", 16, {}, [] (double t, int, int, auto& a, auto& ph) {
            const int lim = 1 + 2 * (int) std::round (t * 40.0);
            for (int n = 1; n <= H; ++n) { a[n] = ((n & 1) && n <= lim) ? 1.0 / n : 0.0; ph[n] = 0; } } });

        d.push_back ({ "Organ", 8, {}, [] (double, int f, int, auto& a, auto& ph) {
            // drawbars 8', 4', 2 2/3', 2', 1 3/5', 1 1/3', 1'
            static const int bars[8][7] = { { 8,0,0,0,0,0,0 }, { 8,8,0,0,0,0,0 }, { 8,8,8,0,0,0,0 }, { 8,0,8,0,0,0,8 },
                                            { 8,8,8,8,0,0,0 }, { 8,6,8,4,0,3,8 }, { 8,8,8,8,8,8,8 }, { 6,8,4,8,6,4,8 } };
            static const int harm[7] = { 1, 2, 3, 4, 5, 6, 8 };
            std::fill (a.begin(), a.end(), 0.0); std::fill (ph.begin(), ph.end(), 0.0);
            for (int b = 0; b < 7; ++b) a[harm[b]] += bars[f][b] / 8.0; } });

        d.push_back ({ "Vowels", 16, {}, [] (double t, int, int, auto& a, auto& ph) {
            const auto v = vowelAt (t);
            for (int n = 1; n <= H; ++n) {
                const double f = 130.0 * n;
                a[n] = (peak (f, v.f1, 90) + 0.7 * peak (f, v.f2, 120) + 0.35 * peak (f, v.f3, 160)) / std::pow (n, 0.4);
                ph[n] = 0; } } });

        d.push_back ({ "Choir", 8, {}, [] (double t, int f, int, auto& a, auto& ph) {
            const auto v = vowelAt (1.0 - t);
            for (int n = 1; n <= H; ++n) {
                const double fr = 220.0 * n;
                a[n] = (peak (fr, v.f1, 160) + 0.6 * peak (fr, v.f2, 260) + 0.3 * peak (fr, v.f3, 320)) / std::pow (n, 0.6)
                       * (0.85 + 0.3 * rnd ((uint32_t) n, (uint32_t) f));
                ph[n] = twoPi * rnd ((uint32_t) n, 77u); } } });

        d.push_back ({ "FM Bell", 16, [] (double x, double t, int, int) {
            return std::sin (twoPi * x + t * 6.0 * std::sin (twoPi * 7.0 * x)); }, {} });

        d.push_back ({ "FM Metal", 16, [] (double x, double t, int, int) {
            const double i = t * 5.0;
            return std::sin (twoPi * x + i * std::sin (twoPi * 5.0 * x + i * 0.5 * std::sin (twoPi * 11.0 * x))); }, {} });

        d.push_back ({ "FM Bass", 16, [] (double x, double t, int, int) {
            return std::sin (twoPi * x + t * 4.0 * std::sin (twoPi * x)); }, {} });

        d.push_back ({ "Wavefold", 16, [] (double x, double t, int, int) {
            return std::sin ((1.0 + t * 8.0) * std::sin (twoPi * x) * 1.5707963); }, {} });

        d.push_back ({ "Digital Bits", 16, [] (double x, double t, int, int) {
            const double levels = std::pow (2.0, 7.0 - t * 5.5) * 0.5;
            return std::round (saw (x) * levels) / levels; }, {} });

        d.push_back ({ "Chip", 8, [] (double x, double, int f, int) {
            auto tri4 = [] (double xx) { const double tr = xx < 0.5 ? 4 * xx - 1 : 3 - 4 * xx; return std::round (tr * 7.5) / 7.5; };
            switch (f) {
                case 0:  return x < 0.125 ? 1.0 : -1.0;
                case 1:  return x < 0.25 ? 1.0 : -1.0;
                case 2:  return x < 0.5 ? 1.0 : -1.0;
                case 3:  return tri4 (x);
                case 4:  return std::round (saw (x) * 7.5) / 7.5;
                case 5:  return (rnd ((uint32_t) (x * 32.0), 5u) > 0.5) ? 1.0 : -1.0;
                case 6:  return 0.5 * (x < 0.125 ? 1.0 : -1.0) + 0.5 * tri4 (x);
                default: return 0.5 * (x < 0.25 ? 1.0 : -1.0) + 0.5 * (frac (2 * x) < 0.5 ? 1.0 : -1.0); } }, {} });

        d.push_back ({ "Growl", 16, {}, [] (double t, int, int, auto& a, auto& ph) {
            const double f1 = 300 + t * 600, f2 = 800 + t * 1700;
            for (int n = 1; n <= H; ++n) {
                const double f = 80.0 * n;
                a[n] = (1.0 / n) * (0.25 + 2.5 * peak (f, f1, 110) + 1.8 * peak (f, f2, 180));
                ph[n] = 0; } } });

        d.push_back ({ "Spectral Comb", 16, {}, [] (double t, int, int, auto& a, auto& ph) {
            const double spacing = 0.04 + t * 0.21;
            for (int n = 1; n <= H; ++n) { a[n] = (1.0 / n) * (0.5 + 0.5 * std::cos (twoPi * n * spacing)); ph[n] = 0; } } });

        d.push_back ({ "Piano", 8, {}, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) {
                a[n] = std::exp (-(n - 1) * (0.38 - t * 0.3)) / std::sqrt ((double) n) * ((n % 7) == 0 ? 0.15 : 1.0);
                ph[n] = twoPi * rnd ((uint32_t) n, 3u) * 0.3; } } });

        d.push_back ({ "Strings", 8, {}, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) {
                const double f = 196.0 * n;
                a[n] = (1.0 / n) * std::exp (-n * (0.05 - 0.04 * t))
                       * (1.0 + 0.9 * peak (f, 450, 150) + 0.7 * peak (f, 1500, 400) + 0.4 * peak (f, 3000, 700));
                ph[n] = twoPi * rnd ((uint32_t) n, 11u); } } });

        d.push_back ({ "Brass", 8, {}, [] (double t, int, int, auto& a, auto& ph) {
            const double slope = 1.7 - t * 1.15;
            for (int n = 1; n <= H; ++n) { a[n] = (n == 1 ? 0.7 : 1.0) / std::pow (n, slope); ph[n] = 0; } } });

        d.push_back ({ "Reed", 8, {}, [] (double t, int, int, auto& a, auto& ph) {
            const double even = 0.06 + t * 0.9;
            for (int n = 1; n <= H; ++n) { a[n] = (1.0 / std::pow (n, 1.15)) * ((n & 1) ? 1.0 : even); ph[n] = 0; } } });

        d.push_back ({ "Glass", 8, {}, [] (double t, int, int, auto& a, auto& ph) {
            std::fill (a.begin(), a.end(), 0.0); std::fill (ph.begin(), ph.end(), 0.0);
            const int hs[] = { 1, 5, 11, 19, 29, 41 };
            for (int i = 0; i < 6; ++i) a[hs[i]] = std::exp (-i * (1.2 - t * 1.0)); } });

        d.push_back ({ "Sub Bass", 8, {}, [] (double t, int, int, auto& a, auto& ph) {
            std::fill (a.begin(), a.end(), 0.0); std::fill (ph.begin(), ph.end(), 0.0);
            a[1] = 1.0; a[2] = t * 0.5; a[3] = t * 0.28; a[4] = t * t * 0.15; a[5] = t * t * 0.08; } });

        d.push_back ({ "Noise Spectra", 16, {}, [] (double t, int f, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) {
                a[n] = (0.2 + rnd ((uint32_t) n, (uint32_t) f + 100u)) / std::pow (n, 0.9 - t * 0.6);
                ph[n] = twoPi * rnd ((uint32_t) n, (uint32_t) f + 300u); } } });

        d.push_back ({ "Phaser Saw", 16, {}, [] (double t, int, int, auto& a, auto& ph) {
            const double dd = 0.015 + t * 0.3;
            for (int n = 1; n <= H; ++n) { a[n] = (1.0 / n) * std::abs (std::cos (3.14159265 * n * dd)); ph[n] = 0; } } });


        // ---- 0.8: many more tables (append only: the index of a table is saved in presets)
        auto T = [&d] (const char* cat, const char* name, int frames, TimeFn fn) { d.push_back ({ name, frames, std::move (fn), {}, cat }); };
        auto S = [&d] (const char* cat, const char* name, int frames, SpecFn fn) { d.push_back ({ name, frames, {}, std::move (fn), cat }); };
        auto tri = [] (double x) { return x < 0.25 ? 4 * x : (x < 0.75 ? 2 - 4 * x : 4 * x - 4); };
        auto clear = [] (std::vector<double>& a, std::vector<double>& ph) { std::fill (a.begin(), a.end(), 0.0); std::fill (ph.begin(), ph.end(), 0.0); };

        // Basic
        T ("Basic", "Triangle", 1, [tri] (double x, double, int, int) { return tri (x); });
        T ("Basic", "Square", 1, [] (double x, double, int, int) { return x < 0.5 ? 1.0 : -1.0; });
        T ("Basic", "Sine > Square", 16, [] (double x, double t, int, int) { const double g = 1.0 + t * 12.0; return std::tanh (g * std::sin (twoPi * x)) / std::tanh (g); });
        S ("Basic", "Sine > Saw", 16, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = n == 1 ? 1.0 : (1.0 / n) * juce::jlimit (0.0, 1.0, t * 24.0 - n + 2.0); ph[n] = 0; } });
        T ("Basic", "Trapezoid", 16, [tri] (double x, double t, int, int) { return juce::jlimit (-1.0, 1.0, tri (x) * (1.0 + t * 4.0)); });
        T ("Basic", "Half Sine", 1, [] (double x, double, int, int) { return 2.0 * std::max (0.0, std::sin (twoPi * x)) - 1.0; });
        T ("Basic", "Double Saw", 16, [] (double x, double t, int, int) { return 0.5 * (saw (x) + saw (frac (x + 0.5 * t))); });

        // Analog
        S ("Analog", "Warm Square", 8, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = (n & 1) ? (1.0 / n) * std::exp (-n * (0.08 - 0.07 * t)) : 0.0; ph[n] = 0; } });
        T ("Analog", "Saw Stack", 8, [] (double x, double t, int, int) { return (saw (x) + 0.5 * t * saw (frac (2 * x)) + 0.25 * t * saw (frac (4 * x))) / (1.0 + 0.4 * t); });
        T ("Analog", "Analog PWM", 16, [] (double x, double t, int, int) { return std::tanh (5.0 * (std::sin (twoPi * x) - 0.92 * t)); });
        S ("Analog", "Lowpass Saw", 16, [] (double t, int, int, auto& a, auto& ph) {
            const double c = 2.0 + t * 60.0;
            for (int n = 1; n <= H; ++n) { a[n] = (1.0 / n) / std::sqrt (1.0 + std::pow (n / c, 4.0)) * (1.0 + 3.0 * peak (n, c, c * 0.15 + 0.5)); ph[n] = 0; } });
        T ("Analog", "Square Sync", 16, [] (double x, double t, int, int) { return (std::sin (twoPi * x * (1.0 + t * 6.0)) >= 0 ? 1.0 : -1.0) * (1.0 - x * 0.3); });
        S ("Analog", "Soft Saw", 8, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = (1.0 / n) * std::exp (-n * (0.2 - 0.18 * t)); ph[n] = 0; } });

        // Digital
        T ("Digital", "Bit Sine", 16, [] (double x, double t, int, int) { const double l = std::pow (2.0, 6.0 - t * 5.0) * 0.5; return std::round (std::sin (twoPi * x) * l) / l; });
        T ("Digital", "Stepped Saw", 16, [] (double x, double t, int, int) { const double st = 16.0 - t * 13.0; return std::round (saw (x) * st) / st; });
        T ("Digital", "XOR", 16, [] (double x, double t, int, int) {
            const int a = (int) (x * 256.0), b = (int) (x * 256.0 * (2.0 + std::round (t * 7.0)));
            return ((a ^ b) & 255) / 127.5 - 1.0; });
        T ("Digital", "Glitch Steps", 16, [] (double x, double, int f, int) { return rnd ((uint32_t) (x * 16.0), (uint32_t) f * 31u + 9u) * 2.0 - 1.0; });
        T ("Digital", "Ring Mod", 16, [] (double x, double t, int, int) { return std::sin (twoPi * x) * std::sin (twoPi * x * (1.0 + std::round (t * 12.0))); });
        T ("Digital", "PD Saw", 16, [] (double x, double t, int, int) {
            const double dd = 0.5 - t * 0.48;
            const double p = x < dd ? x / (2.0 * dd) : 0.5 + (x - dd) / (2.0 * (1.0 - dd));
            return -std::cos (twoPi * p); });
        T ("Digital", "PD Reso", 16, [] (double x, double t, int, int) { return (1.0 - x) * std::cos (twoPi * x * (1.0 + t * 14.0)); });
        T ("Digital", "Fold Sine", 16, [] (double x, double t, int, int) {
            const double y = std::sin (twoPi * x) * (1.0 + t * 10.0);
            return 4.0 * std::abs (frac (y * 0.25 + 0.25) - 0.5) - 1.0; });
        T ("Digital", "Digital Noise", 16, [] (double x, double, int f, int) { return rnd ((uint32_t) (x * 64.0), (uint32_t) f + 500u) * 2.0 - 1.0; });

        // FM
        T ("FM", "FM 2:1", 16, [] (double x, double t, int, int) { return std::sin (twoPi * x + t * 5.0 * std::sin (twoPi * 2.0 * x)); });
        T ("FM", "FM 3:1", 16, [] (double x, double t, int, int) { return std::sin (twoPi * x + t * 5.0 * std::sin (twoPi * 3.0 * x)); });
        T ("FM", "FM 1:9 Glass", 16, [] (double x, double t, int, int) { return std::sin (twoPi * x + t * 3.0 * std::sin (twoPi * 9.0 * x)); });
        T ("FM", "FM E-Piano", 8, [] (double x, double t, int, int) {
            return 0.8 * std::sin (twoPi * x + 0.8 * std::sin (twoPi * x)) + 0.35 * (0.2 + t) * std::sin (twoPi * 14.0 * x + 1.5 * std::sin (twoPi * x)); });
        T ("FM", "FM Growl", 16, [] (double x, double t, int, int) {
            return std::sin (twoPi * x + t * 8.0 * std::sin (twoPi * 2.0 * x + t * 3.0 * std::sin (twoPi * 3.0 * x))); });
        T ("FM", "FM Feedback", 16, [] (double x, double t, int, int) {
            double y = 0.0; const double b = t * 1.5;
            for (int i = 0; i < 12; ++i) y = std::sin (twoPi * x + b * y);
            return y; });
        T ("FM", "FM Chorus Bell", 8, [] (double x, double t, int, int) {
            return 0.6 * std::sin (twoPi * x + 2.0 * t * std::sin (twoPi * 5.0 * x)) + 0.4 * std::sin (twoPi * 3.0 * x + t * std::sin (twoPi * 7.0 * x)); });

        // Vocal & Formant
        S ("Vocal & Formant", "Vowel A-E", 16, [] (double t, int, int, auto& a, auto& ph) {
            const auto v = vowelAt (t * 0.25);
            for (int n = 1; n <= H; ++n) { const double f = 130.0 * n; a[n] = (peak (f, v.f1, 90) + 0.7 * peak (f, v.f2, 120) + 0.35 * peak (f, v.f3, 160)) / std::pow (n, 0.4); ph[n] = 0; } });
        S ("Vocal & Formant", "Vowel I-O-U", 16, [] (double t, int, int, auto& a, auto& ph) {
            const auto v = vowelAt (0.5 + t * 0.5);
            for (int n = 1; n <= H; ++n) { const double f = 130.0 * n; a[n] = (peak (f, v.f1, 90) + 0.7 * peak (f, v.f2, 120) + 0.35 * peak (f, v.f3, 160)) / std::pow (n, 0.4); ph[n] = 0; } });
        S ("Vocal & Formant", "Talking", 16, [] (double, int f, int, auto& a, auto& ph) {
            const auto v = vowelAt (rnd ((uint32_t) f, 41u));
            for (int n = 1; n <= H; ++n) { const double fr = 150.0 * n; a[n] = (peak (fr, v.f1, 100) + 0.8 * peak (fr, v.f2, 140) + 0.3 * peak (fr, v.f3, 200)) / std::pow (n, 0.3); ph[n] = 0; } });
        S ("Vocal & Formant", "Throat", 8, [] (double t, int, int, auto& a, auto& ph) {
            const double f1 = 300 + 200 * t;
            for (int n = 1; n <= H; ++n) { const double f = 90.0 * n; a[n] = (0.3 + 2.0 * peak (f, f1, 70) + peak (f, 2.4 * f1, 120)) / n; ph[n] = 0; } });
        S ("Vocal & Formant", "Whisper", 8, [] (double t, int f, int, auto& a, auto& ph) {
            const auto v = vowelAt (t);
            for (int n = 1; n <= H; ++n) { const double fr = 110.0 * n; a[n] = (0.3 + rnd ((uint32_t) n, (uint32_t) f + 77u)) * (peak (fr, v.f1, 200) + peak (fr, v.f2, 300) + 0.6 * peak (fr, v.f3, 400)) / std::pow (n, 0.2); ph[n] = twoPi * rnd ((uint32_t) n, 12u); } });
        S ("Vocal & Formant", "Robot Voice", 16, [] (double t, int, int, auto& a, auto& ph) {
            const auto v = vowelAt (t);
            for (int n = 1; n <= H; ++n) { const double f = 110.0 * n; a[n] = ((n % 2) ? 1.0 : 0.25) * (peak (f, v.f1, 60) + peak (f, v.f2, 80) + 0.5 * peak (f, v.f3, 100)); ph[n] = 0; } });

        // Additive
        S ("Additive", "Even Harmonics", 16, [] (double t, int, int, auto& a, auto& ph) {
            const int lim = 2 + 2 * (int) std::round (t * 30.0);
            for (int n = 1; n <= H; ++n) { a[n] = n == 1 ? 1.0 : ((n % 2 == 0 && n <= lim) ? 1.0 / n : 0.0); ph[n] = 0; } });
        S ("Additive", "Prime Harmonics", 8, [] (double t, int, int, auto& a, auto& ph) {
            auto isPrime = [] (int n) { if (n < 2) return false; for (int k = 2; k * k <= n; ++k) if (n % k == 0) return false; return true; };
            const int lim = 3 + (int) (t * 60.0);
            for (int n = 1; n <= H; ++n) { a[n] = (n == 1 || (isPrime (n) && n <= lim)) ? 1.0 / std::sqrt ((double) n) : 0.0; ph[n] = 0; } });
        S ("Additive", "Octaves", 8, [clear] (double t, int, int, auto& a, auto& ph) {
            clear (a, ph);
            for (int k = 0, n = 1; n < H && k < 8; ++k, n *= 2) a[n] = std::pow (0.35 + 0.6 * t, k); });
        S ("Additive", "Octaves & Fifths", 8, [clear] (double t, int, int, auto& a, auto& ph) {
            clear (a, ph);
            const int hs[] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32 };
            for (int i = 0; i < 10; ++i) a[hs[i]] = std::pow (0.4 + 0.55 * t, i); });
        S ("Additive", "Harmonic Pairs", 16, [] (double t, int, int, auto& a, auto& ph) {
            const double c = 2.0 + t * 30.0;
            for (int n = 1; n <= H; ++n) { a[n] = (n == 1 ? 0.6 : 0.0) + peak (n, c, 0.8) + peak (n, c * 1.5, 0.8) * 0.7; ph[n] = 0; } });
        S ("Additive", "Spectral Tilt", 16, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = std::pow ((double) n, -(2.5 - 2.3 * t)); ph[n] = 0; } });
        S ("Additive", "Bandpass Sweep", 16, [] (double t, int, int, auto& a, auto& ph) {
            const double c = 1.0 + t * 60.0;
            for (int n = 1; n <= H; ++n) { a[n] = peak (n, c, 2.0 + c * 0.1) + (n == 1 ? 0.2 : 0.0); ph[n] = 0; } });
        S ("Additive", "Sparse Partials", 16, [] (double, int f, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = (n == 1 || rnd ((uint32_t) n, (uint32_t) f + 900u) > 0.82) ? 1.0 / std::pow (n, 0.7) : 0.0; ph[n] = 0; } });

        // Instruments
        S ("Instruments", "E-Piano", 8, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = (n == 1 ? 1.0 : (n == 2 ? 0.25 : 0.0)) + (0.05 + 0.4 * t) * peak (n, 14, 0.9); ph[n] = 0; } });
        S ("Instruments", "Clav", 8, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = std::abs (std::sin (3.14159265358979 * n * (0.08 + 0.1 * t))) / std::pow (n, 0.8); ph[n] = 0; } });
        S ("Instruments", "Flute", 8, [clear] (double t, int, int, auto& a, auto& ph) {
            clear (a, ph); a[1] = 1.0; a[2] = 0.3 + 0.25 * t; a[3] = 0.1 + 0.15 * t; a[4] = 0.04 * t; a[5] = 0.03 * t; });
        S ("Instruments", "Clarinet", 8, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = (n & 1) ? std::exp (-n * (0.3 - 0.22 * t)) : 0.04 * std::exp (-n * 0.2); ph[n] = 0; } });
        S ("Instruments", "Sax", 8, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { const double f = 150.0 * n; a[n] = (1.0 / std::pow (n, 1.1)) * (1.0 + (1.5 + t) * peak (f, 1500, 500) + 0.8 * peak (f, 3200, 600)); ph[n] = 0; } });
        S ("Instruments", "Violin", 8, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { const double f = 440.0 * n; a[n] = (1.0 / n) * (1.0 + 1.2 * peak (f, 460, 120) + (0.8 + t) * peak (f, 2800, 600) + 0.6 * peak (f, 4500, 900)); ph[n] = twoPi * rnd ((uint32_t) n, 21u); } });
        S ("Instruments", "Cello", 8, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { const double f = 130.0 * n; a[n] = (1.0 / n) * std::exp (-n * (0.04 - 0.03 * t)) * (1.0 + 1.5 * peak (f, 200, 60) + peak (f, 400, 100) + 0.7 * peak (f, 1100, 250)); ph[n] = twoPi * rnd ((uint32_t) n, 23u); } });
        S ("Instruments", "Harp", 8, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = std::abs (std::sin (3.14159265358979 * n / 7.0)) * std::exp (-n * (0.5 - 0.35 * t)); ph[n] = 0; } });
        S ("Instruments", "Marimba", 8, [clear] (double t, int, int, auto& a, auto& ph) {
            clear (a, ph); a[1] = 1.0; a[4] = 0.1 + 0.4 * t; a[10] = 0.15 * t; a[3] = 0.05; });
        S ("Instruments", "Accordion", 8, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { const double f = 200.0 * n; a[n] = (1.0 / std::pow (n, 0.9)) * (1.0 + (1.0 + t) * peak (f, 1500, 400)) * std::exp (-n * 0.02); ph[n] = 0; } });
        S ("Instruments", "Harmonica", 8, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { const double f = 300.0 * n; a[n] = (1.0 / n) * ((n & 1) ? 1.0 : 0.6) * (1.0 + (2.0 + 2.0 * t) * peak (f, 1800, 500)); ph[n] = 0; } });
        S ("Instruments", "Bell Tower", 8, [clear] (double t, int, int, auto& a, auto& ph) {
            clear (a, ph); a[1] = 0.6; a[2] = 1.0; a[3] = 0.5; a[4] = 0.7 * (0.4 + t); a[5] = 0.4; a[7] = 0.5 * t; a[9] = 0.3 * t; a[12] = 0.2 * t; });
        S ("Instruments", "Steel Pan", 8, [clear] (double t, int, int, auto& a, auto& ph) {
            clear (a, ph); a[1] = 1.0; a[2] = 0.8; a[3] = 0.5 + 0.3 * t; a[4] = 0.3; a[6] = 0.2 + 0.2 * t; a[8] = 0.1 * t; });
        S ("Instruments", "Guitar Pluck", 8, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = std::abs (std::sin (3.14159265358979 * n / 5.0)) / std::pow (n, 1.1) * std::exp (-n * 0.02 * (1.0 - t)); ph[n] = 0; } });

        // Bass
        T ("Bass", "Reese", 16, [] (double x, double t, int, int) { return 0.5 * (saw (x) + saw (frac (x + 0.03 + t * 0.45))); });
        S ("Bass", "Wobble", 16, [] (double t, int, int, auto& a, auto& ph) {
            const double c = 3.0 + t * 40.0;
            for (int n = 1; n <= H; ++n) { a[n] = (1.0 / n) / (1.0 + std::pow (n / c, 3.0)) * (1.0 + 4.0 * peak (n, c, 1.5)); ph[n] = 0; } });
        T ("Bass", "Neuro", 16, [] (double x, double t, int, int) { return std::tanh (3.0 * std::sin (twoPi * x + t * 6.0 * std::sin (twoPi * 3.0 * x))); });
        T ("Bass", "808 Body", 8, [] (double x, double t, int, int) { const double g = 1.0 + t * 3.0; return std::tanh (std::sin (twoPi * x) * g) / std::tanh (g); });
        S ("Bass", "Acid Square", 16, [] (double t, int, int, auto& a, auto& ph) {
            const double c = 3.0 + t * 50.0;
            for (int n = 1; n <= H; ++n) { a[n] = (n & 1) ? (1.0 / n) / std::sqrt (1.0 + std::pow (n / c, 4.0)) * (1.0 + 5.0 * peak (n, c, 1.0)) : 0.0; ph[n] = 0; } });
        T ("Bass", "Dirty Saw", 16, [] (double x, double t, int, int) { const double g = 1.0 + t * 8.0; return std::tanh (saw (x) * g) / std::tanh (g); });

        // Pads & Atmos
        S ("Pads & Atmos", "Soft Pad", 8, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = std::exp (-n * (0.6 - 0.4 * t)); ph[n] = twoPi * rnd ((uint32_t) n, 51u); } });
        S ("Pads & Atmos", "Glass Pad", 8, [clear] (double t, int, int, auto& a, auto& ph) {
            clear (a, ph);
            const int hs[] = { 1, 3, 6, 10, 15, 21, 28 };
            for (int i = 0; i < 7; ++i) a[hs[i]] = std::exp (-i * (0.9 - 0.7 * t)); });
        S ("Pads & Atmos", "Airy", 16, [] (double t, int f, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = n == 1 ? 1.0 : (n > 16 && n < 90 ? (0.05 + 0.2 * t) * rnd ((uint32_t) n, (uint32_t) f + 61u) / std::sqrt ((double) n) : 0.0); ph[n] = twoPi * rnd ((uint32_t) n, 62u); } });
        S ("Pads & Atmos", "Warm Pad", 8, [] (double t, int, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = n <= 8 + (int) (t * 24) ? 1.0 / std::pow (n, 1.5) : 0.0; ph[n] = twoPi * rnd ((uint32_t) n, 63u); } });
        S ("Pads & Atmos", "Choir Pad", 8, [] (double t, int, int, auto& a, auto& ph) {
            const auto v = vowelAt (0.75 - t * 0.5);
            for (int n = 1; n <= H; ++n) { const double fr = 180.0 * n; a[n] = (peak (fr, v.f1, 180) + 0.6 * peak (fr, v.f2, 280) + 0.3 * peak (fr, v.f3, 350)) / std::pow (n, 0.5); ph[n] = twoPi * rnd ((uint32_t) n, 64u); } });
        S ("Pads & Atmos", "Shimmer", 16, [clear] (double t, int, int, auto& a, auto& ph) {
            clear (a, ph);
            for (int k = 0, n = 1; n < H && k < 8; ++k, n *= 2) { a[n] = std::pow (0.5, k) + t * (k >= 3 ? 0.5 : 0.0); ph[n] = twoPi * rnd ((uint32_t) n, 65u); } });
        S ("Pads & Atmos", "Dark Pad", 8, [clear] (double t, int, int, auto& a, auto& ph) {
            clear (a, ph); a[1] = 1.0; a[2] = 0.4 + 0.2 * t; a[3] = 0.2 + 0.1 * t; a[4] = 0.08 * t; a[5] = 0.05 * t; });

        // Noise & FX
        S ("Noise & FX", "Bright Noise", 16, [] (double, int f, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = 0.3 + rnd ((uint32_t) n, (uint32_t) f + 700u); ph[n] = twoPi * rnd ((uint32_t) n, (uint32_t) f + 701u); } });
        S ("Noise & FX", "Metallic", 8, [] (double t, int f, int, auto& a, auto& ph) {
            for (int n = 1; n <= H; ++n) { a[n] = rnd ((uint32_t) n, (uint32_t) f + 800u) > 0.9 ? (0.4 + t) / std::pow (n, 0.4) : 0.0; ph[n] = twoPi * rnd ((uint32_t) n, 801u); } a[1] = 0.5; });
        S ("Noise & FX", "Granular", 16, [] (double, int f, int, auto& a, auto& ph) {
            const double c = 2.0 + rnd ((uint32_t) f, 33u) * 60.0;
            for (int n = 1; n <= H; ++n) { a[n] = peak (n, c, 6.0) * (0.3 + rnd ((uint32_t) n, (uint32_t) f + 34u)); ph[n] = twoPi * rnd ((uint32_t) n, (uint32_t) f + 35u); } });
        T ("Noise & FX", "Laser", 16, [] (double x, double t, int, int) { const double m = std::round (t * 10.0); return std::sin (twoPi * (x + m * x * x)); });

        // categories of the tables written before 0.8
        static const std::map<juce::String, const char*> oldCats {
            { "Basic Shapes", "Basic" }, { "Sine", "Basic" }, { "Saw", "Basic" }, { "Square PWM", "Basic" }, { "Tri > Saw", "Basic" },
            { "Analog Saw", "Analog" }, { "Hard Sync", "Analog" },
            { "Harmonic Sweep", "Additive" }, { "Odd Harmonics", "Additive" }, { "Spectral Comb", "Additive" }, { "Phaser Saw", "Additive" },
            { "Vowels", "Vocal & Formant" }, { "Choir", "Vocal & Formant" }, { "Growl", "Vocal & Formant" },
            { "FM Bell", "FM" }, { "FM Metal", "FM" }, { "FM Bass", "FM" },
            { "Wavefold", "Digital" }, { "Digital Bits", "Digital" }, { "Chip", "Digital" },
            { "Organ", "Instruments" }, { "Piano", "Instruments" }, { "Strings", "Instruments" }, { "Brass", "Instruments" },
            { "Reed", "Instruments" }, { "Glass", "Instruments" }, { "Sub Bass", "Bass" }, { "Noise Spectra", "Noise & FX" } };
        for (auto& def : d)
            if (def.category == nullptr)
            {
                auto it = oldCats.find (def.name);
                def.category = it != oldCats.end() ? it->second : "Basic";
            }
        return d;
    }

    Wavetables::Table build (const Def& def)
    {
        Wavetables::Table t;
        t.name = def.name;
        t.numFrames = def.frames;
        t.data.assign ((size_t) def.frames * (size_t) Wavetables::frameStride(), 0.0f);

        std::vector<cd> spec ((size_t) N), work ((size_t) N);
        std::vector<double> amp ((size_t) H + 1), ph ((size_t) H + 1);

        for (int f = 0; f < def.frames; ++f)
        {
            const double tt = def.frames > 1 ? (double) f / (def.frames - 1) : 0.0;

            if (def.timeFn)
            {
                for (int i = 0; i < N; ++i)
                    spec[(size_t) i] = cd (def.timeFn ((double) i / N, tt, f, def.frames), 0.0);
                fft (spec, false);
            }
            else
            {
                std::fill (amp.begin(), amp.end(), 0.0);
                std::fill (ph.begin(), ph.end(), 0.0);
                def.specFn (tt, f, def.frames, amp, ph);
                std::fill (spec.begin(), spec.end(), cd (0.0, 0.0));
                for (int n = 1; n < H; ++n)
                {
                    // sin(2*pi*n*x + phi) == cos(2*pi*n*x + phi - pi/2)
                    const cd c = std::polar (amp[(size_t) n] * N * 0.5, ph[(size_t) n] - 1.5707963267948966);
                    spec[(size_t) n] = c;
                    spec[(size_t) (N - n)] = std::conj (c);
                }
            }
            spec[0] = 0.0;              // no DC
            spec[(size_t) H] = 0.0;     // no Nyquist

            float gain = 1.0f;
            for (int mip = 0; mip < Wavetables::kNumMips; ++mip)
            {
                const int maxHarm = H >> mip;
                for (int n = 0; n < N; ++n)
                {
                    const int h = n <= H ? n : N - n;
                    work[(size_t) n] = h <= maxHarm && h < H ? spec[(size_t) n] : cd (0.0, 0.0);
                }
                fft (work, true);

                if (mip == 0)
                {
                    double peakVal = 0.0;
                    for (auto& v : work) peakVal = std::max (peakVal, std::abs (v.real()) / N);
                    gain = peakVal > 1e-9 ? (float) (0.95 / peakVal) : 0.0f;
                }

                const int len = Wavetables::mipLength (mip);
                const int step = N / len;
                float* dst = t.data.data() + (size_t) f * Wavetables::frameStride() + Wavetables::mipOffset (mip);
                for (int i = 0; i < len; ++i)
                    dst[i] = (float) (work[(size_t) (i * step)].real() / N) * gain;
                dst[len] = dst[0];
            }
        }
        return t;
    }
}

const juce::StringArray& Wavetables::names()
{
    static juce::StringArray n = [] { juce::StringArray r; for (auto& d : definitions()) r.add (d.name); return r; }();
    return n;
}

const juce::StringArray& Wavetables::categoryOfEach()
{
    static juce::StringArray c = [] { juce::StringArray r; for (auto& d : definitions()) r.add (d.category); return r; }();
    return c;
}

const juce::StringArray& Wavetables::categories()
{
    static juce::StringArray c { "Basic", "Analog", "Digital", "FM", "Vocal & Formant", "Additive", "Instruments", "Bass",
                                 "Pads & Atmos", "Noise & FX" };
    return c;
}

Wavetables::Wavetables()
{
    for (auto& d : definitions())
        tables.push_back (build (d));
}

const Wavetables& Wavetables::get()
{
    static Wavetables instance;
    return instance;
}

} // namespace pyra
