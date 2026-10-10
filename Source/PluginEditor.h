#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "ui/PyraLookAndFeel.h"
#include "ui/Pages.h"

class PyraAudioProcessorEditor : public juce::AudioProcessorEditor,
                                 public juce::DragAndDropContainer,
                                 private juce::Timer
{
public:
    explicit PyraAudioProcessorEditor (PyraAudioProcessor&);
    ~PyraAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 1280, baseH = 840;

private:
    /** Everything is laid out at 1280x840 inside this component, then scaled. */
    class Root : public juce::Component
    {
    public:
        explicit Root (PyraAudioProcessorEditor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        void resized() override;
        PyraAudioProcessorEditor& owner;
    };

    void timerCallback() override;
    void buildUi();
    void showPage (int index);

    PyraAudioProcessor& proc;
    pyra::PyraLookAndFeel lnf;
    pyra::ModMatrixAccess matrix;
    std::unique_ptr<pyra::UiContext> ui;

    Root root { *this };
    juce::OwnedArray<juce::TextButton> tabs;
    std::vector<std::unique_ptr<pyra::Page>> pages;
    int currentPage = 0;

    std::unique_ptr<pyra::Knob> masterKnob;
    juce::TextButton langButton;
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips { this, 600 };
    int lastVoices = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PyraAudioProcessorEditor)
};
