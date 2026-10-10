// SAMPLE page: the sampled-instrument layer
#include "Pages.h"
#include "PyraLookAndFeel.h"
#include "../Lang.h"
#include "../PluginProcessor.h"
#include "../dsp/Samples.h"

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

    struct Group { const char* title; std::vector<int> items; };
    const std::vector<Group>& groups()
    {
        static const std::vector<Group> g {
            { "KEYS", { 1, 2, 18, 19 } },
            { "STRINGS", { 3, 4, 5, 6, 7, 8, 9 } },
            { "BRASS & WINDS", { 10, 11, 12, 13, 14, 15, 16, 17 } } };
        return g;
    }

    class SamplePage : public Page
    {
    public:
        explicit SamplePage (UiContext& u) : ui (u), listPanel (tr ("INSTRUMENTS")), ctrlPanel (tr ("SAMPLE LAYER")), infoPanel (tr ("ABOUT THE SAMPLES"))
        {
            for (auto* p : { &listPanel, &ctrlPanel, &infoPanel }) addAndMakeVisible (p);
            instParam = ui.st.getParameter ("smp_inst");
            onParam = ui.st.getParameter ("smp_on");
            instValue = ui.st.getRawParameterValue ("smp_inst");

            auto knob = [this] (const juce::String& id, const juce::String& label)
            {
                auto* k = new Knob (ui.st, id, label, nullptr);
                owned.add (k); ctrlPanel.addAndMakeVisible (k); ui.knobs.add (k);
                return k;
            };
            level = knob ("smp_level", "Level"); pan = knob ("smp_pan", "Pan"); oct = knob ("smp_oct", "Octave");
            semi = knob ("smp_semi", "Semi"); fine = knob ("smp_fine", "Fine"); start = knob ("smp_start", "Start");
            vel = knob ("smp_vel", "Vel Layers");
            vel->setTooltip (tr ("How much your playing strength picks soft or loud recordings"));
            start->setTooltip (tr ("Skips the beginning of the recording (softer, slower attack when lower)"));

            on = std::make_unique<ParamToggle> (ui.st, "smp_on", tr ("SAMPLE ON"));
            route = std::make_unique<ParamCombo> (ui.st, "smp_route");
            ctrlPanel.addAndMakeVisible (on.get());
            ctrlPanel.addAndMakeVisible (route.get());
            setRepaintsOnMouseActivity (false);
        }

        // ---- instrument list
        juce::Rectangle<int> itemBounds (int groupIdx, int itemIdx) const
        {
            const int colW = (listPanel.getWidth() - 24) / 3;
            return { listPanel.getX() + 12 + groupIdx * colW, listPanel.getY() + 64 + itemIdx * 40, colW - 10, 34 };
        }

        void paint (juce::Graphics&) override {}

        void paintOverChildren (juce::Graphics& g) override
        {
            auto& bank = SampleBank::get();
            const int current = (int) std::lround (instValue->load());
            const bool isOn = onParam->getValue() > 0.5f;
            for (size_t gi = 0; gi < groups().size(); ++gi)
            {
                const auto& grp = groups()[gi];
                auto hb = itemBounds ((int) gi, 0).translated (0, -26).withHeight (20);
                text (g, tr (grp.title), hb, 11.0f, colours::textDim, juce::Justification::centredLeft, true);
                for (size_t ii = 0; ii < grp.items.size(); ++ii)
                {
                    const int idx = grp.items[ii];
                    auto b = itemBounds ((int) gi, (int) ii).toFloat();
                    const bool sel = idx == current;
                    g.setColour (sel ? colours::accent.withAlpha (isOn ? 0.85f : 0.4f) : colours::knobBody);
                    g.fillRoundedRectangle (b, 6.0f);
                    g.setColour (sel ? colours::accent2 : colours::edge);
                    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
                    text (g, tr (SampleBank::displayNames()[idx]), b.toNearestInt().reduced (12, 0), 14.0f,
                          sel ? juce::Colours::black : colours::text, juce::Justification::centredLeft, sel);
                    const auto st = bank.getStatus (idx);
                    if (st == SampleBank::Loading)
                        text (g, tr ("loading..."), b.toNearestInt().reduced (12, 0), 11.0f, sel ? juce::Colours::black : colours::accent2,
                              juce::Justification::centredRight);
                    else if (st == SampleBank::Ready)
                    {
                        g.setColour (sel ? juce::Colours::black.withAlpha (0.6f) : colours::mod);
                        g.fillEllipse (b.getRight() - 18.0f, b.getCentreY() - 3.0f, 6.0f, 6.0f);
                    }
                }
            }

            // status line
            auto sb = listPanel.getBounds().removeFromBottom (54).reduced (14, 8);
            juce::String status;
            juce::Colour sc = colours::textDim;
            if (! bank.isInstalled())
            {
                status = tr ("Samples not found. Reinstall PYRA with the included installer, or copy the \"Samples\" folder to:")
                         + "  " + bank.getFolder().getFullPathName();
                sc = colours::hot;
            }
            else if (! isOn) status = tr ("Click an instrument to switch the sample layer on.");
            else if (current > 0 && bank.getStatus (current) == SampleBank::Loading) status = tr ("Loading the recordings into memory...");
            else status = tr ("The sample layer goes through the same filters, envelopes and effects as the oscillators.");
            text (g, status, sb, 12.0f, sc, juce::Justification::centredLeft, false, 2);

            // credits
            auto ib = infoPanel.getBounds().withTrimmedTop (32).reduced (14, 6);
            text (g, tr ("SAMPLE_CREDITS"), ib, 12.5f, colours::text, juce::Justification::topLeft, false, 14);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            for (size_t gi = 0; gi < groups().size(); ++gi)
                for (size_t ii = 0; ii < groups()[gi].items.size(); ++ii)
                    if (itemBounds ((int) gi, (int) ii).contains (e.getPosition()))
                    {
                        const int idx = groups()[gi].items[ii];
                        SampleBank::get().acquire (idx);   // start loading right away
                        instParam->beginChangeGesture();
                        instParam->setValueNotifyingHost (instParam->convertTo0to1 ((float) idx));
                        instParam->endChangeGesture();
                        if (onParam->getValue() < 0.5f)
                        {
                            onParam->beginChangeGesture(); onParam->setValueNotifyingHost (1.0f); onParam->endChangeGesture();
                        }
                        repaint();
                        return;
                    }
        }

        void resized() override
        {
            const int m = 8;
            listPanel.setBounds (m, m, 820, getHeight() - 2 * m);
            const int x = m + 820 + m, w = getWidth() - x - m;
            ctrlPanel.setBounds (x, m, w, 330);
            infoPanel.setBounds (x, m + 330 + m, w, getHeight() - 330 - 3 * m);

            on->setBounds (14, 32, 160, 26);
            route->setBounds (w - 150, 32, 136, 24);
            const int kw = (w - 20) / 4;
            juce::Component* row1[] = { level, pan, oct, semi };
            juce::Component* row2[] = { fine, start, vel };
            for (int i = 0; i < 4; ++i) row1[i]->setBounds (10 + i * kw, 72, kw, 110);
            for (int i = 0; i < 3; ++i) row2[i]->setBounds (10 + i * kw, 192, kw, 110);
        }

        void tick() override
        {
            juce::int64 sig = (juce::int64) std::lround (instValue->load()) * 10 + (onParam->getValue() > 0.5f ? 1 : 0);
            for (int i = 1; i < SampleBank::ids().size(); ++i) sig = sig * 5 + (int) SampleBank::get().getStatus (i);
            if (sig != lastSig) { lastSig = sig; repaint(); }
        }

        UiContext& ui;
        Panel listPanel, ctrlPanel, infoPanel;
        juce::RangedAudioParameter* instParam = nullptr, *onParam = nullptr;
        std::atomic<float>* instValue = nullptr;
        juce::OwnedArray<juce::Component> owned;
        Knob* level, *pan, *oct, *semi, *fine, *start, *vel;
        std::unique_ptr<ParamToggle> on;
        std::unique_ptr<ParamCombo> route;
        juce::int64 lastSig = -1;
    };
}

std::unique_ptr<Page> createSamplePage (UiContext& ui) { return std::make_unique<SamplePage> (ui); }

} // namespace pyra
