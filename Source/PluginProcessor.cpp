#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/Wavetables.h"

PyraAudioProcessor::PyraAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PYRA", pyra::createParameterLayout())
{
    pyra::Wavetables::get();   // build the wavetable bank now, never on the audio thread
    refs.init (apvts);
}

void PyraAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
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

    engine.process (buffer, midi, settings, host);
}

juce::AudioProcessorEditor* PyraAudioProcessor::createEditor()
{
    return new PyraAudioProcessorEditor (*this);
}

void PyraAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("pyraVersion", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PyraAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PyraAudioProcessor();
}
