#pragma once

#include "../Params.h"
#include "DspUtil.h"

namespace pyra
{
/** CHAOS (seeded random movement), FOCUS (one knob, many targets) and MOTION (tempo-synced rhythm).
    Computed once per small chunk; produces normalised offsets for synth destinations and FX. */
class Movement
{
public:
    static constexpr int kGens = 8;

    void prepare (double sampleRate);

    /** synthOff must hold DCount floats (overwritten), fxOff FxDestCount floats (overwritten). */
    void compute (const Settings& s, double beat, double bpm, int numSamples, float* synthOff, float* fxOff);

    // values of the three macros as modulation sources (-1..1 / 0..1)
    float chaosValue = 0, focusValue = 0, motionValue = 0;

    // for the UI
    std::atomic<float> uiGen[kGens] {}, uiMotion { 0 };

private:
    void reseed (int seed);
    float motionShape (int shape, double phase, double cycleIndex);

    double sr = 44100.0;
    int seededWith = -1;
    Rng rng;
    float genPhase[kGens] {}, genRate[kGens] {}, genPrev[kGens] {}, genNext[kGens] {}, genVal[kGens] {};
    float sign[32] {};
    int assign[32] {};
    float motionSmoothed = 0, chaosSmoothed = 0;
    double lastCycle = -1; float stepRandom = 0;
};

} // namespace pyra
