#pragma once

#include "../Params.h"
#include "DspUtil.h"
#include "Filters.h"
#include "Envelope.h"
#include "Wavetables.h"

namespace pyra
{
/** Shared, per-render-chunk information handed from the engine to every voice. */
struct VoiceContext
{
    const Settings* s = nullptr;
    float sampleRate = 44100.0f;
    float modWheel = 0;          // 0..1
    float channelPressure = 0;   // 0..1
    float pitchBend = 0;         // -1..1 (global)
    float globalBendSemis = 0;   // already scaled by the bend range
    float lfoGlobalPhase[kNumLfo] {};
    float lfoSyncHz[kNumLfo] {};
    const float* globalOff = nullptr;      // CHAOS / FOCUS / MOTION offsets (normalised)
    float chaos = 0, focus = 0, motion = 0;
};

class Voice
{
public:
    void prepare (double sampleRate);

    /** Starts (or re-starts) the voice. glideFrom < 0 means no glide. */
    void start (int midiNote, int midiChannel, float velocity, float glideFrom, const Settings& s, uint32_t seed);

    /** Mono/legato: moves to a new note without restarting oscillators. */
    void legatoTo (int midiNote, float velocity, bool retriggerEnvelopes, bool glide, const Settings& s);

    void release();
    void kill();

    bool isActive() const noexcept     { return active; }
    bool isReleasing() const noexcept  { return ampEnv.isReleasing(); }
    float getLevel() const noexcept    { return ampEnv.getValue(); }

    void render (float* outL, float* outR, int numSamples, const VoiceContext& ctx);

    // note/MIDI state managed by the engine
    int note = -1, channel = 1;
    bool keyDown = false, sustained = false;
    uint64_t age = 0;
    float polyPressure = 0, mpeBendSemis = 0, mpePressure = 0, mpeSlide = 0;

    /** Values for UI display (written by audio thread, read by UI). */
    float lastLfo[kNumLfo] {}, lastEnv[kNumEnv] {};

private:
    struct OscState
    {
        float phase[kMaxUnison] {};
        float gainL = 0, gainR = 0;    // smoothed output gains (level * pan)
        float lastOut = 0;
    };

    void computeModulation (const VoiceContext& ctx, float* v, float* cutoffOffset);

    float sr = 44100.0f;
    bool active = false;
    float velocity = 1.0f;
    float noteCur = 60.0f, noteTarget = 60.0f, glideStep = 0.0f;
    float randomNote = 0.0f;
    float spreadR[4] {};
    float tolOsc[kNumOsc] {}, tolFilter = 0;
    float driftVal = 0, driftTarget = 0, driftTimer = 0;
    float ampGain = 1.0f;
    float lfoRateMul[kNumLfo] { 1, 1, 1, 1 };

    OscState osc[kNumOsc];
    float subPhase = 0, subOut = 0, subGainL = 0, subGainR = 0;

    // noise
    Rng rng;
    float pink[7] {}, brown = 0, noiseLp = 0, noiseOut = 0, noiseHold = 0;
    int noiseCounter = 0;
    float noiseGainL = 0, noiseGainR = 0;

    MultiFilter filters[kNumFilters];
    Envelope ampEnv, modEnv[kNumEnv - 1];
    Lfo lfos[kNumLfo];
};

} // namespace pyra
