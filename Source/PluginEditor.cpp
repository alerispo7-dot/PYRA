#include "PluginEditor.h"
#include "Lang.h"

using pyra::lang::tr;
namespace colours = pyra::colours;

namespace
{
    enum PageIndex { PageSynth, PageChaos, PageMatrix, PageFx, PageArp, PageAnalog, PagePresets, PageSettings, PageCount };
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

    startTimerHz (30);
}

PyraAudioProcessorEditor::~PyraAudioProcessorEditor()
{
    stopTimer();
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

    const juce::StringArray names { "SYNTH", "CHAOS", "MATRIX", "FX", "ARP", "ANALOG", tr ("PRESETS"), tr ("SETTINGS") };
    for (int i = 0; i < names.size(); ++i)
    {
        auto* t = tabs.add (new juce::TextButton (names[i]));
        t->setClickingTogglesState (false);
        t->onClick = [this, i] { showPage (i); };
        root.addAndMakeVisible (t);
    }

    pages.push_back (pyra::createSynthPage (*ui));
    pages.push_back (pyra::createChaosPage (*ui));
    pages.push_back (pyra::createMatrixPage (*ui));
    pages.push_back (pyra::createFxPage (*ui));
    pages.push_back (pyra::createArpPage (*ui));
    pages.push_back (pyra::createAnalogPage (*ui));
    pages.push_back (pyra::createPresetPage (*ui, presetFilter, [this] { openSaveDialog(); }));
    pages.push_back (pyra::createGlobalPage (*ui, [this] { buildUi(); resized(); }));

    for (auto& pg : pages) root.addChildComponent (*pg);

    masterKnob = std::make_unique<pyra::Knob> (proc.apvts, "master", "Volume", nullptr);
    root.addAndMakeVisible (*masterKnob);

    presetBar = std::make_unique<pyra::PresetBar> (proc, presetFilter);
    presetBar->onOpenBrowser = [this] { showPage (PagePresets); };
    presetBar->onSave = [this] { openSaveDialog(); };
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

void PyraAudioProcessorEditor::showPage (int index)
{
    currentPage = index;
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
}

//==============================================================================
void PyraAudioProcessorEditor::Root::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    // top bar
    auto bar = getLocalBounds().removeFromTop (46);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1416), 0, 0, colours::bg, 0, 46, false));
    g.fillRect (bar);
    g.setColour (colours::edge);
    g.drawHorizontalLine (45, 0.0f, (float) getWidth());

    // logo: a small flame + the name
    juce::Path flame;
    const float fx = 18.0f, fy = 8.0f;
    flame.startNewSubPath (fx + 10, fy);
    flame.cubicTo (fx + 22, fy + 12, fx + 20, fy + 20, fx + 17, fy + 30);
    flame.cubicTo (fx + 15, fy + 24, fx + 12, fy + 22, fx + 11, fy + 19);
    flame.cubicTo (fx + 10, fy + 24, fx + 7, fy + 26, fx + 6, fy + 30);
    flame.cubicTo (fx - 2, fy + 22, fx + 3, fy + 10, fx + 10, fy);
    flame.closeSubPath();
    g.setGradientFill (juce::ColourGradient (colours::accent2, fx + 10, fy + 30, colours::hot, fx + 10, fy, false));
    g.fillPath (flame);

    g.setColour (colours::text);
    g.setFont (pyra::PyraLookAndFeel::font (26.0f, true));
    g.drawText ("PYRA", 46, 4, 100, 38, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (pyra::PyraLookAndFeel::font (10.0f));
    g.drawText ("v" JucePlugin_VersionString, 120, 22, 60, 14, juce::Justification::centredLeft);

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
        const int w = t->getButtonText().length() > 6 ? 98 : 78;
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
