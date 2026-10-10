#pragma once

#include "DspUtil.h"
#include "../Params.h"
#include <vector>

namespace pyra
{
/** Stereo multimode filter used in each voice. Coefficients are interpolated
    per sample between control updates so cutoff sweeps stay smooth. */
class MultiFilter
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        const int maxDelay = (int) (sampleRate / 20.0) + 4;
        for (auto& c : comb) c.assign ((size_t) maxDelay, 0.0f);
        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (auto& s : svf[ch]) s = {};
            for (auto& s : formant[ch]) s = {};
            for (auto& v : ladder[ch]) v = 0.0f;
            for (auto& v : ap[ch]) v = 0.0f;
            ladderOut[ch] = 0.0f; phaserFb[ch] = 0.0f;
            std::fill (comb[ch].begin(), comb[ch].end(), 0.0f);
            combLp[ch] = 0.0f;
        }
        combPos = 0;
        first = true;
    }

    /** Sets new targets; values glide to them across the next `numSamples`. */
    void setTarget (int filterType, float cutoffHz, float res, float driveAmt, float mixAmt, int numSamples)
    {
        if (filterType != type) { type = filterType; reset(); }
        cutoffHz = std::clamp (cutoffHz, 10.0f, sr * 0.45f);
        const float gTarget = std::tan (kPi * cutoffHz / sr);

        resonance = std::clamp (res, 0.0f, 1.0f);
        drive = driveAmt; mix = mixAmt;
        targetHz = cutoffHz;

        if (first) { g = gTarget; gStep = 0.0f; hz = cutoffHz; hzStep = 0.0f; first = false; }
        else
        {
            const float inv = 1.0f / (float) std::max (1, numSamples);
            gStep = (gTarget - g) * inv;
            hzStep = (cutoffHz - hz) * inv;
        }
        cutNorm = std::clamp (std::log (cutoffHz / 20.0f) / std::log (1000.0f), 0.0f, 1.0f);

        if (type == FFormant)
        {
            // vowel position follows the cutoff knob (A-E-I-O-U)
            static const float table[5][3] = { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 280, 2250, 2900 },
                                               { 450, 800, 2830 },  { 325, 700, 2530 } };
            const float p = std::clamp (cutNorm * 4.0f, 0.0f, 3.999f);
            const int vi = (int) p;
            const float vf = p - (float) vi;
            for (int f = 0; f < 3; ++f)
            {
                const float fhz = table[vi][f] + (table[vi + 1][f] - table[vi][f]) * vf;
                formantG[f] = std::tan (kPi * std::min (fhz, sr * 0.45f) / sr);
            }
        }
    }

    inline void process (float& l, float& r) noexcept
    {
        g += gStep; hz += hzStep;
        const float dl = l, dr = r;

        if (drive > 0.0f)
        {
            const float gain = 1.0f + drive * 8.0f, comp = 1.0f / (1.0f + drive * 1.2f);
            l = fastTanh (l * gain) * comp;
            r = fastTanh (r * gain) * comp;
        }

        l = processChannel (0, l);
        r = processChannel (1, r);

        if (mix < 1.0f)
        {
            l = dl + (l - dl) * mix;
            r = dr + (r - dr) * mix;
        }
    }

private:
    struct SvfState { float ic1 = 0, ic2 = 0; };

    struct SvfOut { float low, band, high; };

    static inline SvfOut svfTick (SvfState& s, float x, float gg, float k) noexcept
    {
        const float a1 = 1.0f / (1.0f + gg * (gg + k));
        const float a2 = gg * a1, a3 = gg * a2;
        const float v3 = x - s.ic2;
        const float v1 = a1 * s.ic1 + a2 * v3;
        const float v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
        s.ic1 = 2.0f * v1 - s.ic1;
        s.ic2 = 2.0f * v2 - s.ic2;
        return { v2, v1, x - k * v1 - v2 };
    }

    inline float processChannel (int ch, float x) noexcept
    {
        auto* st = svf[ch];
        const float k12 = 2.0f - 1.97f * resonance;

        switch (type)
        {
            case FLp12:  return svfTick (st[0], x, g, k12).low;
            case FHp12:  return svfTick (st[0], x, g, k12).high;
            case FBp12:  { auto o = svfTick (st[0], x, g, k12); return o.band * k12; }
            case FNotch: { auto o = svfTick (st[0], x, g, k12); return o.low + o.high; }
            case FPeak:  { auto o = svfTick (st[0], x, g, k12); return x + o.band * k12 * (1.0f + resonance * 6.0f) - o.band * k12; }
            case FAllpass: { auto o = svfTick (st[0], x, g, k12); return o.low + o.high - k12 * o.band; }

            case FLp24:
            {
                const float k2 = 1.53f * (1.0f - resonance * 0.99f);
                const float y = svfTick (st[0], x, g, 1.848f).low;
                return svfTick (st[1], y, g, k2).low * (1.0f + resonance * 0.5f);
            }
            case FHp24:
            {
                const float k2 = 1.53f * (1.0f - resonance * 0.99f);
                const float y = svfTick (st[0], x, g, 1.848f).high;
                return svfTick (st[1], y, g, k2).high;
            }
            case FBp24:
            {
                const float k2 = 2.0f - 1.95f * resonance;
                const float y = svfTick (st[0], x, g, 1.414f).band * 1.414f;
                return svfTick (st[1], y, g, k2).band * k2;
            }

            case FLadder:
            {
                // 4 one-pole stages with saturated feedback (Moog-style)
                const float G = g / (1.0f + g);
                const float k = resonance * 3.9f;
                float* s = ladder[ch];
                float u = fastTanh ((x - k * ladderOut[ch]) * (1.0f + k * 0.12f));
                for (int i = 0; i < 4; ++i)
                {
                    const float v = (u - s[i]) * G;
                    const float y = v + s[i];
                    s[i] = y + v;
                    u = y;
                }
                ladderOut[ch] = u;
                return u * (1.0f + k * 0.35f);
            }

            case FCombPlus:
            case FCombMinus:
            {
                auto& buf = comb[ch];
                const int size = (int) buf.size();
                // 0.43 samples compensate the damping low-pass inside the loop, so the comb stays in tune
                const float delay = std::clamp (sr / std::max (hz, 20.0f) - 0.43f, 1.0f, (float) size - 2.0f);
                float rp = (float) combPos - delay;
                if (rp < 0.0f) rp += (float) size;
                const int i0 = (int) rp;
                const int i1 = (i0 + 1) % size;
                const float fr = rp - (float) i0;
                const float d = buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
                combLp[ch] += (d - combLp[ch]) * 0.7f;
                const float fb = (type == FCombPlus ? 1.0f : -1.0f) * resonance * 0.97f;
                const float y = x + fb * combLp[ch];
                buf[(size_t) combPos] = fastTanh (y);
                if (ch == 1) combPos = (combPos + 1) % size;
                return y * (1.0f - std::abs (fb) * 0.45f);
            }

            case FFormant:
            {
                static const float gains[3] = { 1.0f, 0.65f, 0.35f };
                const float k = 1.0f / (3.0f + resonance * 22.0f);
                float out = 0.0f;
                for (int f = 0; f < 3; ++f)
                    out += svfTick (formant[ch][f], x, formantG[f], k).band * k * gains[f];
                return out * 2.5f;
            }

            case FPhaser:
            {
                float* s = ap[ch];
                float y = x + phaserFb[ch] * resonance * 0.85f;
                static const float spread[4] = { 0.5f, 0.85f, 1.3f, 2.1f };
                for (int i = 0; i < 4; ++i)
                {
                    const float gg = std::min (g * spread[i], 20.0f);
                    const float G = gg / (1.0f + gg);
                    const float v = (y - s[i]) * G;
                    const float lp = v + s[i];
                    s[i] = lp + v;
                    y = 2.0f * lp - y;
                }
                phaserFb[ch] = y;
                return 0.5f * (x + y);
            }

            default: return x;
        }
    }

    float sr = 44100.0f;
    int type = -1;
    float g = 0.1f, gStep = 0.0f, hz = 1000.0f, hzStep = 0.0f, targetHz = 1000.0f, cutNorm = 0.5f;
    float formantG[3] { 0.1f, 0.2f, 0.4f };
    float resonance = 0.0f, drive = 0.0f, mix = 1.0f;
    bool first = true;

    SvfState svf[2][2];
    SvfState formant[2][3];
    float ladder[2][4] {};
    float ladderOut[2] {};
    float ap[2][4] {};
    float phaserFb[2] {};
    std::vector<float> comb[2];
    float combLp[2] {};
    int combPos = 0;
};

} // namespace pyra
