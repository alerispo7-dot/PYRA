#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "dsp/Engine.h"
#include "dsp/Effects.h"
#include "dsp/Arp.h"
#include "dsp/Chords.h"
#include "Presets.h"
#include "History.h"
#include <array>

class PyraAudioProcessor : public juce::AudioProcessor
{
public:
    PyraAudioProcessor();
    ~PyraAudioProcessor() override;

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
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    pyra::Engine engine;
    pyra::FxChain fx;
    pyra::ArpPattern arpPattern;
    pyra::Arpeggiator arp;
    pyra::ChordPattern chordPattern;
    pyra::ChordGenerator chords;
    pyra::GlitchPattern glitchPattern;
    pyra::LfoDrawnSet lfoDrawn;   // the hand-drawn LFO shapes
    std::atomic<double> hostBpm { 120.0 };

    // current preset (message thread only); saved with the project
    juce::String presetId, presetName { "Init" };
    pyra::PresetManager& getPresetManager();
    pyra::UndoHistory history { *this };

    /** A hidden second instance used by the section dice (message thread only, created on first use). */
    PyraAudioProcessor& getRandomShadow()
    {
        if (randomShadow == nullptr) randomShadow = std::make_unique<PyraAudioProcessor>();
        return *randomShadow;
    }

    /** Keyboard state shared with the on-screen keyboard. */
    juce::MidiKeyboardState keyboardState;

    //==========================================================================
    // LAYERS: layer A is this sound; layers B..H are complete PYRA sounds living inside this one
    // (own preset, oscillators, FX, ARP, CHORDS), played by the same MIDI and mixed into the output.
    bool isLayerChild = false;
    /** Set by the main sound before each block: its CHORDS / ARP are on, so this layer's own stay quiet. */
    std::atomic<bool> parentChordsOn { false }, parentArpOn { false };
    /** 0 = this processor; 1..7 = the extra layer (nullptr if never used). */
    PyraAudioProcessor* getLayer (int k) const;
    /** Creates the layer the first time it is needed (message thread). */
    PyraAudioProcessor& getOrCreateLayer (int k);
    /** Copies the whole sound (preset, patterns) of layer `from` into layer `to`. */
    void copyLayerSound (int from, int to);
    /** Layer k back to the INIT sound. */
    void initLayerSound (int k);
    int totalVoices() const;

    /** Everything that makes the sound (and, for the main instance, the layers). */
    juce::ValueTree captureState (bool withLayers);
    void restoreState (const juce::ValueTree&);

    std::atomic<float> uiPeak { 0.0f };                       // output peak for the scope (UI resets it)
    std::array<std::atomic<float>, pyra::kNumLayers> layerPeak; // per-layer meters (UI resets them)

private:
    struct LayerRoute
    {
        std::array<uint8_t, 16 * 128> sounding {};   // input (channel, note) -> output note + 1 (0 = silent)
        bool wasOn = false, snap = true;
        juce::SmoothedValue<float> gainL { 0.0f }, gainR { 0.0f };
    };
    struct LayerParams { std::atomic<float>* on {}, *level {}, *pan {}, *trans {}, *lo {}, *hi {}, *vlo {}, *vhi {}, *mute {}, *solo {}, *chan {}; };
    void routeLayerMidi (int k, const juce::MidiBuffer& in, juce::MidiBuffer& out);
    void mixLayers (juce::AudioBuffer<float>& out, const juce::MidiBuffer& input);
    void prepareLayers (double sampleRate, int samplesPerBlock);

    std::array<std::unique_ptr<PyraAudioProcessor>, pyra::kNumLayers> layerOwned;
    std::array<std::atomic<PyraAudioProcessor*>, pyra::kNumLayers> layerPtr;
    std::array<LayerRoute, pyra::kNumLayers> routes;
    std::array<LayerParams, pyra::kNumLayers> lp;
    juce::AudioBuffer<float> layerBuf;
    juce::MidiBuffer layerInput, layerMidi;
    double preparedRate = 0.0;
    int preparedBlock = 0;
    juce::CriticalSection layerLock;

    pyra::ParamRefs refs;
    pyra::Settings settings;
    juce::AudioBuffer<float> tmpRight;
    juce::SmoothedValue<float> outputGain { 1.0f };
    std::unique_ptr<pyra::PresetManager> presetManager;
    std::unique_ptr<PyraAudioProcessor> randomShadow;   // created on first use (message thread)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PyraAudioProcessor)
};
