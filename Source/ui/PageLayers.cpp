// LAYERS page: up to 8 complete PYRA sounds stacked in one instance, each with its own preset, zone and mix.
#include "Pages.h"
#include "HellArt.h"
#include "PyraLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../Lang.h"
#include "../Randomizer.h"

namespace pyra
{
using lang::tr;

namespace
{
    juce::String noteText (int n) { return juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, n), true, true, 3); }
    bool isBlackKey (int n) { const int pc = n % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

    //==========================================================================
    /** Two handles over a range: a tiny keyboard (key zone) or a velocity ramp. Drag the ends or the middle. */
    class RangeBar : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        RangeBar (juce::AudioProcessorValueTreeState& st, const juce::String& loId, const juce::String& hiId, bool keysMode,
                  const juce::String& captionText)
            : lo (*st.getParameter (loId)), hi (*st.getParameter (hiId)), keys (keysMode), caption (captionText),
              loAtt (lo, [this] (float v) { loV = (int) std::lround (v); repaint(); }),
              hiAtt (hi, [this] (float v) { hiV = (int) std::lround (v); repaint(); })
        {
            minV = (int) lo.getNormalisableRange().start;
            maxV = (int) hi.getNormalisableRange().end;
            loAtt.sendInitialUpdate();
            hiAtt.sendInitialUpdate();
            setTooltip (keys ? tr ("Key zone: the layer plays only between these keys. Drag the ends or the middle. Double-click: whole keyboard.")
                             : tr ("Velocity zone: the layer plays only when you hit the keys this hard. Double-click: all velocities."));
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (colours::textDim);
            g.setFont (PyraLookAndFeel::titleFont (11.5f));
            g.drawText (caption, r.removeFromTop (13.0f), juce::Justification::centredLeft);
            auto txt = r.removeFromBottom (14.0f);
            auto bar = barArea();

            if (keys)
            {
                // a small stone keyboard: 128 keys
                const float kw = bar.getWidth() / 128.0f;
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8cec7), 0.0f, bar.getY(), juce::Colour (0xffa99d96), 0.0f, bar.getBottom(), false));
                g.fillRect (bar);
                for (int n = 0; n < 128; ++n)
                    if (isBlackKey (n))
                    {
                        g.setColour (juce::Colour (0xff120d0c));
                        g.fillRect (bar.getX() + kw * (float) n, bar.getY(), kw, bar.getHeight() * 0.62f);
                    }
                    else if (n % 12 == 0)
                    {
                        g.setColour (juce::Colour (0xff6a5a54));
                        g.fillRect (bar.getX() + kw * (float) n, bar.getY(), 1.0f, bar.getHeight());
                    }
            }
            else
            {
                // velocity: a wedge, soft to hard
                juce::Path wedge;
                wedge.addTriangle (bar.getX(), bar.getBottom(), bar.getRight(), bar.getBottom(), bar.getRight(), bar.getY());
                g.setColour (juce::Colour (0xff0b0807));
                g.fillRect (bar);
                g.setColour (juce::Colour (0xff4a3b36));
                g.fillPath (wedge);
            }

            // outside the zone: in the dark; inside: lit by lava
            const float x0 = xOf (loV), x1 = xOf (hiV + 1);
            g.setColour (juce::Colours::black.withAlpha (0.68f));
            g.fillRect (bar.withRight (x0));
            g.fillRect (bar.withLeft (x1));
            auto zone = juce::Rectangle<float> (x0, bar.getY(), std::max (1.0f, x1 - x0), bar.getHeight());
            if (keys)
            {   // the keys of the zone stay lit; a lava seam runs along the top and bottom of it
                g.setColour (colours::accent.withAlpha (0.10f));
                g.fillRect (zone);
                g.setColour (colours::accent.withAlpha (0.9f));
                g.fillRect (zone.withHeight (2.5f));
                g.fillRect (zone.withTop (zone.getBottom() - 2.0f));
            }
            else
            {
                juce::Graphics::ScopedSaveState ss (g);
                juce::Path wedge;
                wedge.addTriangle (bar.getX(), bar.getBottom(), bar.getRight(), bar.getBottom(), bar.getRight(), bar.getY());
                g.reduceClipRegion (zone.toNearestInt());
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a0c06), bar.getX(), 0.0f, juce::Colour (0xffff8a4a), bar.getRight(), 0.0f, false));
                g.fillPath (wedge);
            }
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.drawRect (bar.expanded (1.0f), 1.0f);
            for (float x : { x0, x1 })
            {
                g.setColour (colours::accent.withAlpha (0.35f));
                g.fillRect (x - 2.5f, bar.getY() - 2.0f, 5.0f, bar.getHeight() + 4.0f);
                g.setColour (juce::Colour (0xffffc0a0));
                g.fillRect (x - 0.75f, bar.getY() - 2.0f, 1.5f, bar.getHeight() + 4.0f);
            }

            g.setColour (colours::text);
            g.setFont (PyraLookAndFeel::font (11.5f));
            g.drawText (keys ? noteText (loV) + "  -  " + noteText (hiV) : juce::String (loV) + "  -  " + juce::String (hiV),
                        txt, juce::Justification::centred);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const float x = (float) e.x;
            const float d0 = std::abs (x - xOf (loV)), d1 = std::abs (x - xOf (hiV + 1));
            if (x > xOf (loV) + 6.0f && x < xOf (hiV + 1) - 6.0f && std::min (d0, d1) > 6.0f) dragMode = 2;
            else dragMode = d0 <= d1 ? 0 : 1;
            grab = valueAt (x);
            grabLo = loV; grabHi = hiV;
            loAtt.beginGesture(); hiAtt.beginGesture();
            if (dragMode != 2) mouseDrag (e);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const int v = valueAt ((float) e.x);
            if (dragMode == 0) loAtt.setValueAsPartOfGesture ((float) juce::jlimit (minV, hiV, v));
            else if (dragMode == 1) hiAtt.setValueAsPartOfGesture ((float) juce::jlimit (loV, maxV, v));
            else
            {
                const int span = grabHi - grabLo;
                const int nlo = juce::jlimit (minV, maxV - span, grabLo + (v - grab));
                loAtt.setValueAsPartOfGesture ((float) nlo);
                hiAtt.setValueAsPartOfGesture ((float) (nlo + span));
            }
            if (auto& hook = userActivityHook()) hook();
        }

        void mouseUp (const juce::MouseEvent&) override { loAtt.endGesture(); hiAtt.endGesture(); }

        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            loAtt.setValueAsCompleteGesture ((float) minV);
            hiAtt.setValueAsCompleteGesture ((float) maxV);
        }

    private:
        juce::Rectangle<float> barArea() const { return getLocalBounds().toFloat().withTrimmedTop (16.0f).withTrimmedBottom (16.0f).reduced (3.0f, 0.0f); }
        float xOf (int v) const { auto b = barArea(); return b.getX() + b.getWidth() * (float) (v - minV) / (float) (maxV - minV + 1); }
        int valueAt (float x) const
        {
            auto b = barArea();
            return juce::jlimit (minV, maxV, minV + (int) std::floor ((x - b.getX()) / b.getWidth() * (float) (maxV - minV + 1)));
        }

        juce::RangedAudioParameter& lo;
        juce::RangedAudioParameter& hi;
        const bool keys;
        juce::String caption;
        juce::ParameterAttachment loAtt, hiAtt;
        int loV = 0, hiV = 127, minV = 0, maxV = 127;
        int dragMode = 0, grab = 0, grabLo = 0, grabHi = 0;
    };

    //==========================================================================
    class LayerRow : public juce::Component
    {
    public:
        LayerRow (PyraAudioProcessor& m, int index, std::function<void (int)> editFn, std::function<int()> editingFn)
            : master (m), k (index), onEdit (std::move (editFn)), editing (std::move (editingFn)),
              power (m.apvts, layerParam (index, "on")),
              level (m.apvts, layerParam (index, "level"), "Level", nullptr),
              pan (m.apvts, layerParam (index, "pan"), "Pan", nullptr),
              trans (m.apvts, layerParam (index, "trans"), "Transpose", nullptr),
              keys (m.apvts, layerParam (index, "lo"), layerParam (index, "hi"), true, tr ("KEY ZONE")),
              vel (m.apvts, layerParam (index, "vlo"), layerParam (index, "vhi"), false, tr ("VELOCITY")),
              chan (m.apvts, layerParam (index, "chan"), false)
        {
            for (auto* c : std::initializer_list<juce::Component*> { &power, &presetBtn, &prevBtn, &nextBtn, &editBtn, &muteBtn, &soloBtn,
                                                                     &menuBtn, &dice, &level, &pan, &trans, &keys, &vel, &chan })
                addAndMakeVisible (c);

            power.setTooltip (tr ("Layer on / off"));
            presetBtn.setTooltip (tr ("Click to choose the preset of this layer"));
            presetBtn.onClick = [this] { showPresetMenu(); };
            prevBtn.onClick = [this] { step (-1); };
            nextBtn.onClick = [this] { step (1); };
            prevBtn.setTooltip (tr ("Previous preset (same category)"));
            nextBtn.setTooltip (tr ("Next preset (same category)"));
            editBtn.setTooltip (tr ("Edit this layer: every page (SYNTH, FX, ARP...) now shows this sound"));
            editBtn.onClick = [this] { if (onEdit) onEdit (k); };
            muteBtn.setTooltip (tr ("Mute"));
            soloBtn.setTooltip (tr ("Solo: hear only the soloed layers"));
            for (auto* b : { &muteBtn, &soloBtn }) b->setClickingTogglesState (true);
            muteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (m.apvts, layerParam (k, "mute"), muteBtn);
            soloAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (m.apvts, layerParam (k, "solo"), soloBtn);
            menuBtn.setTooltip (tr ("More: INIT, copy the sound of another layer, reset the mix"));
            menuBtn.onClick = [this] { showMoreMenu(); };
            dice.setTooltip (tr ("Random preset for this layer (right-click: small change to its level and pan)"));
            dice.onRoll = [this] (bool variation)
            {
                randomizer::randomizeLayer (master, k, variation, (uint32_t) juce::Random::getSystemRandom().nextInt());
            };
            chan.setTooltip (tr ("MIDI channel this layer listens to (Omni = all)"));
            trans.setTooltip (tr ("Transpose this layer in semitones (-12 = one octave down)"));
        }

        void tick()
        {
            const float peak = master.layerPeak[(size_t) k].exchange (0.0f);
            const float target = juce::jlimit (0.0f, 1.0f, std::sqrt (peak));
            const float nm = target > meter ? target : meter * 0.86f;
            if (std::abs (nm - meter) > 0.004f) { meter = nm; repaint (meterArea().expanded (2)); }

            auto* snd = master.getLayer (k);
            const juce::String name = snd == nullptr ? juce::String() : snd->getPresetManager().getCurrentName()
                                                      + (snd->getPresetManager().isModified() ? " *" : "");
            if (name != shownName)
            {
                shownName = name;
                presetBtn.setButtonText (name.isEmpty() ? tr ("Empty - click to load a preset") : name);
                presetBtn.setColour (juce::TextButton::textColourOffId, name.isEmpty() ? colours::textDim : colours::text);
            }
            const bool ed = editing && editing() == k;
            if (editBtn.getToggleState() != ed) { editBtn.setToggleState (ed, juce::dontSendNotification); repaint(); }
            const bool on = power.getToggleState();
            if (on != lastOn) { lastOn = on; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f, 2.0f);
            const bool on = power.getToggleState();
            const bool ed = editing && editing() == k;
            juce::Path slab;
            slab.addRoundedRectangle (r, 6.0f);
            art::fillStone (g, slab, { -(float) k * 97.0f, -(float) k * 53.0f }, on ? 0.55f : 0.72f, 0.55f);
            if (ed)
            {
                g.setColour (colours::accent.withAlpha (0.10f));
                g.fillPath (slab);
            }
            g.setColour (juce::Colours::black.withAlpha (0.85f));
            g.drawRoundedRectangle (r.expanded (0.5f), 6.5f, 1.2f);
            g.setColour (ed ? colours::accent : juce::Colour (0xff3e322e));
            g.drawRoundedRectangle (r, 6.0f, ed ? 1.6f : 1.0f);

            // the letter, carved; it burns when the layer is on
            auto letterArea = juce::Rectangle<float> (r.getX() + 4.0f, r.getY() + 2.0f, 42.0f, 44.0f);
            const auto letter = juce::String::charToString ((juce::juce_wchar) ('A' + k));
            g.setFont (PyraLookAndFeel::displayFont (34.0f));
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.drawText (letter, letterArea.translated (1.0f, 1.5f), juce::Justification::centred);
            if (on)
            {
                g.setColour (colours::accent.withAlpha (0.45f));
                g.drawText (letter, letterArea.translated (0.0f, 1.0f).expanded (1.0f), juce::Justification::centred);
            }
            g.setColour (on ? colours::accent.interpolatedWith (colours::text, 0.35f) : colours::textDim.withAlpha (0.6f));
            g.drawText (letter, letterArea, juce::Justification::centred);

            // meter: a lava column
            auto m = meterArea().toFloat();
            g.setColour (juce::Colour (0xff070505));
            g.fillRoundedRectangle (m, 2.0f);
            auto lit = m.withTop (m.getBottom() - m.getHeight() * meter);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffb080), 0.0f, m.getY(), juce::Colour (0xff8a0a06), 0.0f, m.getBottom(), false));
            g.fillRoundedRectangle (lit, 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.drawRoundedRectangle (m, 2.0f, 1.0f);

            g.setColour (colours::textDim);
            g.setFont (PyraLookAndFeel::titleFont (11.5f));
            g.drawText ("MIDI", chan.getBounds().translated (0, -16).withHeight (13), juce::Justification::centredLeft);
        }

        void resized() override
        {
            const int h = getHeight();
            power.setBounds (14, h - 30, 24, 20);
            int x = 54;
            const int y1 = 10, y2 = h - 36;
            prevBtn.setBounds (x, y1, 26, 26);
            presetBtn.setBounds (x + 30, y1, 236, 26);
            nextBtn.setBounds (x + 270, y1, 26, 26);
            editBtn.setBounds (x, y2, 84, 26);
            muteBtn.setBounds (x + 90, y2, 30, 26);
            soloBtn.setBounds (x + 124, y2, 30, 26);
            menuBtn.setBounds (x + 160, y2, 40, 26);
            dice.setBounds (x + 206, y2 + 3, 22, 20);
            x = 362;
            const int kh = std::min (74, h - 4);
            level.setBounds (x, (h - kh) / 2, 66, kh);
            pan.setBounds (x + 70, (h - kh) / 2, 66, kh);
            trans.setBounds (x + 140, (h - kh) / 2, 66, kh);
            x = 584;
            keys.setBounds (x, 6, 380, h - 12);
            vel.setBounds (x + 394, 6, 150, h - 12);
            chan.setBounds (x + 560, h / 2 - 4, 76, 24);
        }

    private:
        juce::Rectangle<int> meterArea() const { return { getWidth() - 20, 8, 9, getHeight() - 16 }; }

        void loadPreset (const juce::String& id)
        {
            auto& snd = master.getOrCreateLayer (k);
            if (auto& hook = userActivityHook()) hook();
            if (id.isEmpty()) snd.getPresetManager().loadInit();
            else snd.getPresetManager().load (id);
            if (auto* p = master.apvts.getParameter (layerParam (k, "on")))
                if (p->getValue() < 0.5f) p->setValueNotifyingHost (1.0f);
        }

        void step (int delta)
        {
            auto& pm = master.getOrCreateLayer (k).getPresetManager();
            juce::String cat;
            if (auto* cur = pm.find (pm.getCurrentId())) cat = cur->category;
            juce::StringArray ids;
            for (auto& p : pm.getAll())
                if (cat.isEmpty() || p.category == cat) ids.add (p.id);
            if (ids.isEmpty()) return;
            int i = ids.indexOf (pm.getCurrentId());
            i = i < 0 ? (delta > 0 ? 0 : ids.size() - 1) : (i + delta + ids.size()) % ids.size();
            loadPreset (ids[i]);
        }

        void showPresetMenu()
        {
            auto& pm = master.getOrCreateLayer (k).getPresetManager();
            juce::PopupMenu menu;
            std::vector<juce::String> ids { {} };
            menu.addItem (1, tr ("INIT (empty sound)"));
            menu.addSeparator();
            juce::StringArray cats = PresetManager::getCategories();
            for (auto& p : pm.getAll())
                if (! cats.contains (p.category) && p.category.isNotEmpty()) cats.add (p.category);
            for (auto& cat : cats)
            {
                juce::PopupMenu sub;
                for (auto& p : pm.getAll())
                    if (p.category == cat)
                    {
                        ids.push_back (p.id);
                        sub.addItem ((int) ids.size(), p.name + (p.factory ? "" : "  (user)"), true, p.id == pm.getCurrentId());
                    }
                if (sub.getNumItems() > 0) menu.addSubMenu (tr (cat), sub);
            }
            juce::Component::SafePointer<LayerRow> safe (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn).withMaximumNumColumns (3),
                                [safe, ids] (int result)
                                {
                                    if (safe == nullptr || result <= 0 || result > (int) ids.size()) return;
                                    safe->loadPreset (ids[(size_t) result - 1]);
                                });
        }

        void showMoreMenu()
        {
            juce::PopupMenu menu, copy;
            menu.addItem (1, tr ("INIT (empty sound)"));
            for (int j = 0; j < kNumLayers; ++j)
                if (j != k && master.getLayer (j) != nullptr)
                    copy.addItem (100 + j, tr ("Layer") + " " + juce::String::charToString ((juce::juce_wchar) ('A' + j))
                                               + "  (" + master.getLayer (j)->getPresetManager().getCurrentName() + ")");
            menu.addSubMenu (tr ("Copy the sound of"), copy, copy.getNumItems() > 0);
            menu.addItem (2, tr ("Reset mix and zones"));
            menu.addItem (3, tr ("Random preset"));
            juce::Component::SafePointer<LayerRow> safe (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuBtn), [safe] (int result)
            {
                if (safe == nullptr || result <= 0) return;
                auto& self = *safe;
                if (auto& hook = userActivityHook()) hook();
                if (result == 1) self.loadPreset ({});
                else if (result == 2)
                {
                    for (auto* n : { "level", "pan", "trans", "lo", "hi", "vlo", "vhi", "mute", "solo", "chan" })
                        if (auto* p = self.master.apvts.getParameter (layerParam (self.k, n))) p->setValueNotifyingHost (p->getDefaultValue());
                }
                else if (result == 3) randomizer::randomizeLayer (self.master, self.k, false, (uint32_t) juce::Random::getSystemRandom().nextInt());
                else if (result >= 100) self.master.copyLayerSound (result - 100, self.k);
            });
        }

        PyraAudioProcessor& master;
        const int k;
        std::function<void (int)> onEdit;
        std::function<int()> editing;
        ParamToggle power;
        juce::TextButton presetBtn, prevBtn { "<" }, nextBtn { ">" }, editBtn { "EDIT" }, muteBtn { "M" }, soloBtn { "S" }, menuBtn { "..." };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAtt, soloAtt;
        DiceButton dice;
        Knob level, pan, trans;
        RangeBar keys, vel;
        ParamCombo chan;
        float meter = 0.0f;
        juce::String shownName { "?" };
        bool lastOn = false;
    };

    //==========================================================================
    class LayersPage : public Page
    {
    public:
        LayersPage (PyraAudioProcessor& m, std::function<void (int)> onEdit, std::function<int()> editing)
            : panel (tr ("LAYERS"))
        {
            addAndMakeVisible (panel);
            for (int k = 0; k < kNumLayers; ++k)
                panel.addAndMakeVisible (rows.add (new LayerRow (m, k, onEdit, editing)));
            panel.tint = colours::accent;
            tick();
        }

        void visibilityChanged() override { if (isVisible()) tick(); }

        void tick() override { for (auto* r : rows) r->tick(); }

        void paintOverChildren (juce::Graphics& g) override
        {
            // the hint in the header of the panel
            auto hb = panel.getBounds().removeFromTop (Panel::headerH).withTrimmedLeft (130).withTrimmedRight (14);
            g.setColour (colours::textDim);
            g.setFont (PyraLookAndFeel::font (12.0f));
            g.drawText (tr ("Up to 8 complete sounds in one PYRA. Each layer has its own preset, FX, ARP and CHORDS: EDIT opens it in every page."),
                        hb, juce::Justification::centredRight, true);
        }

        void resized() override
        {
            panel.setBounds (getLocalBounds().reduced (8, 6));
            auto area = panel.getLocalBounds().withTrimmedTop (Panel::headerH + 4).reduced (8, 4);
            const int h = area.getHeight() / kNumLayers;
            for (int k = 0; k < rows.size(); ++k)
                rows[k]->setBounds (area.getX(), area.getY() + k * h, area.getWidth(), h - 2);
        }

    private:
        Panel panel;
        juce::OwnedArray<LayerRow> rows;
    };
}

std::unique_ptr<Page> createLayersPage (PyraAudioProcessor& master, std::function<void (int)> onEdit, std::function<int()> editing)
{
    return std::make_unique<LayersPage> (master, std::move (onEdit), std::move (editing));
}
} // namespace pyra
