// CHORDS page: scale-based chord progression generator.
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
    void text (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, float size, juce::Colour c,
               juce::Justification j = juce::Justification::centredLeft, bool bold = false, int lines = 1)
    {
        g.setColour (c);
        g.setFont (PyraLookAndFeel::font (size, bold));
        g.drawFittedText (t, r, j, lines, 0.8f);
    }

    juce::String noteName (int n, bool flats)
    {
        return pitchClassName (n, flats) + juce::String (n / 12 - 1);
    }

    /** The 8 chord cards + a small keyboard showing the notes. */
    class ChordCards : public juce::Component
    {
    public:
        explicit ChordCards (PyraAudioProcessor& p) : proc (p), pattern (p.chordPattern)
        {
            countParam = p.apvts.getParameter ("chord_count");
            setRepaintsOnMouseActivity (true);
        }

        juce::Rectangle<int> cardBounds (int k) const
        {
            const int gap = 8;
            const int w = (getWidth() - gap * (kMaxChordSlots - 1)) / kMaxChordSlots;
            return { k * (w + gap), 0, w, cardH };
        }
        juce::Rectangle<int> lockRect (int k) const    { auto c = cardBounds (k); return { c.getRight() - 56, c.getY() + 4, 26, 22 }; }
        juce::Rectangle<int> removeRect (int k) const  { auto c = cardBounds (k); return { c.getRight() - 28, c.getY() + 4, 24, 22 }; }

        /** Takes chord k out of the progression: the ones after it move one place to the left. */
        void removeChord (int k)
        {
            const int count = juce::jlimit (1, kMaxChordSlots, proc.chords.readSettings().count);
            if (count <= 1 || k >= count) return;
            for (int i = k; i < kMaxChordSlots - 1; ++i)
            {
                auto& a = pattern.slots[(size_t) i];
                const auto& b = pattern.slots[(size_t) i + 1];
                a.degree = b.degree.load(); a.shift = b.shift.load(); a.quality = b.quality.load();
                a.length = b.length.load(); a.lock = b.lock.load();
            }
            auto& last = pattern.slots[(size_t) kMaxChordSlots - 1];
            last.degree = 0; last.shift = 0; last.quality = QAuto; last.length = 2; last.lock = false;
            countParam->beginChangeGesture();
            countParam->setValueNotifyingHost (countParam->convertTo0to1 ((float) (count - 1)));
            countParam->endChangeGesture();
            selected = juce::jlimit (0, count - 2, selected);
            ++pattern.changeCounter;
            repaint();
        }
        juce::Rectangle<int> upRect (int k) const      { auto c = cardBounds (k); return { c.getCentreX() - 22, c.getY() + 32, 44, 22 }; }
        juce::Rectangle<int> downRect (int k) const    { auto c = cardBounds (k); return { c.getCentreX() - 22, c.getY() + 140, 44, 22 }; }
        juce::Rectangle<int> qualityRect (int k) const { auto c = cardBounds (k); return { c.getX() + 8, c.getY() + 196, c.getWidth() - 16, 26 }; }
        juce::Rectangle<int> lengthRect (int k) const  { auto c = cardBounds (k); return { c.getX() + 8, c.getY() + 228, c.getWidth() - 16, 26 }; }
        juce::Rectangle<int> playRect (int k) const    { auto c = cardBounds (k); return { c.getX() + 8, c.getY() + 260, c.getWidth() - 16, 28 }; }
        juce::Rectangle<int> keysArea() const          { return { 0, cardH + 14, getWidth(), getHeight() - cardH - 14 }; }

        std::vector<std::vector<int>> allNotes (const ChordSettings& s) const
        {
            std::vector<std::vector<int>> out ((size_t) kMaxChordSlots);
            const int count = juce::jlimit (1, kMaxChordSlots, s.count);
            std::vector<int> prev;
            if (s.inversion == 0 && count > 1) prev = chordNotes (pattern, count - 1, s, {});
            for (int k = 0; k < kMaxChordSlots; ++k)
            {
                out[(size_t) k] = chordNotes (pattern, k, s, k < count ? prev : std::vector<int> {});
                if (k < count) prev = out[(size_t) k];
            }
            return out;
        }

        void paint (juce::Graphics& g) override
        {
            const auto s = proc.chords.readSettings();
            const int count = juce::jlimit (1, kMaxChordSlots, s.count);
            const int playing = proc.chords.currentSlot.load();
            const auto notes = allNotes (s);
            const auto mouse = getMouseXYRelative();
            const bool over = isMouseOver();

            for (int k = 0; k < kMaxChordSlots; ++k)
            {
                const auto& slot = pattern.slots[(size_t) k];
                auto cb = cardBounds (k);
                const bool used = k < count;
                const bool isPlaying = used && k == playing;
                const float alpha = used ? 1.0f : 0.3f;

                g.setColour (isPlaying ? colours::accent.withAlpha (0.22f) : colours::panelHi);
                g.fillRoundedRectangle (cb.toFloat(), 8.0f);
                g.setColour (isPlaying ? colours::accent : (k == selected && used ? colours::accent2.withAlpha (0.7f) : colours::edge));
                g.drawRoundedRectangle (cb.toFloat().reduced (0.5f), 8.0f, isPlaying ? 2.0f : 1.0f);

                text (g, juce::String (k + 1), cb.withHeight (30).reduced (10, 0), 12.0f, colours::textDim.withMultipliedAlpha (alpha),
                      juce::Justification::centredLeft, true);
                // lock
                auto lk = lockRect (k).toFloat().reduced (5.0f, 3.0f);
                g.setColour ((slot.lock.load() ? colours::accent2 : colours::edge).withMultipliedAlpha (alpha));
                g.fillRoundedRectangle (lk.withTrimmedTop (lk.getHeight() * 0.45f), 2.0f);
                g.drawEllipse (lk.withTrimmedBottom (lk.getHeight() * 0.4f).reduced (lk.getWidth() * 0.22f, 1.0f), 1.5f);

                // remove (only when there is more than one chord)
                if (used && count > 1)
                {
                    auto xr = removeRect (k).toFloat().reduced (6.0f, 5.0f);
                    const bool hot = over && removeRect (k).contains (mouse);
                    if (hot)
                    {
                        g.setColour (colours::accent.withAlpha (0.25f));
                        g.fillRoundedRectangle (removeRect (k).toFloat().reduced (1.0f), 4.0f);
                    }
                    g.setColour (hot ? colours::accent2 : colours::textDim);
                    g.drawLine (xr.getX(), xr.getY(), xr.getRight(), xr.getBottom(), 2.0f);
                    g.drawLine (xr.getRight(), xr.getY(), xr.getX(), xr.getBottom(), 2.0f);
                }

                if (! used)
                {
                    text (g, "+", cb.withTrimmedTop (60).withHeight (80), 34.0f, colours::textDim.withAlpha (0.6f), juce::Justification::centred);
                    text (g, tr ("click to add"), cb.withTrimmedTop (130).withHeight (24), 11.5f, colours::textDim.withAlpha (0.6f),
                          juce::Justification::centred);
                    continue;
                }

                auto arrow = [&] (juce::Rectangle<int> r, bool up)
                {
                    const bool hot = over && r.contains (mouse);
                    g.setColour (hot ? colours::accent2 : colours::textDim);
                    juce::Path p;
                    const auto c = r.toFloat().getCentre();
                    if (up) p.addTriangle (c.x - 9, c.y + 5, c.x + 9, c.y + 5, c.x, c.y - 6);
                    else p.addTriangle (c.x - 9, c.y - 5, c.x + 9, c.y - 5, c.x, c.y + 6);
                    g.fillPath (p);
                };
                arrow (upRect (k), true);
                arrow (downRect (k), false);

                text (g, chordRoman (pattern, k, s), { cb.getX() + 4, cb.getY() + 58, cb.getWidth() - 8, 44 }, 30.0f,
                      isPlaying ? colours::accent2 : colours::text, juce::Justification::centred, true);
                text (g, chordName (pattern, k, s), { cb.getX() + 4, cb.getY() + 104, cb.getWidth() - 8, 28 }, 19.0f,
                      colours::accent, juce::Justification::centred, true);

                juce::StringArray ns;
                const bool flats = chordUseFlats (s.scale, s.key) || slot.shift.load() < 0;
                for (int n : notes[(size_t) k]) ns.add (noteName (n, flats));
                text (g, ns.joinIntoString (" "), { cb.getX() + 6, cb.getY() + 166, cb.getWidth() - 12, 24 }, 11.0f, colours::textDim,
                      juce::Justification::centred, false, 2);

                auto button = [&] (juce::Rectangle<int> r, const juce::String& label, bool accent)
                {
                    const bool hot = over && r.contains (mouse);
                    g.setColour (accent ? colours::accent.darker (hot ? 0.1f : 0.4f) : (hot ? colours::panel.brighter (0.15f) : colours::knobBody));
                    g.fillRoundedRectangle (r.toFloat(), 5.0f);
                    g.setColour (colours::edge);
                    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 5.0f, 1.0f);
                    text (g, label, r.reduced (6, 0), 12.0f, colours::text, juce::Justification::centred, accent);
                };
                const int q = slot.quality.load();
                button (qualityRect (k), q == QAuto ? tr ("Auto (scale)") : tr (chordQualityNames()[q]), false);
                button (lengthRect (k), tr ("Length") + " " + chordSlotLengthNames()[juce::jlimit (0, 3, slot.length.load())], false);
                button (playRect (k), juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6  ")) + tr ("PLAY"), true);

                // length bar
                const float rel = (float) chordSlotLengths()[(size_t) juce::jlimit (0, 3, slot.length.load())] / 2.0f;
                auto bar = juce::Rectangle<float> ((float) cb.getX() + 8.0f, (float) cb.getBottom() - 12.0f, (float) (cb.getWidth() - 16), 4.0f);
                g.setColour (colours::knobBody);
                g.fillRoundedRectangle (bar, 2.0f);
                g.setColour (colours::accent2.withAlpha (0.8f));
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.05f, 1.0f, rel)), 2.0f);
            }

            // ---- keyboard with the notes of the chord that plays (or the selected one)
            const int show = playing >= 0 ? playing : juce::jlimit (0, count - 1, selected);
            const auto& shown = notes[(size_t) show];
            auto ka = keysArea().toFloat();
            const int lo = 24, hi = 96;   // C1..C7
            int whites = 0;
            for (int n = lo; n <= hi; ++n) if (! juce::MidiMessage::isMidiNoteBlack (n)) ++whites;
            const float ww = ka.getWidth() / (float) whites;
            float x = ka.getX();
            std::map<int, juce::Rectangle<float>> blackKeys;
            for (int n = lo; n <= hi; ++n)
            {
                if (juce::MidiMessage::isMidiNoteBlack (n)) { blackKeys[n] = { x - ww * 0.32f, ka.getY(), ww * 0.64f, ka.getHeight() * 0.6f }; continue; }
                auto r = juce::Rectangle<float> (x, ka.getY(), ww - 1.0f, ka.getHeight());
                const bool lit = std::find (shown.begin(), shown.end(), n) != shown.end();
                g.setColour (lit ? colours::accent : juce::Colour (0xffd2c7c0));
                g.fillRoundedRectangle (r, 2.0f);
                if (n % 12 == 0)
                    text (g, "C" + juce::String (n / 12 - 1), r.toNearestInt().removeFromBottom (16), 9.5f, juce::Colours::black.withAlpha (0.5f),
                          juce::Justification::centred);
                x += ww;
            }
            for (auto& [n, r] : blackKeys)
            {
                const bool lit = std::find (shown.begin(), shown.end(), n) != shown.end();
                g.setColour (lit ? colours::accent.brighter (0.2f) : juce::Colour (0xff110c0b));
                g.fillRoundedRectangle (r, 2.0f);
            }
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            const auto s = proc.chords.readSettings();
            const int count = juce::jlimit (1, kMaxChordSlots, s.count);
            for (int k = 0; k < kMaxChordSlots; ++k)
            {
                if (! cardBounds (k).contains (e.getPosition())) continue;
                if (auto& hook = userActivityHook()) hook();
                auto& slot = pattern.slots[(size_t) k];
                if (k >= count)
                {   // add chords up to this one
                    countParam->beginChangeGesture();
                    countParam->setValueNotifyingHost (countParam->convertTo0to1 ((float) (k + 1)));
                    countParam->endChangeGesture();
                    repaint();
                    return;
                }
                selected = k;
                if (removeRect (k).contains (e.getPosition())) { removeChord (k); return; }
                if (lockRect (k).contains (e.getPosition())) slot.lock = ! slot.lock.load();
                else if (upRect (k).contains (e.getPosition())) { slot.degree = juce::jmin (21, slot.degree.load() + 1); slot.shift = 0; proc.chords.preview (k); }
                else if (downRect (k).contains (e.getPosition())) { slot.degree = juce::jmax (-7, slot.degree.load() - 1); slot.shift = 0; proc.chords.preview (k); }
                else if (playRect (k).contains (e.getPosition())) proc.chords.preview (k);
                else if (qualityRect (k).contains (e.getPosition())) { showQualityMenu (k); return; }
                else if (lengthRect (k).contains (e.getPosition())) { showLengthMenu (k); return; }
                else if (e.mods.isPopupMenu()) { showQualityMenu (k); return; }
                else proc.chords.preview (k);
                ++pattern.changeCounter;
                repaint();
                return;
            }
        }

        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int count = juce::jlimit (1, kMaxChordSlots, proc.chords.readSettings().count);
            for (int k = 0; k < count; ++k)
                if (cardBounds (k).contains (e.getPosition()))
                {
                    auto& slot = pattern.slots[(size_t) k];
                    const int d = w.deltaY > 0 ? 1 : -1;
                    slot.degree = juce::jlimit (-7, 21, slot.degree.load() + d);
                    slot.shift = 0;
                    ++pattern.changeCounter;
                    repaint();
                    return;
                }
        }

        void showQualityMenu (int k)
        {
            juce::PopupMenu m;
            const int q = pattern.slots[(size_t) k].quality.load();
            for (int i = 0; i < QCount; ++i) m.addItem (i + 1, tr (chordQualityNames()[i]), true, i == q);
            m.addSeparator();
            const int sh = pattern.slots[(size_t) k].shift.load();
            m.addItem (100, tr ("Root a semitone lower (b)"), true, sh < 0);
            m.addItem (101, tr ("Root a semitone higher (#)"), true, sh > 0);
            juce::Component::SafePointer<ChordCards> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (qualityRect (k))), [safe, k] (int r)
            {
                if (safe == nullptr || r == 0) return;
                auto& slot = safe->pattern.slots[(size_t) k];
                if (r == 100) slot.shift = slot.shift.load() < 0 ? 0 : -1;
                else if (r == 101) slot.shift = slot.shift.load() > 0 ? 0 : 1;
                else slot.quality = r - 1;
                ++safe->pattern.changeCounter;
                safe->proc.chords.preview (k);
                safe->repaint();
            });
        }

        void showLengthMenu (int k)
        {
            juce::PopupMenu m;
            const int len = pattern.slots[(size_t) k].length.load();
            for (int i = 0; i < chordSlotLengthNames().size(); ++i)
                m.addItem (i + 1, chordSlotLengthNames()[i] + "  (" + tr ("of the chord length") + ")", true, i == len);
            juce::Component::SafePointer<ChordCards> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (lengthRect (k))), [safe, k] (int r)
            {
                if (safe == nullptr || r == 0) return;
                safe->pattern.slots[(size_t) k].length = r - 1;
                ++safe->pattern.changeCounter;
                safe->repaint();
            });
        }

        void tick()
        {
            juce::int64 sig = pattern.changeCounter.load() * 31 + proc.chords.currentSlot.load() * 7;
            for (auto* id : { "chord_key", "chord_scale", "chord_count", "chord_size", "chord_voicing", "chord_inv", "chord_oct", "chord_bass",
                              "chord_len", "chord_complex" })
                sig = sig * 13 + (juce::int64) std::lround (proc.apvts.getRawParameterValue (id)->load() * 100.0f);
            if (sig != lastSig) { lastSig = sig; repaint(); }
        }

        static constexpr int cardH = 316;
        PyraAudioProcessor& proc;
        ChordPattern& pattern;
        juce::RangedAudioParameter* countParam = nullptr;
        int selected = 0;
        juce::int64 lastSig = -1;
    };

    /** Drag onto a DAW track to get the progression as MIDI; click to save a .mid file. */
    class ChordMidiButton : public juce::TextButton
    {
    public:
        explicit ChordMidiButton (PyraAudioProcessor& p) : juce::TextButton (tr ("DRAG MIDI")), proc (p)
        {
            setTooltip (tr ("Drag onto a track in your DAW to get the chords as MIDI. Click to save a .mid file."));
            onClick = [this] { saveWithChooser(); };
        }

        juce::File writeFile()
        {
            auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PYRA Chords.mid");
            file.deleteFile();
            auto midi = chordsToMidiFile (proc.chords.readSettings(), proc.chordPattern, 2);
            juce::FileOutputStream os (file);
            if (os.openedOk()) midi.writeTo (os);
            return file;
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! dragging && e.getDistanceFromDragStart() > 6)
            {
                dragging = true;
                juce::DragAndDropContainer::performExternalDragDropOfFiles ({ writeFile().getFullPathName() }, false, this);
            }
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (dragging) { dragging = false; return; }
            juce::TextButton::mouseUp (e);
        }

        void saveWithChooser()
        {
            chooser = std::make_unique<juce::FileChooser> (tr ("Save chords as MIDI"),
                juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("PYRA Chords.mid"), "*.mid");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
            {
                auto target = fc.getResult();
                if (target != juce::File()) writeFile().copyFileTo (target.withFileExtension ("mid"));
            });
        }

        PyraAudioProcessor& proc;
        bool dragging = false;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    class ChordsPage : public Page
    {
    public:
        explicit ChordsPage (UiContext& u) : ui (u), top (tr ("CHORDS")), progression (tr ("PROGRESSION")), cards (u.proc), midiDrag (u.proc)
        {
            addAndMakeVisible (top);
            addAndMakeVisible (progression);
            progression.tint = colours::accent2;
            progression.addAndMakeVisible (cards);

            auto combo = [this] (const char* id, bool translate = true)
            {
                auto* c = new ParamCombo (ui.st, id, translate);
                owned.add (c); top.addAndMakeVisible (c);
                return c;
            };
            auto knob = [this] (const char* id, const char* label)
            {
                auto* k = new Knob (ui.st, id, label, nullptr);
                owned.add (k); top.addAndMakeVisible (k); ui.knobs.add (k);
                return k;
            };
            on = std::make_unique<ParamToggle> (ui.st, "chord_on", tr ("CHORDS ON"));
            bass = std::make_unique<ParamToggle> (ui.st, "chord_bass", tr ("Bass note"));
            top.addAndMakeVisible (on.get());
            top.setPowerButton (on.get());
            top.addAndMakeVisible (bass.get());
            mode = combo ("chord_mode"); trigger = combo ("chord_trigger"); key = combo ("chord_key", false); scale = combo ("chord_scale");
            length = combo ("chord_len", false); rhythm = combo ("chord_rhythm"); size = combo ("chord_size"); voicing = combo ("chord_voicing");
            inversion = combo ("chord_inv");
            mood = combo ("chord_mood");
            mood->setTooltip (tr ("Emotion for RANDOM: the progression, the scale, the rhythm and the voicing follow the mood you pick. Any = no preference."));
            count = knob ("chord_count", "Chords"); octave = knob ("chord_oct", "Octave"); gate = knob ("chord_gate", "Gate");
            strum = knob ("chord_strum", "Strum"); human = knob ("chord_human", "Humanize"); vel = knob ("chord_vel", "Velocity");
            swing = knob ("chord_swing", "Swing"); complexity = knob ("chord_complex", "Complexity");

            mode->setTooltip (tr ("Progression: hold a key and the chords play in time. Chord per Key: every key you press plays the chord of the scale built on it."));
            trigger->setTooltip (tr ("Hold Key: plays while you hold a key. Latch: keeps playing after you let go (same key again stops). DAW Play: plays whenever your DAW plays."));
            length->setTooltip (tr ("How long each chord lasts (each card can be 1/4x, 1/2x, 1x or 2x of this)"));
            rhythm->setTooltip (tr ("How the chords are played: held, pulsing, offbeats, syncopated..."));
            size->setTooltip (tr ("Triads, sevenths, ninths, sus, power chords, or a mix"));
            voicing->setTooltip (tr ("How the notes are spread: close together or wide apart"));
            inversion->setTooltip (tr ("Smooth: each chord picks the inversion closest to the one before (smooth voice leading)"));
            complexity->setTooltip (tr ("For RANDOM: low = classic pop progressions, high = sevenths, ninths and borrowed chords"));
            count->setTooltip (tr ("How many chords in the progression"));
            strum->setTooltip (tr ("Plays the notes of each chord one after another, like a guitar strum"));

            for (auto* b : { &randomBtn, &randomAllBtn, &unlockBtn }) top.addAndMakeVisible (*b);
            top.addAndMakeVisible (midiDrag);
            randomBtn.setButtonText (tr ("RANDOM"));
            randomAllBtn.setButtonText (tr ("RANDOM ALL"));
            unlockBtn.setButtonText (tr ("UNLOCK ALL"));
            randomBtn.setColour (juce::TextButton::buttonColourId, colours::accent.darker (0.3f));
            randomAllBtn.setColour (juce::TextButton::buttonColourId, colours::hot.darker (0.4f));
            randomBtn.setTooltip (tr ("New progression in the chosen key and scale (and emotion). Locked chords stay."));
            randomAllBtn.setTooltip (tr ("Also picks a new key, scale, rhythm, voicing and length"));
            auto* pr = &ui.proc;
            randomBtn.onClick = [pr]
            {
                if (auto& hook = userActivityHook()) hook();
                randomizer::randomize (*pr, randomizer::Chords, false, (uint32_t) juce::Random::getSystemRandom().nextInt());
            };
            randomAllBtn.onClick = [pr]
            {
                if (auto& hook = userActivityHook()) hook();
                randomizer::randomizeChordsAll (*pr, (uint32_t) juce::Random::getSystemRandom().nextInt());
            };
            unlockBtn.onClick = [pr] { for (auto& s : pr->chordPattern.slots) s.lock = false; ++pr->chordPattern.changeCounter; };
        }

        void paintOverChildren (juce::Graphics& g) override
        {
            auto t = top.getBounds();
            auto lab = [&] (const juce::String& s, juce::Component* c)
            {
                text (g, s.toUpperCase(), c->getBounds().translated (t.getX(), t.getY() - 15).withHeight (14), 10.5f, colours::textDim,
                      juce::Justification::centredLeft, true);
            };
            lab (tr ("Mode"), mode); lab (tr ("Play"), trigger); lab (tr ("Key"), key); lab (tr ("Scale"), scale);
            lab (tr ("Chord length"), length); lab (tr ("Rhythm"), rhythm); lab (tr ("Chord type"), size); lab (tr ("Voicing"), voicing);
            lab (tr ("Inversion"), inversion);
            lab (tr ("Emotion (for RANDOM)"), mood);

            const auto s = ui.proc.chords.readSettings();
            juce::String hint = s.mode == 1
                ? tr ("Chord per Key: every key you play becomes the chord of the scale that starts on it (white keys in C major / A minor).")
                : tr ("Hold any key to play the progression in time (C4-B4 = as shown, other octaves move it up or down). Turn ARP on to arpeggiate the chords.");
            text (g, hint, progression.getBounds().removeFromBottom (28).reduced (14, 4), 12.0f, colours::textDim,
                  juce::Justification::centredLeft, false, 1);
            if (! (ui.st.getRawParameterValue ("chord_on")->load() > 0.5f))
                text (g, tr ("CHORDS is off: switch it on (top left) to play the chords with your keys. PLAY on a card always works."),
                      progression.getBounds().removeFromTop (28).withTrimmedLeft (160).reduced (8, 4), 12.0f, colours::accent2,
                      juce::Justification::centredRight, false, 1);
        }

        void resized() override
        {
            const int m = 8;
            top.setBounds (m, m, getWidth() - 2 * m, 186);
            progression.setBounds (m, m + 186 + m, getWidth() - 2 * m, getHeight() - 186 - 3 * m);
            cards.setBounds (progression.getLocalBounds().withTrimmedTop (34).withTrimmedBottom (34).reduced (12, 0));

            on->setBounds (14, 36, 130, 26);
            int x = 156;
            auto place = [&] (juce::Component* c, int w) { c->setBounds (x, 40, w, 24); x += w + 10; };
            place (mode, 140); place (trigger, 112); place (key, 66); place (scale, 150); place (length, 88);
            place (rhythm, 118); place (size, 100); place (voicing, 92); place (inversion, 98);

            const int ky = 78, kw = 78, kh = 96;
            int kx = 10;
            for (auto* k : { count, octave, gate, strum, human, vel, swing, complexity }) { k->setBounds (kx, ky, kw, kh); kx += kw; }
            bass->setBounds (kx + 8, ky + 34, 120, 26);

            const int bx = top.getWidth() - 12 - 452;
            mood->setBounds (bx, 90, 168, 30);
            randomBtn.setBounds (bx + 176, 86, 118, 36);
            randomAllBtn.setBounds (bx + 300, 86, 152, 36);
            midiDrag.setBounds (bx, 130, 310, 34);
            unlockBtn.setBounds (bx + 318, 130, 134, 34);
        }

        void tick() override
        {
            cards.tick();
            const bool onNow = ui.st.getRawParameterValue ("chord_on")->load() > 0.5f;
            const int modeNow = (int) std::lround (ui.st.getRawParameterValue ("chord_mode")->load());
            if (onNow != lastOn || modeNow != lastMode) { lastOn = onNow; lastMode = modeNow; repaint(); }
            trigger->setAlpha (modeNow == 0 ? 1.0f : 0.35f);
            rhythm->setAlpha (modeNow == 0 ? 1.0f : 0.35f);
            length->setAlpha (modeNow == 0 ? 1.0f : 0.35f);
        }

        UiContext& ui;
        Panel top, progression;
        ChordCards cards;
        ChordMidiButton midiDrag;
        juce::OwnedArray<juce::Component> owned;
        std::unique_ptr<ParamToggle> on, bass;
        ParamCombo* mode, *trigger, *key, *scale, *length, *rhythm, *size, *voicing, *inversion, *mood;
        Knob* count, *octave, *gate, *strum, *human, *vel, *swing, *complexity;
        juce::TextButton randomBtn, randomAllBtn, unlockBtn;
        bool lastOn = false; int lastMode = -1;
    };
}

std::unique_ptr<Page> createChordsPage (UiContext& ui) { return std::make_unique<ChordsPage> (ui); }

} // namespace pyra
