#include "Params.h"
#include "dsp/Wavetables.h"

namespace pyra
{
//==============================================================================
namespace pid
{
    juce::String osc (int i, const char* p)   { return "osc" + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + "_" + p; }
    juce::String filt (int i, const char* p)  { return "f" + juce::String (i + 1) + "_" + p; }
    juce::String env (int i, const char* p)   { return "env" + juce::String (i + 1) + "_" + p; }
    juce::String lfo (int i, const char* p)   { return "lfo" + juce::String (i + 1) + "_" + p; }
    juce::String mod (int i, const char* p)   { return "mod" + juce::String (i + 1) + "_" + p; }
    juce::String macro (int i)                { return "macro" + juce::String (i + 1); }
}

//==============================================================================
namespace choices
{
    const juce::StringArray& voiceModes()    { static juce::StringArray a { "Poly", "Mono", "Legato" }; return a; }
    const juce::StringArray& warpModes()     { static juce::StringArray a { "Off", "Sync", "Bend +", "Bend -", "PWM", "Asym", "Mirror", "Fold", "Quantize" }; return a; }
    const juce::StringArray& fmSources()     { static juce::StringArray a { "Off", "Osc A", "Osc B", "Osc C", "Sub", "Noise" }; return a; }
    const juce::StringArray& routes()        { static juce::StringArray a { "Filter 1", "Filter 2", "Filter 1+2", "Direct" }; return a; }
    const juce::StringArray& subShapes()
    {
        static juce::StringArray a { "Sine", "Triangle", "Saw", "Square",
                                     // 0.8 (append only)
                                     "Pulse 25%", "Pulse 10%", "Rounded Square", "Half Sine", "Full-wave Sine", "Sine + Octave",
                                     "Sine + Twelfth", "Organ Sub", "Driven Sine", "808 Sub", "Warm Saw", "Soft Triangle" };
        return a;
    }
    const juce::StringArray& noiseTypes()
    {
        static juce::StringArray a { "White", "Pink", "Brown", "Crackle", "Digital",
                                     // 0.8 (append only)
                                     "Blue", "Violet", "Vinyl", "Tape Hiss", "Breath", "Wind", "Ocean", "Rain", "Fire", "Bit-crushed",
                                     "Static", "Geiger" };
        return a;
    }
    const juce::StringArray& filterTypes()
    {
        static juce::StringArray a { "LP 12", "LP 24", "LP Ladder", "HP 12", "HP 24", "BP 12", "BP 24", "Notch", "Peak", "All Pass", "Comb +",
                                     "Comb -", "Formant", "Phaser",
                                     // 0.8 (append only)
                                     "LP 6", "HP 6", "LP 36", "HP 36", "Notch 24", "Low Shelf", "High Shelf", "Diode LP", "MS-20 LP",
                                     "Dual BP", "Resonator" };
        return a;
    }
    const juce::StringArray& filterRouting() { static juce::StringArray a { "Serial", "Parallel" }; return a; }
    const juce::StringArray& lfoShapes()
    {
        static juce::StringArray a { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Pulse", "Stairs", "Exp Up", "Exp Down", "S&H",
                                     "Smooth Random",
                                     // 0.8 (append only)
                                     "Half Sine", "Pulse 12%", "Log Up", "Log Down", "S-Curve Up", "S-Curve Down", "Swell", "Pluck",
                                     "Stairs 8", "Stairs Up-Down", "Gate 1/2", "Gate 1/3", "Gate 1/4", "Dotted Gate", "Trance Gate",
                                     "Stutter", "Bounce", "Heartbeat", "Wobble 2x", "Wobble 3x", "Wobble 4x", "Sine + Harmonic",
                                     "Shark Fin", "Zigzag", "Elastic", "Sidechain Pump", "Triangle Skew", "Custom (drawn)" };
        return a;
    }
    const juce::StringArray& lfoModes()      { static juce::StringArray a { "Retrig", "Free", "Envelope" }; return a; }

    static const std::pair<const char*, double> kDivs[] = {
        { "8 Bars", 32.0 }, { "4 Bars", 16.0 }, { "2 Bars", 8.0 }, { "1 Bar", 4.0 },
        { "1/2", 2.0 }, { "1/2 D", 3.0 }, { "1/2 T", 4.0 / 3.0 },
        { "1/4", 1.0 }, { "1/4 D", 1.5 }, { "1/4 T", 2.0 / 3.0 },
        { "1/8", 0.5 }, { "1/8 D", 0.75 }, { "1/8 T", 1.0 / 3.0 },
        { "1/16", 0.25 }, { "1/16 D", 0.375 }, { "1/16 T", 1.0 / 6.0 },
        { "1/32", 0.125 }, { "1/32 T", 1.0 / 12.0 }, { "1/64", 0.0625 } };

    const juce::StringArray& syncDivisions()
    {
        static juce::StringArray a = [] { juce::StringArray r; for (auto& d : kDivs) r.add (d.first); return r; }();
        return a;
    }

    double syncDivisionBeats (int index)
    {
        return kDivs[juce::jlimit (0, (int) std::size (kDivs) - 1, index)].second;
    }

    const juce::StringArray& modSources()
    {
        static juce::StringArray a { "None", "Env 1", "Env 2", "Env 3", "Env 4",
                                     "LFO 1", "LFO 2", "LFO 3", "LFO 4",
                                     "Velocity", "Note", "Random", "Mod Wheel", "Pressure", "Pitch Bend", "Slide",
                                     "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Chaos", "Focus", "Motion" };
        return a;
    }

    const juce::StringArray& modDestinations()
    {
        static juce::StringArray a = []
        {
            juce::StringArray r;
            r.add ("None");
            const char* of[] = { "Level", "Pan", "Pitch", "Fine", "WT Pos", "Detune", "Blend", "Width", "Warp", "FM" };
            for (int o = 0; o < kNumOsc; ++o)
                for (auto* f : of)
                    r.add ("Osc " + juce::String::charToString ((juce::juce_wchar) ('A' + o)) + " " + f);
            r.add ("Sub Level"); r.add ("Noise Level"); r.add ("Noise Color");
            const char* ff[] = { "Cutoff", "Reso", "Drive", "Mix", "Env Amt" };
            for (int f = 0; f < kNumFilters; ++f)
                for (auto* n : ff)
                    r.add ("Filter " + juce::String (f + 1) + " " + n);
            for (int l = 0; l < kNumLfo; ++l)
                r.add ("LFO " + juce::String (l + 1) + " Rate");
            r.add ("Amp");
            jassert (r.size() == DCount);
            return r;
        }();
        return a;
    }
}

//==============================================================================
juce::String destParamId (int d)
{
    if (d >= DOscBase && d < DSubLevel)
    {
        const char* of[] = { "level", "pan", "semi", "fine", "pos", "detune", "blend", "width", "warpAmt", "fmAmt" };
        const int o = (d - DOscBase) / OCount, f = (d - DOscBase) % OCount;
        return pid::osc (o, of[f]);
    }
    if (d == DSubLevel)   return "sub_level";
    if (d == DNoiseLevel) return "noise_level";
    if (d == DNoiseColor) return "noise_color";
    if (d >= DFiltBase && d < DLfoRateBase)
    {
        const char* ff[] = { "cutoff", "res", "drive", "mix", "env" };
        const int f = (d - DFiltBase) / FCount, field = (d - DFiltBase) % FCount;
        return pid::filt (f, ff[field]);
    }
    if (d >= DLfoRateBase && d < DAmp)
        return pid::lfo (d - DLfoRateBase, "rate");   // shown on the LFO rate knob
    return {};
}

//==============================================================================
namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    juce::NormalisableRange<float> timeRange (float maxSec, float centre)
    {
        juce::NormalisableRange<float> r (0.0f, maxSec, 0.0f);
        r.setSkewForCentre (centre);
        return r;
    }

    juce::NormalisableRange<float> freqRange (float lo, float hi)
    {
        return { lo, hi,
                 [] (float s, float e, float n) { return s * std::pow (e / s, n); },
                 [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
                 [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
    }

    void addFloat (Layout& l, const juce::String& id, const juce::String& name, juce::NormalisableRange<float> r, float def,
                   const juce::String& label = {})
    {
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                            juce::AudioParameterFloatAttributes().withLabel (label)));
    }

    void addInt (Layout& l, const juce::String& id, const juce::String& name, int lo, int hi, int def)
    {
        l.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, lo, hi, def));
    }

    void addChoice (Layout& l, const juce::String& id, const juce::String& name, const juce::StringArray& c, int def)
    {
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, c, def));
    }

    void addBool (Layout& l, const juce::String& id, const juce::String& name, bool def)
    {
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    Layout l;

    // ---- Global
    {
        juce::NormalisableRange<float> r (-48.0f, 24.0f);
        r.setSkewForCentre (0.0f);
        addFloat (l, "master", "Master Volume", r, -4.0f, "dB");
    }
    addInt   (l, "polyphony", "Polyphony", 1, kMaxVoices, 16);
    addChoice(l, "voiceMode", "Voice Mode", choices::voiceModes(), 0);
    addFloat (l, "glide", "Glide", timeRange (5.0f, 0.3f), 0.0f, "s");
    addBool  (l, "glideLegato", "Glide Legato Only", false);
    addInt   (l, "bendUp", "Bend Up", 0, 48, 2);
    addInt   (l, "bendDown", "Bend Down", 0, 48, 2);
    addBool  (l, "mpe", "MPE", false);
    addInt   (l, "mpeRange", "MPE Bend Range", 1, 96, 48);
    addFloat (l, "velAmp", "Velocity > Amp", { 0.0f, 1.0f }, 0.5f);
    addInt   (l, "transpose", "Transpose", -24, 24, 0);
    addFloat (l, "tune", "Master Tune", { -100.0f, 100.0f }, 0.0f, "ct");
    addFloat (l, "drift", "Analog Drift", { 0.0f, 1.0f }, 0.0f);
    addFloat (l, "spread", "Voice Spread", { 0.0f, 1.0f }, 0.0f);
    addChoice(l, "filterRouting", "Filter Routing", choices::filterRouting(), 0);
    for (int m = 0; m < kNumMacros; ++m)
        addFloat (l, pid::macro (m), "Macro " + juce::String (m + 1), { 0.0f, 1.0f }, 0.0f);

    // ---- Oscillators
    for (int o = 0; o < kNumOsc; ++o)
    {
        const auto n = "Osc " + juce::String::charToString ((juce::juce_wchar) ('A' + o)) + " ";
        addBool  (l, pid::osc (o, "on"), n + "On", o == 0);
        addChoice(l, pid::osc (o, "wt"), n + "Wavetable", Wavetables::names(), 0);
        addFloat (l, pid::osc (o, "pos"), n + "WT Position", { 0.0f, 1.0f }, o == 0 ? 2.0f / 3.0f : 0.0f);
        addFloat (l, pid::osc (o, "level"), n + "Level", { 0.0f, 1.0f }, 0.75f);
        addFloat (l, pid::osc (o, "pan"), n + "Pan", { -1.0f, 1.0f }, 0.0f);
        addInt   (l, pid::osc (o, "oct"), n + "Octave", -4, 4, 0);
        addFloat (l, pid::osc (o, "semi"), n + "Semi", { -24.0f, 24.0f, 1.0f }, 0.0f, "st");
        addFloat (l, pid::osc (o, "fine"), n + "Fine", { -100.0f, 100.0f }, 0.0f, "ct");
        addInt   (l, pid::osc (o, "voices"), n + "Unison", 1, kMaxUnison, 1);
        addFloat (l, pid::osc (o, "detune"), n + "Detune", { 0.0f, 1.0f }, 0.25f);
        addFloat (l, pid::osc (o, "blend"), n + "Blend", { 0.0f, 1.0f }, 0.75f);
        addFloat (l, pid::osc (o, "width"), n + "Width", { 0.0f, 1.0f }, 1.0f);
        addFloat (l, pid::osc (o, "phase"), n + "Phase", { 0.0f, 1.0f }, 0.0f);
        addFloat (l, pid::osc (o, "rand"), n + "Random Phase", { 0.0f, 1.0f }, 1.0f);
        addChoice(l, pid::osc (o, "warp"), n + "Warp Mode", choices::warpModes(), 0);
        addFloat (l, pid::osc (o, "warpAmt"), n + "Warp", { 0.0f, 1.0f }, 0.0f);
        addChoice(l, pid::osc (o, "fmSrc"), n + "FM Source", choices::fmSources(), 0);
        addFloat (l, pid::osc (o, "fmAmt"), n + "FM Amount", { 0.0f, 1.0f }, 0.0f);
        addChoice(l, pid::osc (o, "route"), n + "Output", choices::routes(), 0);
    }

    // ---- Sub + Noise
    addBool  (l, "sub_on", "Sub On", false);
    addChoice(l, "sub_shape", "Sub Shape", choices::subShapes(), 0);
    addInt   (l, "sub_oct", "Sub Octave", -3, 0, -1);
    addFloat (l, "sub_level", "Sub Level", { 0.0f, 1.0f }, 0.6f);
    addFloat (l, "sub_pan", "Sub Pan", { -1.0f, 1.0f }, 0.0f);
    addChoice(l, "sub_route", "Sub Output", choices::routes(), 3);

    addBool  (l, "noise_on", "Noise On", false);
    addChoice(l, "noise_type", "Noise Type", choices::noiseTypes(), 0);
    addFloat (l, "noise_level", "Noise Level", { 0.0f, 1.0f }, 0.3f);
    addFloat (l, "noise_pan", "Noise Pan", { -1.0f, 1.0f }, 0.0f);
    addFloat (l, "noise_color", "Noise Color", { -1.0f, 1.0f }, 0.0f);
    addChoice(l, "noise_route", "Noise Output", choices::routes(), 0);

    // ---- Filters
    for (int f = 0; f < kNumFilters; ++f)
    {
        const auto n = "Filter " + juce::String (f + 1) + " ";
        addBool  (l, pid::filt (f, "on"), n + "On", f == 0);
        addChoice(l, pid::filt (f, "type"), n + "Type", choices::filterTypes(), f == 0 ? FLp24 : FHp12);
        addFloat (l, pid::filt (f, "cutoff"), n + "Cutoff", freqRange (20.0f, 20000.0f), f == 0 ? 20000.0f : 20.0f, "Hz");
        addFloat (l, pid::filt (f, "res"), n + "Resonance", { 0.0f, 1.0f }, 0.1f);
        addFloat (l, pid::filt (f, "drive"), n + "Drive", { 0.0f, 1.0f }, 0.0f);
        addFloat (l, pid::filt (f, "mix"), n + "Mix", { 0.0f, 1.0f }, 1.0f);
        addFloat (l, pid::filt (f, "key"), n + "Key Track", { 0.0f, 1.0f }, 0.0f);
        addFloat (l, pid::filt (f, "env"), n + "Env 2 Amount", { -1.0f, 1.0f }, 0.0f);
    }

    // ---- Envelopes
    for (int e = 0; e < kNumEnv; ++e)
    {
        const auto n = "Env " + juce::String (e + 1) + " ";
        const bool amp = e == 0;
        addFloat (l, pid::env (e, "dly"),  n + "Delay",   timeRange (5.0f, 0.5f), 0.0f, "s");
        addFloat (l, pid::env (e, "att"),  n + "Attack",  timeRange (10.0f, 0.5f), amp ? 0.002f : 0.0f, "s");
        addFloat (l, pid::env (e, "hold"), n + "Hold",    timeRange (5.0f, 0.5f), 0.0f, "s");
        addFloat (l, pid::env (e, "dec"),  n + "Decay",   timeRange (20.0f, 1.0f), amp ? 1.0f : 0.6f, "s");
        addFloat (l, pid::env (e, "sus"),  n + "Sustain", { 0.0f, 1.0f }, amp ? 1.0f : 0.0f);
        addFloat (l, pid::env (e, "rel"),  n + "Release", timeRange (20.0f, 1.0f), amp ? 0.15f : 0.3f, "s");
        addFloat (l, pid::env (e, "aCrv"), n + "Attack Curve",  { -1.0f, 1.0f }, 0.3f);
        addFloat (l, pid::env (e, "dCrv"), n + "Decay Curve",   { -1.0f, 1.0f }, 0.6f);
        addFloat (l, pid::env (e, "rCrv"), n + "Release Curve", { -1.0f, 1.0f }, 0.6f);
    }

    // ---- LFOs
    for (int i = 0; i < kNumLfo; ++i)
    {
        const auto n = "LFO " + juce::String (i + 1) + " ";
        addChoice(l, pid::lfo (i, "shape"), n + "Shape", choices::lfoShapes(), 0);
        addFloat (l, pid::lfo (i, "rate"), n + "Rate", freqRange (0.01f, 40.0f), 2.0f, "Hz");
        addBool  (l, pid::lfo (i, "sync"), n + "Sync", true);
        addChoice(l, pid::lfo (i, "div"), n + "Sync Rate", choices::syncDivisions(), 7);
        addChoice(l, pid::lfo (i, "mode"), n + "Mode", choices::lfoModes(), 0);
        addFloat (l, pid::lfo (i, "phase"), n + "Phase", { 0.0f, 1.0f }, 0.0f);
        addFloat (l, pid::lfo (i, "delay"), n + "Delay", timeRange (5.0f, 0.5f), 0.0f, "s");
        addFloat (l, pid::lfo (i, "fade"), n + "Fade In", timeRange (5.0f, 0.5f), 0.0f, "s");
        addBool  (l, pid::lfo (i, "uni"), n + "Unipolar", false);
        addFloat (l, pid::lfo (i, "smooth"), n + "Smooth", { 0.0f, 1.0f }, 0.0f);
    }

    // ---- Modulation matrix
    for (int s = 0; s < kNumModSlots; ++s)
    {
        const auto n = "Mod " + juce::String (s + 1) + " ";
        addChoice(l, pid::mod (s, "src"), n + "Source", choices::modSources(), 0);
        addChoice(l, pid::mod (s, "dst"), n + "Destination", choices::modDestinations(), 0);
        addFloat (l, pid::mod (s, "amt"), n + "Amount", { -1.0f, 1.0f }, 0.0f);
    }

    addPhase3Parameters (l);
    addPhase5Parameters (l);
    addSamplerParameters (l);
    addPhase6Parameters (l);
    addPhase7Parameters (l);
    return l;
}

//==============================================================================
void ParamRefs::init (juce::AudioProcessorValueTreeState& st)
{
    auto p = [&st] (const juce::String& id)
    {
        auto* v = st.getRawParameterValue (id);
        jassert (v != nullptr);
        return v;
    };

    master = p ("master"); polyphony = p ("polyphony"); voiceMode = p ("voiceMode");
    glide = p ("glide"); glideLegato = p ("glideLegato"); glideType = p ("glide_type"); mvMangle = p ("chaos_mangle");
    bendUp = p ("bendUp"); bendDown = p ("bendDown"); mpe = p ("mpe"); mpeRange = p ("mpeRange");
    velAmp = p ("velAmp"); transpose = p ("transpose"); tune = p ("tune");
    drift = p ("drift"); spread = p ("spread"); filterRouting = p ("filterRouting");
    age = p ("an_age"); tolerance = p ("an_tolerance"); circuit = p ("an_circuit");
    mvChaos = p ("chaos_amt"); mvSpeed = p ("chaos_speed"); mvBite = p ("chaos_bite"); mvSpace = p ("chaos_space");
    mvSeed = p ("chaos_seed"); mvFocus = p ("focus_amt"); mvMotion = p ("motion_amt"); mvShape = p ("motion_shape");
    mvRate = p ("motion_rate"); mvSmooth = p ("motion_smooth");
    for (int k = 0; k < kNumFocusSlots; ++k)
    {
        const auto n = "focus" + juce::String (k + 1) + "_";
        focusRefs[k] = { p (n + "dst"), p (n + "depth"), p (n + "curve") };
    }
    smpOn = p ("smp_on"); smpInst = p ("smp_inst"); smpLevel = p ("smp_level"); smpPan = p ("smp_pan"); smpOct = p ("smp_oct");
    smpSemi = p ("smp_semi"); smpFine = p ("smp_fine"); smpStart = p ("smp_start"); smpVel = p ("smp_vel"); smpRoute = p ("smp_route");
    anNoise = p ("an_noise"); anHum = p ("an_hum"); anHumFreq = p ("an_humfreq"); anBits = p ("an_bits"); anRate = p ("an_rate");
    for (int m = 0; m < kNumMacros; ++m) macros[m] = p (pid::macro (m));

    subOn = p ("sub_on"); subShape = p ("sub_shape"); subOct = p ("sub_oct"); subRoute = p ("sub_route"); subPan = p ("sub_pan");
    noiseOn = p ("noise_on"); noiseType = p ("noise_type"); noiseRoute = p ("noise_route"); noisePan = p ("noise_pan");

    for (int o = 0; o < kNumOsc; ++o)
        osc[o] = { p (pid::osc (o, "on")), p (pid::osc (o, "wt")), p (pid::osc (o, "oct")), p (pid::osc (o, "voices")),
                   p (pid::osc (o, "phase")), p (pid::osc (o, "rand")), p (pid::osc (o, "warp")),
                   p (pid::osc (o, "fmSrc")), p (pid::osc (o, "route")) };

    for (int f = 0; f < kNumFilters; ++f)
        filt[f] = { p (pid::filt (f, "on")), p (pid::filt (f, "type")), p (pid::filt (f, "key")) };

    for (int e = 0; e < kNumEnv; ++e)
        env[e] = { p (pid::env (e, "dly")), p (pid::env (e, "att")), p (pid::env (e, "hold")), p (pid::env (e, "dec")),
                   p (pid::env (e, "sus")), p (pid::env (e, "rel")), p (pid::env (e, "aCrv")), p (pid::env (e, "dCrv")),
                   p (pid::env (e, "rCrv")) };

    for (int i = 0; i < kNumLfo; ++i)
        lfo[i] = { p (pid::lfo (i, "shape")), p (pid::lfo (i, "rate")), p (pid::lfo (i, "sync")), p (pid::lfo (i, "div")),
                   p (pid::lfo (i, "mode")), p (pid::lfo (i, "phase")), p (pid::lfo (i, "delay")), p (pid::lfo (i, "fade")),
                   p (pid::lfo (i, "uni")), p (pid::lfo (i, "smooth")) };

    for (int s = 0; s < kNumModSlots; ++s)
        slot[s] = { p (pid::mod (s, "src")), p (pid::mod (s, "dst")), p (pid::mod (s, "amt")) };

    for (int d = 1; d < DCount; ++d)
    {
        auto& c = conv[d];
        if (d >= DLfoRateBase && d < DAmp) { c.kind = DestConv::LfoRate; continue; }
        if (d == DAmp)                     { c.kind = DestConv::Amp; continue; }

        auto id = destParamId (d);
        destRaw[d] = p (id);
        destParam[d] = st.getParameter (id);
        c.range = &destParam[d]->getNormalisableRange();
        // stepped linear ranges (e.g. semitones) must modulate smoothly, not in steps
        if (c.range->interval > 0.0f && c.range->skew == 1.0f)
        {
            c.kind = DestConv::Linear;
            c.start = c.range->start;
            c.end = c.range->end;
        }
    }
}

void ParamRefs::fill (Settings& s) const
{
    auto b = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed) > 0.5f; };
    auto i = [] (const std::atomic<float>* a) { return (int) std::lround (a->load (std::memory_order_relaxed)); };
    auto f = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };

    s.masterGain = juce::Decibels::decibelsToGain (f (master), -48.0f);
    s.polyphony = juce::jlimit (1, kMaxVoices, i (polyphony));
    s.voiceMode = i (voiceMode);
    s.glide = f (glide); s.glideLegato = b (glideLegato); s.glideType = i (glideType);
    s.bendUp = i (bendUp); s.bendDown = i (bendDown);
    s.mpe = b (mpe); s.mpeRange = i (mpeRange);
    s.velAmp = f (velAmp); s.transpose = i (transpose); s.tune = f (tune);
    s.filterRouting = i (filterRouting);
    {
        // AGE pushes every analog control up together; each knob still works on its own
        const float a = f (age);
        s.drift     = std::min (1.0f, f (drift) + a * 0.6f);
        s.spread    = std::min (1.0f, f (spread) + a * 0.5f);
        s.tolerance = std::min (1.0f, f (tolerance) + a * 0.5f);
        s.circuit   = std::min (1.0f, f (circuit) + a * 0.45f);
        s.noiseFloor = std::min (1.0f, f (anNoise) + a * 0.35f);
        s.hum        = std::min (1.0f, f (anHum) + a * 0.15f);
        s.humFreq    = i (anHumFreq);
        s.bits       = f (anBits);        // vintage converter is a separate choice, not part of AGE
        s.rateReduce = f (anRate);
    }
    for (int m = 0; m < kNumMacros; ++m) s.macros[m] = f (macros[m]);

    s.sub = { b (subOn), i (subShape), i (subOct), i (subRoute), f (subPan) };
    s.noise = { b (noiseOn), i (noiseType), i (noiseRoute), f (noisePan) };
    s.smp = { b (smpOn), i (smpInst), f (smpLevel), f (smpPan), i (smpOct), i (smpSemi), f (smpFine), f (smpStart), f (smpVel), i (smpRoute) };

    for (int o = 0; o < kNumOsc; ++o)
    {
        auto& r = osc[o];
        s.osc[o] = { b (r.on), i (r.wt), i (r.oct), juce::jlimit (1, kMaxUnison, i (r.voices)),
                     f (r.phase), f (r.rand), i (r.warp), i (r.fmSrc), i (r.route) };
    }
    for (int k = 0; k < kNumFilters; ++k)
        s.filt[k] = { b (filt[k].on), i (filt[k].type), f (filt[k].key) };

    for (int e = 0; e < kNumEnv; ++e)
    {
        auto& r = env[e];
        s.env[e] = { f (r.dly), f (r.att), f (r.hold), f (r.dec), f (r.sus), f (r.rel), f (r.aCrv), f (r.dCrv), f (r.rCrv) };
    }
    for (int l = 0; l < kNumLfo; ++l)
    {
        auto& r = lfo[l];
        s.lfo[l] = { i (r.shape), f (r.rate), b (r.sync), i (r.div), i (r.mode), f (r.phase), f (r.delay), f (r.fade),
                     b (r.uni), f (r.smooth) };
    }

    // modulatable values
    for (int d = 1; d < DCount; ++d)
    {
        s.conv[d] = conv[d];
        if (destRaw[d] != nullptr)
        {
            s.baseRaw[d] = f (destRaw[d]);
            s.baseNorm[d] = conv[d].kind == DestConv::Linear
                                ? (s.baseRaw[d] - conv[d].start) / (conv[d].end - conv[d].start)
                                : conv[d].range->convertTo0to1 (s.baseRaw[d]);
        }
        else
        {
            s.baseRaw[d] = 1.0f;   // LFO-rate multiplier and amp gain default to 1
            s.baseNorm[d] = 0.0f;
        }
        s.destModded[d] = false;
    }

    // active modulation slots
    s.numSlots = 0;
    for (int e = 0; e < kNumEnv; ++e) s.envUsed[e] = (e == 0);
    for (int l = 0; l < kNumLfo; ++l) s.lfoUsed[l] = false;

    for (int k = 0; k < kNumModSlots; ++k)
    {
        ModSlot m { i (slot[k].src), i (slot[k].dst), f (slot[k].amt) };
        if (m.src <= SrcNone || m.src >= SrcCount || m.dst <= DNone || m.dst >= DCount || m.amt == 0.0f)
            continue;
        s.slots[s.numSlots++] = m;
        s.destModded[m.dst] = true;
        if (m.src >= SrcEnv1 && m.src <= SrcEnv4) s.envUsed[m.src - SrcEnv1] = true;
        if (m.src >= SrcLfo1 && m.src <= SrcLfo4) s.lfoUsed[m.src - SrcLfo1] = true;
    }

    // ---- CHAOS / FOCUS / MOTION
    auto& mv = s.move;
    mv.chaos = f (mvChaos); mv.speed = f (mvSpeed); mv.bite = f (mvBite); mv.space = f (mvSpace); mv.seed = i (mvSeed); mv.mangle = f (mvMangle);
    mv.focus = f (mvFocus); mv.motion = f (mvMotion); mv.shape = i (mvShape); mv.rate = i (mvRate); mv.smooth = f (mvSmooth);
    for (int k = 0; k < kNumFocusSlots; ++k)
        mv.slots[k] = { i (focusRefs[k].dst), f (focusRefs[k].depth), f (focusRefs[k].curve) };

    s.numGlobal = 0;
    auto addGlobal = [&s] (int d)
    {
        if (d <= DNone || d >= DCount) return;
        if (! s.destModded[d]) { s.destModded[d] = true; }
        for (int k = 0; k < s.numGlobal; ++k) if (s.globalDests[k] == d) return;
        s.globalDests[s.numGlobal++] = d;
    };
    if (mv.chaos > 0.0f)
    {
        for (int o = 0; o < kNumOsc; ++o)
            if (s.osc[o].on)
                for (auto fld : { OPos, OWarp, OFm, ODetune, OFine, OPan, OWidth }) addGlobal (dOsc (o, fld));
        for (int k = 0; k < kNumFilters; ++k)
            if (s.filt[k].on) for (auto fld : { FCutoff, FRes, FDrive }) addGlobal (dFilt (k, fld));
        if (s.noise.on) addGlobal (DNoiseColor);
    }
    if (mv.focus > 0.0f)
        for (auto& sl : mv.slots) if (sl.dst > DNone && sl.dst < DCount && sl.depth != 0.0f) addGlobal (sl.dst);
    if (mv.motion > 0.0f)
    {
        addGlobal (DAmp);
        for (int k = 0; k < kNumFilters; ++k) if (s.filt[k].on) addGlobal (dFilt (k, FCutoff));
        for (int o = 0; o < kNumOsc; ++o) if (s.osc[o].on) { addGlobal (dOsc (o, OPan)); addGlobal (dOsc (o, OPos)); }
    }

    for (int k = 0; k < kNumFilters; ++k)
        if (s.filt[k].on && (s.baseRaw[dFilt (k, FEnv)] != 0.0f || s.destModded[dFilt (k, FEnv)]))
            s.envUsed[1] = true;
}

} // namespace pyra
