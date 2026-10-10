#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/Wavetables.h"

PyraAudioProcessor::PyraAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PYRA", pyra::createParameterLayout())
{
    pyra::Wavetables::get();   // build the wavetable bank now, never on the audio thread
    pyra::SampleBank::get();   // start the background sample loader
    refs.init (apvts);
    fx.init (apvts, glitchPattern);
    arp.init (apvts, arpPattern);
    chords.init (apvts, chordPattern);
}

void PyraAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    fx.prepare (sampleRate, samplesPerBlock);
    arp.prepare (sampleRate);
    chords.prepare (sampleRate);
    outputGain.reset (sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("master")->load(), -48.0f));
    keyboardState.reset();
}

bool PyraAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void PyraAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // notes played on the on-screen keyboard
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    refs.fill (settings);

    pyra::HostInfo host;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) host.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) { host.ppq = *ppq; host.ppqValid = true; }
            host.playing = pos->getIsPlaying();
        }

    hostBpm = host.bpm;
    chords.process (midi, buffer.getNumSamples(), host);   // chords first, so the ARP can arpeggiate them
    arp.process (midi, buffer.getNumSamples(), host);
    const float masterGain = settings.masterGain;
    settings.masterGain = 1.0f;   // the output volume is applied after the FX chain (below)
    engine.process (buffer, midi, settings, host);

    fx.setOffsets (engine.fxOffsets);
    pyra::FxTransport transport { host.bpm, host.ppq, host.playing && host.ppqValid };
    if (buffer.getNumChannels() >= 2)
        fx.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples(), transport);
    else
    {
        // mono host: run the FX on a stereo copy and fold back down
        tmpRight.setSize (1, buffer.getNumSamples(), false, false, true);
        tmpRight.copyFrom (0, 0, buffer, 0, 0, buffer.getNumSamples());
        fx.process (buffer.getWritePointer (0), tmpRight.getWritePointer (0), buffer.getNumSamples(), transport);
        buffer.addFrom (0, 0, tmpRight, 0, 0, buffer.getNumSamples());
        buffer.applyGain (0.5f);
    }

    outputGain.setTargetValue (masterGain);
    if (outputGain.isSmoothing() || std::abs (masterGain - 1.0f) > 1.0e-6f)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float g = outputGain.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.getWritePointer (ch)[i] *= g;
        }

    // final safety: never send NaN or anything above 0 dBFS to the speakers
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float x = d[i];
            if (! std::isfinite (x)) x = 0.0f;
            // transparent up to -0.9 dBFS, then a soft knee that never exceeds 0 dBFS (protects ears and speakers)
            const float ax = std::abs (x);
            if (ax > 0.9f) x = std::copysign (0.9f + 0.1f * std::tanh ((ax - 0.9f) * 10.0f), x);
            d[i] = x;
        }
    }
}

juce::AudioProcessorEditor* PyraAudioProcessor::createEditor()
{
    return new PyraAudioProcessorEditor (*this);
}

void PyraAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("pyraVersion", JucePlugin_VersionString, nullptr);
    for (auto* name : { "ARP_PATTERN", "CHORD_PATTERN", "GLITCH_PATTERN" })
        state.removeChild (state.getChildWithName (name), nullptr);
    state.appendChild (arpPattern.toTree(), nullptr);
    state.appendChild (chordPattern.toTree(), nullptr);
    state.appendChild (glitchPattern.toTree(), nullptr);
    state.setProperty ("presetId", presetId, nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PyraAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            arpPattern.fromTree (tree.getChildWithName ("ARP_PATTERN"));
            chordPattern.fromTree (tree.getChildWithName ("CHORD_PATTERN"));
            glitchPattern.fromTree (tree.getChildWithName ("GLITCH_PATTERN"));
            presetId = tree.getProperty ("presetId").toString();
            presetName = tree.getProperty ("presetName", "Init").toString();
            apvts.replaceState (tree);
        }
}

pyra::PresetManager& PyraAudioProcessor::getPresetManager()
{
    if (presetManager == nullptr) presetManager = std::make_unique<pyra::PresetManager> (*this);
    return *presetManager;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PyraAudioProcessor();
}
