#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class PyraAudioProcessor;

namespace pyra
{
/** RANDOM for every page: musical random values inside safe ranges (never deafening, never silent). */
namespace randomizer
{
    enum Page { Synth, Fx, Arp, Chords, Matrix, Chaos, Analog, Settings };

    /** variation = small changes around the current sound instead of a brand new one. */
    void randomize (PyraAudioProcessor&, Page, bool variation, uint32_t seed);

    /** CHORDS: also picks key, scale, rhythm, voicing... (not only the progression). */
    void randomizeChordsAll (PyraAudioProcessor&, uint32_t seed);

    /** One section only (the dice on each panel). Section ids:
        osc0..osc2, subnoise, filter0, filter1, env1..env4, lfo1..lfo4, voice, macro,
        fx:<module> (dist, res, flt, shf, grn, cho, fla, pha, tape, dly, rev, glt, eq, comp),
        chaos, focus, motion, age, movement, circuit, noise, converter, mod<1..16>, mpe. */
    void randomizeSection (PyraAudioProcessor&, const juce::String& section, bool variation, uint32_t seed);

    /** LAYERS: a random multi (presets that go well together in the layers, levels, pans). variation = small mix changes. */
    void randomizeLayers (PyraAudioProcessor& master, bool variation, uint32_t seed);
    /** One layer only: a random preset (of the same category as now when possible). */
    void randomizeLayer (PyraAudioProcessor& master, int layer, bool variation, uint32_t seed);

    /** GLITCH pattern only. */
    void randomizeGlitch (PyraAudioProcessor&, uint32_t seed);
}
} // namespace pyra
