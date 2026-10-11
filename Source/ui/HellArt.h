#pragma once

// Artwork for the PYRA look: cracked dark stone, lava veins, the headphones logo, the PYRA wordmark.
#include <juce_gui_basics/juce_gui_basics.h>

namespace pyra
{
namespace art
{
    // pictures (embedded)
    const juce::Image& logo();       // horned headphones (transparent)
    const juce::Image& wordmark();   // PYRA in cracked stone (transparent)
    const juce::Image& stone();      // seamless cracked stone, for panels / buttons / knobs
    const juce::Image& lava (int size = 512);   // charcoal stone with glowing lava veins (big knobs)

    /** Whole-window background: basalt with a few glowing cracks. */
    void drawBackground (juce::Graphics&, juce::Rectangle<float> area);

    /** Fills a shape with the stone texture (origin keeps neighbouring shapes continuous), then darkens it. */
    void fillStone (juce::Graphics&, const juce::Path& shape, juce::Point<float> origin, float darkness, float scale = 0.5f);

    /** A jagged crack from a to b (optionally with short side branches). */
    juce::Path crackLine (juce::Point<float> a, juce::Point<float> b, uint32_t seed, float rough = 2.0f, bool branches = true);

    /** A crack with lava glowing inside. heat 0 = cold dark crack, 1 = molten. */
    void drawLavaVein (juce::Graphics&, const juce::Path&, float width, float heat, juce::Colour tint = juce::Colour (0xffff3018));

    /** A line carved into stone. */
    void engrave (juce::Graphics&, const juce::Path&, float width, float alpha = 1.0f);

    /** The spiky waveform from the logo, moving with the output level (0..1). */
    void drawSpikeWave (juce::Graphics&, juce::Rectangle<float> area, float level, float time);

    /** A slab of dark cracked stone with a chiselled rim (for areas that are not Panels). */
    void drawStoneSlab (juce::Graphics&, juce::Rectangle<float> r, float radius, uint32_t seed, float darkness = 0.55f);

    /** Chiselled stone frame around an area. */
    void drawCarvedFrame (juce::Graphics&, juce::Rectangle<float> r, float radius);

    /** A few hairline cracks inside an area. */
    juce::Path cracks (juce::Rectangle<float> area, uint32_t seed);

    /** Prison-wall scratches: tally marks counting days, crossed-out rows, scribbles, half-legible words, crude sketches.
        Irregular on purpose. Drawn faint, as a background. */
    void drawWallScratches (juce::Graphics&, juce::Rectangle<float> area, uint32_t seed, float strength = 1.0f);

    /** Creepy child-like doodles (shaky lines): skull, horned demon, staring eyes, ghost, bony hand. */
    enum Doodle { DoodleSkull, DoodleDemon, DoodleEyes, DoodleGhost, DoodleHand };
    void drawDoodle (juce::Graphics&, Doodle, juce::Rectangle<float> area, juce::Colour, float thickness, uint32_t seed);
}
} // namespace pyra
