// Phase 3 pages: FX, ANALOG, ARP
#include "Pages.h"
#include "PyraLookAndFeel.h"
#include "../Lang.h"
#include "../PluginProcessor.h"
#include "../Randomizer.h"

namespace pyra
{
using lang::tr;

namespace
{
    struct Builder
    {
        UiContext& ui;
        juce::Component& parent;
        juce::OwnedArray<juce::Component>& owned;

        Knob* knob (const juce::String& id, const juce::String& label)
        {
            auto* k = new Knob (ui.st, id, label, nullptr);
            owned.add (k); parent.addAndMakeVisible (k); ui.knobs.add (k);
            return k;
        }
        ParamCombo* combo (const juce::String& id, bool translate = true)
        {
            auto* c = new ParamCombo (ui.st, id, translate);
            owned.add (c); parent.addAndMakeVisible (c);
            return c;
        }
        ParamToggle* toggle (const juce::String& id, const juce::String& text = {})
        {
            auto* t = new ParamToggle (ui.st, id, text);
            owned.add (t); parent.addAndMakeVisible (t);
            return t;
        }
    };

    void layoutGrid (const juce::Array<juce::Component*>& items, juce::Rectangle<int> area, int perRow, int rowH)
    {
        const int w = area.getWidth() / perRow;
        for (int i = 0; i < items.size(); ++i)
            items[i]->setBounds (area.getX() + (i % perRow) * w, area.getY() + (i / perRow) * rowH, w, rowH);
    }

    void drawText (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, float size, juce::Colour c,
                   juce::Justification j = juce::Justification::topLeft, bool bold = false, int lines = 4)
    {
        g.setColour (c);
        g.setFont (PyraLookAndFeel::font (size, bold));
        g.drawFittedText (t, r, j, lines, 1.0f);
    }

    //==========================================================================
    /** One effect module: title + on switch + optional combos + knobs. */
    class FxPanel : public Panel
    {
    public:
        FxPanel (UiContext& ui, const juce::String& titleText, const juce::String& prefix,
                 std::initializer_list<const char*> comboIds,
                 std::initializer_list<std::pair<const char*, const char*>> knobDefs)
        {
            Builder b { ui, *this, owned };
            title = titleText;
            titleX = 34;
            on = b.toggle ("fx_" + prefix + "_on");
            for (auto* c : comboIds)
            {
                const juce::String id (c);
                if (id.endsWith ("_sync")) syncToggle = b.toggle (id, tr ("Sync"));
                else if (id.endsWith ("_freeze")) syncToggle = b.toggle (id, tr ("Freeze"));
                else combos.add (b.combo (id, ! id.endsWith ("_div") && ! id.endsWith ("_rate")));
            }
            for (auto& [id, label] : knobDefs) knobs.add (b.knob (id, label));
            enableDice ([pr = &ui.proc, sec = "fx:" + prefix] (bool v)
            {
                randomizer::randomizeSection (*pr, sec, v, (uint32_t) juce::Random::getSystemRandom().nextInt());
            });
        }

        void resized() override
        {
            const int w = getWidth();
            on->setBounds (9, 4, 22, 18);
            int y = 30;
            if (! combos.isEmpty() || syncToggle != nullptr)
            {
                int x = 10;
                if (syncToggle != nullptr) { syncToggle->setBounds (x, y, 74, 22); x += 78; }
                const int cw = juce::jmin (130, (w - x - 10) / juce::jmax (1, combos.size()) - 6);
                for (auto* c : combos) { c->setBounds (x, y, cw, 22); x += cw + 6; }
                y += 30;
            }
            juce::Array<juce::Component*> list;
            for (auto* k : knobs) list.add (k);
            const int perRow = knobs.size() <= 4 ? knobs.size() : (knobs.size() + 1) / 2;
            const int rows = (knobs.size() + perRow - 1) / perRow;
            const int rowH = juce::jmin (rows == 1 ? 120 : 92, (getHeight() - y - 6) / juce::jmax (1, rows));
            layoutGrid (list, { 6, y, w - 12, rowH * rows }, juce::jmax (1, perRow), rowH);
        }

        juce::OwnedArray<juce::Component> owned;
        ParamToggle* on = nullptr, *syncToggle = nullptr;
        juce::Array<ParamCombo*> combos;
        juce::Array<Knob*> knobs;
    };

    class CompPanel : public FxPanel
    {
    public:
        CompPanel (UiContext& ui) : FxPanel (ui, tr ("COMPRESSOR"), "comp", {},
                       { { "fx_comp_thresh", "Threshold" }, { "fx_comp_ratio", "Ratio" }, { "fx_comp_att", "Attack" },
                         { "fx_comp_rel", "Release" }, { "fx_comp_makeup", "Makeup" }, { "fx_comp_mix", "Mix" } }),
                    proc (ui.proc) {}

        void paint (juce::Graphics& g) override
        {
            FxPanel::paint (g);
            // gain reduction meter in the header
            const float gr = juce::jlimit (0.0f, 24.0f, proc.fx.gainReductionDb.load());
            auto r = juce::Rectangle<float> ((float) getWidth() - 110.0f - diceSpace, 8.0f, 96.0f, 10.0f);
            g.setColour (colours::knobBody);
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (colours::mod);
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * gr / 24.0f), 3.0f);
            g.setColour (colours::textDim);
            g.setFont (PyraLookAndFeel::font (10.0f));
            g.drawText ("GR", r.translated (-24.0f, 0).withWidth (22.0f), juce::Justification::centredRight);
        }

        void tick()
        {
            const float gr = proc.fx.gainReductionDb.load();
            if (std::abs (gr - lastGr) > 0.2f) { lastGr = gr; repaint (getWidth() - 170, 0, 170, 26); }
        }

        PyraAudioProcessor& proc;
        float lastGr = -1.0f;
    };

    class ChainInfo : public Panel
    {
    public:
        ChainInfo() : Panel (tr ("SIGNAL CHAIN")) { tint = colours::accent2; }
        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            const juce::StringArray names { "DISTORTION", "RESONATOR", "FILTER", "SHIFTER", "GRANULAR", "CHORUS", "FLANGER",
                                            "PHASER", "TAPE", "DELAY", "REVERB", "GLITCH", "EQ", "COMPRESSOR" };
            auto r = getLocalBounds().reduced (12, 0).withTrimmedTop (30);
            const int colW = r.getWidth() / 2;
            for (int i = 0; i < names.size(); ++i)
            {
                const int col = i / 7, row = i % 7;
                auto cell = juce::Rectangle<int> (r.getX() + col * colW, r.getY() + row * 20, colW, 19);
                const bool creative = names[i] == "SHIFTER" || names[i] == "GRANULAR" || names[i] == "GLITCH";
                drawText (g, juce::String (i + 1) + ".  " + tr (names[i]), cell, 12.0f, creative ? colours::accent2 : colours::text,
                          juce::Justification::centredLeft, false, 1);
            }
            drawText (g, tr ("Orange = on the second FX page. Switched-off effects use no CPU."),
                      r.withTrimmedTop (7 * 20 + 4), 11.0f, colours::textDim);
        }
    };

    //==========================================================================
    juce::Colour glitchColour (int fx)
    {
        switch (fx)
        {
            case GRepeat:   return colours::accent;
            case GStutter:  return colours::accent2;
            case GReverse:  return colours::mod;
            case GTapeStop: return juce::Colour (0xffb07cff);
            case GSlow:     return juce::Colour (0xff6c8cff);
            case GPitchUp:  return juce::Colour (0xff5be3a0);
            case GScratch:  return juce::Colour (0xffff5fa2);
            case GGate:     return juce::Colour (0xff9a97a3);
            case GCrush:    return colours::hot;
            default:        return colours::knobBody;
        }
    }

    /** GLITCH: a step sequencer of effects (repeat, reverse, tape stop...), like Gross Beat / Effectrix. */
    class GlitchPanel : public Panel
    {
    public:
        explicit GlitchPanel (UiContext& ui) : proc (ui.proc), pattern (ui.proc.glitchPattern)
        {
            title = tr ("GLITCH");
            titleX = 34;
            tint = colours::hot;
            Builder b { ui, *this, owned };
            on = b.toggle ("fx_glt_on");
            rate = b.combo ("fx_glt_rate", false);
            rate->setTooltip (tr ("Length of one step"));
            steps = b.knob ("fx_glt_steps", "Steps");
            chance = b.knob ("fx_glt_chance", "Random");
            smooth = b.knob ("fx_glt_smooth", "Smooth");
            mix = b.knob ("fx_glt_mix", "Mix");
            chance->setTooltip (tr ("Random effects on top of the pattern: 0 = only your steps, 100% = every step can glitch"));
            smooth->setTooltip (tr ("Crossfade between steps: low = hard cuts, high = smoother"));
            stepsValue = ui.st.getRawParameterValue ("fx_glt_steps");

            for (auto* btn : { &randomBtn, &clearBtn, &unlockBtn }) addAndMakeVisible (*btn);
            randomBtn.setButtonText (tr ("RANDOM"));
            clearBtn.setButtonText (tr ("CLEAR"));
            unlockBtn.setButtonText (tr ("UNLOCK ALL"));
            randomBtn.setColour (juce::TextButton::buttonColourId, colours::accent.darker (0.3f));
            randomBtn.setTooltip (tr ("New random glitch pattern. Locked steps stay as they are."));
            auto* pr = &ui.proc;
            randomBtn.onClick = [this, pr]
            {
                if (auto& hook = userActivityHook()) hook();
                pattern.randomize ((int) std::lround (stepsValue->load()), (uint32_t) juce::Random::getSystemRandom().nextInt(), 0.45f);
                if (auto* p = pr->apvts.getParameter ("fx_glt_on"); p != nullptr && p->getValue() < 0.5f)
                { p->beginChangeGesture(); p->setValueNotifyingHost (1.0f); p->endChangeGesture(); }
                repaint();
            };
            clearBtn.onClick = [this]
            {
                if (auto& hook = userActivityHook()) hook();
                for (auto& st : pattern.steps) if (! st.lock.load()) st.fx = GOff;
                ++pattern.changeCounter; repaint();
            };
            unlockBtn.onClick = [this] { for (auto& st : pattern.steps) st.lock = false; ++pattern.changeCounter; repaint(); };
            setRepaintsOnMouseActivity (false);
            enableDice ([this, pr] (bool v)
            {
                randomizer::randomizeSection (*pr, "fx:glt", v, (uint32_t) juce::Random::getSystemRandom().nextInt());
                repaint();
            });
        }

        juce::Rectangle<int> paletteCell (int fx) const
        {
            const int col = fx % 2, row = fx / 2;
            return { 12 + col * 100, 116 + row * 34, 96, 30 };
        }
        juce::Rectangle<int> gridArea() const { return { 232, 116, getWidth() - 244, getHeight() - 116 - 74 }; }
        juce::Rectangle<int> lockArea() const { auto g = gridArea(); return { g.getX(), g.getBottom() + 6, g.getWidth(), 22 }; }
        juce::Rectangle<float> cell (int c, juce::Rectangle<int> area) const
        {
            const float cw = (float) area.getWidth() / (float) kMaxGlitchSteps;
            return { (float) area.getX() + cw * (float) c + 2.0f, (float) area.getY(), cw - 4.0f, (float) area.getHeight() };
        }
        int columnAt (int x) const
        {
            const auto g = gridArea();
            return juce::jlimit (0, kMaxGlitchSteps - 1, (int) ((float) (x - g.getX()) / ((float) g.getWidth() / (float) kMaxGlitchSteps)));
        }

        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            const auto& names = choices::glitchEffects();
            drawText (g, tr ("EFFECT TO PAINT"), { 12, 96, 200, 16 }, 10.5f, colours::textDim, juce::Justification::centredLeft, true, 1);
            for (int fx = 0; fx < GCount; ++fx)
            {
                auto b = paletteCell (fx).toFloat();
                const bool sel = fx == brush;
                g.setColour (fx == GOff ? colours::knobBody : glitchColour (fx).withAlpha (sel ? 0.95f : 0.35f));
                g.fillRoundedRectangle (b, 5.0f);
                g.setColour (sel ? colours::text : colours::edge);
                g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, sel ? 2.0f : 1.0f);
                drawText (g, tr (names[fx]), b.toNearestInt(), 12.0f, sel && fx != GOff ? juce::Colours::black : colours::text,
                          juce::Justification::centred, sel, 1);
            }

            const int active = juce::jlimit (1, kMaxGlitchSteps, (int) std::lround (stepsValue->load()));
            const int playing = proc.fx.glitch.uiStep.load();
            const auto ga = gridArea(), la = lockArea();
            for (int c = 0; c < kMaxGlitchSteps; ++c)
            {
                const int fx = pattern.steps[(size_t) c].fx.load();
                const bool inUse = c < active;
                auto cb = cell (c, ga);
                g.setColour ((c / 4) % 2 == 0 ? colours::panelHi : colours::panel.brighter (0.04f));
                g.fillRoundedRectangle (cb, 4.0f);
                if (fx != GOff)
                {
                    g.setColour (glitchColour (fx).withAlpha (inUse ? 0.85f : 0.25f));
                    g.fillRoundedRectangle (cb.reduced (2.0f), 4.0f);
                }
                if (c == playing && inUse)
                {
                    g.setColour (colours::text.withAlpha (0.9f));
                    g.drawRoundedRectangle (cb.reduced (1.0f), 4.0f, 2.0f);
                }
                if (fx != GOff)
                {
                    juce::Graphics::ScopedSaveState ss (g);
                    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, cb.getCentreX(), cb.getCentreY()));
                    auto rotated = juce::Rectangle<float> (cb.getHeight(), cb.getWidth()).withCentre (cb.getCentre());
                    drawText (g, tr (names[fx]), rotated.toNearestInt(), 13.0f, juce::Colours::black.withAlpha (inUse ? 0.85f : 0.5f),
                              juce::Justification::centred, true, 1);
                }
                drawText (g, juce::String (c + 1), cb.toNearestInt().translated (0, -16).withHeight (14), 10.0f,
                          inUse ? colours::textDim : colours::textDim.withAlpha (0.4f), juce::Justification::centred, false, 1);

                auto lk = cell (c, la).reduced (6.0f, 3.0f);
                const bool locked = pattern.steps[(size_t) c].lock.load();
                g.setColour (locked ? colours::accent2 : colours::edge);
                g.fillRoundedRectangle (lk.withTrimmedTop (lk.getHeight() * 0.45f), 2.0f);
                g.drawEllipse (lk.withTrimmedBottom (lk.getHeight() * 0.4f).reduced (lk.getWidth() * 0.25f, 1.0f), 1.5f);
            }
            drawText (g, tr ("Click or drag on the steps to paint the chosen effect. Right-click = Off. The lock row keeps steps when you press RANDOM."),
                      { ga.getX(), la.getBottom() + 6, ga.getWidth(), 18 }, 11.0f, colours::textDim, juce::Justification::centredLeft, false, 1);
            drawText (g, tr ("STEP"), { 232, 26, 120, 14 }, 10.5f, colours::textDim, juce::Justification::centredLeft, true, 1);
        }

        void resized() override
        {
            const int w = getWidth();
            on->setBounds (9, 4, 22, 18);
            rate->setBounds (232, 42, 110, 24);
            const int kx = 360;
            steps->setBounds (kx, 24, 74, 70);
            chance->setBounds (kx + 78, 24, 74, 70);
            smooth->setBounds (kx + 156, 24, 74, 70);
            mix->setBounds (kx + 234, 24, 74, 70);
            unlockBtn.setBounds (w - 12 - 120, 36, 120, 34);
            clearBtn.setBounds (unlockBtn.getX() - 8 - 90, 36, 90, 34);
            randomBtn.setBounds (clearBtn.getX() - 8 - 130, 36, 130, 34);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            for (int fx = 0; fx < GCount; ++fx)
                if (paletteCell (fx).contains (e.getPosition())) { brush = fx; repaint(); return; }
            if (lockArea().contains (e.getPosition()))
            {
                auto& st = pattern.steps[(size_t) columnAt (e.x)];
                st.lock = ! st.lock.load();
                ++pattern.changeCounter;
                repaint();
                return;
            }
            painting = gridArea().contains (e.getPosition());
            erase = e.mods.isPopupMenu();
            paintAt (e);
        }
        void mouseDrag (const juce::MouseEvent& e) override { paintAt (e); }
        void mouseUp (const juce::MouseEvent&) override { painting = false; }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (gridArea().contains (e.getPosition())) { pattern.steps[(size_t) columnAt (e.x)].fx = GOff; ++pattern.changeCounter; repaint(); }
        }

        void paintAt (const juce::MouseEvent& e)
        {
            if (! painting) return;
            auto& st = pattern.steps[(size_t) columnAt (e.x)];
            const int v = erase ? (int) GOff : brush;
            if (st.fx.load() != v) { st.fx = v; ++pattern.changeCounter; repaint(); }
        }

        void tick()
        {
            const int sig = pattern.changeCounter.load() * 64 + proc.fx.glitch.uiStep.load() + (int) std::lround (stepsValue->load()) * 4096;
            if (sig != lastSig) { lastSig = sig; repaint(); }
        }

        PyraAudioProcessor& proc;
        GlitchPattern& pattern;
        juce::OwnedArray<juce::Component> owned;
        ParamToggle* on; ParamCombo* rate; Knob* steps, *chance, *smooth, *mix;
        std::atomic<float>* stepsValue;
        juce::TextButton randomBtn, clearBtn, unlockBtn;
        int brush = GRepeat, lastSig = -1;
        bool painting = false, erase = false;
    };

    class CreativeInfo : public Panel
    {
    public:
        CreativeInfo() : Panel (tr ("TIP")) { tint = colours::accent2; }
        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            drawText (g, tr ("Turn CHAOS up to the top (with MANGLE) and these three effects come in by themselves: the sound gets cut, reversed, stretched and scattered."),
                      getLocalBounds().withTrimmedTop (30).reduced (12, 4), 12.5f, colours::text, juce::Justification::topLeft, false, 8);
        }
    };

    class FxPage : public Page
    {
    public:
        explicit FxPage (UiContext& ui) : glitchPanel (ui)
        {
            addAndMakeVisible (mainView);
            addChildComponent (creativeView);
            for (auto* t : { &tabMain, &tabCreative })
            {
                addAndMakeVisible (*t);
                t->setClickingTogglesState (false);
            }
            tabMain.setButtonText (tr ("MAIN EFFECTS"));
            tabCreative.setButtonText (tr ("GLITCH / GRANULAR / SHIFTER"));
            tabMain.onClick = [this] { showView (0); };
            tabCreative.onClick = [this] { showView (1); };

            auto add = [this] (FxPanel* p) { panels.add (p); mainView.addAndMakeVisible (p); return p; };
            add (new FxPanel (ui, tr ("DISTORTION"), "dist", { "fx_dist_type" },
                 { { "fx_dist_drive", "Drive" }, { "fx_dist_tone", "Tone" }, { "fx_dist_out", "Output" }, { "fx_dist_mix", "Mix" } }));
            add (new FxPanel (ui, tr ("RESONATOR"), "res", { "fx_res_chord" },
                 { { "fx_res_note", "Note" }, { "fx_res_decay", "Decay" }, { "fx_res_bright", "Bright" }, { "fx_res_mix", "Mix" } }));
            add (new FxPanel (ui, tr ("FILTER"), "flt", { "fx_flt_type" },
                 { { "fx_flt_cutoff", "Cutoff" }, { "fx_flt_res", "Resonance" }, { "fx_flt_drive", "Drive" }, { "fx_flt_mix", "Mix" } }));
            add (new FxPanel (ui, tr ("CHORUS"), "cho", {},
                 { { "fx_cho_rate", "Rate" }, { "fx_cho_depth", "Depth" }, { "fx_cho_delay", "Delay" },
                   { "fx_cho_fb", "Feedback" }, { "fx_cho_width", "Width" }, { "fx_cho_mix", "Mix" } }));
            add (new FxPanel (ui, tr ("FLANGER"), "fla", {},
                 { { "fx_fla_rate", "Rate" }, { "fx_fla_depth", "Depth" }, { "fx_fla_fb", "Feedback" }, { "fx_fla_mix", "Mix" } }));
            add (new FxPanel (ui, tr ("PHASER"), "pha", {},
                 { { "fx_pha_rate", "Rate" }, { "fx_pha_depth", "Depth" }, { "fx_pha_freq", "Frequency" },
                   { "fx_pha_fb", "Feedback" }, { "fx_pha_stages", "Stages" }, { "fx_pha_mix", "Mix" } }));
            add (new FxPanel (ui, tr ("TAPE"), "tape", {},
                 { { "fx_tape_wow", "Wow" }, { "fx_tape_flutter", "Flutter" }, { "fx_tape_sat", "Saturation" },
                   { "fx_tape_tone", "Tone" }, { "fx_tape_mix", "Mix" } }));
            add (new FxPanel (ui, tr ("DELAY"), "dly", { "fx_dly_sync", "fx_dly_mode", "fx_dly_div" },
                 { { "fx_dly_time", "Time" }, { "fx_dly_fb", "Feedback" }, { "fx_dly_lowcut", "Low Cut" },
                   { "fx_dly_highcut", "High Cut" }, { "fx_dly_width", "Width" }, { "fx_dly_mix", "Mix" } }));
            add (new FxPanel (ui, tr ("REVERB"), "rev", { "fx_rev_type" },
                 { { "fx_rev_size", "Size" }, { "fx_rev_decay", "Decay" }, { "fx_rev_pre", "Pre-Delay" }, { "fx_rev_damp", "Damping" },
                   { "fx_rev_width", "Width" }, { "fx_rev_shim", "Shimmer" }, { "fx_rev_mix", "Mix" } }));
            add (new FxPanel (ui, "EQ", "eq", {},
                 { { "fx_eq_lowf", "Low Freq" }, { "fx_eq_lowg", "Low Gain" }, { "fx_eq_midf", "Mid Freq" }, { "fx_eq_midg", "Mid Gain" },
                   { "fx_eq_midq", "Mid Q" }, { "fx_eq_highf", "High Freq" }, { "fx_eq_highg", "High Gain" } }));
            comp = new CompPanel (ui);
            add (comp);
            mainView.addAndMakeVisible (info);

            delayPanel = panels[7];
            reverbPanel = panels[8];

            // second page
            creativeView.addAndMakeVisible (glitchPanel);
            granular = std::make_unique<FxPanel> (ui, tr ("GRANULAR"), "grn", std::initializer_list<const char*> { "fx_grn_freeze" },
                 std::initializer_list<std::pair<const char*, const char*>> {
                   { "fx_grn_size", "Grain Size" }, { "fx_grn_density", "Density" }, { "fx_grn_spray", "Spray" }, { "fx_grn_pitch", "Pitch" },
                   { "fx_grn_prand", "Rnd Pitch" }, { "fx_grn_rev", "Reverse" }, { "fx_grn_width", "Width" }, { "fx_grn_mix", "Mix" } });
            shifter = std::make_unique<FxPanel> (ui, tr ("SHIFTER"), "shf", std::initializer_list<const char*> { "fx_shf_mode" },
                 std::initializer_list<std::pair<const char*, const char*>> {
                   { "fx_shf_freq", "Frequency" }, { "fx_shf_fb", "Feedback" }, { "fx_shf_width", "Stereo" }, { "fx_shf_mix", "Mix" } });
            granular->syncToggle->setTooltip (tr ("Freeze: stops recording and keeps playing grains of what is in the buffer"));
            granular->tint = colours::mod;
            shifter->tint = colours::accent2;
            creativeView.addAndMakeVisible (*granular);
            creativeView.addAndMakeVisible (*shifter);
            creativeView.addAndMakeVisible (creativeInfo);
            showView (0);
        }

        void showView (int v)
        {
            view = v;
            mainView.setVisible (v == 0);
            creativeView.setVisible (v == 1);
            tabMain.setToggleState (v == 0, juce::dontSendNotification);
            tabCreative.setToggleState (v == 1, juce::dontSendNotification);
        }

        void resized() override
        {
            const int m = 8;
            tabMain.setBounds (m, 6, 200, 26);
            tabCreative.setBounds (m + 206, 6, 270, 26);
            auto area = getLocalBounds().withTrimmedTop (36);
            mainView.setBounds (area);
            creativeView.setBounds (area);

            const int cols = 4, rows = 3;
            const int w = (area.getWidth() - m * (cols + 1)) / cols, h = (area.getHeight() - m * (rows + 1)) / rows;
            for (int i = 0; i < 12; ++i)
            {
                juce::Component* c = i < panels.size() ? (juce::Component*) panels[i] : (juce::Component*) &info;
                c->setBounds (m + (i % cols) * (w + m), m / 2 + (i / cols) * (h + m), w, h);
            }

            const int gh = 380;
            glitchPanel.setBounds (m, m / 2, area.getWidth() - 2 * m, gh);
            const int y2 = m / 2 + gh + m, h2 = area.getHeight() - y2 - m;
            granular->setBounds (m, y2, 640, h2);
            shifter->setBounds (m + 640 + m, y2, 370, h2);
            creativeInfo.setBounds (m + 640 + m + 370 + m, y2, area.getWidth() - (m + 640 + m + 370 + m) - m, h2);
        }

        void paint (juce::Graphics& g) override
        {
            // a little indicator on the tab: is any creative effect on?
            (void) g;
        }

        void tick() override
        {
            comp->tick();
            glitchPanel.tick();
            // delay: show the time knob or the musical division depending on Sync
            if (delayPanel->syncToggle != nullptr)
            {
                const bool synced = delayPanel->syncToggle->getToggleState();
                delayPanel->knobs[0]->setAlpha (synced ? 0.35f : 1.0f);
                delayPanel->combos[1]->setAlpha (synced ? 1.0f : 0.35f);
            }
            const bool shimmer = reverbPanel->combos[0]->getSelectedId() == 3;
            reverbPanel->knobs[5]->setAlpha (shimmer ? 1.0f : 0.35f);
        }

        juce::Component mainView, creativeView;
        juce::TextButton tabMain, tabCreative;
        int view = 0;
        juce::OwnedArray<FxPanel> panels;
        CompPanel* comp = nullptr;
        FxPanel* delayPanel = nullptr, *reverbPanel = nullptr;
        ChainInfo info;
        GlitchPanel glitchPanel;
        std::unique_ptr<FxPanel> granular, shifter;
        CreativeInfo creativeInfo;
    };

    //==========================================================================
    class AnalogPage : public Page
    {
    public:
        explicit AnalogPage (UiContext& ui)
            : agePanel ("AGE"), movement (tr ("MOVEMENT")), character (tr ("CIRCUIT")), noise (tr ("NOISE & HUM")),
              converter (tr ("VINTAGE CONVERTER"))
        {
            for (auto* p : { &agePanel, &movement, &character, &noise, &converter }) addAndMakeVisible (p);
            agePanel.tint = colours::hot; movement.tint = colours::accent2; character.tint = colours::accent;
            noise.tint = colours::mod; converter.tint = colours::accent2;
            Builder ba { ui, agePanel, owned };   age = ba.knob ("an_age", "AGE");
            Builder bm { ui, movement, owned };   drift = bm.knob ("drift", "Drift"); spread = bm.knob ("spread", "Voice Spread");
                                                  tolerance = bm.knob ("an_tolerance", "Tolerance");
            Builder bc { ui, character, owned };  circuit = bc.knob ("an_circuit", "Saturation");
            Builder bn { ui, noise, owned };      noiseK = bn.knob ("an_noise", "Noise Floor"); hum = bn.knob ("an_hum", "Hum");
                                                  humFreq = bn.combo ("an_humfreq", false);
            Builder bv { ui, converter, owned };  bits = bv.knob ("an_bits", "Bits"); rate = bv.knob ("an_rate", "Sample Rate");
            const std::pair<Panel*, const char*> dice[] = { { &agePanel, "age" }, { &movement, "movement" }, { &character, "circuit" },
                                                            { &noise, "noise" }, { &converter, "converter" } };
            for (auto& [panel, sec] : dice)
                panel->enableDice ([pr = &ui.proc, sec = juce::String (sec)] (bool v)
                {
                    randomizer::randomizeSection (*pr, sec, v, (uint32_t) juce::Random::getSystemRandom().nextInt());
                });
        }

        void paintOverChildren (juce::Graphics& g) override
        {
            auto ab = agePanel.getBounds();
            drawText (g, tr ("Turns a clean digital synth into a worn analog one. At 0 nothing changes: the sound stays clean and digital."),
                      ab.withTrimmedTop (430).withHeight (70).reduced (22, 6), 15.0f, colours::text, juce::Justification::topLeft, false, 5);
            drawText (g, tr ("AGE raises drift, voice spread, tolerance, circuit saturation and a little noise and hum together. Each knob on the right also works on its own."),
                      ab.withTrimmedTop (510).withHeight (80).reduced (22, 6), 12.5f, colours::textDim, juce::Justification::topLeft, false, 5);

            auto help = [&] (juce::Component& p, const juce::String& t)
            {
                drawText (g, t, p.getBounds().withTrimmedTop (182).withHeight (60).reduced (14, 4), 12.0f, colours::textDim,
                          juce::Justification::topLeft, false, 3);
            };
            help (movement, tr ("Drift: slow wandering pitch. Voice Spread: every note slightly different. Tolerance: oscillators and filters never perfectly matched."));
            help (character, tr ("Warm, slightly uneven saturation of the voice circuit."));
            help (noise, tr ("Background hiss and mains hum, only while notes are sounding."));
            help (converter, tr ("Lower bit depth and sample rate, like 80s samplers. Not part of AGE."));
        }

        void resized() override
        {
            const int m = 8;
            agePanel.setBounds (m, m, 420, getHeight() - 2 * m);
            const int x = m + 420 + m, w = getWidth() - x - m, h = (getHeight() - 3 * m) / 2;
            movement.setBounds (x, m, w * 3 / 5 - m / 2, h);
            character.setBounds (x + w * 3 / 5 + m / 2, m, w - w * 3 / 5 - m / 2, h);
            noise.setBounds (x, m + h + m, w / 2 - m / 2, h);
            converter.setBounds (x + w / 2 + m / 2, m + h + m, w - w / 2 - m / 2, h);

            age->setBounds (agePanel.getLocalBounds().withTrimmedTop (40).withHeight (340).reduced (60, 20));
            layoutGrid ({ drift, spread, tolerance }, { 10, 40, movement.getWidth() - 20, 130 }, 3, 130);
            circuit->setBounds (character.getLocalBounds().withTrimmedTop (40).withHeight (130).reduced (20, 0));
            layoutGrid ({ noiseK, hum }, { 10, 40, noise.getWidth() - 140, 130 }, 2, 130);
            humFreq->setBounds (noise.getWidth() - 120, 90, 100, 24);
            layoutGrid ({ bits, rate }, { 10, 40, converter.getWidth() - 20, 130 }, 2, 130);
        }

        Panel agePanel, movement, character, noise, converter;
        juce::OwnedArray<juce::Component> owned;
        Knob* age, *drift, *spread, *tolerance, *circuit, *noiseK, *hum, *bits, *rate;
        ParamCombo* humFreq;
    };

    //==========================================================================
    /** Step editor: lock / note / velocity / gate / on / tie for up to 32 steps. */
    class StepEditor : public juce::Component
    {
    public:
        StepEditor (PyraAudioProcessor& p) : proc (p), pattern (p.arpPattern)
        {
            steps = p.apvts.getRawParameterValue ("arp_steps");
            setRepaintsOnMouseActivity (false);
        }

        enum Row { RLock, RNote, RVel, RGate, ROn, RTie, RCount };

        juce::Rectangle<int> rowBounds (Row r) const
        {
            const int x = labelW, w = getWidth() - labelW;
            switch (r)
            {
                case RLock: return { x, 0, w, 24 };
                case RNote: return { x, 28, w, 210 };
                case RVel:  return { x, 244, w, 96 };
                case RGate: return { x, 346, w, 96 };
                case ROn:   return { x, 448, w, 24 };
                default:    return { x, 476, w, 24 };
            }
        }

        int columnAt (int x) const
        {
            const float cw = (float) (getWidth() - labelW) / (float) kMaxArpSteps;
            return juce::jlimit (0, kMaxArpSteps - 1, (int) ((float) (x - labelW) / cw));
        }

        juce::Rectangle<float> cell (Row r, int c) const
        {
            const auto rb = rowBounds (r).toFloat();
            const float cw = rb.getWidth() / (float) kMaxArpSteps;
            return { rb.getX() + cw * (float) c + 1.5f, rb.getY(), cw - 3.0f, rb.getHeight() };
        }

        void paint (juce::Graphics& g) override
        {
            const int active = juce::jlimit (1, kMaxArpSteps, (int) std::lround (steps->load()));
            const int playing = proc.arp.currentStep.load();
            const char* labels[RCount] = { "LOCK", "NOTE", "VELOCITY", "GATE", "ON", "TIE" };

            for (int r = 0; r < RCount; ++r)
                drawText (g, tr (labels[r]), rowBounds ((Row) r).withX (0).withWidth (labelW - 8), 11.0f, colours::textDim,
                          juce::Justification::centredRight, true, 1);

            for (int c = 0; c < kMaxArpSteps; ++c)
            {
                const auto& s = pattern.steps[(size_t) c];
                const bool inUse = c < active;
                const float alpha = inUse ? 1.0f : 0.25f;

                // column background, beat grouping and playhead
                auto col = cell (RLock, c).withBottom (cell (RTie, c).getBottom());
                g.setColour ((c / 4) % 2 == 0 ? colours::panel : colours::panelHi);
                g.fillRoundedRectangle (col, 3.0f);
                if (c == playing && inUse)
                {
                    g.setColour (colours::accent.withAlpha (0.18f));
                    g.fillRoundedRectangle (col, 3.0f);
                }

                // lock
                auto lk = cell (RLock, c).reduced (3.0f, 3.0f);
                g.setColour ((s.lock.load() ? colours::accent2 : colours::edge).withMultipliedAlpha (alpha));
                g.fillRoundedRectangle (lk.withTrimmedTop (lk.getHeight() * 0.45f), 2.0f);
                g.drawEllipse (lk.withTrimmedBottom (lk.getHeight() * 0.4f).reduced (lk.getWidth() * 0.22f, 1.0f), 1.5f);

                // note degree (bipolar bar)
                auto nb = cell (RNote, c);
                const int d = s.degree.load();
                const float mid = nb.getCentreY();
                const float hgt = (float) d / 14.0f * nb.getHeight() * 0.5f;
                g.setColour (colours::edge.withMultipliedAlpha (alpha));
                g.drawHorizontalLine ((int) mid, nb.getX(), nb.getRight());
                g.setColour ((s.on.load() ? colours::accent : colours::textDim).withMultipliedAlpha (alpha));
                g.fillRect (juce::Rectangle<float> (nb.getX() + 2.0f, std::min (mid, mid - hgt), nb.getWidth() - 4.0f,
                                                    std::max (2.0f, std::abs (hgt))));
                g.setColour (colours::text.withMultipliedAlpha (alpha));
                g.setFont (PyraLookAndFeel::font (10.0f));
                g.drawText ((d > 0 ? "+" : "") + juce::String (d), nb.withHeight (14.0f).translated (0, nb.getHeight() - 14.0f),
                            juce::Justification::centred);

                // velocity + gate bars
                for (auto rr : { RVel, RGate })
                {
                    auto bb = cell (rr, c);
                    const float v = rr == RVel ? s.velocity.load() : s.gate.load();
                    g.setColour (colours::knobBody);
                    g.fillRect (bb);
                    g.setColour ((rr == RVel ? colours::accent2.withAlpha (0.8f) : colours::mod.withAlpha (0.6f))
                                     .withMultipliedAlpha (alpha * (s.on.load() ? 1.0f : 0.4f)));
                    g.fillRect (bb.withTop (bb.getBottom() - bb.getHeight() * v));
                }

                // on / tie
                for (auto rr : { ROn, RTie })
                {
                    auto bb = cell (rr, c).reduced (2.0f, 4.0f);
                    const bool v = rr == ROn ? s.on.load() : s.tie.load();
                    g.setColour ((v ? (rr == ROn ? colours::accent : colours::mod) : colours::knobBody).withMultipliedAlpha (alpha));
                    g.fillRoundedRectangle (bb, 3.0f);
                }

                if (c % 4 == 0)
                    drawText (g, juce::String (c + 1), cell (RTie, c).translated (0, 24).withHeight (14).toNearestInt(), 10.0f,
                              colours::textDim, juce::Justification::centred, false, 1);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragRow = RCount;
            for (int r = 0; r < RCount; ++r)
                if (rowBounds ((Row) r).contains (e.getPosition())) dragRow = r;
            if (dragRow == RCount) return;
            const int c = columnAt (e.x);
            auto& s = pattern.steps[(size_t) c];
            if (dragRow == RLock) { toggleValue = ! s.lock.load(); }
            if (dragRow == ROn)   { toggleValue = ! s.on.load(); }
            if (dragRow == RTie)  { toggleValue = ! s.tie.load(); }
            if (e.mods.isPopupMenu() && dragRow == RNote) { s.degree = 0; changed(); return; }
            apply (e);
        }

        void mouseDrag (const juce::MouseEvent& e) override { apply (e); }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (rowBounds (RNote).contains (e.getPosition())) { pattern.steps[(size_t) columnAt (e.x)].degree = 0; changed(); }
        }

        void apply (const juce::MouseEvent& e)
        {
            if (dragRow == RCount) return;
            const int c = columnAt (e.x);
            auto& s = pattern.steps[(size_t) c];
            const auto rb = rowBounds ((Row) dragRow).toFloat();
            const float rel = juce::jlimit (0.0f, 1.0f, (rb.getBottom() - (float) e.y) / rb.getHeight());
            switch (dragRow)
            {
                case RLock: s.lock = toggleValue; break;
                case RNote: s.degree = juce::jlimit (-14, 14, juce::roundToInt ((rel - 0.5f) * 2.0f * 14.0f)); break;
                case RVel:  s.velocity = juce::jmax (0.05f, rel); break;
                case RGate: s.gate = juce::jmax (0.05f, rel); break;
                case ROn:   s.on = toggleValue; break;
                case RTie:  s.tie = toggleValue; break;
                default: break;
            }
            changed();
        }

        void changed() { ++pattern.changeCounter; repaint(); }

        void tick()
        {
            const int cc = pattern.changeCounter.load(), ps = proc.arp.currentStep.load(), st = (int) std::lround (steps->load());
            if (cc != lastCounter || ps != lastPlaying || st != lastSteps)
            {
                lastCounter = cc; lastPlaying = ps; lastSteps = st;
                repaint();
            }
        }

        static constexpr int labelW = 78;
        PyraAudioProcessor& proc;
        ArpPattern& pattern;
        std::atomic<float>* steps;
        int dragRow = RCount;
        bool toggleValue = false;
        int lastCounter = -1, lastPlaying = -2, lastSteps = -1;
    };

    /** Drag this onto a DAW track to get the arp pattern as MIDI; click to save a .mid file. */
    class MidiDragButton : public juce::TextButton
    {
    public:
        explicit MidiDragButton (PyraAudioProcessor& p) : juce::TextButton (tr ("DRAG MIDI")), proc (p)
        {
            setTooltip (tr ("Drag onto a track in your DAW to get the pattern as MIDI. Click to save a .mid file."));
            onClick = [this] { saveWithChooser(); };
        }

        juce::File writeFile()
        {
            auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PYRA Arp.mid");
            file.deleteFile();
            auto midi = arpToMidiFile (proc.arp.readSettings(), proc.arpPattern, proc.arp.getLastChord(), 2);
            juce::FileOutputStream os (file);
            if (os.openedOk()) midi.writeTo (os);
            return file;
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! dragging && e.getDistanceFromDragStart() > 6)
            {
                dragging = true;
                const auto f = writeFile();
                juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
            }
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (dragging) { dragging = false; return; }
            juce::TextButton::mouseUp (e);
        }

        void saveWithChooser()
        {
            chooser = std::make_unique<juce::FileChooser> (tr ("Save arp pattern as MIDI"),
                juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("PYRA Arp.mid"), "*.mid");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
            {
                auto target = fc.getResult();
                if (target == juce::File()) return;
                writeFile().copyFileTo (target.withFileExtension ("mid"));
            });
        }

        PyraAudioProcessor& proc;
        bool dragging = false;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    class ArpPage : public Page
    {
    public:
        explicit ArpPage (UiContext& ui) : top ("ARP"), editorPanel (tr ("STEPS")), editor (ui.proc), midiDrag (ui.proc)
        {
            addAndMakeVisible (top);
            addAndMakeVisible (editorPanel);
            editorPanel.addAndMakeVisible (editor);
            editorPanel.tint = colours::mod;

            Builder b { ui, top, owned };
            on = b.toggle ("arp_on", tr ("ARP ON"));
            latch = b.toggle ("arp_latch", tr ("Latch"));
            mode = b.combo ("arp_mode");
            rate = b.combo ("arp_rate", false);
            scale = b.combo ("arp_scale");
            key = b.combo ("arp_key", false);
            oct = b.knob ("arp_oct", "Octaves");
            gate = b.knob ("arp_gate", "Gate");
            swing = b.knob ("arp_swing", "Swing");
            stepsK = b.knob ("arp_steps", "Steps");
            vel = b.knob ("arp_velamt", "Step Vel");

            for (auto* btn : { &randomBtn, &clearBtn, &unlockBtn })
                top.addAndMakeVisible (*btn);
            top.addAndMakeVisible (midiDrag);
            randomBtn.setButtonText (tr ("RANDOM"));
            clearBtn.setButtonText (tr ("CLEAR"));
            unlockBtn.setButtonText (tr ("UNLOCK ALL"));
            randomBtn.setTooltip (tr ("New random pattern. Locked steps stay as they are."));
            randomBtn.setColour (juce::TextButton::buttonColourId, colours::accent.darker (0.3f));

            auto* pr = &ui.proc;
            randomBtn.onClick = [pr]
            {
                const int n = (int) std::lround (pr->apvts.getRawParameterValue ("arp_steps")->load());
                pr->arpPattern.randomize (n, (uint32_t) juce::Random::getSystemRandom().nextInt());
                // the random pattern is a melody: switch to Sequencer mode if a plain mode is selected
                if (auto* m = pr->apvts.getParameter ("arp_mode"))
                    if ((int) std::lround (pr->apvts.getRawParameterValue ("arp_mode")->load()) < 6)
                        m->setValueNotifyingHost (m->convertTo0to1 (7.0f));
            };
            clearBtn.onClick = [pr]
            {
                for (auto& s : pr->arpPattern.steps)
                    if (! s.lock.load()) { s.degree = 0; s.velocity = 0.8f; s.gate = 1.0f; s.on = true; s.tie = false; }
                ++pr->arpPattern.changeCounter;
            };
            unlockBtn.onClick = [pr] { for (auto& s : pr->arpPattern.steps) s.lock = false; ++pr->arpPattern.changeCounter; };
        }

        void paintOverChildren (juce::Graphics& g) override
        {
            auto t = top.getBounds();
            auto lab = [&] (const juce::String& s, juce::Component* c)
            {
                drawText (g, s.toUpperCase(), c->getBounds().translated (t.getX(), t.getY() - 15).withHeight (14), 10.5f,
                          colours::textDim, juce::Justification::centredLeft, true, 1);
            };
            lab (tr ("Mode"), mode); lab (tr ("Step Rate"), rate); lab (tr ("Scale"), scale); lab (tr ("Key"), key);
            drawText (g, tr ("NOTE = scale steps added to the played note (Sequencer mode plays them as a melody). Drag to edit, double-click to reset. Locked steps survive RANDOM."),
                      editorPanel.getBounds().removeFromBottom (26).reduced (14, 2), 11.5f, colours::textDim,
                      juce::Justification::centredLeft, false, 1);
        }

        void resized() override
        {
            const int m = 8;
            top.setBounds (m, m, getWidth() - 2 * m, 128);
            editorPanel.setBounds (m, m + 128 + m, getWidth() - 2 * m, getHeight() - 128 - 3 * m);
            editor.setBounds (editorPanel.getLocalBounds().withTrimmedTop (30).withTrimmedBottom (42).reduced (10, 0));

            on->setBounds (14, 34, 100, 26);
            latch->setBounds (14, 72, 100, 26);
            mode->setBounds (126, 46, 130, 24);
            rate->setBounds (266, 46, 90, 24);
            scale->setBounds (126, 94, 130, 24);
            key->setBounds (266, 94, 90, 24);
            layoutGrid ({ oct, gate, swing, stepsK, vel }, { 370, 30, 420, 92 }, 5, 92);
            randomBtn.setBounds (812, 34, 130, 38);
            clearBtn.setBounds (952, 34, 96, 38);
            unlockBtn.setBounds (1058, 34, 120, 38);
            midiDrag.setBounds (812, 80, 366, 36);
        }

        void tick() override { editor.tick(); }

        Panel top, editorPanel;
        StepEditor editor;
        MidiDragButton midiDrag;
        juce::OwnedArray<juce::Component> owned;
        ParamToggle* on, *latch;
        ParamCombo* mode, *rate, *scale, *key;
        Knob* oct, *gate, *swing, *stepsK, *vel;
        juce::TextButton randomBtn, clearBtn, unlockBtn;
    };
}

std::unique_ptr<Page> createFxPage (UiContext& ui)     { return std::make_unique<FxPage> (ui); }
std::unique_ptr<Page> createAnalogPage (UiContext& ui) { return std::make_unique<AnalogPage> (ui); }
std::unique_ptr<Page> createArpPage (UiContext& ui)    { return std::make_unique<ArpPage> (ui); }

} // namespace pyra
