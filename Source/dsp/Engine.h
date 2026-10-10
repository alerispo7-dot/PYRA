#pragma once

#include "Voice.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace pyra
{
struct HostInfo
{
    double bpm = 120.0;
    double ppq = 0.0;
    bool playing = false;
    bool ppqValid = false;
};

/** Voice management, MIDI handling (incl. MPE), mono/legato/glide. */
class Engine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const Settings& s, const HostInfo& host);
    void allNotesOff();

    int getActiveVoiceCount() const noexcept;

    /** Snapshot of the most recent voice's modulators, for the UI. */
    std::atomic<float> uiEnv[kNumEnv], uiLfo[kNumLfo];
    std::atomic<int> uiVoices { 0 };

private:
    static constexpr int kChunk = 16;

    void handleMidi (const juce::MidiMessage& m, const Settings& s);
    void noteOn (int ch, int note, float vel, const Settings& s);
    void noteOff (int ch, int note, const Settings& s);
    Voice* findVoiceToUse (const Settings& s);
    void renderChunk (float* l, float* r, int n, const Settings& s, const HostInfo& host, int offsetInBlock);

    double sr = 44100.0;
    Voice voices[kMaxVoices];
    uint64_t ageCounter = 0;
    uint32_t seedCounter = 1;

    struct Held { int note, ch; float vel; };
    std::vector<Held> held;   // key stack (mono / legato)

    bool sustainPedal = false;
    float modWheel = 0, channelPressure = 0, pitchBend = 0;
    float mpeBend[17] {}, mpePressure[17] {}, mpeSlide[17] {};
    float lastNote = -1.0f;
    double lfoFreePhase[kNumLfo] {};
    juce::SmoothedValue<float> master;
    std::vector<float> scratchR;

    // analog / vintage stage
    void applyAnalog (float* l, float* r, int n, const Settings& s);
    Rng anRng;
    float activity = 0, humPhase = 0, pinkL[3] {}, pinkR[3] {};
    float convLp[2] {}, convHold[2] {}; int convCount = 0;
};

} // namespace pyra
