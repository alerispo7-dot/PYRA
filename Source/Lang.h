#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

namespace pyra
{
/** Interface language (English default, Italian optional). Saved in the user's settings. */
namespace lang
{
    enum Language { English, Italian };

    Language current();
    void set (Language l);

    /** Translates an English UI string; returns it unchanged if no translation exists. */
    juce::String tr (const juce::String& english);

    /** Per-user settings file (language, window size, ...). */
    juce::PropertiesFile& settings();
}
}
