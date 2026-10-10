#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "dsp/Engine.h"

class PyraAudioProcessor : public juce::AudioProcessor
{
public:
    PyraAudioProcessor();
    ~PyraAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PYRA"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    pyra::Engine engine;

    /** Keyboard state shared with the on-screen keyboard. */
    juce::MidiKeyboardState keyboardState;

private:
    pyra::ParamRefs refs;
    pyra::Settings settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PyraAudioProcessor)
};
