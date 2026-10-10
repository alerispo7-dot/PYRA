#include "Wavetables.h"
#include <complex>
#include <functional>
#include <cmath>

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
