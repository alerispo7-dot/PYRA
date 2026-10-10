#pragma once

#include "Widgets.h"

class PyraAudioProcessor;

namespace pyra
{
struct UiContext
{
    PyraAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& st;
    ModMatrixAccess& matrix;
    juce::Array<Knob*> knobs;   // every knob, refreshed by the editor timer
};

/** Base for the pages under the top tabs. */
class Page : public juce::Component
{
public:
    virtual void tick() {}   // ~30 Hz, message thread
};

std::unique_ptr<Page> createSynthPage (UiContext&);
std::unique_ptr<Page> createMatrixPage (UiContext&);
std::unique_ptr<Page> createGlobalPage (UiContext&, std::function<void()> onLanguageChanged);
std::unique_ptr<Page> createFxPage (UiContext&);
std::unique_ptr<Page> createAnalogPage (UiContext&);
std::unique_ptr<Page> createArpPage (UiContext&);
std::unique_ptr<Page> createComingSoonPage (const juce::String& title, const juce::StringArray& lines);

} // namespace pyra
