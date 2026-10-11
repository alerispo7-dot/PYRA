#include "Pages.h"
#include "Prefs.h"
#include "PyraLookAndFeel.h"
#include "../Lang.h"
#include "../PluginProcessor.h"
#include "../dsp/Wavetables.h"
#include "../Randomizer.h"
#include "HellArt.h"

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

        ParamCombo* combo (const juce::String& id, bool translate = true, const char* groups = nullptr)
        {
            auto* c = new ParamCombo (ui.st, id, translate, groups != nullptr ? ParamCombo::groupsFor (groups) : nullptr);
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
            setPowerButton (on);
            wt = b.combo (pid::osc (index, "wt"), false, "wt");
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
            enableDice ([pr = &ui.proc, index] (bool v) { randomizer::randomizeSection (*pr, "osc" + juce::String (index), v, (uint32_t) juce::Random::getSystemRandom().nextInt()); });
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
            drawInset (g, { 5.0f, 113.0f, (float) getWidth() - 10.0f, 122.0f });
            drawInset (g, { 5.0f, 237.0f, (float) getWidth() - 10.0f, 58.0f });
            drawSmallLabel (g, tr ("Warp"), { 10, 240, 96, 14 });
            drawSmallLabel (g, "FM", { 166, 240, 96, 14 });
            drawSmallLabel (g, tr ("Output"), { 322, 240, 90, 14 });
        }

        void resized() override
        {
            const int w = getWidth();
            on->setBounds (9, 4, 22, 18);
            wt->setBounds (96, 3, w - 96 - 64 - diceSpace, 21);
            prev.setBounds (w - 62 - diceSpace, 3, 26, 21);
            next.setBounds (w - 33 - diceSpace, 3, 26, 21);
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
            subShape = b.combo ("sub_shape", true, "sub");
            subRoute = b.combo ("sub_route");
            subOct = b.knob ("sub_oct", "Octave", false);
            subLevel = b.knob ("sub_level", "Level");
            subPan = b.knob ("sub_pan", "Pan", false);

            noiseOn = b.toggle ("noise_on", "NOISE");
            noiseType = b.combo ("noise_type", true, "noise");
            noiseRoute = b.combo ("noise_route");
            noiseLevel = b.knob ("noise_level", "Level");
            noiseColor = b.knob ("noise_color", "Color");
            noisePan = b.knob ("noise_pan", "Pan", false);
            enableDice ([pr = &ui.proc] (bool v) { randomizer::randomizeSection (*pr, "subnoise", v, (uint32_t) juce::Random::getSystemRandom().nextInt()); });
        }

        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            drawInset (g, { 5.0f, 29.0f, (float) getWidth() - 10.0f, 64.0f });
            drawInset (g, { 5.0f, 124.0f, (float) getWidth() - 10.0f, (float) getHeight() - 129.0f });
            g.setColour (tint.withAlpha (0.25f));
            g.fillRect (10.0f, 96.0f, (float) getWidth() - 20.0f, 1.0f);
        }

        void resized() override
        {
            const int w = getWidth();
            subOn->setBounds (8, 5, 70, 20);
            subShape->setBounds (80, 4, 96, 21);
            subRoute->setBounds (182, 4, w - 190 - diceSpace, 21);
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
    class FilterView : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        FilterView (juce::AudioProcessorValueTreeState& st, int f)
        {
            type = st.getRawParameterValue (pid::filt (f, "type"));
            cutoff = st.getRawParameterValue (pid::filt (f, "cutoff"));
            res = st.getRawParameterValue (pid::filt (f, "res"));
            on = st.getRawParameterValue (pid::filt (f, "on"));
            cutG.p = st.getParameter (pid::filt (f, "cutoff"));
            resG.p = st.getParameter (pid::filt (f, "res"));
            setMouseCursor (juce::MouseCursor::CrosshairCursor);
            setTooltip (tr ("Drag the point: left / right = cutoff, up / down = resonance. Double-click: reset."));
        }

        juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (6.0f, 8.0f); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (auto& hook = userActivityHook()) hook();
            cutG.begin(); resG.begin();
            mouseDrag (e);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            auto a = plot();
            const float nx = juce::jlimit (0.0f, 1.0f, (e.position.x - a.getX()) / a.getWidth());
            const float hz = 20.0f * std::pow (1000.0f, nx);
            if (cutG.p != nullptr) cutG.set (cutG.p->convertTo0to1 (hz));
            const float ny = juce::jlimit (0.0f, 1.0f, (a.getBottom() - e.position.y) / a.getHeight());
            if (resG.p != nullptr) resG.set (resG.p->convertTo0to1 (ny));
            if (auto& hook = userActivityHook()) hook();
        }

        void mouseUp (const juce::MouseEvent&) override { cutG.end(); resG.end(); }

        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            for (auto* g : { &cutG, &resG }) { g->begin(); if (g->p != nullptr) g->set (g->p->getDefaultValue()); g->end(); }
        }

        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }

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
                case FLp6:    return 1.0f / std::sqrt (1.0f + w * w);
                case FHp6:    return w / std::sqrt (1.0f + w * w);
                case FLp36:   return lp2 (w, 1.932f) * lp2 (w, 1.414f) * lp2 (w, 1.414f * (1.0f - r * 0.99f)) * (1.0f + r * 0.4f);
                case FHp36:   return hp2 (w, 1.932f) * hp2 (w, 1.414f) * hp2 (w, 1.414f * (1.0f - r * 0.99f));
                case FNotch24:
                {
                    const float n1 = std::abs (1 - w * w) / std::sqrt ((1 - w * w) * (1 - w * w) + k * k * w * w);
                    const float k2 = k * 0.8f;
                    return n1 * std::abs (1 - w * w) / std::sqrt ((1 - w * w) * (1 - w * w) + k2 * k2 * w * w);
                }
                case FLowShelf:
                case FHighShelf:
                {
                    const float A = std::pow (10.0f, (r * 24.0f - 12.0f) / 20.0f);
                    const float part = t == FLowShelf ? lp2 (w, 1.414f) : hp2 (w, 1.414f);
                    return std::max (0.001f, 1.0f + (A - 1.0f) * part);
                }
                case FDiode:  return lp2 (w, 1.6f * (1.0f - r * 0.9f) + 0.1f) / std::sqrt (1.0f + w * w) * (1.0f + r * 0.4f);
                case FMs20:   return std::min (6.0f, lp2 (w, 2.0f - 1.995f * r) * (1.0f + r * 1.5f) * 0.9f);
                case FDualBp: return (bp2 (w, 2.0f - 1.9f * r) + 0.8f * bp2 (w / 2.6f, 2.0f - 1.9f * r)) * 1.1f;
                case FResonator: { const float kk = 0.012f + (1.0f - r) * 0.25f; return bp2 (w, kk) * 1.6f; }
                default: return 1.0f;
            }
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (colours::display);
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
            g.setColour ((isOn ? colours::accent : colours::textDim).withAlpha (0.3f));
            g.strokePath (p, juce::PathStrokeType (4.0f));
            g.setColour (isOn ? colours::lavaCore : colours::textDim);
            g.strokePath (p, juce::PathStrokeType (1.8f));

            // the point you can drag: x = cutoff, y = resonance
            const float px = area.getX() + juce::jlimit (0.0f, 1.0f, std::log (fc / 20.0f) / std::log (1000.0f)) * area.getWidth();
            const float py = area.getBottom() - rs * area.getHeight();
            const bool hot = isMouseOver (true) || cutG.active;
            g.setColour (colours::accent.withAlpha (hot ? 0.4f : 0.2f));
            g.fillEllipse (px - 8.0f, py - 8.0f, 16.0f, 16.0f);
            g.setColour (juce::Colour (0xfff0e6df));
            g.fillEllipse (px - 4.0f, py - 4.0f, 8.0f, 8.0f);
            g.setColour (colours::accent);
            g.drawEllipse (px - 4.0f, py - 4.0f, 8.0f, 8.0f, 1.2f);
        }

    private:
        ParamGesture cutG, resG;
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
            tint = colours::hot;
            titleX = 34;
            on = b.toggle (pid::filt (index, "on"));
            setPowerButton (on);
            type = b.combo (pid::filt (index, "type"), false, "filter");
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
            enableDice ([pr = &ui.proc, index] (bool v) { randomizer::randomizeSection (*pr, "filter" + juce::String (index), v, (uint32_t) juce::Random::getSystemRandom().nextInt()); });
        }

        void resized() override
        {
            const int w = getWidth();
            on->setBounds (9, 4, 22, 18);
            type->setBounds (120, 3, 120, 21);
            if (routing != nullptr) routing->setBounds (w - 118 - diceSpace, 3, 108, 21);
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
            tint = colours::mod;
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
            enableDice ([this, pr = &ui.proc] (bool v)
            {
                randomizer::randomizeSection (*pr, "env" + juce::String (current + 1), v, (uint32_t) juce::Random::getSystemRandom().nextInt());
            });
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
            const int kx = viewW + 12, kw = (getWidth() - kx - 6) / 6;
            g.drawText (current == 0 ? tr ("Env 1 = volume") : tr ("Drag ENV onto a knob"),
                        juce::Rectangle<int> (kx + 4 * kw, 120, getWidth() - kx - 4 * kw - 8, 40), juce::Justification::centred);
        }

        static constexpr int viewW = 150;

        void resized() override
        {
            for (int i = 0; i < kNumEnv; ++i) badges[i]->setBounds (8 + i * 64, 4, 60, 20);
            view.setBounds (8, 30, viewW, getHeight() - 38);
            const int kx = viewW + 12, kw = (getWidth() - kx - 6) / 6;
            for (auto& s : sets)
            {
                rowOfKnobs ({ s.k[0], s.k[1], s.k[2], s.k[3], s.k[4], s.k[5] }, { kx, 30, kw * 6, 76 });
                rowOfKnobs ({ s.k[6], s.k[7], s.k[8] }, { kx, 108, kw * 4, 74 });
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
        explicit LfoPanel (UiContext& ui) : proc (ui.proc), st (ui.st), view (ui.st, &ui.proc.lfoDrawn)
        {
            tint = colours::mod;
            Builder b { ui, *this, owned };
            for (int l = 0; l < kNumLfo; ++l)
            {
                auto* badge = new ModSourceBadge (SrcLfo1 + l, "LFO " + juce::String (l + 1));
                badge->onClick = [this, l] { select (l); };
                badges.add (badge);
                addAndMakeVisible (badge);

                Set s;
                s.shape = b.combo (pid::lfo (l, "shape"), true, "lfo");
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
            enableDice ([this, pr = &ui.proc] (bool v)
            {
                randomizer::randomizeSection (*pr, "lfo" + juce::String (current + 1), v, (uint32_t) juce::Random::getSystemRandom().nextInt());
            });
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
            view.setBounds (8, 30, 150, getHeight() - 38);
            const int x = 164, w = getWidth() - x - 6 - diceSpace;
            for (auto& s : sets)
            {
                s.shape->setBounds (x, 31, w * 40 / 100, 21);
                s.mode->setBounds (x + w * 40 / 100 + 4, 31, w * 32 / 100, 21);
                s.uni->setBounds (x + w * 72 / 100 + 8, 31, w - w * 72 / 100 - 8, 21);
                s.sync->setBounds (x, 58, 66, 21);
                s.div->setBounds (x + 70, 58, 84, 21);
                rowOfKnobs ({ s.rate, s.phase, s.delay, s.fade, s.smooth }, { x - 2, 88, w + 2, 92 });
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
            tint = colours::accent2;
            Builder b { ui, *this, owned };
            for (int m = 0; m < kNumMacros; ++m)
            {
                auto* badge = new ModSourceBadge (SrcMacro1 + m, "M" + juce::String (m + 1));
                badge->setTooltip (tr ("Drag onto any knob: one macro can move many knobs at once"));
                badges.add (badge);
                addAndMakeVisible (badge);
                knobs[m] = b.knob (pid::macro (m), "Macro " + juce::String (m + 1), false);
            }
            enableDice ([pr = &ui.proc] (bool v) { randomizer::randomizeSection (*pr, "macro", v, (uint32_t) juce::Random::getSystemRandom().nextInt()); });
        }

        void resized() override
        {
            // one row of four; the badges sit in the header
            const int cw = (getWidth() - 16) / kNumMacros;
            for (int m = 0; m < kNumMacros; ++m)
            {
                const int x = 8 + m * cw;
                badges[m]->setBounds (x + (cw - 32) / 2, getHeight() - 20, 32, 17);
                knobs[m]->setBounds (x, 20, cw, getHeight() - 40);
            }
        }

        juce::OwnedArray<ModSourceBadge> badges;
        juce::OwnedArray<juce::Component> owned;
        Knob* knobs[kNumMacros] {};
    };

    //==========================================================================
    /** Voice mode + portamento (glide), right on the main page. */
    class VoicePanel : public Panel
    {
    public:
        explicit VoicePanel (UiContext& ui) : Panel (tr ("VOICE & PORTAMENTO"))
        {
            tint = colours::accent2;
            Builder b { ui, *this, owned };
            mode = b.combo ("voiceMode");
            type = b.combo ("glide_type");
            glide = b.knob ("glide", "Portamento", false);
            legato = b.toggle ("glideLegato", tr ("Only legato"));
            mode->setTooltip (tr ("Poly = chords. Mono = one note, retriggers. Legato = one note, slides without retriggering."));
            type->setTooltip (tr ("Constant Time: every slide takes the same time. Constant Rate: bigger jumps take longer."));
            glide->setTooltip (tr ("Portamento: notes slide into each other. 0 = off. Works in Poly, Mono and Legato."));
            legato->setTooltip (tr ("Slide only when you play a note while another is still held"));
            enableDice ([pr = &ui.proc] (bool v) { randomizer::randomizeSection (*pr, "voice", v, (uint32_t) juce::Random::getSystemRandom().nextInt()); });
        }

        void resized() override
        {
            const int w = getWidth(), cw = (w - 106 - diceSpace) / 2;
            glide->setBounds (w - 90 - diceSpace, 4, 82, getHeight() - 6);
            mode->setBounds (10, 30, cw, 22);
            type->setBounds (14 + cw, 30, cw, 22);
            legato->setBounds (10, 57, w - 110 - diceSpace, 20);
        }

        juce::OwnedArray<juce::Component> owned;
        ParamCombo* mode, *type; Knob* glide; ParamToggle* legato;
    };

    //==========================================================================
    class SynthPage : public Page
    {
    public:
        explicit SynthPage (UiContext& ui) : subNoise (ui), envs (ui), lfos (ui), macros (ui), voice (ui)
        {
            addAndMakeVisible (voice);
            for (int o = 0; o < kNumOsc; ++o) addAndMakeVisible (oscs.add (new OscPanel (ui, o)));
            for (int f = 0; f < kNumFilters; ++f) addAndMakeVisible (filters.add (new FilterPanel (ui, f)));
            for (auto* c : std::initializer_list<juce::Component*> { &subNoise, &envs, &lfos, &macros }) addAndMakeVisible (c);

            // global mod sources without a panel of their own
            const std::pair<int, const char*> extra[] = { { SrcVelocity, "VEL" }, { SrcNote, "NOTE" }, { SrcModWheel, "WHEEL" },
                                                          { SrcPressure, "PRESS" }, { SrcPitchBend, "BEND" }, { SrcRandom, "RAND" },
                                                          { SrcChaos, "CHAOS" }, { SrcMotion, "MOTION" } };
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
            const int ew = 452;
            envs.setBounds (m, y3, ew, h3);
            lfos.setBounds (m + ew + m, y3, ew, h3);
            const int rx = m + 2 * ew + 2 * m, rw = w - rx - m;
            const int vh = 84;
            voice.setBounds (rx, y3, rw, vh);
            macros.setBounds (rx, y3 + vh + 6, rw, h3 - vh - 6);

            int x = m;
            for (auto* s : sources) { s->setBounds (x, getHeight() - 24, 62, 20); x += 66; }
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (colours::textDim);
            g.setFont (PyraLookAndFeel::font (11.0f));
            g.drawText (tr ("More mod sources: drag them onto a knob"), 8 + 8 * 66 + 6, getHeight() - 24, 400, 20,
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
        VoicePanel voice;
        juce::OwnedArray<ModSourceBadge> sources;
    };

    //==========================================================================
    class MatrixPage : public Page
    {
    public:
        explicit MatrixPage (UiContext& ui) : matrix (ui.matrix), st (ui.st), frame (tr ("MODULATION MATRIX")), backdrop (*this)
        {
            frame.tint = colours::mod;
            addAndMakeVisible (frame);
            addAndMakeVisible (backdrop);
            backdrop.setInterceptsMouseClicks (false, false);
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
                r.dice = std::make_unique<DiceButton>();
                r.dice->onRoll = [pr = &ui.proc, s] (bool v)
                {
                    randomizer::randomizeSection (*pr, "mod" + juce::String (s + 1), v, (uint32_t) juce::Random::getSystemRandom().nextInt());
                };
                addAndMakeVisible (r.dice.get());
                r.clear = std::make_unique<juce::TextButton> ("x");
                r.clear->setTooltip (tr ("Clear this slot"));
                r.clear->onClick = [this, s] { matrix.clearSlot (s); };
                for (juce::Component* c : { (juce::Component*) r.src.get(), (juce::Component*) r.dst.get(),
                                            (juce::Component*) r.amt.get(), (juce::Component*) r.clear.get() })
                    addAndMakeVisible (c);
                rows.push_back (std::move (r));
            }
        }

        /** Drawn between the stone frame and the controls: column titles, the slabs of the rows, and the routing map. */
        struct Backdrop : public juce::Component
        {
            explicit Backdrop (MatrixPage& p) : page (p) {}
            void paint (juce::Graphics& g) override { page.paintBackdrop (g); }
            MatrixPage& page;
        };

        juce::Rectangle<float> mapArea() const
        {
            const auto last = rowBounds (7);
            return juce::Rectangle<float> (16.0f, (float) last.getBottom() + 12.0f, (float) getWidth() - 32.0f,
                                           (float) getHeight() - (float) last.getBottom() - 30.0f);
        }

        void paintBackdrop (juce::Graphics& g)
        {
            g.setColour (colours::textDim);
            g.setFont (PyraLookAndFeel::font (12.5f));
            g.drawText (tr ("Tip: on the SYNTH page you can drag ENV, LFO, MACRO or any source badge straight onto a knob."),
                        16, 34, getWidth() - 32, 20, juce::Justification::centredLeft);

            const int colW = (getWidth() - 48) / 2;
            for (int c = 0; c < 2; ++c)
            {
                const int x = 16 + c * (colW + 16);
                g.setFont (PyraLookAndFeel::titleFont (12.0f));
                g.drawText (tr ("SOURCE"), x + 34, 64, 150, 18, juce::Justification::centredLeft);
                g.drawText (tr ("AMOUNT"), x + 194, 64, 150, 18, juce::Justification::centredLeft);
                g.drawText (tr ("DESTINATION"), x + colW - 236, 64, 180, 18, juce::Justification::centredLeft);
            }
            for (int s = 0; s < kNumModSlots; ++s)
            {
                const auto b = rowBounds (s).toFloat();
                const bool used = std::abs (st.getRawParameterValue (pid::mod (s, "amt"))->load()) > 0.001f
                                  && st.getRawParameterValue (pid::mod (s, "src"))->load() > 0.5f;
                art::drawStoneSlab (g, b, 6.0f, (uint32_t) s * 131u + 7u, used ? 0.42f : 0.6f);
                if (used)
                {
                    g.setColour (colours::mod.withAlpha (0.7f));
                    g.fillRoundedRectangle (b.getX() + 2.0f, b.getY() + 8.0f, 2.0f, b.getHeight() - 16.0f, 1.0f);
                }
                g.setColour (used ? colours::text : colours::textDim);
                g.setFont (PyraLookAndFeel::font (12.0f, true));
                g.drawText (juce::String (s + 1), b.withWidth (30.0f), juce::Justification::centred);
            }

            // ROUTING: every active slot as a vein from its source to its destination
            auto a = mapArea();
            if (a.getHeight() < 60.0f) return;
            g.setColour (colours::textDim);
            g.setFont (PyraLookAndFeel::titleFont (12.0f));
            g.drawText (tr ("ROUTING"), a.removeFromTop (18.0f), juce::Justification::centredLeft);
            juce::Path well; well.addRoundedRectangle (a, 6.0f);
            g.setColour (juce::Colour (0xb0060404));
            g.fillPath (well);
            art::drawCarvedFrame (g, a, 6.0f);

            struct Link { juce::String src, dst; float amt; };
            std::vector<Link> links;
            juce::StringArray srcs, dsts;
            for (int s = 0; s < kNumModSlots; ++s)
            {
                auto* sp = dynamic_cast<juce::AudioParameterChoice*> (st.getParameter (pid::mod (s, "src")));
                auto* dp = dynamic_cast<juce::AudioParameterChoice*> (st.getParameter (pid::mod (s, "dst")));
                const float amt = st.getRawParameterValue (pid::mod (s, "amt"))->load();
                if (sp == nullptr || dp == nullptr || sp->getIndex() == 0 || dp->getIndex() == 0 || std::abs (amt) < 0.001f) continue;
                links.push_back ({ tr (sp->getCurrentChoiceName()), dp->getCurrentChoiceName(), amt });
                srcs.addIfNotAlreadyThere (links.back().src);
                dsts.addIfNotAlreadyThere (links.back().dst);
            }
            auto inner = a.reduced (18.0f, 12.0f);
            if (links.empty())
            {
                g.setColour (colours::textDim.withAlpha (0.7f));
                g.setFont (PyraLookAndFeel::font (13.0f));
                g.drawText (tr ("Nothing is modulated yet. Pick a source, an amount and a destination above."), inner, juce::Justification::centred);
                return;
            }
            const float nameW = 190.0f;
            auto yOf = [&] (int i, int n) { return inner.getY() + inner.getHeight() * ((float) i + 0.5f) / (float) n; };
            for (const auto& l : links)
            {
                const float y0 = yOf (srcs.indexOf (l.src), srcs.size()), y1 = yOf (dsts.indexOf (l.dst), dsts.size());
                const float x0 = inner.getX() + nameW + 8.0f, x1 = inner.getRight() - nameW - 8.0f;
                juce::Path p;
                p.startNewSubPath (x0, y0);
                p.cubicTo (x0 + (x1 - x0) * 0.45f, y0, x1 - (x1 - x0) * 0.45f, y1, x1, y1);
                art::drawLavaVein (g, p, 0.8f + 2.2f * std::abs (l.amt), 0.5f + 0.5f * std::abs (l.amt), l.amt >= 0.0f ? colours::mod : colours::accent);
            }
            g.setFont (PyraLookAndFeel::font (13.0f, true));
            for (int i = 0; i < srcs.size(); ++i)
            {
                auto r = juce::Rectangle<float> (inner.getX(), yOf (i, srcs.size()) - 11.0f, nameW, 22.0f);
                art::drawStoneSlab (g, r, 4.0f, (uint32_t) i * 17u, 0.5f);
                g.setColour (colours::text);
                g.drawText (srcs[i], r.reduced (8.0f, 0.0f), juce::Justification::centredRight, true);
            }
            for (int i = 0; i < dsts.size(); ++i)
            {
                auto r = juce::Rectangle<float> (inner.getRight() - nameW, yOf (i, dsts.size()) - 11.0f, nameW, 22.0f);
                art::drawStoneSlab (g, r, 4.0f, (uint32_t) i * 29u + 3u, 0.5f);
                g.setColour (colours::text);
                g.drawText (dsts[i], r.reduced (8.0f, 0.0f), juce::Justification::centredLeft, true);
            }
            g.setColour (colours::textDim);
            g.setFont (PyraLookAndFeel::font (11.0f));
            g.drawText (tr ("violet = adds, red = subtracts, thicker = more"), a.reduced (10.0f, 4.0f).removeFromBottom (14.0f),
                        juce::Justification::centredRight);
        }

        void tick() override
        {
            int sig = 0;
            for (int s = 0; s < kNumModSlots; ++s)
                sig = sig * 31 + (int) st.getRawParameterValue (pid::mod (s, "src"))->load() * 7
                      + (int) st.getRawParameterValue (pid::mod (s, "dst"))->load() * 13
                      + (int) std::lround (st.getRawParameterValue (pid::mod (s, "amt"))->load() * 50.0f);
            if (sig != lastSig) { lastSig = sig; backdrop.repaint(); }
        }

        juce::Rectangle<int> rowBounds (int s) const
        {
            const int colW = (getWidth() - 48) / 2;
            const int c = s / 8, r = s % 8;
            return { 16 + c * (colW + 16), 86 + r * 52, colW, 44 };
        }

        void resized() override
        {
            frame.setBounds (getLocalBounds().reduced (8, 6));
            backdrop.setBounds (getLocalBounds());
            for (int s = 0; s < kNumModSlots; ++s)
            {
                auto b = rowBounds (s).reduced (6, 10);
                b.removeFromLeft (26);
                auto& r = rows[(size_t) s];
                r.src->setBounds (b.removeFromLeft (150));
                b.removeFromLeft (10);
                r.clear->setBounds (b.removeFromRight (26));
                b.removeFromRight (4);
                r.dice->setBounds (b.removeFromRight (24));
                b.removeFromRight (6);
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
            std::unique_ptr<DiceButton> dice;
        };
        ModMatrixAccess& matrix;
        juce::AudioProcessorValueTreeState& st;
        Panel frame;
        Backdrop backdrop;
        int lastSig = -1;
        std::vector<Row> rows;
    };

    //==========================================================================
    class GlobalPage : public Page
    {
    public:
        GlobalPage (UiContext& ui, std::function<void()> onLang, std::function<void (int)> onScale, std::function<int()> getScale)
            : voice (tr ("VOICE")), mpePanel ("MPE"), language (tr ("LANGUAGE")), personal (tr ("PERSONALIZE")),
              about (tr ("ABOUT")), onLanguageChanged (std::move (onLang)), onWindowScale (std::move (onScale)), windowPercent (std::move (getScale))
        {
            for (auto* p : { &voice, &mpePanel, &language, &personal, &about }) addAndMakeVisible (p);
            mpePanel.tint = colours::accent2; language.tint = colours::accent2; about.tint = colours::hot; personal.tint = colours::accent;

            Builder bv { ui, voice, owned };
            poly = bv.knob ("polyphony", "Voices", false);
            mode = bv.combo ("voiceMode");
            glide = bv.knob ("glide", "Glide", false);
            glideLegato = bv.toggle ("glideLegato", tr ("Glide only when legato"));
            glideType = bv.combo ("glide_type");
            bendUp = bv.knob ("bendUp", "Bend Up", false);
            bendDown = bv.knob ("bendDown", "Bend Down", false);
            velAmp = bv.knob ("velAmp", "Vel > Amp", false);
            transpose = bv.knob ("transpose", "Transpose", false);
            tune = bv.knob ("tune", "Tune", false);
            master = bv.knob ("master", "Volume", false);

            voice.enableDice ([pr = &ui.proc] (bool v) { randomizer::randomizeSection (*pr, "voice", v, (uint32_t) juce::Random::getSystemRandom().nextInt()); });
            mpePanel.enableDice ([pr = &ui.proc] (bool v) { randomizer::randomizeSection (*pr, "mpe", v, (uint32_t) juce::Random::getSystemRandom().nextInt()); });
            Builder bm { ui, mpePanel, owned };
            mpe = bm.toggle ("mpe", tr ("MPE on"));
            mpeRange = bm.knob ("mpeRange", "Bend Range", false);

            // LANGUAGE: one button per language, each written in its own language
            for (int l = 0; l < lang::NumLanguages; ++l)
            {
                auto* btn = langButtons.add (new juce::TextButton (lang::nativeName ((lang::Language) l)));
                language.addAndMakeVisible (btn);
                btn->setClickingTogglesState (false);
                btn->setToggleState (lang::current() == (lang::Language) l, juce::dontSendNotification);
                btn->onClick = [this, l] { setLanguage ((lang::Language) l); };
            }

            // PERSONALIZE: how the window behaves (saved for you, in every project)
            auto toggle = [this] (prefs::Key k, const juce::String& text, const juce::String& tip)
            {
                auto* t = new juce::ToggleButton (text);
                t->setToggleState (prefs::get (k) != 0, juce::dontSendNotification);
                t->onClick = [t, k] { prefs::set (k, t->getToggleState() ? 1 : 0); };
                t->setTooltip (tip);
                personal.addAndMakeVisible (t); prefControls.add (t);
                return t;
            };
            auto combo = [this] (prefs::Key k, const juce::StringArray& items, const juce::String& tip)
            {
                auto* c = new juce::ComboBox();
                for (int i = 0; i < items.size(); ++i) c->addItem (items[i], i + 1);
                c->setSelectedId (prefs::get (k) + 1, juce::dontSendNotification);
                c->onChange = [c, k] { prefs::set (k, c->getSelectedId() - 1); };
                c->setTooltip (tip);
                personal.addAndMakeVisible (c); prefControls.add (c);
                return c;
            };
            helpToggle = toggle (prefs::HelpBubbles, tr ("On"), tr ("A small bubble explains a control when the mouse rests on it. Switch it off when you know PYRA well."));
            helpDelay = combo (prefs::HelpDelay, { tr ("Fast"), tr ("Normal"), tr ("Slow") }, tr ("How long the mouse has to rest before the help bubble appears"));
            animToggle = toggle (prefs::Animations, tr ("On"), tr ("Moving lava, scope and glowing pictures. Off saves a little CPU."));
            valuesCombo = combo (prefs::ShowValues, { tr ("Names (values on hover)"), tr ("Values (names on hover)") }, tr ("What is written under the knobs"));
            dragCombo = combo (prefs::KnobDrag, { tr ("Up / down"), tr ("Around the knob") }, tr ("How you turn a knob with the mouse"));
            speedCombo = combo (prefs::KnobSpeed, { tr ("Fine"), tr ("Normal"), tr ("Fast") }, tr ("How far a knob turns for each movement of the mouse (Shift = extra fine)"));
            wheelToggle = toggle (prefs::WheelKnobs, tr ("On"), tr ("The mouse wheel turns the knob under the mouse"));
            startCombo = combo (prefs::StartPage, { tr ("Last page used"), "SYNTH" }, tr ("Which page opens when you open PYRA"));
            // window size: - / + in steps of 5%, or straight to a size. 50% = the size PYRA first opens at.
            auto stepSize = [this] (int delta)
            {
                if (! onWindowScale || ! windowPercent) return;
                const int pct = windowPercent();
                onWindowScale (delta > 0 ? (pct / 5 + 1) * 5 : ((pct + 4) / 5 - 1) * 5);
            };
            sizeMinus.setButtonText (juce::String::charToString (0x2212));
            sizePlus.setButtonText ("+");
            sizeMinus.setTooltip (tr ("Smaller window"));
            sizePlus.setTooltip (tr ("Bigger window"));
            sizeMinus.onClick = [stepSize] { stepSize (-1); };
            sizePlus.onClick = [stepSize] { stepSize (1); };
            sizeMinus.setRepeatSpeed (400, 120);
            sizePlus.setRepeatSpeed (400, 120);
            personal.addAndMakeVisible (sizeMinus);
            personal.addAndMakeVisible (sizePlus);
            for (int pct : { 30, 40, 50, 75, 100 })
            {
                auto* b = sizeButtons.add (new juce::TextButton (juce::String (pct) + "%"));
                b->setTooltip (tr ("Size of the PYRA window (you can also drag its corner)"));
                b->onClick = [this, pct] { if (onWindowScale) onWindowScale (pct); };
                personal.addAndMakeVisible (b);
            }
            resetPrefs.setButtonText (tr ("RESET"));
            resetPrefs.setTooltip (tr ("Back to the original settings"));
            resetPrefs.onClick = [this] { prefs::resetAll(); juce::MessageManager::callAsync ([cb = onLanguageChanged] { if (cb) cb(); }); };
            personal.addAndMakeVisible (resetPrefs);

            about.addAndMakeVisible (aboutText);
            aboutText.setMultiLine (true);
            aboutText.setReadOnly (true);
            aboutText.setCaretVisible (false);
            aboutText.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
            aboutText.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            aboutText.setFont (PyraLookAndFeel::font (13.5f));
            aboutText.setText (tr ("ABOUT_TEXT") + "\n\n" + tr ("SAMPLE_CREDITS"));

            voiceHelp = tr ("Poly = chords. Mono = one note, retriggers. Legato = one note, slides without retriggering.");
        }

        void setLanguage (lang::Language l)
        {
            if (l == lang::current()) return;
            lang::set (l);
            juce::MessageManager::callAsync ([cb = onLanguageChanged] { if (cb) cb(); });
        }

        void paintOverChildren (juce::Graphics& g) override
        {
            g.setColour (colours::textDim);
            g.setFont (PyraLookAndFeel::font (12.0f));
            g.drawFittedText (voiceHelp, voice.getBounds().removeFromBottom (40).reduced (14, 4), juce::Justification::centredLeft, 2);

            // labels of the PERSONALIZE rows
            auto pb = personal.getBounds();
            auto label = [&] (juce::Component* c, const juce::String& text)
            {
                auto r = c->getBounds().translated (pb.getX(), pb.getY());
                g.setColour (colours::text);
                g.setFont (PyraLookAndFeel::font (12.5f, true));
                g.drawText (text, r.getX() - 196, r.getY(), 188, r.getHeight(), juce::Justification::centredRight, true);
            };
            label (helpToggle, tr ("Help bubbles"));
            label (helpDelay, tr ("Help delay"));
            label (animToggle, tr ("Animations"));
            label (valuesCombo, tr ("Under the knobs"));
            label (dragCombo, tr ("Knob drag"));
            label (speedCombo, tr ("Knob speed"));
            label (wheelToggle, tr ("Mouse wheel on knobs"));
            label (startCombo, tr ("Open on"));
            label (&sizeMinus, tr ("Window size"));
            {
                auto r = sizeValueArea().translated (pb.getX(), pb.getY());
                g.setColour (colours::display);
                g.fillRoundedRectangle (r.toFloat(), 4.0f);
                g.setColour (colours::accent2);
                g.setFont (PyraLookAndFeel::font (14.0f, true));
                g.drawText (juce::String (windowPercent ? windowPercent() : shownPercent) + "%", r, juce::Justification::centred);
            }
            // the version, kept out of the way
            g.setColour (colours::textDim.withAlpha (0.6f));
            g.setFont (PyraLookAndFeel::font (11.0f));
            g.drawText ("PYRA v" JucePlugin_VersionString, about.getRight() - 150, about.getY() + 6, 138, 16, juce::Justification::centredRight);
        }

        juce::Rectangle<int> sizeValueArea() const { return sizeMinus.getBounds().withX (sizeMinus.getRight() + 4).withWidth (58); }

        void tick() override
        {
            const int pct = windowPercent ? windowPercent() : 50;
            if (pct != shownPercent)
            {
                shownPercent = pct;
                for (int i = 0; i < sizeButtons.size(); ++i)
                    sizeButtons[i]->setToggleState (sizeButtons[i]->getButtonText().getIntValue() == pct, juce::dontSendNotification);
                repaint();
            }
        }

        void resized() override
        {
            const int m = 8;
            voice.setBounds (m, m, 820, 250);
            mpePanel.setBounds (m + 820 + m, m, getWidth() - 836 - 2 * m, 250);
            language.setBounds (m, 266, 820, 140);
            personal.setBounds (m, 414, 820, getHeight() - 422);
            about.setBounds (m + 828 + m, 266, getWidth() - 836 - 2 * m, getHeight() - 266 - m);

            rowOfKnobs ({ poly, glide, bendUp, bendDown, velAmp, transpose, tune, master }, { 10, 34, 800, 90 });
            mode->setBounds (14, 140, 140, 24);
            glideLegato->setBounds (170, 140, 240, 24);
            glideType->setBounds (420, 140, 170, 24);

            mpe->setBounds (14, 34, 120, 24);
            mpeRange->setBounds (14, 70, 90, 90);

            for (int i = 0; i < langButtons.size(); ++i)
                langButtons[i]->setBounds (14 + i * 133, 48, 125, 36);

            // two columns: label | control
            const int rowH = 36, y0 = 40;
            auto place = [&] (juce::Component* c, int col, int row, int w)
            {
                c->setBounds (col == 0 ? 206 : 612, y0 + row * rowH, w, 26);
            };
            place (helpToggle, 0, 0, 80);   place (helpDelay, 0, 1, 180);
            place (animToggle, 0, 2, 80);   place (valuesCombo, 0, 3, 190);
            place (dragCombo, 1, 0, 190);   place (speedCombo, 1, 1, 190);
            place (wheelToggle, 1, 2, 80);  place (startCombo, 1, 3, 190);
            sizeMinus.setBounds (206, y0 + 4 * rowH + 8, 30, 28);
            sizePlus.setBounds (sizeValueArea().getRight() + 4, y0 + 4 * rowH + 8, 30, 28);
            for (int i = 0; i < sizeButtons.size(); ++i)
                sizeButtons[i]->setBounds (sizePlus.getRight() + 14 + i * 60, y0 + 4 * rowH + 8, 54, 28);
            resetPrefs.setBounds (personal.getWidth() - 14 - 120, y0 + 4 * rowH + 8, 120, 28);

            aboutText.setBounds (about.getLocalBounds().withTrimmedTop (28).reduced (10, 4));
        }

        Panel voice, mpePanel, language, personal, about;
        std::function<void()> onLanguageChanged;
        std::function<void (int)> onWindowScale;
        std::function<int()> windowPercent;
        juce::TextButton sizeMinus, sizePlus;
        int shownPercent = -1;
        juce::OwnedArray<juce::Component> owned, prefControls;
        Knob* poly, *glide, *bendUp, *bendDown, *velAmp, *transpose, *tune, *master, *mpeRange;
        ParamCombo* mode, *glideType; ParamToggle* glideLegato, *mpe;
        juce::OwnedArray<juce::TextButton> langButtons, sizeButtons;
        juce::ToggleButton* helpToggle, *animToggle, *wheelToggle;
        juce::ComboBox* helpDelay, *valuesCombo, *dragCombo, *speedCombo, *startCombo;
        juce::TextButton resetPrefs;
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
std::unique_ptr<Page> createGlobalPage (UiContext& ui, std::function<void()> cb, std::function<void (int)> scale, std::function<int()> getScale)
{
    return std::make_unique<GlobalPage> (ui, std::move (cb), std::move (scale), std::move (getScale));
}
std::unique_ptr<Page> createComingSoonPage (const juce::String& t, const juce::StringArray& l) { return std::make_unique<ComingSoonPage> (t, l); }

} // namespace pyra
