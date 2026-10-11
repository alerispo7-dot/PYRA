#pragma once

// CHORDS: scale-based chord generator (progression player and chord-per-key), feeds the ARP and the synth.
#include <juce_audio_processors/juce_audio_processors.h>
#include "DspUtil.h"
#include <array>
#include <vector>

namespace pyra
{
struct HostInfo;
constexpr int kMaxChordSlots = 8;

/** Chord qualities a slot can be forced to (index 0 = follow the scale). Append-only. */
enum ChordQuality { QAuto, QMaj, QMin, QDim, QAug, QSus2, QSus4, QMaj7, QMin7, QDom7, QHalfDim, QAdd9, QMin9, QMaj9, QPower, QCount };
const juce::StringArray& chordQualityNames();
/** Lengths of one slot as a multiple of the base chord length. */
const std::vector<double>& chordSlotLengths();
const juce::StringArray& chordSlotLengthNames();

struct ChordSlot
{
    std::atomic<int>  degree { 0 };     // scale degree (0 = I / i); may be negative or > 6 (octaves)
    std::atomic<int>  shift { 0 };      // chromatic shift of the root (borrowed chords, e.g. bVII = degree 6, shift -1 in major)
    std::atomic<int>  quality { QAuto };
    std::atomic<int>  length { 2 };     // index into chordSlotLengths() (default 1x)
    std::atomic<bool> lock { false };
};

struct ChordSettings
{
    int mode = 0, trigger = 0, key = 0, scale = 2, count = 4, lengthIndex = 2, size = 0, voicing = 1, inversion = 0, rhythm = 0, octave = 0;
    bool bass = true;
    float gate = 0.95f, strum = 0, humanize = 0.15f, velocity = 0.8f, swing = 0, complexity = 0.35f;
};

class ChordPattern
{
public:
    ChordPattern() { clear(); }
    std::array<ChordSlot, kMaxChordSlots> slots;

    void clear();
    /** New progression for the given scale (locked slots are kept). */
    void randomize (int count, int scale, float complexity, int sizeMode, uint32_t seed);
    /** New progression with an emotion (mood > 0, see chordMoodNames()): picks one of the mood's progressions,
        written in roman numerals, and fits it to the scale (locked slots are kept). */
    void randomizeMood (int mood, int count, int scale, float complexity, uint32_t seed);

    juce::var toVar() const;
    void fromVar (const juce::var&);
    juce::ValueTree toTree() const;
    void fromTree (const juce::ValueTree&);
    bool isDefault() const;

    std::atomic<int> changeCounter { 0 };
};

/** The notes of one slot (absolute MIDI notes, before transposition by the played key). `previous` = notes of the chord
    before (for smooth voice leading, may be empty). */
std::vector<int> chordNotes (const ChordPattern& p, int slot, const ChordSettings& s, const std::vector<int>& previous);
/** Root pitch class (0..11) and display names of a slot: "vi" and "Am7". */
int chordRootPc (const ChordPattern& p, int slot, const ChordSettings& s);
juce::String chordName (const ChordPattern& p, int slot, const ChordSettings& s);
juce::String chordRoman (const ChordPattern& p, int slot, const ChordSettings& s);
/** Note spelling that fits the key: flats for C minor, F major..., sharps for E major... */
bool chordUseFlats (int scale, int key);
juce::String pitchClassName (int pc, bool flats);
/** Start (in beats) and length of each hit inside one chord of `lengthBeats`. */
std::vector<std::pair<double, double>> chordRhythmHits (int rhythm, double lengthBeats, float swing);

/** Emotions for the RANDOM button of CHORDS (index 0 = any). Append-only. */
const juce::StringArray& chordMoodNames();
/** How a mood likes to be played: scales and settings RANDOM picks from. */
struct ChordMoodStyle
{
    std::vector<int> scales;                          // indices in choices::scales()
    std::vector<int> rhythms, voicings, sizes, lengths, counts;
    float complexityLo = 0.1f, complexityHi = 0.5f;
    float strumChance = 0.2f, swingChance = 0.1f, bassChance = 0.7f;
    int octave = 0;
    std::vector<const char*> progressions;           // e.g. "i VI III VII", "Imaj7 vi7 ii7 V7"
};
const ChordMoodStyle& chordMoodStyle (int mood);

juce::MidiFile chordsToMidiFile (const ChordSettings& s, const ChordPattern& pattern, int loops = 2);

//==============================================================================
class ChordGenerator
{
public:
    void init (juce::AudioProcessorValueTreeState& state, ChordPattern& pattern);
    void prepare (double sampleRate);

    /** Replaces incoming notes with chords (when enabled). Runs before the ARP. */
    void process (juce::MidiBuffer& midi, int numSamples, const HostInfo& host);
    ChordSettings readSettings() const;

    /** UI: play one slot for a moment (works even when CHORDS is off). */
    void preview (int slot) { previewRequest.store (slot); }

    std::atomic<int> currentSlot { -1 };

private:
    void noteOn (juce::MidiBuffer& out, int note, float vel, int pos);
    void noteOff (juce::MidiBuffer& out, int note, int pos);
    void allOff (juce::MidiBuffer& out, int pos);

    ChordPattern* pattern = nullptr;
    std::atomic<float>* pOn {}, *pMode {}, *pTrig {}, *pKey {}, *pScale {}, *pCount {}, *pLen {}, *pSize {}, *pVoicing {}, *pInv {},
        *pRhythm {}, *pOct {}, *pBass {}, *pGate {}, *pStrum {}, *pHuman {}, *pVel {}, *pSwing {}, *pComplex {};

    double sr = 44100.0;
    bool wasOn = false, wasRunning = false, active = false;
    std::vector<int> held;             // keys physically down (in order)
    int lastKey = -1;                  // latch / transposition key
    double internalBeat = 0.0, origin = 0.0, expectedBeat = -1.0;
    int counts[128] {};                // how many sources hold each output note
    struct Playing { int note; double offBeat; };
    std::vector<Playing> playing;
    std::vector<int> keyChords[128];   // chord-per-key: notes started by each key
    std::vector<int> lastChord;        // for voice leading
    int lastSlot = -1;
    Rng rng;

    std::atomic<int> previewRequest { -1 };
    int previewLeft = 0;
    std::vector<int> previewNotes;
};

} // namespace pyra
