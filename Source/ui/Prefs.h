#pragma once

// SETTINGS > PERSONALIZE: how the plugin window behaves (saved for the user, the same in every project).
#include <juce_events/juce_events.h>

namespace pyra
{
namespace prefs
{
    enum Key { HelpBubbles, HelpDelay, Animations, ShowValues, KnobDrag, KnobSpeed, WheelKnobs, StartPage, KeyCount };

    int get (Key);
    void set (Key, int value);
    void resetAll();
    /** Sends a change message whenever a preference changes. */
    juce::ChangeBroadcaster& broadcaster();

    inline int helpDelayMs() { const int d = get (HelpDelay); return d == 0 ? 250 : (d == 2 ? 1500 : 700); }
}
}
