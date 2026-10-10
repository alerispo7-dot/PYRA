#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DspUtil.h"
#include "../Params.h"
#include "Creative.h"
#include <vector>

namespace pyra
{
//==============================================================================
/** Circular delay line with fractional (linear) reads. */
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        int size = 1;
        while (size < maxSamples + 4) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
    /** delay in samples, measured from the most recently pushed sample (>= 1). */
    inline float read (float delay) const noexcept
    {
        const float rp = (float) w - delay;
        const float fl = std::floor (rp);
        const int i0 = (int) fl & mask;
        const int i1 = (i0 + 1) & mask;
        const float fr = rp - fl;
        return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
    }
    int capacity() const noexcept { return mask; }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

struct OnePole
{
    float z = 0.0f;
    inline float lp (float x, float a) noexcept { z += a * (x - z); return z; }
    inline float hp (float x, float a) noexcept { z += a * (x - z); return x - z; }
    static float coef (float hz, float sr) noexcept { return 1.0f - std::exp (-kTwoPi * std::min (hz, sr * 0.49f) / sr); }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};
    inline float process (int ch, float x) noexcept
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }
    void setPeak (float sr, float f, float q, float db);
    void setLowShelf (float sr, float f, float db);
    void setHighShelf (float sr, float f, float db);
};

/** Click-free on/off ramp for an effect module. */
struct ModuleFade
{
    float g = 0.0f, step = 0.001f;
    void prepare (float sr) { step = 1.0f / (0.012f * sr); }
    bool active (bool on) const noexcept { return on || g > 0.0f; }
    inline float next (bool on) noexcept
    {
        g = on ? std::min (1.0f, g + step) : std::max (0.0f, g - step);
        return g;
    }
};

//==============================================================================
/** The global effect chain (fixed order):
    Distortion > Resonator > Filter > Shifter > Granular > Chorus > Flanger > Phaser > Tape > Delay > Reverb > Glitch > EQ > Compressor */
class FxChain
{
public:
    void init (juce::AudioProcessorValueTreeState& state, GlitchPattern& glitchPattern);
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* l, float* r, int n, const FxTransport& transport);

    GlitchFx glitch;

    /** Normalised offsets from CHAOS / FOCUS / MOTION (FxDestCount values). */
    void setOffsets (const float* offsets);

    std::atomic<float> gainReductionDb { 0.0f };

private:
    void processChunk (float* l, float* r, int n, double bpm, double beat);
    double freeBeat = 0.0;

    using P = std::atomic<float>*;
    static bool b (P p) { return p->load (std::memory_order_relaxed) > 0.5f; }
    static float f (P p) { return p->load (std::memory_order_relaxed); }
    static int i (P p) { return (int) std::lround (p->load (std::memory_order_relaxed)); }
    float fo (int fxDest, P p) const;              // parameter value + its offset
    float fmix (int fxDest, P on, P mix) const;    // mix that also works when the module is forced on
    bool forced (int fxDest) const { return offsets[fxDest] > 0.001f; }
    float offsets[FxDestCount] {};
    juce::RangedAudioParameter* modParam[FxDestCount] {};

    float sr = 44100.0f;
    int maxChunk = 512;
    std::vector<float> tmpL, tmpR;

    // ---- distortion
    struct { P on, type, drive, tone, out, mix; } pd {};
    ModuleFade fdDist;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    OnePole distTone[2], distDc[2];
    DelayLine distDry[2];
    int distLatency = 0;
    float holdL = 0, holdR = 0; int holdCount = 0;

    // ---- resonator
    struct { P on, chord, note, decay, bright, mix; } pr {};
    ModuleFade fdRes;
    DelayLine resLine[5][2];
    float resLp[5][2] {};

    // ---- fx filter
    struct { P on, type, cutoff, res, drive, mix; } pf {};
    ModuleFade fdFlt;
    float svf1[2] {}, svf2[2] {};

    // ---- chorus
    struct { P on, rate, depth, delay, fb, width, mix; } pc {};
    ModuleFade fdCho;
    DelayLine choLine[2];
    float choPhase = 0, choFb[2] {};

    // ---- flanger
    struct { P on, rate, depth, fb, mix; } pl {};
    ModuleFade fdFla;
    DelayLine flaLine[2];
    float flaPhase = 0, flaFb[2] {};

    // ---- phaser
    struct { P on, rate, depth, freq, fb, stages, mix; } pp {};
    ModuleFade fdPha;
    float phaZ[2][12] {}, phaFb[2] {}, phaPhase = 0;

    // ---- tape
    struct { P on, wow, flutter, sat, tone, mix; } pt {};
    ModuleFade fdTape;
    DelayLine tapeLine[2];
    OnePole tapeLp[2];
    float wowPhase = 0, flutPhase1 = 0, flutPhase2 = 0, wowRand = 0, wowTarget = 0, wowTimer = 0;
    Rng rng;

    // ---- delay
    struct { P on, mode, sync, div, time, fb, lowcut, highcut, width, mix; } pdl {};
    ModuleFade fdDly;
    DelayLine dlyLine[2];
    OnePole dlyHp[2], dlyLp[2];
    float dlyTimeSmoothed = -1.0f, dlyWowPhase = 0;

    // ---- reverb
    struct { P on, type, size, decay, pre, damp, width, shim, mix; } pv {};
    ModuleFade fdRev;
    DelayLine revPre[2], revDiff[2][4], revLine[8], shimLine;
    float revLp[8] {}, revModPhase[4] {}, shimPhase = 0, shimFb = 0;

    // ---- eq
    struct { P on, lowf, lowg, midf, midg, midq, highf, highg; } pe {};
    ModuleFade fdEq;
    Biquad eqLow, eqMid, eqHigh;
    float eqCache[7] { -1, -1, -1, -1, -1, -1, -1 };

    // ---- glitch / granular / shifter
    struct { P on, rate, steps, chance, smooth, mix; } pg {};
    ModuleFade fdGlt;
    GlitchPattern* glitchPattern = nullptr;
    struct { P on, size, density, spray, pitch, prand, rev, freeze, width, mix; } pgr {};
    ModuleFade fdGrn;
    GranularFx granular;
    struct { P on, mode, freq, fb, width, mix; } psh {};
    ModuleFade fdShf;
    ShifterFx shifter;
    std::vector<float> wetL, wetR;

    // ---- compressor
    struct { P on, thresh, ratio, att, rel, makeup, mix; } pcmp {};
    ModuleFade fdComp;
    float compEnvDb = 0.0f;
};

} // namespace pyra
