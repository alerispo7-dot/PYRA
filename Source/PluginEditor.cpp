#include "PluginEditor.h"
#include "Lang.h"
#include "Randomizer.h"
#include "ui/HellArt.h"

using pyra::lang::tr;
namespace colours = pyra::colours;

namespace
{
    enum PageIndex { PageSynth, PagePresets, PageLayers, PageFx, PageArp, PageChords, PageMatrix, PageChaos, PageAnalog, PageSettings, PageCount };
}

PyraAudioProcessorEditor::PyraAudioProcessorEditor (PyraAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (root);

    keyboard.setScrollButtonsVisible (false);
    keyboard.setWantsKeyboardFocus (false);

    buildUi();

    setResizable (true, true);
    setResizeLimits (baseW * 6 / 10, baseH * 6 / 10, baseW * 2, baseH * 2);
    getConstrainer()->setFixedAspectRatio ((double) baseW / (double) baseH);

    const int savedW = pyra::lang::settings().getIntValue ("windowWidth", 1120);
    const int w = juce::jlimit (baseW * 6 / 10, baseW * 2, savedW);
    setSize (w, juce::roundToInt ((float) w * (float) baseH / (float) baseW));

    root.addMouseListener (this, true);
    startTimerHz (30);
}

PyraAudioProcessorEditor::~PyraAudioProcessorEditor()
{
    stopTimer();
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
    juce::Component::SafePointer<PyraAudioProcessorEditor> safeEditor (this);
    pyra::userActivityHook() = [safeEditor] { if (safeEditor != nullptr) safeEditor->target().history.noteUserActivity(); };

    const juce::StringArray names { "SYNTH", tr ("PRESETS"), tr ("LAYERS"), "FX", "ARP", tr ("CHORDS"), tr ("MATRIX"), "CHAOS", "ANALOG", tr ("SETTINGS") };
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
    pages.push_back (pyra::createFxPage (*ui));
    pages.push_back (pyra::createArpPage (*ui));
    pages.push_back (pyra::createChordsPage (*ui));
    pages.push_back (pyra::createMatrixPage (*ui));
    pages.push_back (pyra::createChaosPage (*ui));
    pages.push_back (pyra::createAnalogPage (*ui));
    pages.push_back (pyra::createGlobalPage (*ui, [this] { buildUi(); resized(); }));

    for (auto& pg : pages) root.addChildComponent (*pg);

    masterKnob = std::make_unique<pyra::Knob> (proc.apvts, "master", "Volume", nullptr);
    root.addAndMakeVisible (*masterKnob);

    presetBar = std::make_unique<pyra::PresetBar> (tgt, presetFilter);
    presetBar->onOpenBrowser = [this] { showPage (PagePresets); };
    presetBar->onSave = [this] { openSaveDialog(); };
    presetBar->onRandom = [this] (bool variation) { randomizeCurrentPage (variation); };
    root.addAndMakeVisible (*presetBar);

    langButton.setButtonText (pyra::lang::current() == pyra::lang::English ? "EN" : "IT");
    langButton.setTooltip ("English / Italiano");
    juce::Component::SafePointer<PyraAudioProcessorEditor> safeThis (this);
    langButton.onClick = [safeThis]
    {
        pyra::lang::set (pyra::lang::current() == pyra::lang::English ? pyra::lang::Italian : pyra::lang::English);
        juce::MessageManager::callAsync ([safe = safeThis]
        {
            if (safe != nullptr) { safe->buildUi(); safe->resized(); }
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
        case PageArp:      randomize (proc, Arp, variation, seed); break;
        case PageChords:   randomize (proc, Chords, variation, seed); break;
        case PageMatrix:   randomize (proc, Matrix, variation, seed); break;
        case PageChaos:    randomize (proc, Chaos, variation, seed); break;
        case PageAnalog:   randomize (proc, Analog, variation, seed); break;
        case PageSettings: randomize (proc, Settings, variation, seed); break;
        default:           randomize (proc, Synth, variation, seed); break;
    }
}

void PyraAudioProcessorEditor::mouseDown (const juce::MouseEvent&)       { target().history.noteUserActivity(); }
void PyraAudioProcessorEditor::mouseUp (const juce::MouseEvent&)         { target().history.noteUserActivity(); }
void PyraAudioProcessorEditor::mouseDrag (const juce::MouseEvent&)       { target().history.noteUserActivity(); }
void PyraAudioProcessorEditor::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) { target().history.noteUserActivity(); }

void PyraAudioProcessorEditor::showPage (int index)
{
    currentPage = index;
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
    pyra::lang::settings().setValue ("windowWidth", getWidth());
}

void PyraAudioProcessorEditor::timerCallback()
{
    if (currentPage >= 0 && currentPage < (int) pages.size())
        pages[(size_t) currentPage]->tick();

    for (auto* k : ui->knobs) k->refreshModulation();

    const int v = proc.totalVoices();
    if (v != lastVoices) { lastVoices = v; root.repaint (baseW - 330, 0, 140, topH); }

    // the breathing glow behind the logo and the scope (only those corners of the top bar are redrawn)
    {
        const float peak = proc.uiPeak.exchange (0.0f);
        const float target = juce::jlimit (0.0f, 1.0f, std::sqrt (peak) * 1.2f);
        scopeLevel = target > scopeLevel ? target : scopeLevel * 0.85f;
        animTime += 0.033f;
        root.repaint (898, 5, 100, 36);
        if (++animCounter >= 3) { animCounter = 0; root.repaint (0, 0, 46, 45); }
    }

    if (++historyCounter >= 3)
    {
        historyCounter = 0;
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

    // logo + wordmark, exactly as drawn (a soft red breath behind the headphones)
    {
        const float breath = 0.5f + 0.5f * std::sin (owner.animTime * 1.7f);
        juce::ColourGradient glow (pyra::colours::accent.withAlpha (0.10f + 0.10f * breath), 25.0f, 23.0f, juce::Colours::transparentBlack, 25.0f + 30.0f, 23.0f, true);
        g.setGradientFill (glow);
        g.fillEllipse (-5.0f, -7.0f, 60.0f, 60.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (pyra::art::logo(), { 6.0f, 1.0f, 38.0f, 44.0f }, juce::RectanglePlacement::centred);
        g.drawImage (pyra::art::wordmark(), { 44.0f, 2.0f, 76.0f, 42.0f }, juce::RectanglePlacement::centred);
    }
    g.setColour (pyra::colours::textDim);
    g.setFont (pyra::PyraLookAndFeel::font (10.0f));
    g.drawText ("v" JucePlugin_VersionString, 121, 26, 60, 14, juce::Justification::centredLeft);

    // what the window is editing (the main sound or one of the layers)
    if (owner.editLayer > 0)
    {
        g.setColour (pyra::colours::accent2);
        g.setFont (pyra::PyraLookAndFeel::titleFont (11.0f));
        g.drawText (tr ("LAYER") + " " + juce::String::charToString ((juce::juce_wchar) ('A' + owner.editLayer)), 121, 8, 62, 14, juce::Justification::centredLeft);
    }

    // voice meter
    const int v = owner.lastVoices;
    g.setColour (v > 0 ? pyra::colours::accent2 : pyra::colours::textDim);
    g.setFont (pyra::PyraLookAndFeel::font (12.0f, true));
    g.drawText (tr ("Voices") + ": " + juce::String (juce::jmax (0, v)), getWidth() - 330, 0, 130, 46, juce::Justification::centredRight);
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
