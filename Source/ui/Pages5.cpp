// Phase 5 page: CHAOS / FOCUS / MOTION
#include "Pages.h"
#include "PyraLookAndFeel.h"
#include "../Lang.h"
#include "../PluginProcessor.h"

namespace pyra
{
using lang::tr;

namespace
{
    void text (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, float size, juce::Colour c,
               juce::Justification j = juce::Justification::centredLeft, bool bold = false, int lines = 1)
    {
        g.setColour (c);
        g.setFont (PyraLookAndFeel::font (size, bold));
        g.drawFittedText (t, r, j, lines, 1.0f);
    }

    /** A large knob with an ember glow that grows with its value. */
    class BigKnob : public juce::Component
    {
    public:
        BigKnob (UiContext& ui, const juce::String& id, const juce::String& label, juce::Colour c)
            : knob (ui.st, id, label, nullptr), colour (c)
        {
            addAndMakeVisible (knob);
            ui.knobs.add (&knob);
            knob.slider.setMouseDragSensitivity (320);
        }

        void setActivity (float a) { if (std::abs (a - activity) > 0.01f) { activity = a; repaint(); } }

        void paint (juce::Graphics& g) override
        {
            const float v = (float) knob.slider.valueToProportionOfLength (knob.slider.getValue());
            const auto b = knob.slider.getBounds().toFloat();
            const auto c = b.getCentre();
            const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
            const float glow = juce::jlimit (0.0f, 1.0f, v * (0.85f + 0.3f * activity));
            if (glow > 0.01f)
            {
                const float gr = r * (1.05f + 0.45f * glow);
                juce::ColourGradient grad (colour.withAlpha (0.55f * glow), c.x, c.y, colour.withAlpha (0.0f), c.x + gr, c.y, true);
                g.setGradientFill (grad);
                g.fillEllipse (c.x - gr, c.y - gr, gr * 2.0f, gr * 2.0f);
            }
            lastV = v;
        }

        // the glow needs room around the knob, so the knob sits inside a larger area
        void resized() override { knob.setBounds (getLocalBounds().reduced (40, 0).withTrimmedTop (22).withTrimmedBottom (14)); }

        void tick()
        {
            const float v = (float) knob.slider.valueToProportionOfLength (knob.slider.getValue());
            if (std::abs (v - lastV) > 0.002f) repaint();
        }

        Knob knob;
        juce::Colour colour;
        float activity = 0, lastV = -1;
    };

    class MacroPanel : public Panel
    {
    public:
        MacroPanel (const juce::String& t, const juce::String& sub) : Panel (t), subtitle (sub) {}
        void paint (juce::Graphics& g) override
        {
            Panel::paint (g);
            text (g, subtitle, { 12, 26, getWidth() - 24, 34 }, 12.0f, colours::textDim, juce::Justification::topLeft, false, 2);
        }
        juce::String subtitle;
    };

    //==========================================================================
    class ChaosPage : public Page
    {
    public:
        explicit ChaosPage (UiContext& u)
            : ui (u), proc (u.proc),
              chaosPanel ("CHAOS", tr ("Random movement inside safe ranges. Every turn makes the sound alive and never the same.")),
              focusPanel ("FOCUS", tr ("One knob, many targets: you choose what it moves, how far and with which curve.")),
              motionPanel ("MOTION", tr ("Rhythmic movement locked to the tempo of your DAW.")),
              chaosKnob (u, "chaos_amt", "CHAOS", colours::hot),
              focusKnob (u, "focus_amt", "FOCUS", colours::accent2),
              motionKnob (u, "motion_amt", "MOTION", colours::mod)
        {
            for (auto* p : { &chaosPanel, &focusPanel, &motionPanel }) addAndMakeVisible (p);
            chaosPanel.addAndMakeVisible (chaosKnob);
            focusPanel.addAndMakeVisible (focusKnob);
            motionPanel.addAndMakeVisible (motionKnob);

            auto knob = [this] (juce::Component& parent, const juce::String& id, const juce::String& label)
            {
                auto* k = new Knob (ui.st, id, label, nullptr);
                owned.add (k); parent.addAndMakeVisible (k); ui.knobs.add (k);
                return k;
            };
            speed = knob (chaosPanel, "chaos_speed", "Speed");
            bite = knob (chaosPanel, "chaos_bite", "Bite");
            space = knob (chaosPanel, "chaos_space", "Space");
            speed->setTooltip (tr ("How fast the chaos moves"));
            bite->setTooltip (tr ("How much chaos touches grit: warp, FM, resonance, drive, distortion"));
            space->setTooltip (tr ("How much chaos touches space: stereo, width, reverb, delay"));

            chaosPanel.addAndMakeVisible (roll);
            roll.setButtonText (tr ("ROLL"));
            roll.setTooltip (tr ("New random character for the chaos (same amount, different movement)"));
            roll.setColour (juce::TextButton::buttonColourId, colours::hot.darker (0.4f));
            seedParam = ui.st.getParameter ("chaos_seed");
            roll.onClick = [this]
            {
                const float v = (float) juce::Random::getSystemRandom().nextInt ({ 1, 10000 });
                seedParam->beginChangeGesture();
                seedParam->setValueNotifyingHost (seedParam->convertTo0to1 (v));
                seedParam->endChangeGesture();
            };

            // FOCUS assignments
            for (int k = 0; k < kNumFocusSlots; ++k)
            {
                const auto n = "focus" + juce::String (k + 1) + "_";
                Row r;
                r.dst = std::make_unique<ParamCombo> (ui.st, n + "dst", false);
                for (auto* sl : { &r.depth, &r.curve })
                {
                    *sl = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
                    (*sl)->setColour (juce::Slider::trackColourId, colours::accent2);
                    (*sl)->setColour (juce::Slider::backgroundColourId, colours::edge);
                    (*sl)->setColour (juce::Slider::thumbColourId, colours::text);
                    (*sl)->setDoubleClickReturnValue (true, 0.0);
                    focusPanel.addAndMakeVisible (sl->get());
                }
                r.depth->setTooltip (tr ("How far FOCUS moves this target (left = opposite direction)"));
                r.curve->setTooltip (tr ("Curve: left = acts early, centre = straight, right = acts late"));
                r.depthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (ui.st, n + "depth", *r.depth);
                r.curveAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (ui.st, n + "curve", *r.curve);
                focusPanel.addAndMakeVisible (r.dst.get());
                rows.push_back (std::move (r));
            }

            // MOTION
            shape = std::make_unique<ParamCombo> (ui.st, "motion_shape");
            rate = std::make_unique<ParamCombo> (ui.st, "motion_rate", false);
            motionPanel.addAndMakeVisible (shape.get());
            motionPanel.addAndMakeVisible (rate.get());
            smooth = knob (motionPanel, "motion_smooth", "Smooth");
            shapeParam = ui.st.getRawParameterValue ("motion_shape");
        }

        void resized() override
        {
            const int m = 8, w = (getWidth() - 4 * m) / 3, h = getHeight() - 2 * m;
            chaosPanel.setBounds (m, m, w, h);
            focusPanel.setBounds (2 * m + w, m, w, h);
            motionPanel.setBounds (3 * m + 2 * w, m, w, h);

            const int big = 230;
            for (auto* k : { &chaosKnob, &focusKnob, &motionKnob })
                k->setBounds ((w - 300) / 2, 44, 300, 284);

            // chaos
            const int ky = 66 + big + 34;
            const int kw = (w - 24) / 3;
            speed->setBounds (12, ky, kw, 92);
            bite->setBounds (12 + kw, ky, kw, 92);
            space->setBounds (12 + 2 * kw, ky, kw, 92);
            roll.setBounds (w / 2 - 70, ky + 106, 140, 36);

            // focus rows
            int y = 66 + big + 40;
            for (auto& r : rows)
            {
                r.dst->setBounds (12, y, 168, 24);
                r.depth->setBounds (186, y, (w - 198) / 2 + 20, 24);
                r.curve->setBounds (186 + (w - 198) / 2 + 26, y, w - 12 - (186 + (w - 198) / 2 + 26), 24);
                y += 31;
            }

            // motion
            shape->setBounds (12, ky + 18, w / 2 - 18, 26);
            rate->setBounds (w / 2 + 6, ky + 18, w / 2 - 18, 26);
            smooth->setBounds (12, ky + 56, 90, 90);
        }

        juce::Rectangle<int> chaosBarsArea() const { return { 12, chaosPanel.getHeight() - 118, chaosPanel.getWidth() - 24, 104 }; }
        juce::Rectangle<int> motionViewArea() const { return { 112, 66 + 230 + 34 + 56, motionPanel.getWidth() - 124, 90 }; }

        void paintOverChildren (juce::Graphics& g) override
        {
            // ---- chaos activity bars
            {
                auto area = chaosBarsArea().translated (chaosPanel.getX(), chaosPanel.getY());
                g.setColour (juce::Colour (0xff0d0d10));
                g.fillRoundedRectangle (area.toFloat(), 5.0f);
                const int n = Movement::kGens;
                const float bw = (float) area.getWidth() / (float) n;
                for (int i = 0; i < n; ++i)
                {
                    const float v = juce::jlimit (-1.0f, 1.0f, proc.engine.movement.uiGen[i].load());
                    auto cell = juce::Rectangle<float> ((float) area.getX() + bw * (float) i + 3.0f, (float) area.getY() + 8.0f,
                                                        bw - 6.0f, (float) area.getHeight() - 16.0f);
                    const float mid = cell.getCentreY();
                    const float hh = v * cell.getHeight() * 0.5f;
                    g.setColour (colours::hot.withAlpha (0.25f + 0.6f * std::abs (v)));
                    g.fillRect (juce::Rectangle<float> (cell.getX(), std::min (mid, mid - hh), cell.getWidth(), std::max (1.5f, std::abs (hh))));
                }
                text (g, tr ("chaos activity"), area.reduced (8, 4), 10.5f, colours::textDim, juce::Justification::topLeft);
            }

            // ---- focus column headers
            {
                const int y = focusPanel.getY() + 66 + 230 + 22;
                const int x0 = focusPanel.getX();
                const int w = focusPanel.getWidth();
                text (g, tr ("TARGET"), { x0 + 14, y, 160, 16 }, 10.5f, colours::textDim, juce::Justification::centredLeft, true);
                text (g, tr ("DEPTH"), { x0 + 188, y, 120, 16 }, 10.5f, colours::textDim, juce::Justification::centredLeft, true);
                text (g, tr ("CURVE"), { x0 + 186 + (w - 198) / 2 + 28, y, 120, 16 }, 10.5f, colours::textDim, juce::Justification::centredLeft, true);
            }

            // ---- motion shape preview with live dot
            {
                auto area = motionViewArea().translated (motionPanel.getX(), motionPanel.getY());
                g.setColour (juce::Colour (0xff0d0d10));
                g.fillRoundedRectangle (area.toFloat(), 5.0f);
                auto in = area.toFloat().reduced (8.0f, 10.0f);
                const int sh = (int) std::lround (shapeParam->load());
                juce::Path p;
                for (int i = 0; i <= 200; ++i)
                {
                    const float t = (float) i / 200.0f;
                    const float ph = std::fmod (t * 2.0f, 1.0f);
                    const int cyc = (int) (t * 2.0f);
                    float v;
                    switch (sh)
                    {
                        case 0: { const float x = juce::jlimit (0.0f, 1.0f, ph / 0.55f); v = 1.0f - x * x * (3.0f - 2.0f * x); break; }
                        case 1: v = ph >= 0.5f ? 1.0f : 0.0f; break;
                        case 2: v = (cyc == 1 || ph > 0.7f) ? 1.0f : 0.0f; break;
                        case 3: v = 1.0f - ph; break;
                        case 4: v = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * ph); break;
                        case 5: v = 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * ph); break;
                        case 6: v = std::fmod (ph * 2.0f, 1.0f) > 0.5f ? 1.0f : 0.0f; break;
                        case 7: v = cyc == 0 ? 0.7f : 0.25f; break;
                        default: v = (1.0f - ph) * (1.0f - ph); break;
                    }
                    const bool dip = sh <= 2 || sh == 6 || sh == 8;   // these shapes lower the volume
                    const float y = dip ? in.getY() + v * in.getHeight() : in.getBottom() - v * in.getHeight();
                    const float x = in.getX() + t * in.getWidth();
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                g.setColour (colours::mod.withAlpha (0.25f));
                g.strokePath (p, juce::PathStrokeType (4.0f));
                g.setColour (colours::mod);
                g.strokePath (p, juce::PathStrokeType (1.6f));
                const float live = proc.engine.movement.uiMotion.load();
                if (live != 0.0f)
                {
                    g.setColour (colours::accent2);
                    g.fillEllipse (in.getRight() - 6.0f, in.getCentreY() - 3.0f - live * in.getHeight() * 0.4f, 6.0f, 6.0f);
                }
                text (g, tr ("one cycle = the rate above"), area.reduced (8, 3), 10.5f, colours::textDim, juce::Justification::bottomRight);
            }

            // ---- labels
            text (g, tr ("SHAPE"), { motionPanel.getX() + 14, motionPanel.getY() + 66 + 230 + 34, 120, 16 }, 10.5f, colours::textDim,
                  juce::Justification::centredLeft, true);
            text (g, tr ("RATE"), { motionPanel.getX() + motionPanel.getWidth() / 2 + 8, motionPanel.getY() + 66 + 230 + 34, 120, 16 },
                  10.5f, colours::textDim, juce::Justification::centredLeft, true);
            text (g, tr ("Tip: all three also appear as mod sources (Chaos, Focus, Motion) in the MATRIX."),
                  { motionPanel.getX() + 14, motionPanel.getBottom() - 60, motionPanel.getWidth() - 28, 44 }, 11.5f, colours::textDim,
                  juce::Justification::topLeft, false, 3);
            text (g, tr ("Default targets: brightness, wavetable, drive and reverb. Change them freely."),
                  { focusPanel.getX() + 14, focusPanel.getBottom() - 46, focusPanel.getWidth() - 28, 36 }, 11.5f, colours::textDim,
                  juce::Justification::topLeft, false, 2);
        }

        void tick() override
        {
            float act = 0;
            for (auto& gv : proc.engine.movement.uiGen) act = std::max (act, std::abs (gv.load()));
            chaosKnob.setActivity (act);
            motionKnob.setActivity (std::abs (proc.engine.movement.uiMotion.load()));
            for (auto* k : { &chaosKnob, &focusKnob, &motionKnob }) k->tick();
            repaint (chaosBarsArea().translated (chaosPanel.getX(), chaosPanel.getY()));
            repaint (motionViewArea().translated (motionPanel.getX(), motionPanel.getY()));
        }

        struct Row
        {
            std::unique_ptr<ParamCombo> dst;
            std::unique_ptr<juce::Slider> depth, curve;
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAtt, curveAtt;
        };

        UiContext& ui;
        PyraAudioProcessor& proc;
        MacroPanel chaosPanel, focusPanel, motionPanel;
        BigKnob chaosKnob, focusKnob, motionKnob;
        juce::OwnedArray<juce::Component> owned;
        Knob* speed, *bite, *space, *smooth;
        juce::TextButton roll;
        juce::RangedAudioParameter* seedParam = nullptr;
        std::vector<Row> rows;
        std::unique_ptr<ParamCombo> shape, rate;
        std::atomic<float>* shapeParam = nullptr;
    };
}

std::unique_ptr<Page> createChaosPage (UiContext& ui) { return std::make_unique<ChaosPage> (ui); }

} // namespace pyra
