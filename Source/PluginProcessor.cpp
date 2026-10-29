#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/Wavetables.h"
#include "Lang.h"

PyraAudioProcessor::PyraAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PYRA", pyra::createParameterLayout())
{
    pyra::Wavetables::get();   // build the wavetable bank now, never on the audio thread
    pyra::SampleBank::get();   // start the background sample loader
    concertA = juce::jlimit (400.0f, 480.0f, (float) pyra::lang::settings().getDoubleValue ("concertA", 440.0));   // the default you chose last
    refs.init (apvts);
    fx.init (apvts, glitchPattern);
    arp.init (apvts, arpPattern);
    chords.init (apvts, chordPattern);
    for (auto& p : layerPtr) p.store (nullptr);
    for (auto& p : layerPeak) p.store (0.0f);
    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
            if (rp->getParameterID().startsWith ("arp_") || rp->getParameterID().startsWith ("chord_"))
                apvts.addParameterListener (rp->getParameterID(), &sectionWatch);
    for (int k = 0; k < pyra::kNumLayers; ++k)
    {
        auto raw = [&] (const char* n) { return apvts.getRawParameterValue (pyra::layerParam (k, n)); };
        lp[(size_t) k] = { raw ("on"), raw ("level"), raw ("pan"), raw ("trans"), raw ("lo"), raw ("hi"), raw ("vlo"), raw ("vhi"),
                           raw ("mute"), raw ("solo"), raw ("chan") };
    }
}

PyraAudioProcessor::~PyraAudioProcessor()
{
    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
            apvts.removeParameterListener (rp->getParameterID(), &sectionWatch);
    for (auto& p : layerPtr) p.store (nullptr);
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
    prepareLayers (sampleRate, samplesPerBlock);
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

    // a note that ends exactly where the next one starts (a looped or repeated clip): some hosts send the new
    // note-on before the old note-off at the same instant, which would silence the new note (and the ARP)
    {
        bool needs = false;
        int lastPos = -1; bool onAtPos = false;
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (meta.samplePosition != lastPos) { lastPos = meta.samplePosition; onAtPos = false; }
            if (m.isNoteOn()) onAtPos = true;
            else if (m.isNoteOff() && onAtPos) { needs = true; break; }
        }
        if (needs)
        {
            std::vector<std::pair<int, juce::MidiMessage>> ev;
            for (const auto meta : midi) ev.push_back ({ meta.samplePosition, meta.getMessage() });
            std::stable_sort (ev.begin(), ev.end(), [] (const auto& a, const auto& b)
            {
                auto rank = [] (const juce::MidiMessage& m) { return m.isNoteOff() ? 0 : (m.isNoteOn() ? 2 : 1); };
                if (a.first != b.first) return a.first < b.first;
                return rank (a.second) < rank (b.second);
            });
            midi.clear();
            for (auto& [pos, m] : ev) midi.addEvent (m, pos);
        }
    }

    refs.fill (settings);
    if (const float a4 = concertA.load(); std::abs (a4 - 440.0f) > 0.01f)
        settings.tune += 1200.0f * std::log2 (a4 / 440.0f);   // cents
    for (int k = 0; k < pyra::kNumLfo; ++k) settings.lfo[k].drawn = &lfoDrawn.lfo[(size_t) k];

    pyra::HostInfo host;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) host.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) { host.ppq = *ppq; host.ppqValid = true; }
            host.playing = pos->getIsPlaying();
        }
    hostBpm = host.bpm;

    // CHORDS first, so the ARP can arpeggiate them. In a layer, its own CHORDS / ARP only run when the
    // main sound's ones are off (the main CHORDS / ARP already shaped the notes the layer receives).
    if (! (isLayerChild && parentChordsOn.load())) chords.process (midi, buffer.getNumSamples(), host);
    if (! (isLayerChild && parentArpOn.load()))    arp.process (midi, buffer.getNumSamples(), host);

    // LAYERS: every layer gets the notes after CHORDS and ARP; layer A (this sound) gets its own key / velocity / channel zone
    if (! isLayerChild)
    {
        layerInput.clear();
        layerInput.addEvents (midi, 0, buffer.getNumSamples(), 0);
        routeLayerMidi (0, layerInput, layerMidi);
        midi.swapWith (layerMidi);
    }

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

    if (! isLayerChild)
        mixLayers (buffer, layerInput);

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

    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = std::max (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
    if (peak > uiPeak.load()) uiPeak.store (peak);
}

juce::AudioProcessorEditor* PyraAudioProcessor::createEditor()
{
    return new PyraAudioProcessorEditor (*this);
}

void PyraAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = captureState (true).createXml())
    {
        if (editorWidth.load() > 0) xml->setAttribute ("uiWidth", editorWidth.load());   // the window keeps its size in the project
        copyXmlToBinary (*xml, destData);
    }
}

void PyraAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            if (xml->hasAttribute ("uiWidth")) editorWidth = xml->getIntAttribute ("uiWidth");
            xml->removeAttribute ("uiWidth");
            const bool hasTuning = xml->hasAttribute ("concertA");
            restoreState (juce::ValueTree::fromXml (*xml));
            if (! hasTuning) concertA = 440.0f;   // projects saved before the tuning setting existed were all at 440 Hz
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
