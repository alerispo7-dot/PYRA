#pragma once

#include "Pages.h"
#include "../Presets.h"

namespace pyra
{
/** What the browser currently shows (shared with the arrows in the preset bar). */
struct PresetFilter
{
    juce::String category;     // "" = all, "*fav" = favourites, "*user" = user presets, "#tag" = personal tag
    juce::String subCategory;
    juce::String search;
    juce::StringArray visibleIds;
};

/** Small button that draws an undo / redo arrow. */
class ArrowButton : public juce::Button
{
public:
    ArrowButton (bool redoArrow) : juce::Button (redoArrow ? "redo" : "undo"), redo (redoArrow) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    const bool redo;
};

/** Bar under the tabs: favourite star, name, undo / redo, RANDOM (for the page on screen), previous / next, save. */
class PresetBar : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    PresetBar (PyraAudioProcessor&, PresetFilter&);
    ~PresetBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent& e) override { if (e.eventComponent == &random) randomRightClick = e.mods.isPopupMenu(); }

    std::function<void()> onOpenBrowser, onSave;
    /** RANDOM for the page on screen; variation = small changes only. */
    std::function<void (bool variation)> onRandom;
    void setRandomLabel (const juce::String& pageName);
    void updateUndoButtons();
    /** Picks a random preset among the ones the browser shows. */
    void loadRandomPreset();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    void timerCallback() override;
    void step (int delta);
    juce::Rectangle<int> starArea() const { return { 6, 4, 28, getHeight() - 8 }; }
    juce::Rectangle<int> nameArea() const;

    PyraAudioProcessor& proc;
    PresetManager& pm;
    PresetFilter& filter;
    juce::TextButton prev { "<" }, next { ">" }, save, init, random;
    ArrowButton undo { false }, redo { true };
    bool modified = false, randomRightClick = false;
};

/** Small window to save the current sound as a user preset. */
class SaveDialog : public juce::Component
{
public:
    SaveDialog (PyraAudioProcessor&, std::function<void()> onClose);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void doSave (bool overwrite);

    PyraAudioProcessor& proc;
    std::function<void()> close;
    juce::TextEditor name, tags;
    juce::ComboBox category;
    juce::TextButton saveBtn, overwriteBtn, cancelBtn;
    juce::String overwriteId;
};

std::unique_ptr<Page> createPresetPage (UiContext&, PresetFilter&, std::function<void()> onSave);

} // namespace pyra
