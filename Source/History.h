#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class PyraAudioProcessor;

namespace pyra
{
/** Undo / redo of everything the user changes in the plugin window (knobs, presets, RANDOM, patterns).
    Changes are grouped: one knob drag or one RANDOM click = one step. Message thread only. */
class UndoHistory
{
public:
    explicit UndoHistory (PyraAudioProcessor&);

    /** Call ~10 times per second while the editor is open. */
    void tick();
    /** The user clicked / typed in the plugin window (changes without this are host automation and are not recorded). */
    void noteUserActivity() { lastActivity = juce::Time::getMillisecondCounterHiRes(); }

    bool canUndo() const;
    bool canRedo() const { return index + 1 < (int) entries.size(); }
    void undo();
    void redo();
    void clear();

private:
    struct Snapshot
    {
        std::vector<float> params;
        int patternSig = 0;
        juce::var patterns;
        juce::String presetId, presetName;
        std::vector<float> presetLoaded;
    };

    Snapshot capture() const;
    int patternSignature() const;
    void apply (const Snapshot&);
    void commit();

    PyraAudioProcessor& proc;
    std::vector<Snapshot> entries;
    int index = -1;
    std::vector<float> lastSeen;
    int lastSeenSig = 0;
    double lastActivity = -1.0e9, lastChange = 0.0;
    bool pending = false;
    static constexpr int maxEntries = 120;
};

} // namespace pyra
