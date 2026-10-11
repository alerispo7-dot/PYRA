#include "PluginEditor.h"
#include "Lang.h"
#include "Randomizer.h"
#include "ui/HellArt.h"

using pyra::lang::tr;
namespace colours = pyra::colours;

namespace
{
    enum PageIndex { PageSynth, PagePresets, PageFx, PageArp, PageChords, PageMatrix, PageChaos, PageAnalog, PageSettings, PageCount };
}

PyraAudioProcessorEditor::PyraAudioProcessorEditor (PyraAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), matrix (p.apvts),
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
    pyra::userActivityHook() = [&p] { p.history.noteUserActivity(); };
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

    ui.reset (new pyra::UiContext { proc, proc.apvts, matrix, {} });

    const juce::StringArray names { "SYNTH", tr ("PRESETS"), "FX", "ARP", tr ("CHORDS"), tr ("MATRIX"), "CHAOS", "ANALOG", tr ("SETTINGS") };
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

    presetBar = std::make_unique<pyra::PresetBar> (proc, presetFilter);
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
    saveDialog = std::make_unique<pyra::SaveDialog> (proc, [safeThis]
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

void PyraAudioProcessorEditor::randomizeCurrentPage (bool variation)
{
    proc.history.noteUserActivity();
    const auto seed = (uint32_t) juce::Random::getSystemRandom().nextInt();
    using namespace pyra::randomizer;
    switch (currentPage)
    {
        case PagePresets:  if (presetBar != nullptr) presetBar->loadRandomPreset(); break;
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

void PyraAudioProcessorEditor::mouseDown (const juce::MouseEvent&)       { proc.history.noteUserActivity(); }
void PyraAudioProcessorEditor::mouseUp (const juce::MouseEvent&)         { proc.history.noteUserActivity(); }
void PyraAudioProcessorEditor::mouseDrag (const juce::MouseEvent&)       { proc.history.noteUserActivity(); }
void PyraAudioProcessorEditor::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) { proc.history.noteUserActivity(); }

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

    const int v = proc.engine.uiVoices.load();
    if (v != lastVoices) { lastVoices = v; root.repaint (baseW - 330, 0, 140, topH); }

    // flickering flames in the logo and the lava pit (only the top bar is redrawn)
    if (++animCounter >= 2)
    {
        animCounter = 0;
        animTime += 0.066f;
        root.repaint (0, 0, 180, 44);
        root.repaint (896, 0, 104, 46);
    }

    if (++historyCounter >= 3)
    {
        historyCounter = 0;
        proc.history.tick();
        if (presetBar != nullptr) presetBar->updateUndoButtons();
    }
}

//==============================================================================
void PyraAudioProcessorEditor::Root::paint (juce::Graphics& g)
{
    // hell behind everything
    g.drawImageAt (pyra::art::background (getWidth(), getHeight()), 0, 0);

    // top bar: smoke above a line of lava
    auto bar = getLocalBounds().removeFromTop (46).toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xf0080303), 0, 0, juce::Colour (0xd0180605), 0, 46, false));
    g.fillRect (bar);

    // the damned, reaching out of the lava in the free corner of the bar
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (juce::Rectangle<int> (896, 0, 104, 46));
        juce::ColourGradient pit (juce::Colour (0x00ff6020), 948, 20, juce::Colour (0x90ff5a1a), 948, 46, false);
        g.setGradientFill (pit);
        g.fillRect (896, 20, 104, 26);
        pyra::art::drawSkeleton (g, 914.0f, 46.0f, 34.0f, -0.4f, 0.55f, 3u);
        pyra::art::drawSkeleton (g, 948.0f, 48.0f, 42.0f, 0.1f, 0.6f, 5u);
        pyra::art::drawSkeleton (g, 982.0f, 46.0f, 30.0f, 0.5f, 0.5f, 9u);
        pyra::art::drawLavaStrip (g, { 896.0f, 41.0f, 104.0f, 5.0f }, owner.animTime * 3.0f);
    }
    g.setGradientFill (juce::ColourGradient (colours::accent2, 0, 45, colours::hot, (float) getWidth(), 45, false));
    g.fillRect (0.0f, 44.5f, (float) getWidth(), 1.5f);
    g.setColour (colours::accent.withAlpha (0.18f));
    g.fillRect (0.0f, 46.0f, (float) getWidth(), 3.0f);

    // logo: the treble clef on fire + the name in molten letters
    pyra::art::drawLogo (g, { 10.0f, 4.0f, 34.0f, 38.0f }, owner.animTime);
    const auto name = juce::String ("PYRA");
    juce::GlyphArrangement ga;
    ga.addLineOfText (pyra::PyraLookAndFeel::displayFont (28.0f), name, 46.0f, 33.0f);
    juce::Path letters;
    ga.createPath (letters);
    g.setColour (colours::accent.withAlpha (0.35f));
    g.strokePath (letters, juce::PathStrokeType (3.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff0c0), 0, 10, colours::accent, 0, 34, false));
    g.fillPath (letters);
    g.setColour (colours::textDim);
    g.setFont (pyra::PyraLookAndFeel::font (10.0f));
    g.drawText ("v" JucePlugin_VersionString, 46 + (int) letters.getBounds().getWidth() + 4, 22, 60, 14, juce::Justification::centredLeft);

    // voice meter
    const int v = owner.proc.engine.uiVoices.load();
    g.setColour (v > 0 ? colours::accent2 : colours::textDim);
    g.setFont (pyra::PyraLookAndFeel::font (12.0f, true));
    g.drawText (tr ("Voices") + ": " + juce::String (v), getWidth() - 330, 0, 130, 46, juce::Justification::centredRight);
}

void PyraAudioProcessorEditor::Root::resized()
{
    auto& e = owner;
    int x = 186;
    for (auto* t : e.tabs)
    {
        const int w = t->getButtonText().length() > 7 ? 96 : (t->getButtonText().length() > 6 ? 86 : 70);
        t->setBounds (x, 9, w, 28);
        x += w + 4;
    }
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
