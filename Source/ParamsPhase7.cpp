// Parameters for phase 7: CHORDS emotions and LAYERS (up to 8 complete sounds in one PYRA).
#include "Params.h"
#include "dsp/Chords.h"

namespace pyra
{
namespace choices
{
    const juce::StringArray& layerChannels()
    {
        static juce::StringArray a = [] { juce::StringArray r { "Omni" }; for (int c = 1; c <= 16; ++c) r.add ("Ch " + juce::String (c)); return r; }();
        return a;
    }
}

juce::String layerParam (int layer, const char* name) { return "layer" + juce::String (layer) + "_" + name; }

void addPhase7Parameters (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "chord_mood", 1 }, "Chords Emotion", chordMoodNames(), 0));

    // LINK: moving a knob moves the same knob on every layer that is on (all layers change together)
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "layer_link", 1 }, "Layers Link Knobs", true));

    auto noteName = [] (int n, int) { return juce::MidiMessage::getMidiNoteName (n, true, true, 3); };
    for (int k = 0; k < kNumLayers; ++k)
    {
        const auto letter = juce::String::charToString ((juce::juce_wchar) ('A' + k));
        auto id = [k] (const char* n) { return juce::ParameterID { layerParam (k, n), 1 }; };
        l.add (std::make_unique<juce::AudioParameterBool> (id ("on"), "Layer " + letter + " On", k == 0));
        juce::NormalisableRange<float> db (-40.0f, 6.0f, 0.1f);
        db.setSkewForCentre (-6.0f);
        l.add (std::make_unique<juce::AudioParameterFloat> (id ("level"), "Layer " + letter + " Level", db, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")
                .withStringFromValueFunction ([] (float v, int) { return v <= -39.9f ? juce::String ("-inf") : juce::String (v, 1); })));
        l.add (std::make_unique<juce::AudioParameterFloat> (id ("pan"), "Layer " + letter + " Pan", juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));
        l.add (std::make_unique<juce::AudioParameterInt> (id ("trans"), "Layer " + letter + " Transpose", -36, 36, 0,
            juce::AudioParameterIntAttributes().withLabel ("st")));
        l.add (std::make_unique<juce::AudioParameterInt> (id ("lo"), "Layer " + letter + " Lowest Key", 0, 127, 0,
            juce::AudioParameterIntAttributes().withStringFromValueFunction (noteName)));
        l.add (std::make_unique<juce::AudioParameterInt> (id ("hi"), "Layer " + letter + " Highest Key", 0, 127, 127,
            juce::AudioParameterIntAttributes().withStringFromValueFunction (noteName)));
        l.add (std::make_unique<juce::AudioParameterInt> (id ("vlo"), "Layer " + letter + " Lowest Velocity", 1, 127, 1));
        l.add (std::make_unique<juce::AudioParameterInt> (id ("vhi"), "Layer " + letter + " Highest Velocity", 1, 127, 127));
        l.add (std::make_unique<juce::AudioParameterBool> (id ("mute"), "Layer " + letter + " Mute", false));
        l.add (std::make_unique<juce::AudioParameterBool> (id ("solo"), "Layer " + letter + " Solo", false));
        l.add (std::make_unique<juce::AudioParameterChoice> (id ("chan"), "Layer " + letter + " MIDI Channel", choices::layerChannels(), 0));
    }
}
} // namespace pyra
