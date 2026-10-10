#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DspUtil.h"
#include <array>
#include <vector>

namespace pyra
{
struct HostInfo;

constexpr int kMaxArpSteps = 32;

/** One step of the pattern. Shared between UI and audio thread (atomics). */
struct ArpStep
{
    std::atomic<int>   degree { 0 };      // scale steps (-14..+14) added to the played note
    std::atomic<float> velocity { 0.8f };
    std::atomic<float> gate { 1.0f };     // multiplies the global gate
    std::atomic<bool>  on { true };
    std::atomic<bool>  tie { false };
    std::atomic<bool>  lock { false };
};

class ArpPattern
{
public:
    ArpPattern() { clear(); }

    std::array<ArpStep, kMaxArpSteps> steps;

    void clear();
    /** Randomizes every unlocked step within the first numSteps. */
    void randomize (int numSteps, uint32_t seed);

    juce::ValueTree toTree() const;
    void fromTree (const juce::ValueTree&);

    std::atomic<int> changeCounter { 0 };   // bumps on every edit (for UI refresh)
};

/** A held key, as seen by the arpeggiator. */
struct HeldNote { int note; float velocity; int order; };

struct ArpSettings
{
    int mode = 0, rate = 13, octaves = 1, steps = 16, scale = 0, key = 0;
    float gate = 0.6f, swing = 0.0f, velAmount = 1.0f;
    bool latch = false;
};

/** Notes a single arp step produces (pure function, also used for MIDI export). */
std::vector<std::pair<int, float>> arpStepNotes (const std::vector<HeldNote>& held, int fireIndex,
                                                 const ArpSettings& s, const ArpPattern& pattern, uint32_t randomSeed);

int scaleShift (int note, int degrees, int scale, int key);

/** Renders `loops` passes of the pattern to a MIDI file (for dragging into the DAW). */
juce::MidiFile arpToMidiFile (const ArpSettings& s, const ArpPattern& pattern, std::vector<HeldNote> held, int loops = 2);

//==============================================================================
class Arpeggiator
{
public:
    void init (juce::AudioProcessorValueTreeState& state, ArpPattern& pattern);
    void prepare (double sampleRate);

    /** Replaces the incoming notes in `midi` with arpeggiated ones (when enabled). */
    void process (juce::MidiBuffer& midi, int numSamples, const HostInfo& host);

    ArpSettings readSettings() const;

    std::atomic<int> currentStep { -1 };

    /** Last chord played (for MIDI export from the UI). */
    std::vector<HeldNote> getLastChord() const;

private:
    void allOff (juce::MidiBuffer& out, int samplePos);

    juce::AudioProcessorValueTreeState* st = nullptr;
    ArpPattern* pattern = nullptr;
    std::atomic<float>* pOn {}, *pMode {}, *pRate {}, *pOct {}, *pGate {}, *pSwing {}, *pLatch {}, *pSteps {}, *pScale {}, *pKey {}, *pVel {};

    double sr = 44100.0;
    std::vector<HeldNote> held, latched;
    int physicallyDown = 0, orderCounter = 0;
    bool wasOn = false, wasPlaying = false;
    double internalBeat = 0.0;
    long long fireIndex = 0;
    long long lastStepIndex = -1;
    uint32_t seed = 1;

    struct Playing { int note; double offBeat; };
    std::vector<Playing> playing;

    mutable juce::SpinLock chordLock;
    std::vector<HeldNote> lastChord;
};

} // namespace pyra
