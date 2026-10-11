#include "ParamHelp.h"
#include "Lang.h"

namespace pyra
{
juce::String paramFamily (const juce::String& id)
{
    if (id.startsWith ("osc") && id.length() > 4 && id[4] == '_') return "osc" + id.substring (4);
    for (auto* prefix : { "layer", "focus", "macro", "env", "lfo", "mod", "f" })
    {
        const juce::String pre (prefix);
        if (id.startsWith (pre) && id.length() > pre.length() && juce::CharacterFunctions::isDigit (id[pre.length()]))
        {
            int i = pre.length();
            while (i < id.length() && juce::CharacterFunctions::isDigit (id[i])) ++i;
            return pre + id.substring (i);
        }
    }
    return id;
}

juce::String paramHelp (const juce::String& id)
{
    const auto& t = paramHelpTable();
    auto it = t.find (paramFamily (id));
    if (it == t.end()) return {};
    return lang::tr (juce::String::fromUTF8 (it->second));
}
}
