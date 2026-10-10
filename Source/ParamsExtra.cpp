// Parameters for phase 3: FX chain, ANALOG section, ARP.
#include "Params.h"
#include "dsp/Samples.h"

namespace pyra
{
namespace choices
{
    const juce::StringArray& distTypes()   { static juce::StringArray a { "Soft Clip", "Hard Clip", "Tube", "Fold", "Diode", "Overdrive", "Bitcrush", "Downsample" }; return a; }
    const juce::StringArray& chords()      { static juce::StringArray a { "Single", "Octaves", "Fifth", "Major", "Minor", "Sus4", "Maj7", "Min7", "Power" }; return a; }
    const juce::StringArray& fxFilterTypes() { static juce::StringArray a { "Low Pass", "High Pass", "Band Pass", "Notch" }; return a; }
    const juce::StringArray& delayModes()  { static juce::StringArray a { "Digital", "Tape Echo", "Ping-Pong" }; return a; }
    const juce::StringArray& reverbTypes() { static juce::StringArray a { "Room", "Hall", "Shimmer" }; return a; }
    const juce::StringArray& humFreqs()    { static juce::StringArray a { "50 Hz", "60 Hz" }; return a; }
    const juce::StringArray& arpModes()    { static juce::StringArray a { "Up", "Down", "Up-Down", "Down-Up", "Random", "As Played", "Chord", "Sequencer" }; return a; }
    const juce::StringArray& noteNames()   { static juce::StringArray a { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return a; }

    struct ScaleDef { const char* name; std::vector<int> steps; };
    static const std::vector<ScaleDef>& scaleDefs()
    {
        static const std::vector<ScaleDef> s {
            { "Chromatic",        { 0,1,2,3,4,5,6,7,8,9,10,11 } },
            { "Major",            { 0,2,4,5,7,9,11 } },
            { "Minor",            { 0,2,3,5,7,8,10 } },
            { "Harmonic Minor",   { 0,2,3,5,7,8,11 } },
            { "Melodic Minor",    { 0,2,3,5,7,9,11 } },
            { "Dorian",           { 0,2,3,5,7,9,10 } },
            { "Phrygian",         { 0,1,3,5,7,8,10 } },
            { "Lydian",           { 0,2,4,6,7,9,11 } },
            { "Mixolydian",       { 0,2,4,5,7,9,10 } },
            { "Locrian",          { 0,1,3,5,6,8,10 } },
            { "Major Pentatonic", { 0,2,4,7,9 } },
            { "Minor Pentatonic", { 0,3,5,7,10 } },
            { "Blues",            { 0,3,5,6,7,10 } },
            { "Whole Tone",       { 0,2,4,6,8,10 } },
            { "Diminished",       { 0,2,3,5,6,8,9,11 } },
            { "Phrygian Dominant",{ 0,1,4,5,7,8,10 } },
            { "Hungarian Minor",  { 0,2,3,6,7,8,11 } },
            { "Arabic",           { 0,1,4,5,7,8,11 } },
            { "Hirajoshi",        { 0,2,3,7,8 } },
            { "In Sen",           { 0,1,5,7,10 } },
        };
        return s;
    }

    const juce::StringArray& scales()
    {
        static juce::StringArray a = [] { juce::StringArray r; for (auto& d : scaleDefs()) r.add (d.name); return r; }();
        return a;
    }

    const juce::StringArray& motionShapes()
    {
        static juce::StringArray a { "Pump", "Gate", "Trance Gate", "Filter Sweep", "Wobble", "Auto-Pan", "Stutter",
                                     "Random Steps", "Swell" };
        return a;
    }

    static const char* kFxDestIds[FxDestCount] = { "fx_dist_drive", "fx_dist_mix", "fx_res_mix", "fx_flt_cutoff", "fx_flt_res",
        "fx_cho_mix", "fx_fla_mix", "fx_pha_mix", "fx_tape_wow", "fx_dly_mix", "fx_dly_fb", "fx_rev_mix", "fx_rev_size" };

    const char* fxDestParamId (int d) { return kFxDestIds[juce::jlimit (0, FxDestCount - 1, d)]; }

    const juce::StringArray& focusDestinations()
    {
        static juce::StringArray a = []
        {
            auto r = modDestinations();
            for (auto* n : { "FX Distortion Drive", "FX Distortion Mix", "FX Resonator Mix", "FX Filter Cutoff", "FX Filter Reso",
                             "FX Chorus Mix", "FX Flanger Mix", "FX Phaser Mix", "FX Tape Wow", "FX Delay Mix", "FX Delay Feedback",
                             "FX Reverb Mix", "FX Reverb Size" })
                r.add (n);
            return r;
        }();
        return a;
    }

    const std::vector<int>& scaleSteps (int index)
    {
        const auto& d = scaleDefs();
        return d[(size_t) juce::jlimit (0, (int) d.size() - 1, index)].steps;
    }
}

namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        return { lo, hi,
                 [] (float s, float e, float n) { return s * std::pow (e / s, n); },
                 [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
                 [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
    }

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

void addPhase3Parameters (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::NormalisableRange<float> unit (0.0f, 1.0f);

    // ---- FX: Distortion
    B (l, "fx_dist_on", "Distortion On", false);
    C (l, "fx_dist_type", "Distortion Type", choices::distTypes(), 0);
    F (l, "fx_dist_drive", "Distortion Drive", unit, 0.35f);
    F (l, "fx_dist_tone", "Distortion Tone", unit, 0.7f);
    F (l, "fx_dist_out", "Distortion Output", { -18.0f, 12.0f }, 0.0f, "dB");
    F (l, "fx_dist_mix", "Distortion Mix", unit, 1.0f);

    // ---- FX: Resonator
    B (l, "fx_res_on", "Resonator On", false);
    C (l, "fx_res_chord", "Resonator Chord", choices::chords(), 3);
    I (l, "fx_res_note", "Resonator Note", 24, 96, 48);
    F (l, "fx_res_decay", "Resonator Decay", unit, 0.6f);
    F (l, "fx_res_bright", "Resonator Brightness", unit, 0.6f);
    F (l, "fx_res_mix", "Resonator Mix", unit, 0.35f);

    // ---- FX: Filter
    B (l, "fx_flt_on", "FX Filter On", false);
    C (l, "fx_flt_type", "FX Filter Type", choices::fxFilterTypes(), 0);
    F (l, "fx_flt_cutoff", "FX Filter Cutoff", logRange (20.0f, 20000.0f), 4000.0f, "Hz");
    F (l, "fx_flt_res", "FX Filter Resonance", unit, 0.2f);
    F (l, "fx_flt_drive", "FX Filter Drive", unit, 0.0f);
    F (l, "fx_flt_mix", "FX Filter Mix", unit, 1.0f);

    // ---- FX: Chorus
    B (l, "fx_cho_on", "Chorus On", false);
    F (l, "fx_cho_rate", "Chorus Rate", logRange (0.05f, 5.0f), 0.6f, "Hz");
    F (l, "fx_cho_depth", "Chorus Depth", unit, 0.5f);
    F (l, "fx_cho_delay", "Chorus Delay", { 3.0f, 30.0f }, 12.0f, "ms");
    F (l, "fx_cho_fb", "Chorus Feedback", unit, 0.1f);
    F (l, "fx_cho_width", "Chorus Width", unit, 1.0f);
    F (l, "fx_cho_mix", "Chorus Mix", unit, 0.5f);

    // ---- FX: Flanger
    B (l, "fx_fla_on", "Flanger On", false);
    F (l, "fx_fla_rate", "Flanger Rate", logRange (0.02f, 5.0f), 0.25f, "Hz");
    F (l, "fx_fla_depth", "Flanger Depth", unit, 0.7f);
    F (l, "fx_fla_fb", "Flanger Feedback", { -0.95f, 0.95f }, 0.5f);
    F (l, "fx_fla_mix", "Flanger Mix", unit, 0.5f);

    // ---- FX: Phaser
    B (l, "fx_pha_on", "Phaser On", false);
    F (l, "fx_pha_rate", "Phaser Rate", logRange (0.02f, 5.0f), 0.3f, "Hz");
    F (l, "fx_pha_depth", "Phaser Depth", unit, 0.7f);
    F (l, "fx_pha_freq", "Phaser Frequency", logRange (100.0f, 8000.0f), 800.0f, "Hz");
    F (l, "fx_pha_fb", "Phaser Feedback", { -0.95f, 0.95f }, 0.5f);
    I (l, "fx_pha_stages", "Phaser Stages", 2, 12, 6);
    F (l, "fx_pha_mix", "Phaser Mix", unit, 0.5f);

    // ---- FX: Tape (wow & flutter)
    B (l, "fx_tape_on", "Tape On", false);
    F (l, "fx_tape_wow", "Tape Wow", unit, 0.35f);
    F (l, "fx_tape_flutter", "Tape Flutter", unit, 0.25f);
    F (l, "fx_tape_sat", "Tape Saturation", unit, 0.3f);
    F (l, "fx_tape_tone", "Tape Tone", unit, 0.7f);
    F (l, "fx_tape_mix", "Tape Mix", unit, 1.0f);

    // ---- FX: Delay
    B (l, "fx_dly_on", "Delay On", false);
    C (l, "fx_dly_mode", "Delay Mode", choices::delayModes(), 0);
    B (l, "fx_dly_sync", "Delay Sync", true);
    C (l, "fx_dly_div", "Delay Sync Time", choices::syncDivisions(), 11); // 1/8 D
    F (l, "fx_dly_time", "Delay Time", skewed (1.0f, 2000.0f, 300.0f), 375.0f, "ms");
    F (l, "fx_dly_fb", "Delay Feedback", { 0.0f, 1.05f }, 0.4f);
    F (l, "fx_dly_lowcut", "Delay Low Cut", logRange (20.0f, 2000.0f), 120.0f, "Hz");
    F (l, "fx_dly_highcut", "Delay High Cut", logRange (1000.0f, 20000.0f), 9000.0f, "Hz");
    F (l, "fx_dly_width", "Delay Width", unit, 0.6f);
    F (l, "fx_dly_mix", "Delay Mix", unit, 0.3f);

    // ---- FX: Reverb
    B (l, "fx_rev_on", "Reverb On", false);
    C (l, "fx_rev_type", "Reverb Type", choices::reverbTypes(), 1);
    F (l, "fx_rev_size", "Reverb Size", unit, 0.6f);
    F (l, "fx_rev_decay", "Reverb Decay", skewed (0.2f, 20.0f, 2.5f), 2.5f, "s");
    F (l, "fx_rev_pre", "Reverb Pre-Delay", { 0.0f, 250.0f }, 10.0f, "ms");
    F (l, "fx_rev_damp", "Reverb Damping", unit, 0.5f);
    F (l, "fx_rev_width", "Reverb Width", unit, 1.0f);
    F (l, "fx_rev_shim", "Reverb Shimmer", unit, 0.5f);
    F (l, "fx_rev_mix", "Reverb Mix", unit, 0.25f);

    // ---- FX: EQ
    B (l, "fx_eq_on", "EQ On", false);
    F (l, "fx_eq_lowf", "EQ Low Freq", logRange (20.0f, 500.0f), 100.0f, "Hz");
    F (l, "fx_eq_lowg", "EQ Low Gain", { -18.0f, 18.0f }, 0.0f, "dB");
    F (l, "fx_eq_midf", "EQ Mid Freq", logRange (100.0f, 10000.0f), 1000.0f, "Hz");
    F (l, "fx_eq_midg", "EQ Mid Gain", { -18.0f, 18.0f }, 0.0f, "dB");
    F (l, "fx_eq_midq", "EQ Mid Q", logRange (0.3f, 8.0f), 1.0f);
    F (l, "fx_eq_highf", "EQ High Freq", logRange (1000.0f, 20000.0f), 6000.0f, "Hz");
    F (l, "fx_eq_highg", "EQ High Gain", { -18.0f, 18.0f }, 0.0f, "dB");

    // ---- FX: Compressor
    B (l, "fx_comp_on", "Compressor On", false);
    F (l, "fx_comp_thresh", "Compressor Threshold", { -60.0f, 0.0f }, -18.0f, "dB");
    F (l, "fx_comp_ratio", "Compressor Ratio", skewed (1.0f, 20.0f, 4.0f), 4.0f, ":1");
    F (l, "fx_comp_att", "Compressor Attack", skewed (0.1f, 100.0f, 10.0f), 5.0f, "ms");
    F (l, "fx_comp_rel", "Compressor Release", skewed (10.0f, 1000.0f, 150.0f), 120.0f, "ms");
    F (l, "fx_comp_makeup", "Compressor Makeup", { 0.0f, 24.0f }, 3.0f, "dB");
    F (l, "fx_comp_mix", "Compressor Mix", unit, 1.0f);

    // ---- ANALOG (all at 0 = clean digital)
    F (l, "an_age", "Analog Age", unit, 0.0f);
    F (l, "an_tolerance", "Component Tolerance", unit, 0.0f);
    F (l, "an_circuit", "Circuit Saturation", unit, 0.0f);
    F (l, "an_noise", "Noise Floor", unit, 0.0f);
    F (l, "an_hum", "Hum", unit, 0.0f);
    C (l, "an_humfreq", "Hum Frequency", choices::humFreqs(), 0);
    F (l, "an_bits", "Vintage Bits", unit, 0.0f);
    F (l, "an_rate", "Vintage Rate", unit, 0.0f);

    // ---- ARP
    B (l, "arp_on", "Arp On", false);
    C (l, "arp_mode", "Arp Mode", choices::arpModes(), 0);
    C (l, "arp_rate", "Arp Rate", choices::syncDivisions(), 13);   // 1/16
    I (l, "arp_oct", "Arp Octaves", 1, 4, 1);
    F (l, "arp_gate", "Arp Gate", { 0.05f, 1.0f }, 0.6f);
    F (l, "arp_swing", "Arp Swing", unit, 0.0f);
    B (l, "arp_latch", "Arp Latch", false);
    I (l, "arp_steps", "Arp Steps", 1, 32, 16);
    C (l, "arp_scale", "Arp Scale", choices::scales(), 0);
    C (l, "arp_key", "Arp Key", choices::noteNames(), 0);
    F (l, "arp_velamt", "Arp Step Velocity", unit, 1.0f);
}

} // namespace pyra

namespace pyra
{
void addPhase5Parameters (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::NormalisableRange<float> unit (0.0f, 1.0f);
    F (l, "chaos_amt", "Chaos", unit, 0.0f);
    F (l, "chaos_speed", "Chaos Speed", logRange (0.05f, 12.0f), 0.6f, "Hz");
    F (l, "chaos_bite", "Chaos Bite", unit, 0.5f);
    F (l, "chaos_space", "Chaos Space", unit, 0.5f);
    I (l, "chaos_seed", "Chaos Seed", 1, 9999, 1);

    F (l, "focus_amt", "Focus", unit, 0.0f);
    // default mapping: one knob = "more intensity" for every preset
    const int defDst[kNumFocusSlots]    = { dFilt (0, FCutoff), dOsc (0, OPos), dFilt (0, FDrive), DCount + FxRevMix, 0, 0, 0, 0 };
    const float defDepth[kNumFocusSlots] = { 0.45f, 0.3f, 0.35f, 0.25f, 0, 0, 0, 0 };
    for (int k = 0; k < kNumFocusSlots; ++k)
    {
        const auto n = "focus" + juce::String (k + 1) + "_";
        const auto name = "Focus " + juce::String (k + 1) + " ";
        C (l, n + "dst", name + "Destination", choices::focusDestinations(), defDst[k]);
        F (l, n + "depth", name + "Depth", { -1.0f, 1.0f }, defDepth[k]);
        F (l, n + "curve", name + "Curve", { -1.0f, 1.0f }, 0.0f);
    }

    F (l, "motion_amt", "Motion", unit, 0.0f);
    C (l, "motion_shape", "Motion Shape", choices::motionShapes(), 0);
    C (l, "motion_rate", "Motion Rate", choices::syncDivisions(), 7);
    F (l, "motion_smooth", "Motion Smooth", unit, 0.2f);
}
}

namespace pyra
{
void addSamplerParameters (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    B (l, "smp_on", "Sample On", false);
    C (l, "smp_inst", "Sample Instrument", SampleBank::displayNames(), 0);
    F (l, "smp_level", "Sample Level", { 0.0f, 1.0f }, 0.8f);
    F (l, "smp_pan", "Sample Pan", { -1.0f, 1.0f }, 0.0f);
    I (l, "smp_oct", "Sample Octave", -3, 3, 0);
    I (l, "smp_semi", "Sample Semi", -12, 12, 0);
    F (l, "smp_fine", "Sample Fine", { -100.0f, 100.0f }, 0.0f, "ct");
    F (l, "smp_start", "Sample Start", { 0.0f, 1.0f }, 0.0f);
    F (l, "smp_vel", "Sample Velocity Layers", { 0.0f, 1.0f }, 1.0f);
    C (l, "smp_route", "Sample Output", choices::routes(), 0);
}
}
