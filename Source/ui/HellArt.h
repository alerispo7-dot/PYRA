#pragma once

// Procedural artwork for the PYRA look: hell background, lava texture, skeletons, flaming treble clef logo.
#include <juce_gui_basics/juce_gui_basics.h>

namespace pyra
{
namespace art
{
    /** Whole-window background (cached): dark smoky sky, volcanoes, embers, a lava glow at the bottom. */
    const juce::Image& background (int width, int height);

    /** Square molten-lava texture (cached), like a cracked magma ball. */
    const juce::Image& lava (int size);

    /** The logo: a treble clef on fire. */
    void drawLogo (juce::Graphics&, juce::Rectangle<float> area);

    /** A skeleton sinking in lava, arms raised. x = centre, baseY = lava surface, h = visible height. */
    void drawSkeleton (juce::Graphics&, float x, float baseY, float h, float lean, float alpha, uint32_t seed);

    /** A strip of lava with flames on top (skeletons stand in it). */
    void drawLavaStrip (juce::Graphics&, juce::Rectangle<float> area, float time = 0.0f);

    /** Creepy child-like doodles (shaky lines): skull, horned demon, staring eyes, ghost, bony hand. */
    enum Doodle { DoodleSkull, DoodleDemon, DoodleEyes, DoodleGhost, DoodleHand };
    void drawDoodle (juce::Graphics&, Doodle, juce::Rectangle<float> area, juce::Colour, float thickness, uint32_t seed);

    /** A decorative scene: lava pit, volcanoes and damned souls. Used in empty areas. */
    void drawHellScene (juce::Graphics&, juce::Rectangle<float> area);
}
} // namespace pyra
