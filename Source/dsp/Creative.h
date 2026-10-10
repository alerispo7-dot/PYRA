#pragma once

// Creative effects: GLITCH (step effects like Gross Beat / Effectrix), GRANULAR, SHIFTER.
#include <juce_audio_processors/juce_audio_processors.h>
#include "DspUtil.h"
#include "../Params.h"
#include <array>
#include <vector>
#include <climits>

namespace pyra
{
constexpr int kMaxGlitchSteps = 16;

/** One step of the GLITCH pattern (shared UI / audio thread). */
struct GlitchStep
{
    std::atomic<int>  fx { GOff };
    std::atomic<bool> lock { false };
};

class GlitchPattern
{
public:
    GlitchPattern() { clear(); }
    std::array<GlitchStep, kMaxGlitchSteps> steps;

    void clear();
    /** New random pattern: `density` 0..1 = how many steps get an effect. Locked steps are kept. */
    void randomize (int numSteps, uint32_t seed, float density = 0.45f);

    juce::var toVar() const;               // null when the pattern is empty
    void fromVar (const juce::var&);        // empty var = clear
    juce::ValueTree toTree() const;
    void fromTree (const juce::ValueTree&);
    bool isEmpty() const;

    std::atomic<int> changeCounter { 0 };
};

/** Ring buffer with absolute (ever-increasing) sample positions. */
class HistoryBuffer
{
public:
    void prepare (int minSamples)
    {
        int size = 1;
        while (size < minSamples) size <<= 1;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        mask = size - 1;
    }
    void clear() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); }
    inline void write (long long t, float l, float r) noexcept
    {
        buf[0][(size_t) (t & mask)] = l;
        buf[1][(size_t) (t & mask)] = r;
    }
    /** Linear read at an absolute fractional position. */
    inline float read (int ch, double pos) const noexcept
    {
        const double fl = std::floor (pos);
        const long long i0 = (long long) fl;
        const float fr = (float) (pos - fl);
        const float a = buf[ch][(size_t) (i0 & mask)], b = buf[ch][(size_t) ((i0 + 1) & mask)];
        return a + fr * (b - a);
    }
    int capacity() const noexcept { return mask; }

private:
    std::vector<float> buf[2];
    long long mask = 0;
};

struct FxTransport
{
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
};

//==============================================================================
class GlitchFx
{
public:
    void prepare (float sampleRate);
    void reset();

    struct Params { int rateIndex = 13, steps = 16; float chance = 0, smooth = 0.3f; bool patternOn = true; };
    /** Processes in place (fully wet). `beat` = beat position of the first sample. */
    void process (float* l, float* r, int n, double beat, double bpm, const Params& p, const GlitchPattern& pattern);
    /** While switched off: keep recording, so the first glitch never replays old audio. */
    void idle (const float* l, const float* r, int n);

    std::atomic<int> uiStep { -1 }, uiEffect { 0 };

private:
    int effectFor (long long stepIndex, const Params& p, const GlitchPattern& pattern);
    double positionFor (long long t) const;

    float sr = 44100.0f;
    HistoryBuffer hist;
    long long now = 0;                // absolute sample counter
    long long curStep = LLONG_MIN;
    int curFx = GOff, prevFx = GOff;
    long long stepStart = 0, anchor = 0;
    double stepLen = 1000.0;          // samples
    double lastPos = 0.0, lastSpeed = 1.0;
    // crossfade from the previous read head
    double fadePos = 0.0, fadeSpeed = 1.0; int fadeLeft = 0, fadeLen = 1;
    float gateGain = 1.0f, crushHoldL = 0, crushHoldR = 0; int crushCount = 0;
    Rng rng;
};

//==============================================================================
class GranularFx
{
public:
    void prepare (float sampleRate);
    void reset();

    struct Params { float sizeMs = 90, density = 18, spray = 0.35f, pitch = 0, pitchRand = 0.2f, reverse = 0.15f, width = 0.8f; bool freeze = false; };
    void process (float* l, float* r, int n, const Params& p);   // fully wet
    void idle (const float* l, const float* r, int n);

private:
    struct Grain { bool active = false; double pos = 0, speed = 1; int age = 0, length = 1; float gl = 1, gr = 1; };
    float sr = 44100.0f;
    HistoryBuffer hist;
    long long writeT = 0, soundSamples = 0;
    double spawnIn = 0.0;
    std::array<Grain, 48> grains;
    Rng rng;
};

//==============================================================================
class ShifterFx
{
public:
    void prepare (float sampleRate);
    void reset();

    struct Params { int mode = 0; float freq = 80, feedback = 0, width = 0.5f; };
    void process (float* l, float* r, int n, const Params& p);   // fully wet

private:
    struct Hilbert
    {
        float xa[4][2] {}, ya[4][2] {}, xb[4][2] {}, yb[4][2] {}, delayB = 0;
        void reset() { *this = {}; }
        void process (float x, float& re, float& im) noexcept;
    };
    float sr = 44100.0f;
    Hilbert hil[2];
    double phase[2] {};
    float fb[2] {};
};

} // namespace pyra
