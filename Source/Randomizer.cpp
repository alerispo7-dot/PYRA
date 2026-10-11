#include "Randomizer.h"
#include "PluginProcessor.h"
#include "dsp/Wavetables.h"
#include "Presets.h"

namespace pyra
{
namespace randomizer
{
namespace
{
    struct R
    {
        PyraAudioProcessor& p;
        juce::Random rnd;

        R (PyraAudioProcessor& proc, uint32_t seed) : p (proc), rnd ((juce::int64) seed * 7919 + 17) {}

        juce::RangedAudioParameter* param (const juce::String& id) const { return p.apvts.getParameter (id); }

        void set (const juce::String& id, float value)
        {
            if (auto* q = param (id))
            {
                const float n = q->convertTo0to1 (q->getNormalisableRange().snapToLegalValue (value));
                if (std::abs (q->getValue() - n) < 1.0e-7f) return;
                q->beginChangeGesture();
                q->setValueNotifyingHost (n);
                q->endChangeGesture();
            }
        }
        void setDefault (const juce::String& id)
        {
            if (auto* q = param (id)) set (id, q->convertFrom0to1 (q->getDefaultValue()));
        }
        float get (const juce::String& id) const { return p.apvts.getRawParameterValue (id)->load(); }

        float uni() { return rnd.nextFloat(); }
        float range (float a, float b) { return a + (b - a) * rnd.nextFloat(); }
        float logRange (float a, float b) { return a * std::pow (b / a, rnd.nextFloat()); }
        /** Skewed towards a: exponent > 1 = mostly small values. */
        float skew (float a, float b, float exponent) { return a + (b - a) * std::pow (rnd.nextFloat(), exponent); }
        int pick (int n) { return rnd.nextInt (juce::jmax (1, n)); }
        bool chance (float p01) { return rnd.nextFloat() < p01; }
        template <typename T> T oneOf (std::initializer_list<T> list)
        {
            auto it = list.begin();
            std::advance (it, pick ((int) list.size()));
            return *it;
        }
    };

    bool isPageParam (Page page, const juce::String& id)
    {
        auto any = [&id] (std::initializer_list<const char*> prefixes)
        {
            for (auto* pre : prefixes) if (id.startsWith (pre)) return true;
            return false;
        };
        switch (page)
        {
            case Synth:    return any ({ "osc", "sub_", "noise_", "f1_", "f2_", "env", "lfo" }) || id == "filterRouting";
            case Fx:       return id.startsWith ("fx_");
            case Arp:      return id.startsWith ("arp_") && id != "arp_on" && id != "arp_latch";
            case Chords:   return id.startsWith ("chord_") && id != "chord_on" && id != "chord_trigger" && id != "chord_mode" && id != "chord_mood";
            case Matrix:   return id.startsWith ("mod") && id.endsWith ("_amt");
            case Chaos:    return any ({ "chaos_", "focus", "motion_" }) && id != "chaos_seed";
            case Analog:   return id.startsWith ("an_") || id == "drift" || id == "spread";
            case Settings: return any ({ "voiceMode", "glide", "bend", "velAmp" });
        }
        return false;
    }

    /** Small changes around the current values. */
    void vary (R& r, Page page)
    {
        for (auto* ap : r.p.getParameters())
        {
            auto* q = dynamic_cast<juce::RangedAudioParameter*> (ap);
            if (q == nullptr || ! isPageParam (page, q->getParameterID())) continue;
            const auto id = q->getParameterID();
            if (id.endsWith ("_on") || id == "voiceMode") continue;   // never switch sections on/off in a variation
            const bool discrete = dynamic_cast<juce::AudioParameterChoice*> (q) != nullptr
                                  || dynamic_cast<juce::AudioParameterBool*> (q) != nullptr;
            float n = q->getValue();
            if (discrete) { if (r.chance (0.08f)) n = r.uni(); }
            else if (dynamic_cast<juce::AudioParameterInt*> (q) != nullptr) { if (r.chance (0.15f)) n += r.range (-0.1f, 0.1f); }
            else n += (r.uni() + r.uni() + r.uni() - 1.5f) * 0.09f;
            n = juce::jlimit (0.0f, 1.0f, n);
            if (std::abs (n - q->getValue()) > 1.0e-6f)
            {
                q->beginChangeGesture();
                q->setValueNotifyingHost (n);
                q->endChangeGesture();
            }
        }
        if (page == Matrix)
            for (int s = 0; s < kNumModSlots; ++s)   // keep the matrix amounts away from zero-crossing surprises
                if (std::abs (r.get (pid::mod (s, "amt"))) < 0.02f) r.set (pid::mod (s, "amt"), 0.0f);
    }

    //==========================================================================
    void synth (R& r)
    {
        const int numTables = Wavetables::names().size();
        // character of the sound decides a few things together
        const int character = r.pick (5);   // 0 bass, 1 lead, 2 pad, 3 pluck, 4 keys / anything

        for (int o = 0; o < kNumOsc; ++o)
        {
            const bool on = o == 0 || r.chance (o == 1 ? 0.6f : 0.3f);
            r.set (pid::osc (o, "on"), on ? 1.0f : 0.0f);
            int wt = r.pick (numTables);
            if (wt == 26 && r.chance (0.7f)) wt = 2;   // noise spectra rarely
            r.set (pid::osc (o, "wt"), (float) wt);
            r.set (pid::osc (o, "pos"), r.uni());
            r.set (pid::osc (o, "level"), o == 0 ? r.range (0.6f, 0.85f) : r.range (0.35f, 0.7f));
            r.set (pid::osc (o, "pan"), o == 0 ? 0.0f : r.range (-0.4f, 0.4f));
            r.set (pid::osc (o, "oct"), (float) (character == 0 ? r.oneOf ({ -1, -1, 0 }) : (o == 0 ? r.oneOf ({ 0, 0, 0, -1 }) : r.oneOf ({ -1, 0, 0, 1 }))));
            r.set (pid::osc (o, "semi"), o == 0 ? 0.0f : (float) (r.chance (0.25f) ? r.oneOf ({ 7, 12, -12, 5, -5 }) : 0));
            r.set (pid::osc (o, "fine"), o == 0 ? 0.0f : r.range (-12.0f, 12.0f));
            const int voices = character == 2 ? r.oneOf ({ 3, 5, 7, 9 }) : r.oneOf ({ 1, 1, 1, 3, 5, 7 });
            r.set (pid::osc (o, "voices"), (float) voices);
            r.set (pid::osc (o, "detune"), r.skew (0.05f, 0.45f, 1.6f));
            r.set (pid::osc (o, "blend"), r.range (0.5f, 0.9f));
            r.set (pid::osc (o, "width"), r.range (0.5f, 1.0f));
            r.set (pid::osc (o, "phase"), 0.0f);
            r.set (pid::osc (o, "rand"), character == 3 || character == 0 ? r.range (0.0f, 0.3f) : 1.0f);
            const bool warp = r.chance (0.35f);
            r.set (pid::osc (o, "warp"), warp ? (float) (1 + r.pick (WarpCount - 1)) : 0.0f);
            r.set (pid::osc (o, "warpAmt"), warp ? r.skew (0.05f, 0.6f, 1.5f) : 0.0f);
            const bool fm = o > 0 && r.chance (0.2f);
            r.set (pid::osc (o, "fmSrc"), fm ? (float) r.oneOf ({ 1, 2, 4 }) : 0.0f);
            r.set (pid::osc (o, "fmAmt"), fm ? r.skew (0.05f, 0.45f, 1.5f) : 0.0f);
            r.set (pid::osc (o, "route"), (float) (r.chance (0.85f) ? RouteF1 : r.oneOf ({ (int) RouteF2, (int) RouteBoth })));
        }

        const bool sub = character == 0 ? r.chance (0.75f) : r.chance (0.2f);
        r.set ("sub_on", sub ? 1.0f : 0.0f);
        r.set ("sub_shape", (float) r.oneOf ({ 0, 0, 1, 3 }));
        r.set ("sub_oct", (float) r.oneOf ({ -1, -1, -2 }));
        r.set ("sub_level", r.range (0.35f, 0.7f));
        const bool noise = r.chance (0.2f);
        r.set ("noise_on", noise ? 1.0f : 0.0f);
        r.set ("noise_type", (float) r.pick (5));
        r.set ("noise_level", r.skew (0.03f, 0.25f, 1.5f));
        r.set ("noise_color", r.range (-0.6f, 0.6f));

        // filters
        const int type = r.oneOf ({ (int) FLp24, (int) FLp24, (int) FLadder, (int) FLp12, (int) FBp12, (int) FHp12, (int) FFormant,
                                    (int) FPhaser, (int) FCombPlus, (int) FPeak });
        r.set ("f1_on", r.chance (0.9f) ? 1.0f : 0.0f);
        r.set ("f1_type", (float) type);
        const float cutLo = type == FHp12 ? 60.0f : (character == 0 ? 180.0f : 400.0f);
        const float cutHi = type == FHp12 ? 900.0f : 14000.0f;
        r.set ("f1_cutoff", r.logRange (cutLo, cutHi));
        r.set ("f1_res", r.skew (0.0f, 0.7f, 1.4f));
        r.set ("f1_drive", r.skew (0.0f, 0.5f, 1.8f));
        r.set ("f1_mix", 1.0f);
        r.set ("f1_key", r.range (0.0f, 0.6f));
        r.set ("f1_env", character == 2 ? r.range (-0.1f, 0.3f) : r.range (0.0f, 0.6f));
        const bool f2 = r.chance (0.25f);
        r.set ("f2_on", f2 ? 1.0f : 0.0f);
        r.set ("f2_type", (float) r.oneOf ({ (int) FHp12, (int) FHp12, (int) FNotch, (int) FPeak, (int) FBp24 }));
        r.set ("f2_cutoff", r.logRange (60.0f, 2000.0f));
        r.set ("f2_res", r.skew (0.0f, 0.5f, 1.5f));
        r.set ("filterRouting", r.chance (0.8f) ? 0.0f : 1.0f);

        // envelopes: shaped by the character
        float att, dec, sus, rel;
        switch (character)
        {
            case 0:  att = r.skew (0.001f, 0.02f, 2.0f); dec = r.range (0.2f, 1.2f); sus = r.range (0.5f, 1.0f); rel = r.range (0.05f, 0.3f); break;
            case 1:  att = r.skew (0.001f, 0.08f, 2.0f); dec = r.range (0.3f, 2.0f); sus = r.range (0.6f, 1.0f); rel = r.range (0.1f, 0.6f); break;
            case 2:  att = r.range (0.2f, 2.0f); dec = r.range (1.0f, 4.0f); sus = r.range (0.6f, 1.0f); rel = r.range (0.8f, 3.5f); break;
            case 3:  att = r.skew (0.001f, 0.005f, 2.0f); dec = r.range (0.15f, 0.9f); sus = r.skew (0.0f, 0.25f, 2.0f); rel = r.range (0.1f, 0.6f); break;
            default: att = r.skew (0.001f, 0.05f, 2.0f); dec = r.range (0.5f, 3.0f); sus = r.range (0.2f, 0.8f); rel = r.range (0.2f, 1.2f); break;
        }
        r.set ("env1_dly", 0.0f); r.set ("env1_hold", 0.0f);
        r.set ("env1_att", att); r.set ("env1_dec", dec); r.set ("env1_sus", sus); r.set ("env1_rel", rel);
        r.set ("env1_aCrv", r.range (0.0f, 0.5f)); r.set ("env1_dCrv", r.range (0.3f, 0.8f)); r.set ("env1_rCrv", r.range (0.3f, 0.8f));
        for (int e = 1; e < kNumEnv; ++e)
        {
            r.set (pid::env (e, "dly"), 0.0f);
            r.set (pid::env (e, "att"), r.skew (0.0f, e == 1 && character == 2 ? 1.5f : 0.3f, 2.0f));
            r.set (pid::env (e, "hold"), 0.0f);
            r.set (pid::env (e, "dec"), r.range (0.08f, 2.0f));
            r.set (pid::env (e, "sus"), r.skew (0.0f, 0.7f, 1.5f));
            r.set (pid::env (e, "rel"), r.range (0.1f, 1.5f));
            r.set (pid::env (e, "aCrv"), r.range (-0.3f, 0.6f));
            r.set (pid::env (e, "dCrv"), r.range (0.2f, 0.9f));
            r.set (pid::env (e, "rCrv"), r.range (0.2f, 0.9f));
        }

        for (int l = 0; l < kNumLfo; ++l)
        {
            r.set (pid::lfo (l, "shape"), (float) r.pick (choices::lfoShapes().size() - 1));   // never the hand-drawn one
            r.set (pid::lfo (l, "sync"), r.chance (0.75f) ? 1.0f : 0.0f);
            r.set (pid::lfo (l, "div"), (float) r.oneOf ({ 3, 4, 5, 7, 8, 10, 11, 13 }));
            r.set (pid::lfo (l, "rate"), r.logRange (0.1f, 8.0f));
            r.set (pid::lfo (l, "mode"), (float) r.oneOf ({ 0, 0, 1 }));
            r.set (pid::lfo (l, "phase"), r.uni());
            r.set (pid::lfo (l, "delay"), 0.0f);
            r.set (pid::lfo (l, "fade"), r.chance (0.3f) ? r.range (0.1f, 1.5f) : 0.0f);
            r.set (pid::lfo (l, "uni"), r.chance (0.3f) ? 1.0f : 0.0f);
            r.set (pid::lfo (l, "smooth"), r.skew (0.0f, 0.6f, 1.5f));
        }
    }

    //==========================================================================
    void matrix (R& r)
    {
        for (int s = 0; s < kNumModSlots; ++s)
        {
            r.set (pid::mod (s, "src"), 0.0f); r.set (pid::mod (s, "dst"), 0.0f); r.set (pid::mod (s, "amt"), 0.0f);
        }
        const int sources[] = { SrcEnv2, SrcEnv3, SrcEnv4, SrcLfo1, SrcLfo2, SrcLfo3, SrcLfo4, SrcVelocity, SrcModWheel, SrcRandom,
                                SrcMacro1, SrcMacro2, SrcNote, SrcLfo1, SrcLfo2, SrcEnv2 };
        std::vector<int> dests;
        for (int o = 0; o < kNumOsc; ++o)
            if (r.get (pid::osc (o, "on")) > 0.5f)
                for (auto f : { OPos, OPos, OWarp, OFm, OPan, OFine, ODetune, OLevel }) dests.push_back (dOsc (o, f));
        for (int k = 0; k < kNumFilters; ++k)
            if (r.get (pid::filt (k, "on")) > 0.5f)
                for (auto f : { FCutoff, FCutoff, FRes, FDrive }) dests.push_back (dFilt (k, f));
        for (int l = 0; l < kNumLfo; ++l) dests.push_back (DLfoRateBase + l);
        dests.push_back (DAmp);
        if (r.get ("noise_on") > 0.5f) dests.push_back (DNoiseLevel);

        const int count = 3 + r.pick (4);
        std::vector<std::pair<int, int>> used;
        for (int s = 0, made = 0; s < kNumModSlots && made < count; ++s)
        {
            const int src = sources[r.pick ((int) std::size (sources))];
            const int dst = dests[(size_t) r.pick ((int) dests.size())];
            if (std::find (used.begin(), used.end(), std::make_pair (src, dst)) != used.end()) continue;
            used.push_back ({ src, dst });
            float amt = r.skew (0.12f, 0.6f, 1.3f) * (r.chance (0.25f) ? -1.0f : 1.0f);
            if (dst == DAmp) amt = -std::abs (amt) * 0.6f;                                   // never louder than the preset
            if (dst >= DOscBase && dst < DSubLevel && (dst - DOscBase) % OCount == OFine) amt *= 0.3f;   // gentle pitch
            r.set (pid::mod (made, "src"), (float) src);
            r.set (pid::mod (made, "dst"), (float) dst);
            r.set (pid::mod (made, "amt"), amt);
            ++made;
        }
    }

    //==========================================================================
    void fx (R& r)
    {
        struct Mod { const char* prefix; float probability; };
        const Mod mods[] = { { "dist", 0.35f }, { "res", 0.12f }, { "flt", 0.2f }, { "shf", 0.12f }, { "grn", 0.15f },
                             { "cho", 0.35f }, { "fla", 0.15f }, { "pha", 0.2f }, { "tape", 0.25f }, { "dly", 0.6f },
                             { "rev", 0.7f }, { "glt", 0.22f }, { "eq", 0.2f }, { "comp", 0.3f } };
        int onCount = 0;
        for (auto& m : mods)
        {
            const bool on = r.chance (m.probability);
            onCount += on ? 1 : 0;
            r.set (juce::String ("fx_") + m.prefix + "_on", on ? 1.0f : 0.0f);
        }
        if (onCount == 0) r.set ("fx_rev_on", 1.0f);

        r.set ("fx_dist_type", (float) r.pick (choices::distTypes().size()));
        r.set ("fx_dist_drive", r.skew (0.1f, 0.65f, 1.4f)); r.set ("fx_dist_tone", r.range (0.4f, 0.9f));
        r.set ("fx_dist_out", -3.0f); r.set ("fx_dist_mix", r.range (0.3f, 1.0f));
        r.set ("fx_res_chord", (float) r.pick (choices::chords().size())); r.set ("fx_res_note", (float) (36 + r.pick (24)));
        r.set ("fx_res_decay", r.range (0.3f, 0.8f)); r.set ("fx_res_bright", r.range (0.3f, 0.8f)); r.set ("fx_res_mix", r.range (0.15f, 0.45f));
        r.set ("fx_flt_type", (float) r.pick (4)); r.set ("fx_flt_cutoff", r.logRange (300.0f, 9000.0f));
        r.set ("fx_flt_res", r.skew (0.0f, 0.6f, 1.4f)); r.set ("fx_flt_drive", r.skew (0.0f, 0.4f, 2.0f)); r.set ("fx_flt_mix", 1.0f);
        r.set ("fx_shf_mode", r.chance (0.7f) ? 0.0f : 1.0f); r.set ("fx_shf_freq", r.oneOf ({ -1.0f, 1.0f }) * r.logRange (3.0f, 400.0f));
        r.set ("fx_shf_fb", r.skew (0.0f, 0.6f, 2.0f)); r.set ("fx_shf_width", r.uni()); r.set ("fx_shf_mix", r.range (0.2f, 0.6f));
        r.set ("fx_grn_size", r.logRange (25.0f, 300.0f)); r.set ("fx_grn_density", r.logRange (4.0f, 40.0f));
        r.set ("fx_grn_spray", r.range (0.05f, 0.7f)); r.set ("fx_grn_pitch", (float) r.oneOf ({ 0, 0, 0, 12, -12, 7 }));
        r.set ("fx_grn_prand", r.skew (0.0f, 0.6f, 1.5f)); r.set ("fx_grn_rev", r.skew (0.0f, 0.6f, 1.5f));
        r.set ("fx_grn_freeze", 0.0f); r.set ("fx_grn_width", r.range (0.4f, 1.0f)); r.set ("fx_grn_mix", r.range (0.2f, 0.6f));
        r.set ("fx_cho_rate", r.logRange (0.1f, 2.0f)); r.set ("fx_cho_depth", r.range (0.2f, 0.8f)); r.set ("fx_cho_delay", r.range (6.0f, 20.0f));
        r.set ("fx_cho_fb", r.skew (0.0f, 0.4f, 1.5f)); r.set ("fx_cho_width", r.range (0.6f, 1.0f)); r.set ("fx_cho_mix", r.range (0.25f, 0.6f));
        r.set ("fx_fla_rate", r.logRange (0.05f, 1.5f)); r.set ("fx_fla_depth", r.range (0.3f, 0.9f));
        r.set ("fx_fla_fb", r.range (-0.7f, 0.8f)); r.set ("fx_fla_mix", r.range (0.25f, 0.6f));
        r.set ("fx_pha_rate", r.logRange (0.05f, 2.0f)); r.set ("fx_pha_depth", r.range (0.3f, 0.9f)); r.set ("fx_pha_freq", r.logRange (300.0f, 3000.0f));
        r.set ("fx_pha_fb", r.range (-0.6f, 0.8f)); r.set ("fx_pha_stages", (float) r.oneOf ({ 4, 6, 8 })); r.set ("fx_pha_mix", r.range (0.3f, 0.6f));
        r.set ("fx_tape_wow", r.skew (0.05f, 0.6f, 1.5f)); r.set ("fx_tape_flutter", r.skew (0.05f, 0.5f, 1.5f));
        r.set ("fx_tape_sat", r.range (0.1f, 0.6f)); r.set ("fx_tape_tone", r.range (0.4f, 0.9f)); r.set ("fx_tape_mix", 1.0f);
        r.set ("fx_dly_mode", (float) r.pick (3)); r.set ("fx_dly_sync", 1.0f); r.set ("fx_dly_div", (float) r.oneOf ({ 7, 8, 10, 11, 12, 13 }));
        r.set ("fx_dly_fb", r.range (0.2f, 0.65f)); r.set ("fx_dly_lowcut", r.logRange (80.0f, 500.0f));
        r.set ("fx_dly_highcut", r.logRange (3000.0f, 12000.0f)); r.set ("fx_dly_width", r.range (0.3f, 1.0f)); r.set ("fx_dly_mix", r.range (0.12f, 0.35f));
        r.set ("fx_rev_type", (float) r.oneOf ({ 0, 1, 1, 2 })); r.set ("fx_rev_size", r.range (0.3f, 0.9f)); r.set ("fx_rev_decay", r.logRange (0.8f, 7.0f));
        r.set ("fx_rev_pre", r.range (0.0f, 60.0f)); r.set ("fx_rev_damp", r.range (0.2f, 0.7f)); r.set ("fx_rev_width", r.range (0.7f, 1.0f));
        r.set ("fx_rev_shim", r.range (0.2f, 0.7f)); r.set ("fx_rev_mix", r.range (0.12f, 0.38f));
        r.set ("fx_glt_rate", (float) r.oneOf ({ 10, 13, 13, 13, 16 })); r.set ("fx_glt_steps", (float) r.oneOf ({ 8, 16, 16 }));
        r.set ("fx_glt_chance", r.skew (0.0f, 0.4f, 2.0f)); r.set ("fx_glt_smooth", r.range (0.15f, 0.5f)); r.set ("fx_glt_mix", 1.0f);
        r.set ("fx_eq_lowg", r.range (-3.0f, 3.0f)); r.set ("fx_eq_midg", r.range (-3.0f, 3.0f)); r.set ("fx_eq_highg", r.range (-3.0f, 3.0f));
        r.set ("fx_eq_midf", r.logRange (300.0f, 4000.0f));
        r.set ("fx_comp_thresh", r.range (-24.0f, -10.0f)); r.set ("fx_comp_ratio", r.range (2.0f, 5.0f)); r.set ("fx_comp_att", r.range (2.0f, 20.0f));
        r.set ("fx_comp_rel", r.range (60.0f, 250.0f)); r.set ("fx_comp_makeup", r.range (0.0f, 4.0f)); r.set ("fx_comp_mix", 1.0f);

        if (r.get ("fx_glt_on") > 0.5f) r.p.glitchPattern.randomize ((int) std::lround (r.get ("fx_glt_steps")), (uint32_t) r.rnd.nextInt(), 0.4f);
    }

    //==========================================================================
    void chaos (R& r)
    {
        r.set ("chaos_amt", r.range (0.15f, 0.85f));
        r.set ("chaos_speed", r.logRange (0.1f, 4.0f));
        r.set ("chaos_bite", r.uni()); r.set ("chaos_space", r.uni()); r.set ("chaos_mangle", r.uni());
        r.set ("chaos_seed", (float) (1 + r.pick (9999)));
        r.set ("focus_amt", r.range (0.0f, 0.7f));
        std::vector<int> dests { dFilt (0, FCutoff), dFilt (0, FRes), dFilt (0, FDrive), dOsc (0, OPos), dOsc (0, OWarp), dOsc (0, ODetune),
                                 dOsc (1, OPos), dOsc (0, OFm), DCount + FxRevMix, DCount + FxDlyMix, DCount + FxDistMix, DCount + FxChoMix,
                                 DCount + FxGrnMix, DCount + FxGltMix, DCount + FxShfMix, DCount + FxFltCutoff };
        for (int k = 0; k < kNumFocusSlots; ++k)
        {
            const auto n = "focus" + juce::String (k + 1) + "_";
            const bool used = k < 3 || r.chance (0.35f);
            r.set (n + "dst", used ? (float) dests[(size_t) r.pick ((int) dests.size())] : 0.0f);
            r.set (n + "depth", used ? r.range (0.15f, 0.6f) * (r.chance (0.2f) ? -1.0f : 1.0f) : 0.0f);
            r.set (n + "curve", r.range (-0.6f, 0.6f));
        }
        r.set ("motion_amt", r.chance (0.7f) ? r.range (0.2f, 0.8f) : 0.0f);
        r.set ("motion_shape", (float) r.pick (choices::motionShapes().size()));
        r.set ("motion_rate", (float) r.oneOf ({ 3, 4, 7, 7, 10, 10, 13 }));
        r.set ("motion_smooth", r.range (0.05f, 0.6f));
    }

    void analog (R& r)
    {
        r.set ("an_age", r.skew (0.0f, 0.8f, 1.2f));
        r.set ("drift", r.skew (0.0f, 0.6f, 1.5f)); r.set ("spread", r.skew (0.0f, 0.6f, 1.5f));
        r.set ("an_tolerance", r.skew (0.0f, 0.7f, 1.5f)); r.set ("an_circuit", r.skew (0.0f, 0.7f, 1.4f));
        r.set ("an_noise", r.skew (0.0f, 0.5f, 2.0f)); r.set ("an_hum", r.skew (0.0f, 0.4f, 2.5f));
        r.set ("an_humfreq", (float) r.pick (2));
        const bool vintage = r.chance (0.3f);
        r.set ("an_bits", vintage ? r.range (0.1f, 0.6f) : 0.0f);
        r.set ("an_rate", vintage ? r.range (0.0f, 0.5f) : 0.0f);
    }

    void settings (R& r)
    {
        const int mode = r.chance (0.55f) ? ModePoly : (r.chance (0.5f) ? ModeMono : ModeLegato);
        r.set ("voiceMode", (float) mode);
        const bool glide = mode != ModePoly ? r.chance (0.75f) : r.chance (0.3f);
        r.set ("glide", glide ? r.skew (0.02f, 0.4f, 1.6f) : 0.0f);
        r.set ("glideLegato", r.chance (0.5f) ? 1.0f : 0.0f);
        r.set ("glide_type", r.chance (0.7f) ? 0.0f : 1.0f);
        const int bend = r.oneOf ({ 2, 2, 7, 12 });
        r.set ("bendUp", (float) bend); r.set ("bendDown", (float) bend);
        r.set ("velAmp", r.range (0.2f, 0.8f));
    }

    void arp (R& r)
    {
        r.set ("arp_rate", (float) r.oneOf ({ 10, 12, 13, 13, 13, 15, 16 }));
        r.set ("arp_oct", (float) r.oneOf ({ 1, 1, 2, 2, 3 }));
        r.set ("arp_gate", r.range (0.25f, 0.9f));
        r.set ("arp_swing", r.chance (0.4f) ? r.range (0.05f, 0.4f) : 0.0f);
        r.set ("arp_steps", (float) r.oneOf ({ 8, 16, 16, 12 }));
        r.set ("arp_velamt", r.range (0.4f, 1.0f));
        const int steps = (int) std::lround (r.get ("arp_steps"));
        r.p.arpPattern.randomize (steps, (uint32_t) r.rnd.nextInt());
        r.set ("arp_mode", (float) r.oneOf ({ 0, 2, 4, 7, 7, 7 }));
        r.set ("arp_scale", (float) r.oneOf ({ 0, 1, 2, 2, 5, 11, 10 }));
        r.set ("arp_key", (float) r.pick (12));
    }
}

void randomize (PyraAudioProcessor& p, Page page, bool variation, uint32_t seed)
{
    R r (p, seed);
    if (variation)
    {
        vary (r, page);
        return;
    }
    switch (page)
    {
        case Synth:    synth (r); break;
        case Fx:       fx (r); break;
        case Arp:      arp (r); break;
        case Matrix:   matrix (r); break;
        case Chaos:    chaos (r); break;
        case Analog:   analog (r); break;
        case Settings: settings (r); break;
        case Chords:
        {
            const int mood = (int) p.apvts.getRawParameterValue ("chord_mood")->load();
            if (mood > 0)
            {
                // the emotion picks the scale too (the current one stays if it already fits)
                const auto& style = chordMoodStyle (mood);
                const int scale = (int) r.get ("chord_scale");
                if (std::find (style.scales.begin(), style.scales.end(), scale) == style.scales.end())
                    r.set ("chord_scale", (float) style.scales[(size_t) r.pick ((int) style.scales.size())]);
            }
            const auto s = p.chords.readSettings();
            p.chordPattern.randomizeMood (mood, s.count, s.scale, s.complexity, seed);
            break;
        }
    }
}

namespace
{
    /** Which parameters belong to a section, and which page randomizer knows how to make them musical. */
    struct SectionInfo { Page page; std::function<bool (const juce::String&)> owns; juce::String onSwitch; };

    bool sectionInfo (const juce::String& sec, SectionInfo& out)
    {
        auto prefix = [] (juce::StringArray pre, juce::StringArray exact = {})
        {
            return [pre, exact] (const juce::String& id)
            {
                if (exact.contains (id)) return true;
                for (auto& x : pre) if (id.startsWith (x)) return true;
                return false;
            };
        };
        if (sec.startsWith ("osc"))
        {
            const int o = sec.getTrailingIntValue();
            out = { Synth, prefix ({ pid::osc (o, "") }), pid::osc (o, "on") };
            return true;
        }
        if (sec == "subnoise") { out = { Synth, prefix ({ "sub_", "noise_" }), {} }; return true; }
        if (sec.startsWith ("filter"))
        {
            const int f = sec.getTrailingIntValue();
            out = { Synth, prefix ({ pid::filt (f, "") }, f == 0 ? juce::StringArray { "filterRouting" } : juce::StringArray {}), pid::filt (f, "on") };
            return true;
        }
        if (sec.startsWith ("env")) { out = { Synth, prefix ({ pid::env (sec.getTrailingIntValue() - 1, "") }), {} }; return true; }
        if (sec.startsWith ("lfo")) { out = { Synth, prefix ({ pid::lfo (sec.getTrailingIntValue() - 1, "") }), {} }; return true; }
        if (sec == "voice") { out = { Settings, prefix ({}, { "voiceMode", "glide", "glideLegato", "glide_type", "bendUp", "bendDown", "velAmp" }), {} }; return true; }
        if (sec.startsWith ("fx:"))
        {
            const auto m = sec.fromFirstOccurrenceOf (":", false, false);
            out = { Fx, prefix ({ "fx_" + m + "_" }), "fx_" + m + "_on" };
            return true;
        }
        if (sec == "chaos")    { out = { Chaos, prefix ({ "chaos_" }), {} }; return true; }
        if (sec == "focus")    { out = { Chaos, prefix ({ "focus" }), {} }; return true; }
        if (sec == "motion")   { out = { Chaos, prefix ({ "motion_" }), {} }; return true; }
        if (sec == "age")      { out = { Analog, prefix ({}, { "an_age" }), {} }; return true; }
        if (sec == "movement") { out = { Analog, prefix ({}, { "drift", "spread", "an_tolerance" }), {} }; return true; }
        if (sec == "circuit")  { out = { Analog, prefix ({}, { "an_circuit" }), {} }; return true; }
        if (sec == "noise")    { out = { Analog, prefix ({}, { "an_noise", "an_hum", "an_humfreq" }), {} }; return true; }
        if (sec == "converter"){ out = { Analog, prefix ({}, { "an_bits", "an_rate" }), {} }; return true; }
        return false;
    }

    void copyParam (PyraAudioProcessor& from, PyraAudioProcessor& to, const juce::String& id)
    {
        auto* a = from.apvts.getParameter (id);
        auto* b = to.apvts.getParameter (id);
        if (a == nullptr || b == nullptr || std::abs (a->getValue() - b->getValue()) < 1.0e-7f) return;
        b->beginChangeGesture();
        b->setValueNotifyingHost (a->getValue());
        b->endChangeGesture();
    }
}

void randomizeSection (PyraAudioProcessor& p, const juce::String& sec, bool variation, uint32_t seed)
{
    R r (p, seed);

    // sections that are easier to do directly
    if (sec == "macro")
    {
        for (int m = 0; m < kNumMacros; ++m)
            r.set (pid::macro (m), variation ? juce::jlimit (0.0f, 1.0f, r.get (pid::macro (m)) + r.range (-0.12f, 0.12f)) : r.uni());
        return;
    }
    if (sec == "mpe")
    {
        r.set ("mpeRange", (float) r.oneOf ({ 12, 24, 48, 48, 96 }));
        return;
    }
    if ((sec == "age" || sec == "circuit") && ! variation)
    {
        r.set (sec == "age" ? "an_age" : "an_circuit", r.range (0.15f, 0.85f));
        return;
    }
    if (sec == "converter" && ! variation)
    {
        r.set ("an_bits", r.range (0.15f, 0.7f));
        r.set ("an_rate", r.range (0.0f, 0.6f));
        return;
    }
    if (sec.startsWith ("mod") && sec.substring (3).containsOnly ("0123456789"))
    {
        const int slot = juce::jlimit (1, kNumModSlots, sec.substring (3).getIntValue()) - 1;
        if (variation)
        {
            r.set (pid::mod (slot, "amt"), juce::jlimit (-1.0f, 1.0f, r.get (pid::mod (slot, "amt")) + r.range (-0.12f, 0.12f)));
            return;
        }
        // let the matrix randomizer pick a musical source / destination, then keep only its first slot
        auto& shadow = p.getRandomShadow();
        for (auto* ap : p.getParameters())
            if (auto* q = dynamic_cast<juce::RangedAudioParameter*> (ap))
                if (auto* t = shadow.apvts.getParameter (q->getParameterID())) t->setValueNotifyingHost (q->getValue());
        randomize (shadow, Matrix, false, seed);
        for (auto* f : { "src", "dst", "amt" })
            r.set (pid::mod (slot, f), shadow.apvts.getRawParameterValue (pid::mod (0, f))->load());
        return;
    }

    SectionInfo info;
    if (! sectionInfo (sec, info)) return;

    // run the page's musical randomizer on a hidden copy of the plugin, then take only this section's values
    auto& shadow = p.getRandomShadow();
    for (auto* ap : p.getParameters())
        if (auto* q = dynamic_cast<juce::RangedAudioParameter*> (ap))
            if (auto* t = shadow.apvts.getParameter (q->getParameterID())) t->setValueNotifyingHost (q->getValue());
    if (sec == "fx:glt") shadow.glitchPattern.fromVar (p.glitchPattern.toVar());

    if (variation)
    {
        R rs (shadow, seed);
        for (auto* ap : shadow.getParameters())
        {
            auto* q = dynamic_cast<juce::RangedAudioParameter*> (ap);
            if (q == nullptr || ! info.owns (q->getParameterID()) || q->getParameterID().endsWith ("_on")) continue;
            const bool discrete = dynamic_cast<juce::AudioParameterChoice*> (q) != nullptr || dynamic_cast<juce::AudioParameterBool*> (q) != nullptr;
            float n = q->getValue();
            if (discrete) { if (rs.chance (0.1f)) n = rs.uni(); }
            else if (dynamic_cast<juce::AudioParameterInt*> (q) != nullptr) { if (rs.chance (0.2f)) n += rs.range (-0.1f, 0.1f); }
            else n += (rs.uni() + rs.uni() + rs.uni() - 1.5f) * 0.1f;
            q->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, n));
        }
    }
    else
        randomize (shadow, info.page, false, seed);

    for (auto* ap : shadow.getParameters())
        if (auto* q = dynamic_cast<juce::RangedAudioParameter*> (ap))
        {
            const auto id = q->getParameterID();
            if (info.owns (id) && id != info.onSwitch) copyParam (shadow, p, id);
        }
    if (info.onSwitch.isNotEmpty()) r.set (info.onSwitch, 1.0f);   // rolling a section switches it on, so you hear it
    if (sec == "fx:glt" && ! variation)
        p.glitchPattern.randomize ((int) std::lround (r.get ("fx_glt_steps")), seed * 13u + 5u, 0.45f);
}

void randomizeChordsAll (PyraAudioProcessor& p, uint32_t seed)
{
    R r (p, seed);
    const int mood = (int) p.apvts.getRawParameterValue ("chord_mood")->load();
    r.set ("chord_key", (float) r.pick (12));
    if (mood > 0)
    {
        // everything follows the emotion: scale, rhythm, voicing, colours, length
        const auto& st = chordMoodStyle (mood);
        auto from = [&] (const std::vector<int>& v, int fallback) { return v.empty() ? fallback : v[(size_t) r.pick ((int) v.size())]; };
        r.set ("chord_scale", (float) from (st.scales, 2));
        r.set ("chord_count", (float) from (st.counts, 4));
        r.set ("chord_len", (float) from (st.lengths, 2));
        r.set ("chord_size", (float) from (st.sizes, 0));
        r.set ("chord_voicing", (float) from (st.voicings, 1));
        r.set ("chord_rhythm", (float) from (st.rhythms, 0));
        r.set ("chord_inv", r.chance (0.8f) ? 0.0f : (float) (1 + r.pick (3)));
        r.set ("chord_bass", r.chance (st.bassChance) ? 1.0f : 0.0f);
        r.set ("chord_strum", r.chance (st.strumChance) ? r.range (0.08f, 0.45f) : 0.0f);
        r.set ("chord_swing", r.chance (st.swingChance) ? r.range (0.15f, 0.45f) : 0.0f);
        r.set ("chord_complex", r.range (st.complexityLo, st.complexityHi));
        r.set ("chord_oct", (float) st.octave);
    }
    else
    {
        r.set ("chord_scale", (float) r.oneOf ({ 1, 1, 2, 2, 2, 3, 5, 6, 8 }));
        r.set ("chord_count", (float) r.oneOf ({ 4, 4, 4, 2, 3, 6, 8 }));
        r.set ("chord_len", (float) r.oneOf ({ 2, 2, 2, 1, 3 }));
        r.set ("chord_size", (float) r.oneOf ({ 0, 1, 5, 5, 2 }));
        r.set ("chord_voicing", (float) r.pick (4));
        r.set ("chord_inv", r.chance (0.75f) ? 0.0f : (float) (1 + r.pick (3)));
        r.set ("chord_rhythm", (float) r.oneOf ({ 0, 0, 1, 2, 3, 4, 5, 6 }));
        r.set ("chord_bass", r.chance (0.7f) ? 1.0f : 0.0f);
        r.set ("chord_strum", r.chance (0.3f) ? r.range (0.05f, 0.5f) : 0.0f);
        r.set ("chord_swing", r.chance (0.3f) ? r.range (0.1f, 0.5f) : 0.0f);
        r.set ("chord_complex", r.range (0.1f, 0.8f));
    }
    const auto s = p.chords.readSettings();
    p.chordPattern.randomizeMood (mood, s.count, s.scale, s.complexity, seed * 31u + 7u);
}

namespace
{
    juce::String randomPresetOf (PresetManager& pm, const juce::String& category, juce::Random& rnd)
    {
        juce::StringArray ids;
        for (auto& info : pm.getAll())
            if (category.isEmpty() || info.category == category) ids.add (info.id);
        if (ids.isEmpty()) for (auto& info : pm.getAll()) ids.add (info.id);
        return ids.isEmpty() ? juce::String() : ids[rnd.nextInt (ids.size())];
    }
}

void randomizeLayer (PyraAudioProcessor& master, int layer, bool variation, uint32_t seed)
{
    R r (master, seed);
    if (variation)
    {
        const auto lv = layerParam (layer, "level"), pn = layerParam (layer, "pan");
        r.set (lv, juce::jlimit (-24.0f, 3.0f, r.get (lv) + r.range (-2.5f, 2.5f)));
        r.set (pn, juce::jlimit (-1.0f, 1.0f, r.get (pn) + r.range (-0.15f, 0.15f)));
        return;
    }
    auto& snd = master.getOrCreateLayer (layer);
    auto& pm = snd.getPresetManager();
    juce::String cat;
    if (auto* cur = pm.find (pm.getCurrentId())) cat = cur->category;
    juce::Random rnd ((juce::int64) seed);
    const auto id = randomPresetOf (pm, cat, rnd);
    if (id.isNotEmpty()) pm.load (id);
    r.set (layerParam (layer, "on"), 1.0f);
}

void randomizeLayers (PyraAudioProcessor& master, bool variation, uint32_t seed)
{
    R r (master, seed);
    juce::Array<int> active;
    for (int k = 0; k < kNumLayers; ++k)
        if (r.get (layerParam (k, "on")) > 0.5f) active.add (k);

    if (variation)
    {
        for (int k : active) randomizeLayer (master, k, true, seed + (uint32_t) k * 977u);
        return;
    }

    // a fresh multi: 2 to 4 layers that play different roles
    if (active.size() <= 1)
    {
        active.clearQuick();
        const int n = 2 + r.pick (3);
        for (int k = 0; k < n; ++k) active.add (k);
        for (int k = 0; k < kNumLayers; ++k) r.set (layerParam (k, "on"), active.contains (k) ? 1.0f : 0.0f);
    }
    juce::StringArray roles { "Pad", "Bass", "Pluck", "Keys", "Lead", "Strings", "Arp", "Brass & Winds" };
    juce::Random rnd ((juce::int64) seed);
    for (int i = roles.size() - 1; i > 0; --i) roles.getReference (i).swapWith (roles.getReference (rnd.nextInt (i + 1)));
    // the bass, if any, always goes first in the list of roles, so a multi has a bottom
    if (roles.indexOf ("Bass") > 1) { roles.removeString ("Bass"); roles.insert (1, "Bass"); }

    for (int i = 0; i < active.size(); ++i)
    {
        const int k = active[i];
        auto& pm = master.getOrCreateLayer (k).getPresetManager();
        const auto id = randomPresetOf (pm, roles[i % roles.size()], rnd);
        if (id.isNotEmpty()) pm.load (id);
        r.set (layerParam (k, "level"), i == 0 ? 0.0f : r.range (-9.0f, -2.0f));
        r.set (layerParam (k, "pan"), i == 0 ? 0.0f : r.range (-0.35f, 0.35f));
        r.set (layerParam (k, "mute"), 0.0f);
        r.set (layerParam (k, "solo"), 0.0f);
    }
}

void randomizeParam (PyraAudioProcessor& p, const juce::String& id, bool variation, uint32_t seed)
{
    auto* q = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id));
    if (q == nullptr) return;
    if (variation)
    {
        R r (p, seed);
        const bool discrete = dynamic_cast<juce::AudioParameterChoice*> (q) != nullptr || dynamic_cast<juce::AudioParameterBool*> (q) != nullptr;
        float n = q->getValue();
        if (discrete) n = r.uni();
        else n += (r.uni() + r.uni() + r.uni() - 1.5f) * 0.12f;
        q->beginChangeGesture();
        q->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, n));
        q->endChangeGesture();
        return;
    }
    // the page's musical randomizer runs on a hidden copy; only this one value is taken
    Page page = Synth;
    bool found = false;
    for (auto pg : { Synth, Fx, Arp, Chords, Matrix, Chaos, Analog, Settings })
        if (isPageParam (pg, id)) { page = pg; found = true; break; }
    auto& shadow = p.getRandomShadow();
    for (auto* ap : p.getParameters())
        if (auto* src = dynamic_cast<juce::RangedAudioParameter*> (ap))
            if (auto* t = shadow.apvts.getParameter (src->getParameterID())) t->setValueNotifyingHost (src->getValue());
    if (found) randomize (shadow, page, false, seed);
    auto* t = shadow.apvts.getParameter (id);
    float n = t != nullptr ? t->getValue() : q->getValue();
    if (! found || std::abs (n - q->getValue()) < 1.0e-6f)
    {   // the page left it alone (or it is not on a page): any value in range
        juce::Random rnd ((juce::int64) seed);
        n = rnd.nextFloat();
    }
    q->beginChangeGesture();
    q->setValueNotifyingHost (n);
    q->endChangeGesture();
}

void randomizeGlitch (PyraAudioProcessor& p, uint32_t seed)
{
    const int steps = (int) std::lround (p.apvts.getRawParameterValue ("fx_glt_steps")->load());
    p.glitchPattern.randomize (steps, seed, 0.45f);
}

} // namespace randomizer
} // namespace pyra
