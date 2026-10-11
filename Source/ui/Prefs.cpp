#include "Prefs.h"
#include "../Lang.h"

namespace pyra
{
namespace prefs
{
    namespace
    {
        const char* names[KeyCount] = { "prefHelp", "prefHelpDelay", "prefAnim", "prefValues", "prefKnobDrag", "prefKnobSpeed",
                                        "prefWheel", "prefStartPage" };
        const int defaults[KeyCount] = { 1, 1, 1, 0, 0, 1, 1, 0 };
    }

    juce::ChangeBroadcaster& broadcaster() { static juce::ChangeBroadcaster b; return b; }

    int get (Key k) { return lang::settings().getIntValue (names[k], defaults[k]); }

    void set (Key k, int v)
    {
        if (get (k) == v) return;
        lang::settings().setValue (names[k], v);
        lang::settings().saveIfNeeded();
        broadcaster().sendChangeMessage();
    }

    void resetAll()
    {
        for (int k = 0; k < KeyCount; ++k) lang::settings().setValue (names[k], defaults[k]);
        lang::settings().saveIfNeeded();
        broadcaster().sendChangeMessage();
    }
}
}
