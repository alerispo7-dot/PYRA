// Parameters for phase 6: portamento type, CHAOS mangle, GLITCH / GRANULAR / SHIFTER effects, CHORDS generator.
#include "Params.h"

namespace pyra
{
namespace choices
{
    const juce::StringArray& glideTypes()     { static juce::StringArray a { "Constant Time", "Constant Rate" }; return a; }
    const juce::StringArray& glitchEffects()
    {
        static juce::StringArray a { "Off", "Repeat", "Stutter", "Reverse", "Tape Stop", "Slow", "Pitch Up", "Scratch", "Gate", "Crush" };
        return a;
    }
    const juce::StringArray& shifterModes()   { static juce::StringArray a { "Freq Shift", "Ring Mod" }; return a; }
    const juce::StringArray& chordModes()     { static juce::StringArray a { "Progression", "Chord per Key" }; return a; }
    const juce::StringArray& chordTriggers()  { static juce::StringArray a { "Hold Key", "Latch", "DAW Play" }; return a; }

    static const std::pair<const char*, double> kChordLens[] = { { "4 Bars", 16.0 }, { "2 Bars", 8.0 }, { "1 Bar", 4.0 },
                                                                 { "1/2", 2.0 }, { "1/4", 1.0 } };
    const juce::StringArray& chordLengths()
    {
        static juce::StringArray a = [] { juce::StringArray r; for (auto& d : kChordLens) r.add (d.first); return r; }();
        return a;
    }
    double chordLengthBeats (int index) { return kChordLens[juce::jlimit (0, (int) std::size (kChordLens) - 1, index)].second; }

    const juce::StringArray& chordSizes()      { static juce::StringArray a { "Triads", "Sevenths", "Ninths", "Sus", "Power", "Mixed" }; return a; }
    const juce::StringArray& chordVoicings()   { static juce::StringArray a { "Close", "Open", "Spread", "Drop 2" }; return a; }
    const juce::StringArray& chordInversions() { static juce::StringArray a { "Smooth", "Root", "1st Inv", "2nd Inv" }; return a; }
    const juce::StringArray& chordRhythms()
    {
        static juce::StringArray a { "Sustain", "1/4 Pulse", "1/8 Pulse", "Offbeats", "Syncopated", "Charleston", "Stabs" };
        return a;
    }
}

namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }
    void F (Layout& l, const juce::String& id, const juce::String& name, juce::NormalisableRange<float> r, float def, const juce::String& unit = {})
    {
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                            juce::AudioParameterFloatAttributes().withLabel (unit)));
    }
    void I (Layout& l, const juce::String& id, const juce::String& name, int lo, int hi, int def)
    {
        l.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, lo, hi, def));
    }
    void C (Layout& l, const juce::String& id, const juce::String& name, const juce::StringArray& c, int def)
    {
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, c, def));
    }
    void B (Layout& l, const juce::String& id, const juce::String& name, bool def)
    {
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    }
}

void addPhase6Parameters (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::NormalisableRange<float> unit (0.0f, 1.0f);

    C (l, "glide_type", "Portamento Type", choices::glideTypes(), 0);
    F (l, "chaos_mangle", "Chaos Mangle", unit, 0.5f);

    // ---- FX: Glitch (step effects, Gross Beat / Effectrix style)
    B (l, "fx_glt_on", "Glitch On", false);
    C (l, "fx_glt_rate", "Glitch Step", choices::syncDivisions(), 13);   // 1/16
    I (l, "fx_glt_steps", "Glitch Steps", 1, 16, 16);
    F (l, "fx_glt_chance", "Glitch Random", unit, 0.0f);
    F (l, "fx_glt_smooth", "Glitch Smooth", unit, 0.3f);
    F (l, "fx_glt_mix", "Glitch Mix", unit, 1.0f);

    // ---- FX: Granular
    B (l, "fx_grn_on", "Granular On", false);
    F (l, "fx_grn_size", "Granular Grain Size", skewed (10.0f, 500.0f, 100.0f), 90.0f, "ms");
    F (l, "fx_grn_density", "Granular Density", skewed (1.0f, 60.0f, 12.0f), 18.0f, "/s");
    F (l, "fx_grn_spray", "Granular Spray", unit, 0.35f);
    F (l, "fx_grn_pitch", "Granular Pitch", { -24.0f, 24.0f, 1.0f }, 0.0f, "st");
    F (l, "fx_grn_prand", "Granular Random Pitch", unit, 0.2f);
    F (l, "fx_grn_rev", "Granular Reverse", unit, 0.15f);
    B (l, "fx_grn_freeze", "Granular Freeze", false);
    F (l, "fx_grn_width", "Granular Width", unit, 0.8f);
    F (l, "fx_grn_mix", "Granular Mix", unit, 0.5f);

    // ---- FX: Shifter (frequency shifter / ring modulator)
    B (l, "fx_shf_on", "Shifter On", false);
    C (l, "fx_shf_mode", "Shifter Mode", choices::shifterModes(), 0);
    F (l, "fx_shf_freq", "Shifter Frequency", juce::NormalisableRange<float> (-1000.0f, 1000.0f, 0.0f, 0.35f, true), 80.0f, "Hz");
    F (l, "fx_shf_fb", "Shifter Feedback", { 0.0f, 0.9f }, 0.0f);
    F (l, "fx_shf_width", "Shifter Stereo", unit, 0.5f);
    F (l, "fx_shf_mix", "Shifter Mix", unit, 0.5f);

    // ---- CHORDS generator
    B (l, "chord_on", "Chords On", false);
    C (l, "chord_mode", "Chords Mode", choices::chordModes(), 0);
    C (l, "chord_trigger", "Chords Trigger", choices::chordTriggers(), 0);
    C (l, "chord_key", "Chords Key", choices::noteNames(), 0);
    C (l, "chord_scale", "Chords Scale", choices::scales(), 2);   // Minor
    I (l, "chord_count", "Chords Count", 1, 8, 4);
    C (l, "chord_len", "Chord Length", choices::chordLengths(), 2);   // 1 bar
    C (l, "chord_size", "Chord Size", choices::chordSizes(), 0);
    C (l, "chord_voicing", "Chord Voicing", choices::chordVoicings(), 1);
    C (l, "chord_inv", "Chord Inversion", choices::chordInversions(), 0);
    C (l, "chord_rhythm", "Chord Rhythm", choices::chordRhythms(), 0);
    I (l, "chord_oct", "Chord Octave", -2, 2, 0);
    B (l, "chord_bass", "Chord Bass Note", true);
    F (l, "chord_gate", "Chord Gate", { 0.1f, 1.0f }, 0.95f);
    F (l, "chord_strum", "Chord Strum", unit, 0.0f);
    F (l, "chord_human", "Chord Humanize", unit, 0.15f);
    F (l, "chord_vel", "Chord Velocity", { 0.1f, 1.0f }, 0.8f);
    F (l, "chord_swing", "Chord Swing", unit, 0.0f);
    F (l, "chord_complex", "Chord Complexity", unit, 0.35f);
}

} // namespace pyra
