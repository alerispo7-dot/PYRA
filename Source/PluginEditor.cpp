#include "PluginEditor.h"
#include "Lang.h"
#include "Randomizer.h"
#include "ui/HellArt.h"
#include "ui/Prefs.h"

using pyra::lang::tr;
namespace colours = pyra::colours;

namespace
{
    enum PageIndex { PageSynth, PagePresets, PageLayers, PageChords, PageArp, PageFx, PageChaos, PageAnalog, PageMatrix, PageSettings, PageCount };
}

PyraAudioProcessorEditor::PyraAudioProcessorEditor (PyraAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (root);

    keyboard.setScrollButtonsVisible (false);
    keyboard.setWantsKeyboardFocus (false);

    currentPage = pyra::prefs::get (pyra::prefs::StartPage) == 1 ? 0 : juce::jlimit (0, 9, pyra::lang::settings().getIntValue ("lastPage", 0));
    prefsSignature = pyra::prefs::get (pyra::prefs::KnobDrag) * 100 + pyra::prefs::get (pyra::prefs::KnobSpeed) * 10
                     + pyra::prefs::get (pyra::prefs::WheelKnobs) + pyra::prefs::get (pyra::prefs::ShowValues) * 1000;
    buildUi();

    setResizable (true, true);
    const int minW = percentToWidth (minPercent), maxW = percentToWidth (maxPercent);
    setResizeLimits (minW, minW * baseH / baseW, maxW, maxW * baseH / baseW);
    getConstrainer()->setFixedAspectRatio ((double) baseW / (double) baseH);

    // the size this PYRA had last time (saved in the project); a new PYRA opens at the last size you chose
    int w = proc.editorWidth.load();
    if (w <= 0) w = pyra::lang::settings().getIntValue ("windowWidth2", defaultW);
    w = juce::jlimit (minW, maxW, w);
    setSize (w, juce::roundToInt ((float) w * (float) baseH / (float) baseW));
    pendingWidth = 0;

    root.addMouseListener (this, true);
    pyra::prefs::broadcaster().addChangeListener (this);
    applyPrefs();
    startTimerHz (30);
}

PyraAudioProcessorEditor::~PyraAudioProcessorEditor()
{
    stopTimer();
    pyra::prefs::broadcaster().removeChangeListener (this);
    tooltips.reset();
    pyra::userActivityHook() = nullptr;
    root.removeMouseListener (this);
    pages.clear();
    setLookAndFeel (nullptr);
}

void PyraAudioProcessorEditor::buildUi()
{
    root.removeAllChildren();
    pages.clear();
    tabs.clear();
    masterKnob.reset();

    auto& tgt = target();
    matrix = std::make_unique<pyra::ModMatrixAccess> (tgt.apvts);
    ui.reset (new pyra::UiContext { tgt, tgt.apvts, *matrix, {} });
    // CHORDS and ARP belong to the whole sound: they always show the main PYRA, whatever layer is being edited
    masterMatrix = std::make_unique<pyra::ModMatrixAccess> (proc.apvts);
    masterUi.reset (new pyra::UiContext { proc, proc.apvts, *masterMatrix, {} });
    juce::Component::SafePointer<PyraAudioProcessorEditor> safeEditor (this);
    pyra::userActivityHook() = [safeEditor] { if (safeEditor != nullptr) safeEditor->noteActivity(); };
    linkSnapshot.clear();

    const juce::StringArray names { "SYNTH", tr ("PRESETS"), tr ("LAYERS"), tr ("CHORDS"), "ARP", "FX", "CHAOS", "ANALOG", tr ("MATRIX"), tr ("SETTINGS") };
    tabNames = names;
    for (int i = 0; i < names.size(); ++i)
    {
        auto* t = tabs.add (new juce::TextButton (names[i]));
        t->setClickingTogglesState (false);
        t->onClick = [this, i] { showPage (i); };
        root.addAndMakeVisible (t);
    }

    pages.push_back (pyra::createSynthPage (*ui));
    pages.push_back (pyra::createPresetPage (*ui, presetFilter, [this] { openSaveDialog(); }));
    pages.push_back (pyra::createLayersPage (proc, [this] (int k) { editLayerSound (k); }, [this] { return editLayer; }));
    pages.push_back (pyra::createChordsPage (*masterUi));
    pages.push_back (pyra::createArpPage (*masterUi));
    pages.push_back (pyra::createFxPage (*ui));
    pages.push_back (pyra::createChaosPage (*ui));
    pages.push_back (pyra::createAnalogPage (*ui));
    pages.push_back (pyra::createMatrixPage (*ui));
    pages.push_back (pyra::createGlobalPage (*ui, [this] { buildUi(); resized(); },
                                             [this] (int pct) { setWindowPercent (pct); },
                                             [this] { return windowPercent(); }));

    for (auto& pg : pages) root.addChildComponent (*pg);

    masterKnob = std::make_unique<pyra::Knob> (proc.apvts, "master", "Volume", nullptr);
    root.addAndMakeVisible (*masterKnob);

    presetBar = std::make_unique<pyra::PresetBar> (tgt, presetFilter);
    presetBar->onOpenBrowser = [this] { showPage (PagePresets); };
    presetBar->onSave = [this] { openSaveDialog(); };
    presetBar->onRandom = [this] (bool variation) { randomizeCurrentPage (variation); };
    root.addAndMakeVisible (*presetBar);

    langButton.setButtonText (pyra::lang::code (pyra::lang::current()));
    langButton.setTooltip (tr ("Language"));
    juce::Component::SafePointer<PyraAudioProcessorEditor> safeThis (this);
    langButton.onClick = [safeThis]
    {
        if (safeThis == nullptr) return;
        juce::PopupMenu menu;
        for (int l = 0; l < pyra::lang::NumLanguages; ++l)
            menu.addItem (l + 1, pyra::lang::nativeName ((pyra::lang::Language) l), true, l == (int) pyra::lang::current());
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&safeThis->langButton), [safeThis] (int r)
        {
            if (safeThis == nullptr || r <= 0 || r - 1 == (int) pyra::lang::current()) return;
            pyra::lang::set ((pyra::lang::Language) (r - 1));
            juce::MessageManager::callAsync ([safe = safeThis] { if (safe != nullptr) { safe->buildUi(); safe->resized(); } });
        });
    };
    root.addAndMakeVisible (langButton);
    root.addAndMakeVisible (keyboard);

    showPage (juce::jlimit (0, (int) pages.size() - 1, currentPage));
    root.resized();
    for (auto* k : ui->knobs) k->refreshModulation();
}

void PyraAudioProcessorEditor::openSaveDialog()
{
    juce::Component::SafePointer<PyraAudioProcessorEditor> safeThis (this);
    saveDialog = std::make_unique<pyra::SaveDialog> (target(), [safeThis]
    {
        juce::MessageManager::callAsync ([safe = safeThis]
        {
            if (safe != nullptr) safe->saveDialog.reset();
        });
    });
    root.addAndMakeVisible (*saveDialog);
    saveDialog->setBounds (root.getLocalBounds());
    saveDialog->toFront (true);
}

PyraAudioProcessor& PyraAudioProcessorEditor::target()
{
    return editLayer == 0 ? proc : proc.getOrCreateLayer (editLayer);
}

void PyraAudioProcessorEditor::editLayerSound (int layer)
{
    layer = juce::jlimit (0, pyra::kNumLayers - 1, layer);
    const bool same = layer == editLayer;
    editLayer = layer;
    juce::Component::SafePointer<PyraAudioProcessorEditor> safeThis (this);
    juce::MessageManager::callAsync ([safeThis, same]
    {
        if (safeThis == nullptr) return;
        if (! same) { safeThis->buildUi(); safeThis->resized(); }
        safeThis->showPage (PageSynth);
        safeThis->root.repaint();
    });
}

void PyraAudioProcessorEditor::randomizeCurrentPage (bool variation)
{
    auto& proc = target();
    proc.history.noteUserActivity();
    const auto seed = (uint32_t) juce::Random::getSystemRandom().nextInt();
    using namespace pyra::randomizer;
    switch (currentPage)
    {
        case PagePresets:  if (presetBar != nullptr) presetBar->loadRandomPreset(); break;
        case PageLayers:   randomizeLayers (this->proc, variation, seed); break;
        case PageFx:       randomize (proc, Fx, variation, seed); break;
        case PageArp:      randomize (this->proc, Arp, variation, seed); break;
        case PageChords:   randomize (this->proc, Chords, variation, seed); break;
        case PageMatrix:   randomize (proc, Matrix, variation, seed); break;
        case PageChaos:    randomize (proc, Chaos, variation, seed); break;
        case PageAnalog:   randomize (proc, Analog, variation, seed); break;
        case PageSettings: randomize (proc, Settings, variation, seed); break;
        default:           randomize (proc, Synth, variation, seed); break;
    }
}

void PyraAudioProcessorEditor::applyPrefs()
{
    using namespace pyra::prefs;
    if (get (HelpBubbles) != 0)
    {
        if (tooltips == nullptr) tooltips = std::make_unique<juce::TooltipWindow> (this, helpDelayMs());
        tooltips->setMillisecondsBeforeTipAppears (helpDelayMs());
    }
    else tooltips.reset();
}

void PyraAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    using namespace pyra::prefs;
    applyPrefs();
    // knob behaviour is set when the knobs are built: rebuild the window if it changed
    const int sig = get (KnobDrag) * 100 + get (KnobSpeed) * 10 + get (WheelKnobs) + get (ShowValues) * 1000;
    if (prefsSignature >= 0 && sig != prefsSignature)
    {
        juce::Component::SafePointer<PyraAudioProcessorEditor> safe (this);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->buildUi(); safe->resized(); } });
    }
    prefsSignature = sig;
    root.repaint();
}

void PyraAudioProcessorEditor::noteActivity()
{
    target().history.noteUserActivity();
    if (editLayer != 0) proc.history.noteUserActivity();
    lastActivityMs = juce::Time::getMillisecondCounterHiRes();
}

void PyraAudioProcessorEditor::mouseDown (const juce::MouseEvent&)       { noteActivity(); }
void PyraAudioProcessorEditor::mouseUp (const juce::MouseEvent&)         { noteActivity(); }
void PyraAudioProcessorEditor::mouseDrag (const juce::MouseEvent&)       { noteActivity(); }
void PyraAudioProcessorEditor::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) { noteActivity(); }

void PyraAudioProcessorEditor::linkLayers()
{
    // LINK: a knob moved by hand on the sound being edited moves the same knob on every other layer that is on,
    // by the same amount (each layer keeps its own character). Presets, RANDOM and undo (many changes at once) are not copied.
    auto& tgt = target();
    const auto& params = tgt.getParameters();
    if ((int) linkSnapshot.size() != params.size())
    {
        linkSnapshot.clear();
        for (auto* p : params) linkSnapshot.push_back (p->getValue());
        return;
    }
    std::vector<std::pair<int, float>> moved;
    for (int i = 0; i < params.size(); ++i)
    {
        const float v = params[i]->getValue();
        const float d = v - linkSnapshot[(size_t) i];
        if (std::abs (d) > 1.0e-6f) { moved.push_back ({ i, d }); linkSnapshot[(size_t) i] = v; }
    }
    if (moved.empty() || moved.size() > 4) return;
    if (proc.apvts.getRawParameterValue ("layer_link")->load() < 0.5f) return;
    if (juce::Time::getMillisecondCounterHiRes() - lastActivityMs > 600.0) return;   // automation from the DAW: not linked

    for (int k = 0; k < pyra::kNumLayers; ++k)
    {
        if (k == editLayer || proc.apvts.getRawParameterValue (pyra::layerParam (k, "on"))->load() < 0.5f) continue;
        auto* other = proc.getLayer (k);
        if (other == nullptr) continue;
        const auto& op = other->getParameters();
        for (auto [i, d] : moved)
        {
            auto* src = dynamic_cast<juce::RangedAudioParameter*> (params[i]);
            if (src == nullptr || src->getParameterID().startsWith ("layer")) continue;
            if (dynamic_cast<juce::AudioParameterChoice*> (src) != nullptr || dynamic_cast<juce::AudioParameterBool*> (src) != nullptr) continue;
            if (i >= op.size()) continue;
            auto* dst = op[i];
            const float nv = juce::jlimit (0.0f, 1.0f, dst->getValue() + d);
            if (std::abs (nv - dst->getValue()) > 1.0e-7f) dst->setValueNotifyingHost (nv);
        }
    }
}

void PyraAudioProcessorEditor::showPage (int index)
{
    currentPage = index;
    pyra::lang::settings().setValue ("lastPage", index);
    if (presetBar != nullptr && juce::isPositiveAndBelow (index, tabNames.size()))
        presetBar->setRandomLabel (index == PagePresets ? tr ("PRESET") : tabNames[index]);
    for (int i = 0; i < (int) pages.size(); ++i)
        pages[(size_t) i]->setVisible (i == index);
    for (int i = 0; i < tabs.size(); ++i)
        tabs[i]->setToggleState (i == index, juce::dontSendNotification);
}

void PyraAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
}


void PyraAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseW;
    root.setTransform (juce::AffineTransform::scale (scale));
    root.setBounds (0, 0, baseW, baseH);
    // hosts sometimes shrink or move the window while closing it: only a size that stays put is remembered
    pendingWidth = getWidth();
    pendingSince = juce::Time::getMillisecondCounterHiRes();
}

void PyraAudioProcessorEditor::rememberWidth (int w)
{
    pendingWidth = 0;
    if (w <= 0) return;
    proc.editorWidth = w;
    pyra::lang::settings().setValue ("windowWidth2", w);
}

void PyraAudioProcessorEditor::setWindowPercent (int pct)
{
    pct = juce::jlimit (minPercent, maxPercent, pct);
    int w = percentToWidth (pct);
    // never bigger than the screen
    if (auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
    {
        const auto area = d->userArea;
        w = std::min (w, std::min (area.getWidth() - 20, (area.getHeight() - 60) * baseW / baseH));
    }
    w = std::max (w, percentToWidth (minPercent));
    setSize (w, juce::roundToInt ((float) w * (float) baseH / (float) baseW));
    rememberWidth (getWidth());
}

void PyraAudioProcessorEditor::timerCallback()
{
    if (pendingWidth > 0 && isShowing() && juce::Time::getMillisecondCounterHiRes() - pendingSince > 600.0)
        rememberWidth (pendingWidth);

    if (currentPage >= 0 && currentPage < (int) pages.size())
        pages[(size_t) currentPage]->tick();

    for (auto* k : ui->knobs) k->refreshModulation();
    if (masterUi != nullptr && masterUi.get() != ui.get()) for (auto* k : masterUi->knobs) k->refreshModulation();

    const int v = proc.totalVoices();
    if (v != lastVoices) { lastVoices = v; root.repaint (baseW - 330, 0, 140, topH); }

    // the breathing glow behind the logo and the scope (only those corners of the top bar are redrawn)
    if (pyra::prefs::get (pyra::prefs::Animations) != 0)
    {
        const float peak = proc.uiPeak.exchange (0.0f);
        const float target = juce::jlimit (0.0f, 1.0f, std::sqrt (peak) * 1.2f);
        scopeLevel = target > scopeLevel ? target : scopeLevel * 0.85f;
        animTime += 0.033f;
        root.repaint (898, 5, 100, 36);
        if (++animCounter >= 3) { animCounter = 0; root.repaint (0, 0, 46, 45); }
    }

    linkLayers();

    if (++historyCounter >= 3)
    {
        historyCounter = 0;
        if (editLayer != 0) proc.history.tick();
        target().history.tick();
        if (presetBar != nullptr) presetBar->updateUndoButtons();
    }
}

//==============================================================================
void PyraAudioProcessorEditor::Root::paint (juce::Graphics& g)
{
    // cracked basalt with lava behind everything
    pyra::art::drawBackground (g, getLocalBounds().toFloat());

    // top bar: a band of dark stone, closed by a crack of lava
    auto bar = getLocalBounds().removeFromTop (46).toFloat();
    {
        juce::Path band; band.addRectangle (bar);
        pyra::art::fillStone (g, band, { -40.0f, -300.0f }, 0.62f, 0.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f), 0, 0, juce::Colours::transparentBlack, 0, 46, false));
        g.fillRect (bar);
    }
    if (headerCrack.isEmpty() || headerCrackWidth != getWidth())
    {
        headerCrackWidth = getWidth();
        headerCrack = pyra::art::crackLine ({ 0.0f, 45.0f }, { (float) getWidth(), 45.5f }, 666u, 1.3f, true);
    }
    pyra::art::drawLavaVein (g, headerCrack, 1.2f, 0.85f);

    // the scope: the spiky waveform of the logo, beating with the sound
    {
        auto pit = juce::Rectangle<float> (900.0f, 7.0f, 96.0f, 32.0f);
        juce::Path well; well.addRoundedRectangle (pit, 5.0f);
        g.setColour (juce::Colour (0xe0060404));
        g.fillPath (well);
        pyra::art::drawCarvedFrame (g, pit, 5.0f);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (well);
        pyra::art::drawSpikeWave (g, pit.reduced (4.0f, 3.0f), owner.scopeLevel, owner.animTime);
    }

    // logo + wordmark, centred in the space before the tabs; the wordmark slightly widened to fill it
    {
        const float breath = 0.5f + 0.5f * std::sin (owner.animTime * 1.7f);
        const juce::Rectangle<float> logoR (20.0f, 0.0f, 42.0f, 46.0f), wordR (66.0f, 0.0f, 100.0f, 46.0f);
        const auto lc = logoR.getCentre();
        juce::ColourGradient glow (pyra::colours::accent.withAlpha (0.12f + 0.10f * breath), lc.x, lc.y, juce::Colours::transparentBlack, lc.x + 32.0f, lc.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (lc.x - 32.0f, lc.y - 30.0f, 64.0f, 60.0f);
        // shrink the big pictures in steps (sharp, no sparkle) to the real pixel size of the window
        const float px = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (std::abs (px - cachedArtScale) > 0.01f || cachedLogo.isNull())
        {
            cachedArtScale = px;
            auto shrink = [] (juce::Image img, int w, int h)
            {
                while (img.getWidth() / 2 >= w && img.getHeight() / 2 >= h)
                    img = img.rescaled (img.getWidth() / 2, img.getHeight() / 2, juce::Graphics::mediumResamplingQuality);
                return img.rescaled (std::max (1, w), std::max (1, h), juce::Graphics::highResamplingQuality);
            };
            const auto& L = pyra::art::logo();
            const float lh = logoR.getHeight() * px, lw = lh * (float) L.getWidth() / (float) L.getHeight();
            cachedLogo = shrink (L, juce::roundToInt (lw), juce::roundToInt (lh));
            cachedWord = shrink (pyra::art::wordmark(), juce::roundToInt (wordR.getWidth() * px), juce::roundToInt (wordR.getHeight() * px));
        }
        g.drawImage (cachedLogo, logoR, juce::RectanglePlacement::centred);
        g.drawImage (cachedWord, wordR, juce::RectanglePlacement::stretchToFit);
    }

    // what the window is editing (the main sound or one of the layers)
    if (owner.editLayer > 0)
    {
        g.setColour (pyra::colours::accent2);
        g.setFont (pyra::PyraLookAndFeel::titleFont (11.0f));
        g.drawText (tr ("LAYER") + " " + juce::String::charToString ((juce::juce_wchar) ('A' + owner.editLayer)), getWidth() - 330, 24, 130, 18, juce::Justification::centredRight);
    }

    // voice meter
    const int v = owner.lastVoices;
    g.setColour (v > 0 ? pyra::colours::accent2 : pyra::colours::textDim);
    g.setFont (pyra::PyraLookAndFeel::font (12.0f, true));
    g.drawText (tr ("Voices") + ": " + juce::String (juce::jmax (0, v)), getWidth() - 330, 0, 130, owner.editLayer > 0 ? 28 : 46, juce::Justification::centredRight);
}

void PyraAudioProcessorEditor::layoutTabs()
{
    // as wide as their names, squeezed evenly if they do not fit between the logo and the scope
    const int x0 = 186, x1 = 894, gap = 4;
    std::vector<int> widths;
    int total = 0;
    for (auto* t : tabs)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (lnf.getTextButtonFont (*t, 28), t->getButtonText(), 0.0f, 0.0f);
        const int w = std::max (48, (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth()) + 24);
        widths.push_back (w);
        total += w;
    }
    const int avail = x1 - x0 - gap * (tabs.size() - 1);
    const float squeeze = total > avail ? (float) avail / (float) total : 1.0f;
    int x = x0;
    for (int i = 0; i < tabs.size(); ++i)
    {
        const int w = (int) std::floor ((float) widths[(size_t) i] * squeeze);
        tabs[i]->setBounds (x, 9, w, 28);
        x += w + gap;
    }
}

void PyraAudioProcessorEditor::Root::resized()
{
    auto& e = owner;
    e.layoutTabs();
    e.langButton.setBounds (getWidth() - 56, 10, 44, 26);
    if (e.masterKnob != nullptr) e.masterKnob->setBounds (getWidth() - 186, 0, 120, 46);

    const int kbH = 58;
    if (e.presetBar != nullptr) e.presetBar->setBounds (8, topH + 2, getWidth() - 16, presetH - 6);
    if (e.saveDialog != nullptr) e.saveDialog->setBounds (getLocalBounds());
    auto content = getLocalBounds().withTrimmedTop (topH + presetH).withTrimmedBottom (kbH);
    for (auto& p : e.pages) p->setBounds (content);
    auto kb = getLocalBounds().removeFromBottom (kbH).reduced (8, 4);
    e.keyboard.setAvailableRange (21, 108);                    // 88 keys, like a piano
    e.keyboard.setKeyWidth ((float) kb.getWidth() / 52.0f);   // 52 white keys fill the width
    e.keyboard.setBounds (kb);
}
