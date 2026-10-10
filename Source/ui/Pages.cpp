#include "Pages.h"
#include "PyraLookAndFeel.h"
#include "../Lang.h"
#include "../PluginProcessor.h"
#include "../dsp/Wavetables.h"

namespace pyra
{
using lang::tr;

namespace
{
    // Helpers to create & register controls
    struct Builder
    {
        UiContext& ui;
        juce::Component& parent;
        juce::OwnedArray<juce::Component>& owned;

        Knob* knob (const juce::String& id, const juce::String& label, bool modulatable = true)
        {
            auto* k = new Knob (ui.st, id, label, modulatable ? &ui.matrix : nullptr);
            owned.add (k);
            parent.addAndMakeVisible (k);
            ui.knobs.add (k);
            return k;
        }

        ParamCombo* combo (const juce::String& id, bool translate = true)
        {
            auto* c = new ParamCombo (ui.st, id, translate);
            owned.add (c);
            parent.addAndMakeVisible (c);
            return c;
        }

        ParamToggle* toggle (const juce::String& id, const juce::String& text = {})
        {
            auto* t = new ParamToggle (ui.st, id, text);
            owned.add (t);
            parent.addAndMakeVisible (t);
            return t;
        }
    };

    void drawSmallLabel (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r)
    {
        g.setColour (colours::textDim);
        g.setFont (PyraLookAndFeel::font (10.5f, true));
        g.drawText (t.toUpperCase(), r, juce::Justification::centredLeft);
    }

    void rowOfKnobs (std::initializer_list<juce::Component*> list, juce::Rectangle<int> area)
    {
        const int n = (int) list.size();
        if (n == 0) return;
        const int w = area.getWidth() / n;
        for (auto* c : list)
            if (c != nullptr) c->setBounds (area.removeFromLeft (w));
    }

    //==========================================================================
    class OscPanel : public Panel
    {
    public:
        OscPanel (UiContext& ui, int index) : osc (index), view (ui.st, index)
        {
            Builder b { ui, *this, owned };
            title = "OSC " + juce::String::charToString ((juce::juce_wchar) ('A' + index));
            titleX = 34;
            on = b.toggle (pid::osc (index, "on"));
            wt = b.combo (pid::osc (index, "wt"), false);
            addAndMakeVisible (view);

            for (auto* btn : { &prev, &next })
            {
                addAndMakeVisible (*btn);
                btn->setTooltip (tr ("Previous / next wavetable"));
            }
            prev.setButtonText ("<"); next.setButtonText (">");
            prev.onClick = [this] { step (-1); };
            next.onClick = [this] { step (1); };

            level = b.knob (pid::osc (index, "level"), "Level");
            pan = b.knob (pid::osc (index, "pan"), "Pan");
            pos = b.knob (pid::osc (index, "pos"), "WT Pos");
            oct = b.knob (pid::osc (index, "oct"), "Octave", false);
            semi = b.knob (pid::osc (index, "semi"), "Semi");
            fine = b.knob (pid::osc (index, "fine"), "Fine");
            voices = b.knob (pid::osc (index, "voices"), "Unison", false);
            detune = b.knob (pid::osc (index, "detune"), "Detune");
            blend = b.knob (pid::osc (index, "blend"), "Blend");
            width = b.knob (pid::osc (index, "width"), "Width");
            phase = b.knob (pid::osc (index, "phase"), "Phase", false);
            rand = b.knob (pid::osc (index, "rand"), "Random", false);
            warp = b.combo (pid::osc (index, "warp"));
            warpAmt = b.knob (pid::osc (index, "warpAmt"), "Warp");
            fmSrc = b.combo (pid::osc (index, "fmSrc"));
            fmAmt = b.knob (pid::osc (index, "fmAmt"), "FM");
            route = b.combo (pid::osc (index, "route"));
        }

        void step (int d)
        {
            const int n = wt->getNumItems();
            int id = wt->getSelectedId() + d;
            if (id < 1) id = n;
            if (id > n) id = 1;
            wt->setSelectedId (id);
        }

        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            drawSmallLabel (g, tr ("Warp"), { 10, 240, 96, 14 });
            drawSmallLabel (g, "FM", { 166, 240, 96, 14 });
            drawSmallLabel (g, tr ("Output"), { 322, 240, 90, 14 });
        }

        void resized() override
        {
            const int w = getWidth();
            on->setBounds (9, 4, 22, 18);
            wt->setBounds (96, 3, w - 96 - 64, 21);
            prev.setBounds (w - 62, 3, 26, 21);
            next.setBounds (w - 33, 3, 26, 21);
            view.setBounds (8, 30, w - 16, 82);
            rowOfKnobs ({ level, pan, pos, oct, semi, fine }, { 8, 115, w - 16, 58 });
            rowOfKnobs ({ voices, detune, blend, width, phase, rand }, { 8, 176, w - 16, 58 });
            warp->setBounds (10, 256, 92, 22);
            warpAmt->setBounds (104, 238, 58, 58);
            fmSrc->setBounds (166, 256, 92, 22);
            fmAmt->setBounds (260, 238, 58, 58);
            route->setBounds (322, 256, w - 330, 22);
        }

        void tick() { view.refresh(); }

        const int osc;
        WavetableView view;
        juce::TextButton prev, next;
        juce::OwnedArray<juce::Component> owned;
        ParamToggle* on; ParamCombo* wt, *warp, *fmSrc, *route;
        Knob* level, *pan, *pos, *oct, *semi, *fine, *voices, *detune, *blend, *width, *phase, *rand, *warpAmt, *fmAmt;
    };

    //==========================================================================
    class SubNoisePanel : public Panel
    {
    public:
        explicit SubNoisePanel (UiContext& ui)
        {
            Builder b { ui, *this, owned };
            subOn = b.toggle ("sub_on", "SUB");
            subShape = b.combo ("sub_shape");
            subRoute = b.combo ("sub_route");
            subOct = b.knob ("sub_oct", "Octave", false);
            subLevel = b.knob ("sub_level", "Level");
            subPan = b.knob ("sub_pan", "Pan", false);

            noiseOn = b.toggle ("noise_on", "NOISE");
            noiseType = b.combo ("noise_type");
            noiseRoute = b.combo ("noise_route");
            noiseLevel = b.knob ("noise_level", "Level");
            noiseColor = b.knob ("noise_color", "Color");
            noisePan = b.knob ("noise_pan", "Pan", false);
        }

        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            g.setColour (colours::edge);
            g.drawHorizontalLine (96, 10.0f, (float) getWidth() - 10.0f);
        }

        void resized() override
        {
            const int w = getWidth();
            subOn->setBounds (8, 5, 70, 20);
            subShape->setBounds (80, 4, 96, 21);
            subRoute->setBounds (182, 4, w - 190, 21);
            rowOfKnobs ({ subOct, subLevel, subPan }, { 8, 31, w - 16, 60 });

            noiseOn->setBounds (8, 102, 70, 20);
            noiseType->setBounds (80, 101, 96, 21);
            noiseRoute->setBounds (182, 101, w - 190, 21);
            rowOfKnobs ({ noiseLevel, noiseColor, noisePan }, { 8, 126, w - 16, 58 });
        }

        juce::OwnedArray<juce::Component> owned;
        ParamToggle* subOn, *noiseOn; ParamCombo* subShape, *subRoute, *noiseType, *noiseRoute;
        Knob* subOct, *subLevel, *subPan, *noiseLevel, *noiseColor, *noisePan;
    };

    //==========================================================================
    /** Approximate magnitude response of the selected filter. */
    class FilterView : public juce::Component
    {
    public:
        FilterView (juce::AudioProcessorValueTreeState& st, int f)
        {
            type = st.getRawParameterValue (pid::filt (f, "type"));
            cutoff = st.getRawParameterValue (pid::filt (f, "cutoff"));
            res = st.getRawParameterValue (pid::filt (f, "res"));
            on = st.getRawParameterValue (pid::filt (f, "on"));
            setInterceptsMouseClicks (false, false);
        }

        void refresh()
        {
            const float sig = type->load() * 1000.0f + cutoff->load() * 0.37f + res->load() * 13.0f + on->load() * 7.0f;
            if (sig != last) { last = sig; repaint(); }
        }

        static float magnitude (int t, float f, float fc, float r)
        {
            const float w = f / fc;
            const float k = 2.0f - 1.97f * r;
            auto lp2 = [] (float ww, float kk) { return 1.0f / std::sqrt ((1 - ww * ww) * (1 - ww * ww) + kk * kk * ww * ww); };
            auto hp2 = [] (float ww, float kk) { return ww * ww / std::sqrt ((1 - ww * ww) * (1 - ww * ww) + kk * kk * ww * ww); };
            auto bp2 = [] (float ww, float kk) { return kk * ww / std::sqrt ((1 - ww * ww) * (1 - ww * ww) + kk * kk * ww * ww); };
            switch (t)
            {
                case FLp12:   return lp2 (w, k);
                case FLp24:   return lp2 (w, 1.848f) * lp2 (w, 1.53f * (1.0f - r * 0.99f)) * (1.0f + r * 0.5f);
                case FLadder: return lp2 (w, 1.848f) * lp2 (w, 1.53f * (1.0f - r * 0.95f));
                case FHp12:   return hp2 (w, k);
                case FHp24:   return hp2 (w, 1.848f) * hp2 (w, 1.53f * (1.0f - r * 0.99f));
                case FBp12:   return bp2 (w, k);
                case FBp24:   return bp2 (w, 1.414f) * bp2 (w, 2.0f - 1.95f * r);
                case FNotch:  return std::abs (1 - w * w) / std::sqrt ((1 - w * w) * (1 - w * w) + k * k * w * w);
                case FPeak:   return 1.0f + bp2 (w, k) * r * 6.0f;
                case FAllpass: return 1.0f;
                case FCombPlus:
                case FCombMinus:
                {
                    const float fb = r * 0.97f * (t == FCombPlus ? 1.0f : -1.0f);
                    const float ph = juce::MathConstants<float>::twoPi * f / fc;
                    const float re = 1 - fb * std::cos (ph), im = fb * std::sin (ph);
                    return (1.0f - std::abs (fb) * 0.45f) / std::sqrt (re * re + im * im);
                }
                case FFormant:
                {
                    const float n = std::clamp (std::log (fc / 20.0f) / std::log (1000.0f), 0.0f, 1.0f);
                    static const float tb[5][3] = { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 280, 2250, 2900 },
                                                    { 450, 800, 2830 }, { 325, 700, 2530 } };
                    const float p = std::clamp (n * 4.0f, 0.0f, 3.999f);
                    const int vi = (int) p; const float vf = p - (float) vi;
                    const float kk = 1.0f / (3.0f + r * 22.0f);
                    float m = 0.0f;
                    const float gains[3] = { 1.0f, 0.65f, 0.35f };
                    for (int i = 0; i < 3; ++i)
                        m += gains[i] * bp2 (f / (tb[vi][i] + (tb[vi + 1][i] - tb[vi][i]) * vf), kk);
                    return m * 1.2f;
                }
                case FPhaser:
                {
                    float m = 1.0f;
                    for (float s : { 0.5f, 0.85f, 1.3f, 2.1f })
                        m *= 0.55f + 0.45f * std::abs (1.0f - 1.0f / (1.0f + std::pow (std::log2 (w / s), 2.0f) * 3.0f));
                    return m;
                }
                default: return 1.0f;
            }
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff0d0d10));
            g.fillRoundedRectangle (r, 5.0f);
            auto area = r.reduced (6.0f, 8.0f);

            g.setColour (colours::edge.withAlpha (0.6f));
            for (float f : { 100.0f, 1000.0f, 10000.0f })
            {
                const float x = area.getX() + std::log (f / 20.0f) / std::log (1000.0f) * area.getWidth();
                g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
            }
            const float zeroDb = area.getY() + area.getHeight() * 0.35f;
            g.drawHorizontalLine ((int) zeroDb, area.getX(), area.getRight());

            const int t = (int) std::lround (type->load());
            const float fc = cutoff->load(), rs = res->load();
            juce::Path p;
            for (int i = 0; i <= 160; ++i)
            {
                const float n = (float) i / 160.0f;
                const float f = 20.0f * std::pow (1000.0f, n);
                const float db = juce::Decibels::gainToDecibels (magnitude (t, f, fc, rs), -60.0f);
                const float y = juce::jlimit (area.getY(), area.getBottom(), zeroDb - db / 24.0f * area.getHeight() * 0.35f);
                const float x = area.getX() + n * area.getWidth();
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            const bool isOn = on->load() > 0.5f;
            juce::Path fill (p);
            fill.lineTo (area.getRight(), area.getBottom()); fill.lineTo (area.getX(), area.getBottom()); fill.closeSubPath();
            g.setColour ((isOn ? colours::accent : colours::textDim).withAlpha (0.15f));
            g.fillPath (fill);
            g.setColour (isOn ? colours::accent2 : colours::textDim);
            g.strokePath (p, juce::PathStrokeType (1.8f));
        }

    private:
        std::atomic<float>* type, *cutoff, *res, *on;
        float last = -1.0f;
    };

    class FilterPanel : public Panel
    {
    public:
        FilterPanel (UiContext& ui, int index) : view (ui.st, index)
        {
            Builder b { ui, *this, owned };
            title = tr ("FILTER") + " " + juce::String (index + 1);
            titleX = 34;
            on = b.toggle (pid::filt (index, "on"));
            type = b.combo (pid::filt (index, "type"), false);
            if (index == 0)
            {
                routing = b.combo ("filterRouting");
                routing->setTooltip (tr ("Serial: Filter 1 feeds Filter 2. Parallel: both filters side by side."));
            }
            cutoff = b.knob (pid::filt (index, "cutoff"), "Cutoff");
            res = b.knob (pid::filt (index, "res"), "Resonance");
            drive = b.knob (pid::filt (index, "drive"), "Drive");
            mix = b.knob (pid::filt (index, "mix"), "Mix");
            key = b.knob (pid::filt (index, "key"), "Key Track", false);
            env = b.knob (pid::filt (index, "env"), "Env 2");
            addAndMakeVisible (view);
        }

        void resized() override
        {
            const int w = getWidth();
            on->setBounds (9, 4, 22, 18);
            type->setBounds (120, 3, 120, 21);
            if (routing != nullptr) routing->setBounds (w - 118, 3, 108, 21);
            cutoff->setBounds (8, 30, 104, 120);
            rowOfKnobs ({ res, drive, mix }, { 112, 30, 198, 72 });
            rowOfKnobs ({ key, env }, { 112, 106, 132, 72 });
            view.setBounds (312, 32, w - 320, getHeight() - 42);
        }

        FilterView view;
        juce::OwnedArray<juce::Component> owned;
        ParamToggle* on; ParamCombo* type, *routing = nullptr;
        Knob* cutoff, *res, *drive, *mix, *key, *env;
    };

    //==========================================================================
    class EnvPanel : public Panel
    {
    public:
        explicit EnvPanel (UiContext& ui) : proc (ui.proc), view (ui.st)
        {
            Builder b { ui, *this, owned };
            for (int e = 0; e < kNumEnv; ++e)
            {
                auto* badge = new ModSourceBadge (SrcEnv1 + e, "ENV " + juce::String (e + 1));
                if (e == 0) badge->setTooltip (tr ("Env 1 always shapes the volume. Drag it onto a knob to also modulate that knob."));
                badge->onClick = [this, e] { select (e); };
                badges.add (badge);
                addAndMakeVisible (badge);

                Set s;
                s.k[0] = b.knob (pid::env (e, "dly"), "Delay", false);
                s.k[1] = b.knob (pid::env (e, "att"), "Attack", false);
                s.k[2] = b.knob (pid::env (e, "hold"), "Hold", false);
                s.k[3] = b.knob (pid::env (e, "dec"), "Decay", false);
                s.k[4] = b.knob (pid::env (e, "sus"), "Sustain", false);
                s.k[5] = b.knob (pid::env (e, "rel"), "Release", false);
                s.k[6] = b.knob (pid::env (e, "aCrv"), "Att Curve", false);
                s.k[7] = b.knob (pid::env (e, "dCrv"), "Dec Curve", false);
                s.k[8] = b.knob (pid::env (e, "rCrv"), "Rel Curve", false);
                sets.push_back (s);
            }
            addAndMakeVisible (view);
            select (0);
        }

        void select (int e)
        {
            current = e;
            for (int i = 0; i < kNumEnv; ++i)
            {
                badges[i]->setSelected (i == e);
                for (auto* k : sets[(size_t) i].k) k->setVisible (i == e);
            }
            view.setEnvelope (e);
        }

        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            g.setColour (colours::textDim);
            g.setFont (PyraLookAndFeel::font (11.0f));
            g.drawText (current == 0 ? tr ("Env 1 = volume") : tr ("Drag ENV onto a knob"),
                        juce::Rectangle<int> (186 + 3 * 52, 120, getWidth() - 194 - 3 * 52, 40), juce::Justification::centred);
        }

        void resized() override
        {
            for (int i = 0; i < kNumEnv; ++i) badges[i]->setBounds (8 + i * 64, 4, 60, 20);
            view.setBounds (8, 30, 172, getHeight() - 38);
            for (auto& s : sets)
            {
                rowOfKnobs ({ s.k[0], s.k[1], s.k[2], s.k[3], s.k[4], s.k[5] }, { 184, 30, getWidth() - 190, 76 });
                rowOfKnobs ({ s.k[6], s.k[7], s.k[8] }, { 184, 108, 3 * 52, 74 });
            }
        }

        void tick()
        {
            view.refresh();
            view.setLive (proc.engine.uiVoices.load() > 0 ? proc.engine.uiEnv[current].load() : 0.0f);
            for (int i = 0; i < kNumEnv; ++i)
                badges[i]->setLiveValue (proc.engine.uiVoices.load() > 0 ? proc.engine.uiEnv[i].load() : 0.0f);
        }

        struct Set { Knob* k[9]; };
        PyraAudioProcessor& proc;
        EnvelopeView view;
        juce::OwnedArray<ModSourceBadge> badges;
        juce::OwnedArray<juce::Component> owned;
        std::vector<Set> sets;
        int current = 0;
    };

    //==========================================================================
    class LfoPanel : public Panel
    {
    public:
        explicit LfoPanel (UiContext& ui) : proc (ui.proc), st (ui.st), view (ui.st)
        {
            Builder b { ui, *this, owned };
            for (int l = 0; l < kNumLfo; ++l)
            {
                auto* badge = new ModSourceBadge (SrcLfo1 + l, "LFO " + juce::String (l + 1));
                badge->onClick = [this, l] { select (l); };
                badges.add (badge);
                addAndMakeVisible (badge);

                Set s;
                s.shape = b.combo (pid::lfo (l, "shape"));
                s.mode = b.combo (pid::lfo (l, "mode"));
                s.uni = b.toggle (pid::lfo (l, "uni"), tr ("Unipolar"));
                s.sync = b.toggle (pid::lfo (l, "sync"), tr ("Sync"));
                s.div = b.combo (pid::lfo (l, "div"), false);
                s.rate = b.knob (pid::lfo (l, "rate"), "Rate");
                s.phase = b.knob (pid::lfo (l, "phase"), "Phase", false);
                s.delay = b.knob (pid::lfo (l, "delay"), "Delay", false);
                s.fade = b.knob (pid::lfo (l, "fade"), "Fade In", false);
                s.smooth = b.knob (pid::lfo (l, "smooth"), "Smooth", false);
                s.mode->setTooltip (tr ("Retrig: restarts on every note. Free: keeps running. Envelope: plays once."));
                sets.push_back (s);
            }
            addAndMakeVisible (view);
            select (0);
        }

        void select (int l)
        {
            current = l;
            for (int i = 0; i < kNumLfo; ++i)
            {
                badges[i]->setSelected (i == l);
                auto& s = sets[(size_t) i];
                for (juce::Component* c : { (juce::Component*) s.shape, (juce::Component*) s.mode, (juce::Component*) s.uni,
                                            (juce::Component*) s.sync, (juce::Component*) s.div, (juce::Component*) s.rate,
                                            (juce::Component*) s.phase, (juce::Component*) s.delay, (juce::Component*) s.fade,
                                            (juce::Component*) s.smooth })
                    c->setVisible (i == l);
            }
            view.setLfo (l);
        }

        void resized() override
        {
            for (int i = 0; i < kNumLfo; ++i) badges[i]->setBounds (8 + i * 64, 4, 60, 20);
            view.setBounds (8, 30, 172, getHeight() - 38);
            for (auto& s : sets)
            {
                s.shape->setBounds (186, 31, 116, 21);
                s.mode->setBounds (306, 31, 96, 21);
                s.uni->setBounds (408, 31, 88, 21);
                s.sync->setBounds (186, 58, 66, 21);
                s.div->setBounds (254, 58, 84, 21);
                rowOfKnobs ({ s.rate, s.phase, s.delay, s.fade, s.smooth }, { 184, 88, getWidth() - 190, 92 });
            }
        }

        void tick()
        {
            view.refresh();
            view.setLive (proc.engine.uiLfo[current].load());
            for (int i = 0; i < kNumLfo; ++i)
            {
                badges[i]->setLiveValue (proc.engine.uiVoices.load() > 0 ? proc.engine.uiLfo[i].load() : 0.0f);
                auto& s = sets[(size_t) i];
                const bool synced = s.sync->getToggleState();
                s.rate->setAlpha (synced ? 0.35f : 1.0f);
                s.div->setAlpha (synced ? 1.0f : 0.35f);
            }
        }

        struct Set
        {
            ParamCombo* shape, *mode, *div; ParamToggle* uni, *sync;
            Knob* rate, *phase, *delay, *fade, *smooth;
        };
        PyraAudioProcessor& proc;
        juce::AudioProcessorValueTreeState& st;
        LfoView view;
        juce::OwnedArray<ModSourceBadge> badges;
        juce::OwnedArray<juce::Component> owned;
        std::vector<Set> sets;
        int current = 0;
    };

    //==========================================================================
    class MacroPanel : public Panel
    {
    public:
        explicit MacroPanel (UiContext& ui) : Panel ("MACRO")
        {
            Builder b { ui, *this, owned };
            for (int m = 0; m < kNumMacros; ++m)
            {
                auto* badge = new ModSourceBadge (SrcMacro1 + m, "M" + juce::String (m + 1));
                badge->setTooltip (tr ("Drag onto any knob: one macro can move many knobs at once"));
                badges.add (badge);
                addAndMakeVisible (badge);
                knobs[m] = b.knob (pid::macro (m), "Macro " + juce::String (m + 1), false);
            }
        }

        void resized() override
        {
            const int cw = (getWidth() - 16) / 2, ch = (getHeight() - 30) / 2;
            for (int m = 0; m < kNumMacros; ++m)
            {
                const int x = 8 + (m % 2) * cw, y = 28 + (m / 2) * ch;
                badges[m]->setBounds (x + 4, y, 34, 18);
                knobs[m]->setBounds (x, y + 14, cw, ch - 14);
            }
        }

        juce::OwnedArray<ModSourceBadge> badges;
        juce::OwnedArray<juce::Component> owned;
        Knob* knobs[kNumMacros] {};
    };

    //==========================================================================
    class SynthPage : public Page
    {
    public:
        explicit SynthPage (UiContext& ui) : subNoise (ui), envs (ui), lfos (ui), macros (ui)
        {
            for (int o = 0; o < kNumOsc; ++o) addAndMakeVisible (oscs.add (new OscPanel (ui, o)));
            for (int f = 0; f < kNumFilters; ++f) addAndMakeVisible (filters.add (new FilterPanel (ui, f)));
            for (auto* c : std::initializer_list<juce::Component*> { &subNoise, &envs, &lfos, &macros }) addAndMakeVisible (c);

            // global mod sources without a panel of their own
            const std::pair<int, const char*> extra[] = { { SrcVelocity, "VEL" }, { SrcNote, "NOTE" }, { SrcModWheel, "WHEEL" },
                                                          { SrcPressure, "PRESS" }, { SrcPitchBend, "BEND" }, { SrcRandom, "RAND" } };
            for (auto& e : extra)
            {
                auto* b = new ModSourceBadge (e.first, e.second);
                b->setTooltip (tr (choices::modSources()[e.first]) + " - " + tr ("drag onto a knob to modulate it"));
                addAndMakeVisible (sources.add (b));
            }
        }

        void resized() override
        {
            const int m = 8, w = getWidth();
            const int oscW = (w - 2 * m - 2 * m) / 3;
            for (int o = 0; o < kNumOsc; ++o) oscs[o]->setBounds (m + o * (oscW + m), m, oscW, 300);

            const int y2 = 316, h2 = 186;
            subNoise.setBounds (m, y2, 300, h2);
            const int fw = (w - 2 * m - 300 - 2 * m) / 2;
            filters[0]->setBounds (m + 300 + m, y2, fw, h2);
            filters[1]->setBounds (m + 300 + m + fw + m, y2, fw, h2);

            const int y3 = y2 + h2 + m, h3 = getHeight() - y3 - m - 26;
            envs.setBounds (m, y3, 500, h3);
            lfos.setBounds (m + 500 + m, y3, 500, h3);
            macros.setBounds (m + 1000 + 2 * m, y3, w - (m + 1000 + 2 * m) - m, h3);

            int x = m;
            for (auto* s : sources) { s->setBounds (x, getHeight() - 24, 62, 20); x += 66; }
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (colours::textDim);
            g.setFont (PyraLookAndFeel::font (11.0f));
            g.drawText (tr ("More mod sources: drag them onto a knob"), 8 + 6 * 66 + 6, getHeight() - 24, 400, 20,
                        juce::Justification::centredLeft);
        }

        void tick() override
        {
            for (auto* o : oscs) o->tick();
            for (auto* f : filters) f->view.refresh();
            envs.tick();
            lfos.tick();
        }

        juce::OwnedArray<OscPanel> oscs;
        juce::OwnedArray<FilterPanel> filters;
        SubNoisePanel subNoise;
        EnvPanel envs;
        LfoPanel lfos;
        MacroPanel macros;
        juce::OwnedArray<ModSourceBadge> sources;
    };

    //==========================================================================
    class MatrixPage : public Page
    {
    public:
        explicit MatrixPage (UiContext& ui) : matrix (ui.matrix)
        {
            for (int s = 0; s < kNumModSlots; ++s)
            {
                Row r;
                r.src = std::make_unique<ParamCombo> (ui.st, pid::mod (s, "src"));
                r.dst = std::make_unique<ParamCombo> (ui.st, pid::mod (s, "dst"), false);
                r.amt = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
                r.amt->setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
                r.amt->setColour (juce::Slider::trackColourId, colours::mod);
                r.amt->setColour (juce::Slider::backgroundColourId, colours::edge);
                r.amt->setColour (juce::Slider::thumbColourId, colours::text);
                r.amt->setDoubleClickReturnValue (true, 0.0);
                r.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (ui.st, pid::mod (s, "amt"), *r.amt);
                r.amt->textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
                r.amt->updateText();
                r.clear = std::make_unique<juce::TextButton> ("x");
                r.clear->setTooltip (tr ("Clear this slot"));
                r.clear->onClick = [this, s] { matrix.clearSlot (s); };
                for (juce::Component* c : { (juce::Component*) r.src.get(), (juce::Component*) r.dst.get(),
                                            (juce::Component*) r.amt.get(), (juce::Component*) r.clear.get() })
                    addAndMakeVisible (c);
                rows.push_back (std::move (r));
            }
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (colours::accent);
            g.setFont (PyraLookAndFeel::font (16.0f, true));
            g.drawText (tr ("MODULATION MATRIX"), 16, 8, 400, 26, juce::Justification::centredLeft);
            g.setColour (colours::textDim);
            g.setFont (PyraLookAndFeel::font (12.5f));
            g.drawText (tr ("Tip: on the SYNTH page you can drag ENV, LFO, MACRO or any source badge straight onto a knob."),
                        16, 34, getWidth() - 32, 20, juce::Justification::centredLeft);

            const int colW = (getWidth() - 48) / 2;
            for (int c = 0; c < 2; ++c)
            {
                const int x = 16 + c * (colW + 16);
                g.setFont (PyraLookAndFeel::font (11.0f, true));
                g.drawText (tr ("SOURCE"), x + 34, 64, 150, 18, juce::Justification::centredLeft);
                g.drawText (tr ("AMOUNT"), x + 194, 64, 150, 18, juce::Justification::centredLeft);
                g.drawText (tr ("DESTINATION"), x + colW - 236, 64, 180, 18, juce::Justification::centredLeft);
            }
            for (int s = 0; s < kNumModSlots; ++s)
            {
                const auto b = rowBounds (s);
                g.setColour (colours::panel);
                g.fillRoundedRectangle (b.toFloat(), 6.0f);
                g.setColour (colours::textDim);
                g.setFont (PyraLookAndFeel::font (12.0f, true));
                g.drawText (juce::String (s + 1), b.withWidth (30), juce::Justification::centred);
            }
        }

        juce::Rectangle<int> rowBounds (int s) const
        {
            const int colW = (getWidth() - 48) / 2;
            const int c = s / 8, r = s % 8;
            return { 16 + c * (colW + 16), 86 + r * 52, colW, 44 };
        }

        void resized() override
        {
            for (int s = 0; s < kNumModSlots; ++s)
            {
                auto b = rowBounds (s).reduced (6, 10);
                b.removeFromLeft (26);
                auto& r = rows[(size_t) s];
                r.src->setBounds (b.removeFromLeft (150));
                b.removeFromLeft (10);
                r.clear->setBounds (b.removeFromRight (26));
                b.removeFromRight (8);
                r.dst->setBounds (b.removeFromRight (200));
                b.removeFromRight (10);
                r.amt->setBounds (b);
            }
        }

        struct Row
        {
            std::unique_ptr<ParamCombo> src, dst;
            std::unique_ptr<juce::Slider> amt;
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
            std::unique_ptr<juce::TextButton> clear;
        };
        ModMatrixAccess& matrix;
        std::vector<Row> rows;
    };

    //==========================================================================
    class GlobalPage : public Page
    {
    public:
        GlobalPage (UiContext& ui, std::function<void()> onLang)
            : voice (tr ("VOICE")), mpePanel ("MPE"), language (tr ("LANGUAGE")),
              about (tr ("ABOUT")), onLanguageChanged (std::move (onLang))
        {
            for (auto* p : { &voice, &mpePanel, &language, &about }) addAndMakeVisible (p);

            Builder bv { ui, voice, owned };
            poly = bv.knob ("polyphony", "Voices", false);
            mode = bv.combo ("voiceMode");
            glide = bv.knob ("glide", "Glide", false);
            glideLegato = bv.toggle ("glideLegato", tr ("Glide only when legato"));
            bendUp = bv.knob ("bendUp", "Bend Up", false);
            bendDown = bv.knob ("bendDown", "Bend Down", false);
            velAmp = bv.knob ("velAmp", "Vel > Amp", false);
            transpose = bv.knob ("transpose", "Transpose", false);
            tune = bv.knob ("tune", "Tune", false);
            master = bv.knob ("master", "Volume", false);

            Builder bm { ui, mpePanel, owned };
            mpe = bm.toggle ("mpe", tr ("MPE on"));
            mpeRange = bm.knob ("mpeRange", "Bend Range", false);


            for (auto* btn : { &en, &it })
            {
                language.addAndMakeVisible (*btn);
                btn->setClickingTogglesState (false);
            }
            en.setButtonText ("English"); it.setButtonText ("Italiano");
            en.setToggleState (lang::current() == lang::English, juce::dontSendNotification);
            it.setToggleState (lang::current() == lang::Italian, juce::dontSendNotification);
            en.onClick = [this] { setLanguage (lang::English); };
            it.onClick = [this] { setLanguage (lang::Italian); };

            about.addAndMakeVisible (aboutText);
            aboutText.setMultiLine (true);
            aboutText.setReadOnly (true);
            aboutText.setCaretVisible (false);
            aboutText.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
            aboutText.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            aboutText.setFont (PyraLookAndFeel::font (13.5f));
            aboutText.setText (tr ("ABOUT_TEXT"));

            voiceHelp = tr ("Poly = chords. Mono = one note, retriggers. Legato = one note, slides without retriggering.");
        }

        void setLanguage (lang::Language l)
        {
            if (l == lang::current()) return;
            lang::set (l);
            juce::MessageManager::callAsync ([cb = onLanguageChanged] { if (cb) cb(); });
        }

        void paint (juce::Graphics&) override {}

        void paintOverChildren (juce::Graphics& g) override
        {
            g.setColour (colours::textDim);
            g.setFont (PyraLookAndFeel::font (12.0f));
            g.drawFittedText (voiceHelp, voice.getBounds().removeFromBottom (40).reduced (14, 4), juce::Justification::centredLeft, 2);
        }

        void resized() override
        {
            const int m = 8;
            voice.setBounds (m, m, 820, 250);
            mpePanel.setBounds (m + 820 + m, m, getWidth() - 836 - 2 * m, 250);
            language.setBounds (m, 266, 820, 140);
            about.setBounds (m + 828 + m, 266, getWidth() - 836 - 2 * m, getHeight() - 266 - m);

            rowOfKnobs ({ poly, glide, bendUp, bendDown, velAmp, transpose, tune, master }, { 10, 34, 800, 90 });
            mode->setBounds (14, 140, 140, 24);
            glideLegato->setBounds (170, 140, 260, 24);

            mpe->setBounds (14, 34, 120, 24);
            mpeRange->setBounds (14, 70, 90, 90);


            en.setBounds (14, 40, 130, 34);
            it.setBounds (154, 40, 130, 34);

            aboutText.setBounds (about.getLocalBounds().withTrimmedTop (28).reduced (10, 4));
        }

        Panel voice, mpePanel, language, about;
        std::function<void()> onLanguageChanged;
        juce::OwnedArray<juce::Component> owned;
        Knob* poly, *glide, *bendUp, *bendDown, *velAmp, *transpose, *tune, *master, *mpeRange;
        ParamCombo* mode; ParamToggle* glideLegato, *mpe;
        juce::TextButton en, it;
        juce::TextEditor aboutText;
        juce::String voiceHelp;
    };

    //==========================================================================
    class ComingSoonPage : public Page
    {
    public:
        ComingSoonPage (juce::String t, juce::StringArray l) : title (std::move (t)), lines (std::move (l)) {}

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().reduced (60, 50);
            g.setColour (colours::panel);
            g.fillRoundedRectangle (r.toFloat(), 10.0f);
            g.setColour (colours::edge);
            g.drawRoundedRectangle (r.toFloat(), 10.0f, 1.0f);

            auto inner = r.reduced (40, 36);
            g.setColour (colours::accent);
            g.setFont (PyraLookAndFeel::font (26.0f, true));
            g.drawText (title, inner.removeFromTop (40), juce::Justification::centredLeft);
            g.setColour (colours::accent2);
            g.setFont (PyraLookAndFeel::font (14.0f, true));
            g.drawText (tr ("Coming in the next build"), inner.removeFromTop (26), juce::Justification::centredLeft);
            inner.removeFromTop (14);
            g.setColour (colours::text);
            g.setFont (PyraLookAndFeel::font (15.0f));
            for (auto& l : lines)
                g.drawFittedText (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa2  ")) + l, inner.removeFromTop (30),
                                  juce::Justification::centredLeft, 1);
        }

        juce::String title;
        juce::StringArray lines;
    };
}

std::unique_ptr<Page> createSynthPage (UiContext& ui)  { return std::make_unique<SynthPage> (ui); }
std::unique_ptr<Page> createMatrixPage (UiContext& ui) { return std::make_unique<MatrixPage> (ui); }
std::unique_ptr<Page> createGlobalPage (UiContext& ui, std::function<void()> cb) { return std::make_unique<GlobalPage> (ui, std::move (cb)); }
std::unique_ptr<Page> createComingSoonPage (const juce::String& t, const juce::StringArray& l) { return std::make_unique<ComingSoonPage> (t, l); }

} // namespace pyra
