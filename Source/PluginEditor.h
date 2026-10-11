#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "ui/PyraLookAndFeel.h"
#include "ui/Pages.h"
#include "ui/PresetUi.h"

class PyraAudioProcessorEditor : public juce::AudioProcessorEditor,
                                 public juce::DragAndDropContainer,
                                 private juce::Timer,
                                 private juce::ChangeListener
{
public:
    explicit PyraAudioProcessorEditor (PyraAudioProcessor&);
    ~PyraAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int baseW = 1280, baseH = 880;
    static constexpr int defaultW = 1120;              // the size PYRA opens at the first time = "50%"
    static constexpr int minPercent = 10, maxPercent = 100;
    static int percentToWidth (int pct) { return juce::roundToInt ((float) defaultW * (float) pct / 50.0f); }
    int windowPercent() const { return juce::roundToInt ((float) getWidth() * 50.0f / (float) defaultW); }
    void setWindowPercent (int pct);
    static constexpr int topH = 46, presetH = 38;

private:
    /** Everything is laid out at 1280x840 inside this component, then scaled. */
    class Root : public juce::Component
    {
    public:
        explicit Root (PyraAudioProcessorEditor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        void resized() override;
        PyraAudioProcessorEditor& owner;
        juce::Path headerCrack;
        int headerCrackWidth = -1;
        juce::Image cachedLogo, cachedWord;
        float cachedArtScale = 0.0f;
    };

    void timerCallback() override;
    void buildUi();
    void showPage (int index);
    void openSaveDialog();
    void randomizeCurrentPage (bool variation);
    void editLayerSound (int layer);
    void layoutTabs();
    void noteActivity();
    void linkLayers();
    /** The sound the pages are editing: this PYRA (layer A) or one of its layers. */
    PyraAudioProcessor& target();

    PyraAudioProcessor& proc;
    pyra::PyraLookAndFeel lnf;
    std::unique_ptr<pyra::ModMatrixAccess> matrix;
    std::unique_ptr<pyra::UiContext> ui;
    std::unique_ptr<pyra::ModMatrixAccess> masterMatrix;
    std::unique_ptr<pyra::UiContext> masterUi;   // CHORDS and ARP (shared by all the layers)
    std::vector<float> linkSnapshot;
    double lastActivityMs = -1.0e9;

    Root root { *this };
    juce::OwnedArray<juce::TextButton> tabs;
    std::vector<std::unique_ptr<pyra::Page>> pages;
    int currentPage = 0;

    std::unique_ptr<pyra::Knob> masterKnob;
    pyra::PresetFilter presetFilter;
    std::unique_ptr<pyra::PresetBar> presetBar;
    std::unique_ptr<pyra::SaveDialog> saveDialog;
    juce::TextButton langButton;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::TooltipWindow> tooltips;   // the help bubbles (SETTINGS > PERSONALIZE can switch them off)
    void applyPrefs();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    int prefsSignature = -1;
    int pendingWidth = 0; double pendingSince = 0.0;   // the size is remembered once it has stayed put for a moment
    void rememberWidth (int w);
    int lastVoices = -1, historyCounter = 0, animCounter = 0;
    float animTime = 0.0f, scopeLevel = 0.0f;
    int editLayer = 0;   // 0 = the main sound (layer A), 1..7 = layers B..H
    juce::StringArray tabNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PyraAudioProcessorEditor)
};
