#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace pyra
{
constexpr int kNumOsc       = 3;
constexpr int kNumFilters   = 2;
constexpr int kNumEnv       = 4;
constexpr int kNumLfo       = 4;
constexpr int kNumMacros    = 4;
constexpr int kNumModSlots  = 16;
constexpr int kMaxVoices    = 64;
constexpr int kMaxUnison    = 16;

//==============================================================================
// Parameter IDs
namespace pid
{
    juce::String osc (int i, const char* p);      // oscA_level ...
    juce::String filt (int i, const char* p);     // f1_cutoff ...
    juce::String env (int i, const char* p);      // env1_att ...
    juce::String lfo (int i, const char* p);      // lfo1_rate ...
    juce::String mod (int i, const char* p);      // mod1_src ...
    juce::String macro (int i);                   // macro1 ...
}

//==============================================================================
// Choice lists (indices are saved in projects/presets: only ever append!)
namespace choices
{
    const juce::StringArray& voiceModes();
    const juce::StringArray& warpModes();
    const juce::StringArray& fmSources();
    const juce::StringArray& routes();
    const juce::StringArray& subShapes();
    const juce::StringArray& noiseTypes();
    const juce::StringArray& filterTypes();
    const juce::StringArray& filterRouting();
    const juce::StringArray& lfoShapes();
    const juce::StringArray& lfoModes();
    const juce::StringArray& syncDivisions();
    double syncDivisionBeats (int index);
    const juce::StringArray& modSources();
    const juce::StringArray& modDestinations();

    // phase 3
    const juce::StringArray& distTypes();
    const juce::StringArray& chords();
    const juce::StringArray& fxFilterTypes();
    const juce::StringArray& delayModes();
    const juce::StringArray& reverbTypes();
    const juce::StringArray& humFreqs();
    const juce::StringArray& arpModes();
    const juce::StringArray& noteNames();
    const juce::StringArray& scales();
    const std::vector<int>& scaleSteps (int index);
}

void addPhase3Parameters (juce::AudioProcessorValueTreeState::ParameterLayout&);
void addPhase5Parameters (juce::AudioProcessorValueTreeState::ParameterLayout&);
void addSamplerParameters (juce::AudioProcessorValueTreeState::ParameterLayout&);
void addPhase6Parameters (juce::AudioProcessorValueTreeState::ParameterLayout&);
void addPhase7Parameters (juce::AudioProcessorValueTreeState::ParameterLayout&);
/** LAYERS: layer 0 (A) is the sound of this PYRA, 1..7 (B..H) are complete extra sounds. */
constexpr int kNumLayers = 8;
juce::String layerParam (int layer, const char* name);   // "layer3_level"
namespace choices { const juce::StringArray& layerChannels(); }

constexpr int kNumFocusSlots = 8;

/** FX parameters that CHAOS / FOCUS / MOTION can move (append-only). */
enum FxDest { FxDistDrive, FxDistMix, FxResMix, FxFltCutoff, FxFltRes, FxChoMix, FxFlaMix, FxPhaMix,
              FxTapeWow, FxDlyMix, FxDlyFb, FxRevMix, FxRevSize,
              FxGltMix, FxGltChance, FxGrnMix, FxGrnSpray, FxGrnPitch, FxShfMix, FxShfFreq, FxDestCount };

namespace choices
{
    const juce::StringArray& motionShapes();
    const juce::StringArray& focusDestinations();   // synth destinations, then FX destinations
    const char* fxDestParamId (int fxDest);

    // phase 6
    const juce::StringArray& glideTypes();
    const juce::StringArray& glitchEffects();     // step effects of the GLITCH module (not a parameter)
    const juce::StringArray& shifterModes();
    const juce::StringArray& chordModes();
    const juce::StringArray& chordTriggers();
    const juce::StringArray& chordLengths();
    double chordLengthBeats (int index);
    const juce::StringArray& chordSizes();
    const juce::StringArray& chordVoicings();
    const juce::StringArray& chordInversions();
    const juce::StringArray& chordRhythms();
}

enum GlitchEffect { GOff, GRepeat, GStutter, GReverse, GTapeStop, GSlow, GPitchUp, GScratch, GGate, GCrush, GCount };

enum WarpMode   { WarpOff, WarpSync, WarpBendPlus, WarpBendMinus, WarpPwm, WarpAsym, WarpMirror, WarpFold, WarpQuantize, WarpCount };
enum FmSource   { FmOff, FmOscA, FmOscB, FmOscC, FmSub, FmNoise };
enum Route      { RouteF1, RouteF2, RouteBoth, RouteDirect };
enum FilterType { FLp12, FLp24, FLadder, FHp12, FHp24, FBp12, FBp24, FNotch, FPeak, FAllpass, FCombPlus, FCombMinus, FFormant, FPhaser,
                  FLp6, FHp6, FLp36, FHp36, FNotch24, FLowShelf, FHighShelf, FDiode, FMs20, FDualBp, FResonator, FTypeCount };
enum LfoMode    { LfoRetrig, LfoFree, LfoEnvelope };
enum VoiceMode  { ModePoly, ModeMono, ModeLegato };

enum ModSource
{
    SrcNone = 0,
    SrcEnv1, SrcEnv2, SrcEnv3, SrcEnv4,
    SrcLfo1, SrcLfo2, SrcLfo3, SrcLfo4,
    SrcVelocity, SrcNote, SrcRandom, SrcModWheel, SrcPressure, SrcPitchBend, SrcSlide,
    SrcMacro1, SrcMacro2, SrcMacro3, SrcMacro4,
    SrcChaos, SrcFocus, SrcMotion,
    SrcCount
};

// Modulation destinations (append-only)
enum OscField  { OLevel, OPan, OPitch, OFine, OPos, ODetune, OBlend, OWidth, OWarp, OFm, OCount };
enum FiltField { FCutoff, FRes, FDrive, FMix, FEnv, FCount };

enum Dest
{
    DNone        = 0,
    DOscBase     = 1,
    DSubLevel    = DOscBase + kNumOsc * OCount,
    DNoiseLevel,
    DNoiseColor,
    DFiltBase,
    DLfoRateBase = DFiltBase + kNumFilters * FCount,
    DAmp         = DLfoRateBase + kNumLfo,
    DCount
};

inline int dOsc (int osc, OscField f)    { return DOscBase + osc * OCount + f; }
inline int dFilt (int flt, FiltField f)  { return DFiltBase + flt * FCount + f; }

juce::String destParamId (int dest);   // empty for special destinations

//==============================================================================
// Plain, lock-free copy of every parameter, refreshed once per audio block.
struct OscSettings
{
    bool on = false; int wt = 0; int oct = 0; int voices = 1;
    float phase = 0, rand = 1; int warp = 0; int fmSrc = 0; int route = 0;
};

struct SubSettings    { bool on = false; int shape = 0; int oct = -1; int route = 0; float pan = 0; };
struct SampleSettings { bool on = false; int inst = 0; float level = 0.8f, pan = 0; int oct = 0, semi = 0; float fine = 0, start = 0, velLayers = 1; int route = 0; };
struct NoiseSettings  { bool on = false; int type = 0; int route = 0; float pan = 0; };
struct FilterSettings { bool on = false; int type = 0; float key = 0; };

struct EnvSettings
{
    float dly = 0, att = 0.002f, hold = 0, dec = 1, sus = 1, rel = 0.15f;
    float aCrv = 0.3f, dCrv = 0.6f, rCrv = 0.6f;
};

/** A hand-drawn LFO shape (the "Custom (drawn)" shape): 32 points, -1..1, read with linear interpolation. */
constexpr int kLfoPoints = 32;
struct LfoDrawn
{
    std::array<std::atomic<float>, kLfoPoints> pts;
    LfoDrawn() { reset(); }
    void reset() { for (int i = 0; i < kLfoPoints; ++i) pts[(size_t) i].store (std::sin (6.2831853f * (float) i / (float) kLfoPoints)); }
    float read (float p) const noexcept
    {
        const float x = (p - std::floor (p)) * (float) kLfoPoints;
        const int i0 = (int) x % kLfoPoints, i1 = (i0 + 1) % kLfoPoints;
        const float a = pts[(size_t) i0].load (std::memory_order_relaxed), b = pts[(size_t) i1].load (std::memory_order_relaxed);
        return a + (b - a) * (x - std::floor (x));
    }
    bool isDefault() const
    {
        for (int i = 0; i < kLfoPoints; ++i)
            if (std::abs (pts[(size_t) i].load() - std::sin (6.2831853f * (float) i / (float) kLfoPoints)) > 1.0e-4f) return false;
        return true;
    }
};

struct LfoDrawnSet
{
    std::array<LfoDrawn, kNumLfo> lfo;
    std::atomic<int> changeCounter { 0 };
    bool isDefault() const { for (auto& l : lfo) if (! l.isDefault()) return false; return true; }
    juce::var toVar() const
    {
        juce::Array<juce::var> all;
        for (auto& l : lfo) { juce::Array<juce::var> a; for (auto& p : l.pts) a.add (p.load()); all.add (a); }
        return all;
    }
    void fromVar (const juce::var& v)
    {
        auto* all = v.getArray();
        for (int k = 0; k < kNumLfo; ++k)
        {
            auto* a = (all != nullptr && k < all->size()) ? all->getReference (k).getArray() : nullptr;
            if (a == nullptr || a->size() != kLfoPoints) { lfo[(size_t) k].reset(); continue; }
            for (int i = 0; i < kLfoPoints; ++i) lfo[(size_t) k].pts[(size_t) i].store (juce::jlimit (-1.0f, 1.0f, (float) a->getReference (i)));
        }
        ++changeCounter;
    }
    juce::ValueTree toTree() const { juce::ValueTree t ("LFO_DRAWN"); t.setProperty ("pts", juce::JSON::toString (toVar(), true), nullptr); return t; }
    void fromTree (const juce::ValueTree& t) { if (t.isValid()) fromVar (juce::JSON::parse (t.getProperty ("pts").toString())); else { for (auto& l : lfo) l.reset(); ++changeCounter; } }
};

/** Index of the "Custom (drawn)" LFO shape (last of choices::lfoShapes). */
constexpr int kLfoShapeCustom = 38;

struct LfoSettings
{
    int shape = 0; float rate = 1; bool sync = true; int div = 7; int mode = 0;
    float phase = 0, delay = 0, fade = 0; bool uni = false; float smooth = 0;
    const LfoDrawn* drawn = nullptr;
};

struct ModSlot { int src = 0; int dst = 0; float amt = 0; };

struct DestConv
{
    enum Kind { Param, Linear, LfoRate, Amp };
    Kind kind = Param;
    float start = 0, end = 1;
    const juce::NormalisableRange<float>* range = nullptr;

    float toRaw (float norm) const
    {
        norm = juce::jlimit (0.0f, 1.0f, norm);
        if (kind == Linear) return start + norm * (end - start);
        return range->convertFrom0to1 (norm);
    }
};

struct MovementSettings
{
    float chaos = 0, speed = 0.6f, bite = 0.5f, space = 0.5f, mangle = 0.5f; int seed = 1;
    float focus = 0;
    struct FocusSlot { int dst = 0; float depth = 0, curve = 0; } slots[kNumFocusSlots];
    float motion = 0; int shape = 0, rate = 7; float smooth = 0.2f;
};

struct Settings
{
    // global
    float masterGain = 1; int polyphony = 16; int voiceMode = 0;
    float glide = 0; bool glideLegato = false; int glideType = 0;
    int bendUp = 2, bendDown = 2; bool mpe = false; int mpeRange = 48;
    float velAmp = 0.5f; int transpose = 0; float tune = 0;
    float drift = 0, spread = 0; int filterRouting = 0;
    float tolerance = 0, circuit = 0;   // ANALOG (already combined with AGE)
    float noiseFloor = 0, hum = 0, bits = 0, rateReduce = 0; int humFreq = 0;
    float macros[kNumMacros] {};

    OscSettings    osc[kNumOsc];
    SubSettings    sub;
    NoiseSettings  noise;
    SampleSettings smp;
    FilterSettings filt[kNumFilters];
    EnvSettings    env[kNumEnv];
    LfoSettings    lfo[kNumLfo];

    ModSlot slots[kNumModSlots]; int numSlots = 0;   // only active slots
    MovementSettings move;
    int globalDests[DCount] {}; int numGlobal = 0;   // destinations moved by CHAOS / FOCUS / MOTION
    bool destModded[DCount] {};
    bool envUsed[kNumEnv] {};
    bool lfoUsed[kNumLfo] {};

    float baseRaw[DCount] {};    // current value of each modulatable parameter
    float baseNorm[DCount] {};   // same, 0..1
    DestConv conv[DCount];
};

//==============================================================================
class ParamRefs
{
public:
    void init (juce::AudioProcessorValueTreeState& state);
    void fill (Settings& s) const;

private:
    struct OscRefs  { std::atomic<float>* on, *wt, *oct, *voices, *phase, *rand, *warp, *fmSrc, *route; };
    struct FiltRefs { std::atomic<float>* on, *type, *key; };
    struct EnvRefs  { std::atomic<float>* dly, *att, *hold, *dec, *sus, *rel, *aCrv, *dCrv, *rCrv; };
    struct LfoRefs  { std::atomic<float>* shape, *rate, *sync, *div, *mode, *phase, *delay, *fade, *uni, *smooth; };
    struct SlotRefs { std::atomic<float>* src, *dst, *amt; };

    std::atomic<float>* master {}, *polyphony {}, *voiceMode {}, *glide {}, *glideLegato {},
        *bendUp {}, *bendDown {}, *mpe {}, *mpeRange {}, *velAmp {}, *transpose {}, *tune {},
        *drift {}, *spread {}, *filterRouting {}, *age {}, *tolerance {}, *circuit {},
        *anNoise {}, *anHum {}, *anHumFreq {}, *anBits {}, *anRate {};
    std::atomic<float>* glideType {}, *mvMangle {};
    std::atomic<float>* mvChaos {}, *mvSpeed {}, *mvBite {}, *mvSpace {}, *mvSeed {}, *mvFocus {}, *mvMotion {},
        *mvShape {}, *mvRate {}, *mvSmooth {};
    struct FocusRefs { std::atomic<float>* dst, *depth, *curve; } focusRefs[kNumFocusSlots] {};
    std::atomic<float>* smpOn {}, *smpInst {}, *smpLevel {}, *smpPan {}, *smpOct {}, *smpSemi {}, *smpFine {}, *smpStart {},
        *smpVel {}, *smpRoute {};
    std::atomic<float>* macros[kNumMacros] {};
    std::atomic<float>* subOn {}, *subShape {}, *subOct {}, *subRoute {}, *subPan {};
    std::atomic<float>* noiseOn {}, *noiseType {}, *noiseRoute {}, *noisePan {};

    OscRefs  osc[kNumOsc] {};
    FiltRefs filt[kNumFilters] {};
    EnvRefs  env[kNumEnv] {};
    LfoRefs  lfo[kNumLfo] {};
    SlotRefs slot[kNumModSlots] {};

    std::atomic<float>* destRaw[DCount] {};
    juce::RangedAudioParameter* destParam[DCount] {};
    DestConv conv[DCount];
};

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace pyra
