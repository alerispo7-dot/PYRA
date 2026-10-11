#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <map>

namespace pyra
{
/** Interface language (English default). Saved in the user's settings. */
namespace lang
{
    enum Language { English, Italian, Spanish, French, German, Portuguese, NumLanguages };

    /** "EN", "IT", ... and "English", "Italiano", "Español"... */
    juce::String code (Language);
    juce::String nativeName (Language);
    /** Translations for the languages after Italian (in LangTables.cpp). */
    const std::map<juce::String, const char*>& table (Language);

    Language current();
    void set (Language l);

    /** Translates an English UI string; returns it unchanged if no translation exists. */
    juce::String tr (const juce::String& english);

    /** Per-user settings file (language, window size, ...). */
    juce::PropertiesFile& settings();
}
}
