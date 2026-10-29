// Phase 3 pages: FX, ANALOG, ARP
#include "Pages.h"
#include "Prefs.h"
#include "PyraLookAndFeel.h"
#include "HellArt.h"
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
            setPowerButton (on);
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
            setPowerButton (on);
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
                if (paletteCell (fx).contains (e.getPosition())) { brush = fx; if (onBrush) onBrush (fx); repaint(); return; }
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
        std::function<void (int)> onBrush;
    };

    /** The settings of the GLITCH effect chosen in the palette (every step painted with it uses them). */
    class GlitchSettings : public Panel
    {
    public:
        explicit GlitchSettings (GlitchPattern& p) : pattern (p)
        {
            tint = colours::hot;
            for (int i = 0; i < kGlitchParams; ++i)
            {
                auto* sl = sliders.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox));
                sl->setPopupDisplayEnabled (false, false, nullptr);
                sl->setDoubleClickReturnValue (true, 0.0);
                sl->onValueChange = [this, i]
                {
                    if (updating) return;
                    pattern.setParam (fx, i, (float) sliders[i]->getValue());
                    repaint();
                };
                addChildComponent (sl);
            }
            setEffect (GRepeat);
        }

        void setEffect (int newFx)
        {
            fx = newFx;
            title = tr ("SETTINGS") + ": " + tr (choices::glitchEffects()[fx]).toUpperCase();
            const juce::ScopedValueSetter<bool> svs (updating, true);
            for (int i = 0; i < kGlitchParams; ++i)
            {
                const auto& sp = glitchParamSpec (fx, i);
                auto* sl = sliders[i];
                sl->setVisible (sp.name != nullptr);
                if (sp.name == nullptr) continue;
                sl->setRange (sp.min, sp.max, sp.integer ? 1.0 : 0.0);
                sl->setDoubleClickReturnValue (true, sp.def);
                sl->setValue (pattern.param (fx, i), juce::dontSendNotification);
                sl->setTooltip (tr (helpFor (fx, i)));
            }
            resized();
            repaint();
        }

        static const char* helpFor (int fx, int i)
        {
            switch (fx)
            {
                case GRepeat:   return i == 0 ? "How much of the step is looped" : i == 1 ? "Each repeat a little quieter" : "Plays the loop higher or lower";
                case GStutter:  return i == 0 ? "Length of each stutter slice" : i == 1 ? "How much of each slice sounds (lower = choppier)" : "Each repeat goes up (or down) by this many semitones";
                case GReverse:  return i == 0 ? "Speed of the backwards playback (2x = an octave higher)" : "Fades the reversed sound in, like a reverse swell";
                case GTapeStop: return i == 0 ? "How long the tape takes to stop" : "Stop = slows down to silence. Start = speeds up from nothing";
                case GSlow:     return "Playback speed (0.5x = an octave lower)";
                case GPitchUp:  return "How many semitones higher the step is replayed";
                case GScratch:  return i == 0 ? "How far the record is pushed back and forth" : "How many back-and-forth movements per step";
                case GGate:     return i == 0 ? "1 = the whole step is silent; more = the step is chopped into pieces" : "How much of each piece stays open";
                case GCrush:    return i == 0 ? "Bit depth (lower = harsher)" : "Sample-rate reduction (higher = more aliasing)";
                default:        return "";
            }
        }

        juce::Rectangle<int> cellFor (int i) const
        {
            const int n = juce::jmax (1, visibleCount()), w = getWidth() - 20;
            const int cols = w / n >= 86 ? n : 2;
            const int cw = w / cols;
            const int row = i / cols, col = i % cols;
            return { 10 + col * cw, 40 + row * 120, cw, 116 };
        }
        int visibleCount() const
        {
            int n = 0;
            for (int i = 0; i < kGlitchParams; ++i) if (glitchParamSpec (fx, i).name != nullptr) ++n;
            return n;
        }

        void resized() override
        {
            for (int i = 0; i < kGlitchParams; ++i)
                sliders[i]->setBounds (cellFor (i).withTrimmedTop (16).withTrimmedBottom (18).withSizeKeepingCentre (78, 78));
        }

        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            if (fx == GOff)
            {
                drawText (g, tr ("Pick an effect in EFFECT TO PAINT to see its settings."), getLocalBounds().withTrimmedTop (34).reduced (12, 4), 12.5f,
                          colours::textDim, juce::Justification::topLeft, false, 4);
                return;
            }
            for (int i = 0; i < kGlitchParams; ++i)
            {
                const auto& sp = glitchParamSpec (fx, i);
                if (sp.name == nullptr) continue;
                auto c = cellFor (i);
                drawText (g, tr (sp.name).toUpperCase(), c.withHeight (16), 10.5f, colours::textDim, juce::Justification::centred, true, 1);
                drawText (g, tr (glitchParamText (fx, i, pattern.param (fx, i))), c.withTrimmedTop (c.getHeight() - 18), 12.0f, colours::accent2,
                          juce::Justification::centred, true, 1);
            }
            drawText (g, tr ("Every step painted with this effect uses these settings."), getLocalBounds().removeFromBottom (36).reduced (12, 2), 11.0f,
                      colours::textDim, juce::Justification::centredLeft, false, 2);
        }

        void tick()
        {
            const int cc = pattern.changeCounter.load();
            if (cc == lastCounter) return;
            lastCounter = cc;
            const juce::ScopedValueSetter<bool> svs (updating, true);
            for (int i = 0; i < kGlitchParams; ++i)
                if (glitchParamSpec (fx, i).name != nullptr) sliders[i]->setValue (pattern.param (fx, i), juce::dontSendNotification);
            repaint();
        }

        GlitchPattern& pattern;
        juce::OwnedArray<juce::Slider> sliders;
        int fx = GRepeat, lastCounter = -1;
        bool updating = false;
    };

    class FxPage : public Page
    {
    public:
        explicit FxPage (UiContext& ui) : glitchPanel (ui), creativeInfo (ui.proc.glitchPattern)
        {
            glitchPanel.onBrush = [this] (int fx) { creativeInfo.setEffect (fx); };
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
            granular->tint = colours::hot;
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
            granular->setBounds (m, y2, 600, h2);
            shifter->setBounds (m + 600 + m, y2, 340, h2);
            creativeInfo.setBounds (m + 600 + m + 340 + m, y2, area.getWidth() - (m + 600 + m + 340 + m) - m, h2);
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
            creativeInfo.tick();
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
        GlitchSettings creativeInfo;
    };

    //==========================================================================
    /** Pictures carved into the stone of the ANALOG panels; they wake up (glow, crack, move) with the knobs. */
    class AnalogArt : public juce::Component
    {
    public:
        enum Kind { Age, Movement, Circuit, Noise, Converter };
        AnalogArt (Kind k, juce::AudioProcessorValueTreeState& st) : kind (k)
        {
            setInterceptsMouseClicks (false, false);
            auto p = [&st] (const char* id) { return st.getRawParameterValue (id); };
            age = p ("an_age"); drift = p ("drift"); spread = p ("spread"); tolerance = p ("an_tolerance"); circuit = p ("an_circuit");
            noiseAmt = p ("an_noise"); hum = p ("an_hum"); humFreq = p ("an_humfreq"); bits = p ("an_bits"); rate = p ("an_rate");
        }

        /** AGE: where the knob sits (cracks grow out of it). */
        void setKnobCircle (juce::Point<float> c, float r) { centre = c; radius = r; buildCracks(); }

        bool tick (float dt)
        {
            time += dt;
            const float sig = age->load() + drift->load() * 3.1f + spread->load() * 5.3f + tolerance->load() * 7.7f + circuit->load() * 11.0f
                              + noiseAmt->load() * 13.0f + hum->load() * 17.0f + humFreq->load() * 19.0f + bits->load() * 23.0f + rate->load() * 29.0f;
            bool animated = (kind == Movement && (drift->load() + spread->load()) > 0.01f)
                                  || (kind == Noise && (noiseAmt->load() + hum->load()) > 0.01f)
                                  || (kind == Circuit && circuit->load() > 0.01f);
            if (prefs::get (prefs::Animations) == 0) animated = false;
            if (std::abs (sig - lastSig) > 1.0e-4f || animated) { lastSig = sig; repaint(); return true; }
            return false;
        }

        void resized() override { buildCracks(); }

        void paint (juce::Graphics& g) override
        {
            switch (kind)
            {
                case Age:       paintAge (g); break;
                case Movement:  paintMovement (g); break;
                case Circuit:   paintCircuit (g); break;
                case Noise:     paintNoise (g); break;
                case Converter: paintConverter (g); break;
            }
        }

    private:
        juce::Rectangle<float> strip() const { return getLocalBounds().toFloat().withTrimmedTop (238.0f).reduced (16.0f, 10.0f); }

        void buildCracks()
        {
            cracks.clear();
            if (kind != Age || radius <= 0.0f) return;
            Rng r; r.seed (1958u);
            const int n = 34;
            for (int i = 0; i < n; ++i)
            {
                Crack c;
                float a = (float) i / (float) n * juce::MathConstants<float>::twoPi + r.bi() * 0.12f;
                float rad = radius + 16.0f;
                juce::Point<float> pt (centre.x + std::cos (a) * rad, centre.y + std::sin (a) * rad);
                c.path.startNewSubPath (pt);
                const float len = 40.0f + r.uni() * (i % 3 == 0 ? 220.0f : 110.0f);
                float walked = 0.0f;
                while (walked < len)
                {
                    a += r.bi() * 0.45f;
                    const float stepLen = 6.0f + r.uni() * 9.0f;
                    pt += juce::Point<float> (std::cos (a), std::sin (a)) * stepLen;
                    c.path.lineTo (pt);
                    walked += stepLen;
                    if (r.uni() < 0.12f)
                    {
                        const float b = a + (r.uni() < 0.5f ? -1.0f : 1.0f) * (0.5f + r.uni() * 0.6f);
                        auto e = pt + juce::Point<float> (std::cos (b), std::sin (b)) * (8.0f + r.uni() * 22.0f);
                        c.path.lineTo (e);
                        c.path.startNewSubPath (pt);
                    }
                }
                c.threshold = r.uni() * 0.95f;
                cracks.push_back (std::move (c));
            }
        }

        void paintAge (juce::Graphics& g)
        {
            const float a = age->load();
            // years carved around the knob: they catch fire one by one as AGE goes up
            const int ticks = 40;
            for (int i = 0; i < ticks; ++i)
            {
                const float ang = -juce::MathConstants<float>::halfPi + (float) i / (float) ticks * juce::MathConstants<float>::twoPi;
                const float r0 = radius + 6.0f, r1 = radius + (i % 5 == 0 ? 15.0f : 10.0f);
                juce::Path t;
                t.startNewSubPath (centre.x + std::cos (ang) * r0, centre.y + std::sin (ang) * r0);
                t.lineTo (centre.x + std::cos (ang) * r1, centre.y + std::sin (ang) * r1);
                const bool lit = (float) i < a * (float) ticks;
                if (lit) art::drawLavaVein (g, t, 1.6f, 0.9f, colours::hot);
                else art::engrave (g, t, 1.6f, 0.9f);
            }
            // the older it gets, the more the stone cracks
            for (const auto& c : cracks)
            {
                if (a <= c.threshold) continue;
                const float heat = juce::jlimit (0.0f, 1.0f, (a - c.threshold) * 3.0f);
                art::drawLavaVein (g, c.path, 0.6f + 0.6f * heat, 0.15f + 0.75f * heat, colours::hot);
            }
            // at the bottom: an old circuit carved in the stone, the lava runs further along it with AGE
            auto s = getLocalBounds().toFloat().withTrimmedTop (getHeight() - 120.0f).reduced (24.0f, 18.0f);
            juce::Path trace, parts;
            const float y = s.getCentreY();
            trace.startNewSubPath (s.getX(), y);
            const int n = 6;
            const float seg = s.getWidth() / (float) n;
            for (int i = 0; i < n; ++i)
            {
                const float x0 = s.getX() + seg * (float) i, xm = x0 + seg * 0.5f;
                trace.lineTo (xm - 14.0f, y);
                switch (i % 3)
                {
                    case 0:   // resistor
                        for (int k = 0; k < 6; ++k) trace.lineTo (xm - 14.0f + 4.7f * (float) (k + 1) - 2.3f, y + (k % 2 == 0 ? -6.0f : 6.0f));
                        trace.lineTo (xm + 14.0f, y);
                        break;
                    case 1:   // capacitor
                        trace.lineTo (xm - 3.0f, y); trace.startNewSubPath (xm + 3.0f, y); trace.lineTo (xm + 14.0f, y);
                        parts.startNewSubPath (xm - 3.0f, y - 10.0f); parts.lineTo (xm - 3.0f, y + 10.0f);
                        parts.startNewSubPath (xm + 3.0f, y - 10.0f); parts.lineTo (xm + 3.0f, y + 10.0f);
                        break;
                    default:  // a little tube
                        parts.addEllipse (xm - 12.0f, y - 16.0f, 24.0f, 30.0f);
                        trace.lineTo (xm - 6.0f, y); trace.lineTo (xm - 6.0f, y - 6.0f); trace.lineTo (xm + 6.0f, y - 6.0f);
                        trace.lineTo (xm + 6.0f, y); trace.lineTo (xm + 14.0f, y);
                        break;
                }
                trace.lineTo (x0 + seg, y);
            }
            art::engrave (g, trace, 2.0f);
            art::engrave (g, parts, 2.0f);
            if (a > 0.001f)
            {
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (s.withWidth (s.getWidth() * a).expanded (0.0f, 20.0f).toNearestInt());
                art::drawLavaVein (g, trace, 1.0f, 0.4f + 0.6f * a, colours::hot);
                art::drawLavaVein (g, parts, 1.0f, 0.3f + 0.5f * a, colours::hot);
            }
        }

        void paintMovement (juce::Graphics& g)
        {
            auto s = strip();
            const float d = drift->load(), sp = spread->load(), tol = tolerance->load();
            // three voices carved as waves: drift makes them wander, spread pulls them apart, tolerance makes them uneven
            for (int v = 0; v < 3; ++v)
            {
                juce::Path p;
                const float yc = s.getCentreY() + ((float) v - 1.0f) * (6.0f + 14.0f * sp);
                const float amp = s.getHeight() * 0.16f * (1.0f + tol * ((float) v - 1.0f) * 0.6f);
                const float phase = (float) v * (0.3f + 1.4f * sp) + time * (0.6f + 0.3f * (float) v) * d;
                for (int i = 0; i <= 80; ++i)
                {
                    const float t = (float) i / 80.0f;
                    const float wander = d * 6.0f * std::sin (t * 3.1f + time * 0.7f * (float) (v + 1));
                    const float y = yc + std::sin (t * juce::MathConstants<float>::twoPi * 3.0f + phase) * amp + wander;
                    const float x = s.getX() + t * s.getWidth();
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                art::engrave (g, p, 1.6f);
                const float heat = juce::jlimit (0.0f, 1.0f, (d + sp + tol) * 0.6f);
                if (heat > 0.01f) art::drawLavaVein (g, p, 0.7f, heat, colours::accent2);
            }
        }

        void paintCircuit (juce::Graphics& g)
        {
            auto s = strip();
            const float c = circuit->load();
            // a vacuum tube carved in the stone: the filament burns with the saturation
            auto bulb = juce::Rectangle<float> (s.getHeight() * 0.62f, s.getHeight()).withCentre (s.getCentre());
            juce::Path glass, pins, filament;
            glass.addRoundedRectangle (bulb.withTrimmedBottom (bulb.getHeight() * 0.18f), bulb.getWidth() * 0.45f);
            const float by = bulb.getBottom() - bulb.getHeight() * 0.18f;
            glass.addRectangle (bulb.getX() + 4.0f, by, bulb.getWidth() - 8.0f, bulb.getHeight() * 0.12f);
            for (int k = 0; k < 4; ++k)
            {
                const float px = bulb.getX() + bulb.getWidth() * (0.2f + 0.2f * (float) k);
                pins.startNewSubPath (px, by + bulb.getHeight() * 0.12f);
                pins.lineTo (px, bulb.getBottom());
            }
            const float fx = bulb.getCentreX(), ft = bulb.getY() + bulb.getHeight() * 0.25f, fb = by - 6.0f;
            filament.startNewSubPath (fx - 9.0f, fb);
            filament.lineTo (fx - 9.0f, ft + 10.0f);
            for (int k = 0; k < 4; ++k) filament.lineTo (fx - 9.0f + 4.5f * (float) (k + 1), ft + (k % 2 == 0 ? 0.0f : 10.0f));
            filament.lineTo (fx + 9.0f, fb);
            art::engrave (g, glass, 2.0f);
            art::engrave (g, pins, 2.0f);
            if (c > 0.005f)
            {
                const float flick = 0.92f + 0.08f * std::sin (time * 9.0f) * std::sin (time * 3.7f);
                g.setColour (colours::accent.withAlpha (0.12f * c * flick));
                g.fillEllipse (bulb.reduced (bulb.getWidth() * 0.1f, bulb.getHeight() * 0.2f));
                art::drawLavaVein (g, filament, 1.2f, c * flick, colours::accent);
            }
            else art::engrave (g, filament, 1.4f);
            // wires out to the sides
            juce::Path wires;
            wires.startNewSubPath (s.getX(), bulb.getBottom() - 4.0f); wires.lineTo (bulb.getX() + bulb.getWidth() * 0.2f, bulb.getBottom() - 4.0f);
            wires.startNewSubPath (bulb.getX() + bulb.getWidth() * 0.8f, bulb.getBottom() - 4.0f); wires.lineTo (s.getRight(), bulb.getBottom() - 4.0f);
            art::engrave (g, wires, 1.6f);
            if (c > 0.005f) art::drawLavaVein (g, wires, 0.6f, c * 0.7f, colours::accent);
        }

        void paintNoise (juce::Graphics& g)
        {
            auto s = strip();
            const float n = noiseAmt->load(), h = hum->load();
            const float cycles = humFreq->load() > 0.5f ? 6.0f : 5.0f;   // 60 Hz vs 50 Hz
            juce::Path p;
            Rng r; r.seed ((uint32_t) (time * 12.0f) * 7u + 3u);
            for (int i = 0; i <= 160; ++i)
            {
                const float t = (float) i / 160.0f;
                const float y = s.getCentreY() + std::sin (t * juce::MathConstants<float>::twoPi * cycles + time * 2.0f) * s.getHeight() * 0.3f * (0.15f + h)
                                + r.bi() * s.getHeight() * 0.25f * n;
                const float x = s.getX() + t * s.getWidth();
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            art::engrave (g, p, 1.5f);
            if (n + h > 0.01f) art::drawLavaVein (g, p, 0.6f, juce::jlimit (0.0f, 1.0f, (n + h) * 0.8f), colours::accent);
        }

        void paintConverter (juce::Graphics& g)
        {
            auto s = strip();
            const float b = bits->load(), rt = rate->load();
            // a smooth wave and the stair steps the old converter makes of it
            const float levels = juce::jmap (b, 0.0f, 1.0f, 24.0f, 3.0f);
            const int holds = (int) juce::jmap (rt, 0.0f, 1.0f, 120.0f, 10.0f);
            juce::Path smooth, steps;
            float prevY = 0.0f;
            for (int i = 0; i <= 120; ++i)
            {
                const float t = (float) i / 120.0f;
                const float v = std::sin (t * juce::MathConstants<float>::twoPi * 1.5f);
                const float x = s.getX() + t * s.getWidth();
                const float y = s.getCentreY() - v * s.getHeight() * 0.38f;
                if (i == 0) smooth.startNewSubPath (x, y); else smooth.lineTo (x, y);
                const float th = std::floor (t * (float) holds) / (float) holds;
                const float vq = std::round (std::sin (th * juce::MathConstants<float>::twoPi * 1.5f) * levels * 0.5f) / (levels * 0.5f);
                const float yq = s.getCentreY() - vq * s.getHeight() * 0.38f;
                if (i == 0) steps.startNewSubPath (x, yq);
                else { steps.lineTo (x, prevY); steps.lineTo (x, yq); }
                prevY = yq;
            }
            art::engrave (g, smooth, 1.2f, 0.6f);
            art::engrave (g, steps, 1.8f);
            const float heat = juce::jlimit (0.0f, 1.0f, (b + rt) * 0.8f);
            if (heat > 0.01f) art::drawLavaVein (g, steps, 0.8f, heat, colours::accent2);
        }

        struct Crack { juce::Path path; float threshold = 0.0f; };
        Kind kind;
        std::atomic<float>* age {}, *drift {}, *spread {}, *tolerance {}, *circuit {}, *noiseAmt {}, *hum {}, *humFreq {}, *bits {}, *rate {};
        juce::Point<float> centre;
        float radius = 0.0f, time = 0.0f, lastSig = -1.0f;
        std::vector<Crack> cracks;
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
            // the carved pictures sit behind the knobs
            const std::pair<Panel*, AnalogArt::Kind> arts[] = { { &agePanel, AnalogArt::Age }, { &movement, AnalogArt::Movement },
                                                                { &character, AnalogArt::Circuit }, { &noise, AnalogArt::Noise },
                                                                { &converter, AnalogArt::Converter } };
            for (auto& [panel, kind] : arts)
                panel->addAndMakeVisible (artworks.add (new AnalogArt (kind, ui.st)));
            agePanel.tint = colours::hot; movement.tint = colours::accent2; character.tint = colours::accent;
            noise.tint = colours::accent; converter.tint = colours::accent2;
            Builder ba { ui, agePanel, owned };   age = ba.knob ("an_age", "AGE");
            age->slider.getProperties().set ("lava", true);   // the heart of the page: a molten core like the CHAOS knobs
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

            for (auto* a : artworks) a->setBounds (a->getParentComponent()->getLocalBounds().withTrimmedTop (Panel::headerH));
            const auto kb = age->slider.getBounds().toFloat() + age->getPosition().toFloat() - artworks[0]->getPosition().toFloat();
            artworks[0]->setKnobCircle (kb.getCentre(), std::min (kb.getWidth(), kb.getHeight()) * 0.5f);
        }

        void tick() override
        {
            for (auto* a : artworks) a->tick (1.0f / 30.0f);
        }

        Panel agePanel, movement, character, noise, converter;
        juce::OwnedArray<AnalogArt> artworks;
        juce::OwnedArray<juce::Component> owned;
        Knob* age, *drift, *spread, *tolerance, *circuit, *noiseK, *hum, *bits, *rate;
        ParamCombo* humFreq;
    };

    //==========================================================================
    /** Step editor as a piano roll: each column is a step, each row a note of the scale (relative to the key you play).
        Click a cell to put the step's note there, click the note again to silence the step, drag to draw a melody.
        Under the roll: velocity, gate and tie lanes; the lock row keeps steps when you press RANDOM. */
    class StepEditor : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        StepEditor (PyraAudioProcessor& p) : proc (p), pattern (p.arpPattern)
        {
            steps = p.apvts.getRawParameterValue ("arp_steps");
            scaleParam = p.apvts.getRawParameterValue ("arp_scale");
            keyParam = p.apvts.getRawParameterValue ("arp_key");
            setRepaintsOnMouseActivity (true);
            setTooltip (tr ("Click a cell to place the note of that step, click it again to silence the step, drag to draw. Right-click = silence. Double-click = back to the root."));
        }

        static constexpr int kTop = 14, kBottom = -14, kRows = kTop - kBottom + 1;
        enum Lane { LLock, LRoll, LVel, LGate, LTie, LCount };

        juce::Rectangle<int> laneBounds (Lane l) const
        {
            const int x = labelW, w = getWidth() - labelW, h = getHeight();
            const int lockH = 22, velH = 62, gateH = 62, tieH = 20, numH = 16, gap = 6;
            const int rollY = lockH + gap, rollH = h - (lockH + velH + gateH + tieH + numH + 5 * gap);
            switch (l)
            {
                case LLock: return { x, 0, w, lockH };
                case LRoll: return { x, rollY, w, rollH };
                case LVel:  return { x, rollY + rollH + gap, w, velH };
                case LGate: return { x, rollY + rollH + gap + velH + gap, w, gateH };
                default:    return { x, rollY + rollH + gap + velH + gap + gateH + gap, w, tieH };
            }
        }
        float rowH() const { return (float) laneBounds (LRoll).getHeight() / (float) kRows; }
        float rowY (int degree) const { return (float) laneBounds (LRoll).getY() + (float) (kTop - degree) * rowH(); }
        int degreeAt (int y) const { return juce::jlimit (kBottom, kTop, kTop - (int) std::floor (((float) y - (float) laneBounds (LRoll).getY()) / rowH())); }
        float colW() const { return (float) (getWidth() - labelW) / (float) kMaxArpSteps; }
        int columnAt (int x) const { return juce::jlimit (0, kMaxArpSteps - 1, (int) ((float) (x - labelW) / colW())); }
        juce::Rectangle<float> cell (Lane l, int c) const
        {
            const auto rb = laneBounds (l).toFloat();
            return { rb.getX() + colW() * (float) c + 1.0f, rb.getY(), colW() - 2.0f, rb.getHeight() };
        }

        static juce::String pcName (int n)
        {
            static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            return names[((n % 12) + 12) % 12];
        }

        void paint (juce::Graphics& g) override
        {
            const int active = juce::jlimit (1, kMaxArpSteps, (int) std::lround (steps->load()));
            const int playing = proc.arp.currentStep.load();
            const int scale = (int) std::lround (scaleParam->load()), key = (int) std::lround (keyParam->load());
            const int perOct = juce::jmax (1, (int) choices::scaleSteps (scale).size());
            const auto roll = laneBounds (LRoll).toFloat();
            const auto mouse = getMouseXYRelative();
            const bool over = isMouseOver() && laneBounds (LRoll).contains (mouse);

            // row labels and background stripes: the root and its octaves stand out, like the C rows of a piano roll
            for (int d = kBottom; d <= kTop; ++d)
            {
                const auto r = juce::Rectangle<float> ((float) labelW, rowY (d), roll.getWidth(), rowH());
                const bool root = ((d % perOct) + perOct) % perOct == 0;
                g.setColour (root ? colours::accent.withAlpha (d == 0 ? 0.16f : 0.09f) : ((d & 1) ? colours::panel : colours::panelHi).withAlpha (0.7f));
                g.fillRect (r);
                if (over && degreeAt (mouse.y) == d) { g.setColour (colours::text.withAlpha (0.05f)); g.fillRect (r); }
                const int note = scaleShift (60 + key, d, scale, key);
                const juce::String lab = (d == 0 ? tr ("ROOT") : (d > 0 ? "+" : "") + juce::String (d)) + "  " + pcName (note);
                if (rowH() >= 9.0f || root)
                    drawText (g, lab, juce::Rectangle<float> (0.0f, rowY (d), (float) labelW - 6.0f, rowH()).toNearestInt(),
                              juce::jmin (10.5f, rowH() + 0.5f), root ? colours::accent2 : colours::textDim, juce::Justification::centredRight, root, 1);
            }

            // columns: beat groups, steps not in use, playhead, hover column
            for (int c = 0; c < kMaxArpSteps; ++c)
            {
                const float x = (float) labelW + colW() * (float) c;
                g.setColour ((c % 4 == 0 ? colours::edge : colours::edge.withAlpha (0.35f)));
                g.drawVerticalLine ((int) x, roll.getY(), roll.getBottom());
                if (c >= active) { g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRect (x, roll.getY(), colW(), roll.getHeight()); }
                if (c == playing && c < active) { g.setColour (colours::accent.withAlpha (0.16f)); g.fillRect (x, roll.getY(), colW(), roll.getHeight()); }
            }

            // notes
            for (int c = 0; c < kMaxArpSteps; ++c)
            {
                const auto& s = pattern.steps[(size_t) c];
                const bool inUse = c < active;
                const int d = juce::jlimit (kBottom, kTop, s.degree.load());
                const float x = (float) labelW + colW() * (float) c;
                const bool tie = s.tie.load() && c + 1 < kMaxArpSteps;
                const float len = tie ? colW() + 1.0f : juce::jmax (colW() * 0.3f, colW() * juce::jlimit (0.05f, 1.0f, s.gate.load()));
                auto nb = juce::Rectangle<float> (x + 2.0f, rowY (d) + 1.0f, len - (tie ? 0.0f : 4.0f), rowH() - 2.0f);
                if (! s.on.load())
                {
                    g.setColour (colours::textDim.withAlpha (inUse ? 0.35f : 0.15f));
                    g.drawRoundedRectangle (nb, 2.0f, 1.0f);
                    continue;
                }
                const float a = (inUse ? 1.0f : 0.3f) * (0.45f + 0.55f * s.velocity.load());
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffb27a).withMultipliedAlpha (a), nb.getX(), nb.getY(),
                                                         juce::Colour (0xffd8360c).withMultipliedAlpha (a), nb.getX(), nb.getBottom(), false));
                g.fillRoundedRectangle (nb, 2.5f);
                g.setColour ((c == playing && inUse ? juce::Colours::white : juce::Colour (0xffffe0c8)).withMultipliedAlpha (inUse ? 0.9f : 0.3f));
                g.drawRoundedRectangle (nb, 2.5f, c == playing && inUse ? 1.6f : 0.8f);
            }
            g.setColour (colours::edge);
            g.drawRect (roll, 1.0f);

            // lanes under the roll
            const char* labels[] = { "LOCK", "", "VELOCITY", "GATE", "TIE" };
            for (auto l : { LLock, LVel, LGate, LTie })
                drawText (g, tr (labels[l]), laneBounds (l).withX (0).withWidth (labelW - 8), 11.0f, colours::textDim, juce::Justification::centredRight, true, 1);
            for (int c = 0; c < kMaxArpSteps; ++c)
            {
                const auto& s = pattern.steps[(size_t) c];
                const float alpha = c < active ? 1.0f : 0.25f;
                auto lk = cell (LLock, c).reduced (3.0f, 3.0f);
                g.setColour ((s.lock.load() ? colours::accent2 : colours::edge).withMultipliedAlpha (alpha));
                g.fillRoundedRectangle (lk.withTrimmedTop (lk.getHeight() * 0.45f), 2.0f);
                g.drawEllipse (lk.withTrimmedBottom (lk.getHeight() * 0.4f).reduced (lk.getWidth() * 0.22f, 1.0f), 1.5f);
                for (auto l : { LVel, LGate })
                {
                    auto bb = cell (l, c);
                    g.setColour (juce::Colour (0xff0c0908));
                    g.fillRect (bb);
                    const float v = l == LVel ? s.velocity.load() : s.gate.load();
                    const float a = alpha * (s.on.load() ? 1.0f : 0.4f);
                    auto bar = bb.withTop (bb.getBottom() - bb.getHeight() * v);
                    if (l == LVel)
                        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff9a5a).withMultipliedAlpha (a), 0.0f, bb.getY(),
                                                                 juce::Colour (0xff8a0c06).withMultipliedAlpha (a), 0.0f, bb.getBottom(), false));
                    else
                        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffa99b93).withMultipliedAlpha (a * 0.9f), 0.0f, bb.getY(),
                                                                 juce::Colour (0xff3e322e).withMultipliedAlpha (a), 0.0f, bb.getBottom(), false));
                    g.fillRect (bar);
                }
                auto tb = cell (LTie, c).reduced (2.0f, 3.0f);
                g.setColour ((s.tie.load() ? colours::accent2 : colours::knobBody).withMultipliedAlpha (alpha));
                g.fillRoundedRectangle (tb, 3.0f);
                if (c % 4 == 0)
                    drawText (g, juce::String (c + 1), cell (LTie, c).translated (0, 22).withHeight (14).toNearestInt(), 10.0f,
                              colours::textDim, juce::Justification::centredLeft, false, 1);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            lane = LCount;
            for (int l = 0; l < LCount; ++l)
                if (laneBounds ((Lane) l).contains (e.getPosition())) lane = l;
            if (lane == LCount) return;
            const int c = columnAt (e.x);
            auto& s = pattern.steps[(size_t) c];
            lastCol = -1;
            if (lane == LLock) toggleValue = ! s.lock.load();
            if (lane == LTie)  toggleValue = ! s.tie.load();
            if (lane == LRoll)
            {
                const int d = degreeAt (e.y);
                // clicking the note that is already there silences the step; dragging then erases
                if (e.mods.isPopupMenu()) rollMode = Erase;
                else rollMode = (s.on.load() && s.degree.load() == d) ? Erase : Draw;
            }
            apply (e);
        }

        void mouseDrag (const juce::MouseEvent& e) override { apply (e); }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (laneBounds (LRoll).contains (e.getPosition()))
            {
                auto& s = pattern.steps[(size_t) columnAt (e.x)];
                s.degree = 0; s.on = true;
                changed();
            }
        }

        void apply (const juce::MouseEvent& e)
        {
            if (lane == LCount) return;
            const int c = columnAt (e.x);
            auto& s = pattern.steps[(size_t) c];
            const auto rb = laneBounds ((Lane) lane).toFloat();
            const float rel = juce::jlimit (0.0f, 1.0f, (rb.getBottom() - (float) e.y) / rb.getHeight());
            switch (lane)
            {
                case LLock: s.lock = toggleValue; break;
                case LRoll:
                    if (rollMode == Erase) { if (c != lastCol || ! e.mouseWasDraggedSinceMouseDown()) s.on = false; }
                    else { s.degree = degreeAt (e.y); s.on = true; }
                    break;
                case LVel:  s.velocity = juce::jmax (0.05f, rel); break;
                case LGate: s.gate = juce::jmax (0.05f, rel); break;
                case LTie:  s.tie = toggleValue; break;
                default: break;
            }
            lastCol = c;
            changed();
        }

        void changed() { ++pattern.changeCounter; proc.markArpEdited(); repaint(); }

        void tick()
        {
            const int cc = pattern.changeCounter.load(), ps = proc.arp.currentStep.load(), st = (int) std::lround (steps->load());
            const int sk = (int) std::lround (scaleParam->load()) * 16 + (int) std::lround (keyParam->load());
            if (cc != lastCounter || ps != lastPlaying || st != lastSteps || sk != lastScaleKey)
            {
                lastCounter = cc; lastPlaying = ps; lastSteps = st; lastScaleKey = sk;
                repaint();
            }
        }

        static constexpr int labelW = 78;
        enum RollMode { Draw, Erase };
        PyraAudioProcessor& proc;
        ArpPattern& pattern;
        std::atomic<float>* steps, *scaleParam, *keyParam;
        int lane = LCount, lastCol = -1;
        RollMode rollMode = Draw;
        bool toggleValue = false;
        int lastCounter = -1, lastPlaying = -2, lastSteps = -1, lastScaleKey = -1;
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
            editorPanel.tint = colours::accent;

            Builder b { ui, top, owned };
            on = b.toggle ("arp_on", tr ("ARP ON"));
            top.setPowerButton (on);
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
            randomBtn.setTooltip (tr ("New random pattern of steps. Locked steps stay as they are."));
            randomBtn.setColour (juce::TextButton::buttonColourId, colours::accent.darker (0.3f));

            auto* pr = &ui.proc;
            top.addAndMakeVisible (randomAllBtn);
            randomAllBtn.setButtonText (tr ("RANDOM ALL"));
            randomAllBtn.setColour (juce::TextButton::buttonColourId, colours::hot.darker (0.4f));
            randomAllBtn.setTooltip (tr ("New values for every ARP control (mode, rate, scale, key, octaves, gate, swing, steps) and a new pattern"));
            randomAllBtn.onClick = [pr]
            {
                if (auto& hook = userActivityHook()) hook();
                randomizer::randomize (*pr, randomizer::Arp, false, (uint32_t) juce::Random::getSystemRandom().nextInt());
                pr->markArpEdited();
            };
            // a small die next to every control: rolls just that one
            const std::pair<juce::Component*, const char*> diceFor[] = { { mode, "arp_mode" }, { rate, "arp_rate" }, { scale, "arp_scale" },
                { key, "arp_key" }, { oct, "arp_oct" }, { gate, "arp_gate" }, { swing, "arp_swing" }, { stepsK, "arp_steps" }, { vel, "arp_velamt" } };
            for (auto& [comp, id] : diceFor)
            {
                auto* d = dice.add (new DiceButton());
                d->setTooltip (tr ("Random value for this control only (right-click: small change)"));
                d->onRoll = [pr, pid = juce::String (id)] (bool variation)
                {
                    randomizer::randomizeParam (*pr, pid, variation, (uint32_t) juce::Random::getSystemRandom().nextInt());
                };
                top.addAndMakeVisible (d);
                diceTargets.add (comp);
            }
            randomBtn.onClick = [pr]
            {
                if (auto& hook = userActivityHook()) hook();
                const int n = (int) std::lround (pr->apvts.getRawParameterValue ("arp_steps")->load());
                pr->arpPattern.randomize (n, (uint32_t) juce::Random::getSystemRandom().nextInt());
                pr->markArpEdited();
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
                pr->markArpEdited();
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
            drawText (g, tr ("Rows = notes of the scale from the key you hold (ROOT). In Sequencer mode the roll is the melody; in the other modes it moves each arpeggio note up or down. Locked steps survive RANDOM."),
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
            mode->setBounds (122, 46, 122, 24);
            rate->setBounds (274, 46, 84, 24);
            scale->setBounds (122, 94, 122, 24);
            key->setBounds (274, 94, 84, 24);
            layoutGrid ({ oct, gate, swing, stepsK, vel }, { 392, 30, 410, 92 }, 5, 92);
            for (int i = 0; i < dice.size(); ++i)
            {
                auto b = diceTargets[i]->getBounds();
                if (dynamic_cast<Knob*> (diceTargets[i]) != nullptr) dice[i]->setBounds (b.getRight() - 22, b.getY() - 2, 20, 18);
                else dice[i]->setBounds (b.getRight() + 4, b.getY() + 3, 20, 18);
            }
            randomBtn.setBounds (812, 34, 108, 38);
            randomAllBtn.setBounds (926, 34, 150, 38);
            clearBtn.setBounds (1082, 34, 92, 38);
            midiDrag.setBounds (812, 80, 262, 36);
            unlockBtn.setBounds (1082, 80, 160, 36);
        }

        void tick() override { editor.tick(); }

        Panel top, editorPanel;
        StepEditor editor;
        MidiDragButton midiDrag;
        juce::OwnedArray<juce::Component> owned;
        ParamToggle* on, *latch;
        ParamCombo* mode, *rate, *scale, *key;
        Knob* oct, *gate, *swing, *stepsK, *vel;
        juce::TextButton randomBtn, randomAllBtn, clearBtn, unlockBtn;
        juce::OwnedArray<DiceButton> dice;
        juce::Array<juce::Component*> diceTargets;
    };
}

std::unique_ptr<Page> createFxPage (UiContext& ui)     { return std::make_unique<FxPage> (ui); }
std::unique_ptr<Page> createAnalogPage (UiContext& ui) { return std::make_unique<AnalogPage> (ui); }
std::unique_ptr<Page> createArpPage (UiContext& ui)    { return std::make_unique<ArpPage> (ui); }

} // namespace pyra
