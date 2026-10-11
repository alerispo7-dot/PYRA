#pragma once

// Short explanations of every parameter, shown in the help bubble when the mouse rests on a control.
#include <juce_core/juce_core.h>
#include <map>

namespace pyra
{
/** English help text for each parameter family ("osc_level", "f_cutoff", "lfo_rate"...). */
const std::map<juce::String, const char*>& paramHelpTable();
/** "oscB_level" -> "osc_level", "f2_cutoff" -> "f_cutoff", "layer3_pan" -> "layer_pan"... */
juce::String paramFamily (const juce::String& paramId);
/** The help text of a parameter in the current language (empty if there is none). */
juce::String paramHelp (const juce::String& paramId);
}
