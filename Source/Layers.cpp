// LAYERS: up to 8 complete PYRA sounds in one instance (like a multi-part ROMpler).
#include "PluginProcessor.h"

PyraAudioProcessor* PyraAudioProcessor::getLayer (int k) const
{
    if (k == 0) return const_cast<PyraAudioProcessor*> (this);
    if (isLayerChild || ! juce::isPositiveAndBelow (k, pyra::kNumLayers)) return nullptr;
    return layerPtr[(size_t) k].load();
}

PyraAudioProcessor& PyraAudioProcessor::getOrCreateLayer (int k)
{
    if (k <= 0 || isLayerChild || k >= pyra::kNumLayers) return *this;
    if (auto* p = layerPtr[(size_t) k].load()) return *p;

    const juce::ScopedLock sl (layerLock);
    auto child = std::make_unique<PyraAudioProcessor>();
    child->isLayerChild = true;
    if (preparedRate > 0.0)
    {
        child->setRateAndBufferSizeDetails (preparedRate, preparedBlock);
        child->prepareToPlay (preparedRate, preparedBlock);
    }
    auto* raw = child.get();
    layerOwned[(size_t) k] = std::move (child);
    layerPtr[(size_t) k].store (raw);   // the audio thread sees it only now, fully prepared
    return *raw;
}

void PyraAudioProcessor::prepareLayers (double sampleRate, int samplesPerBlock)
{
    if (isLayerChild) return;
    const juce::ScopedLock sl (layerLock);
    preparedRate = sampleRate;
    preparedBlock = samplesPerBlock;
    layerBuf.setSize (2, std::max (1, samplesPerBlock), false, true, false);
    layerInput.ensureSize (4096);
    layerMidi.ensureSize (4096);
    for (auto& r : routes)
    {
        r.sounding.fill (0);
        r.wasOn = false;
        r.gainL.reset (sampleRate, 0.02);
        r.gainR.reset (sampleRate, 0.02);
        r.snap = true;   // after a reset the gains start where they should be, no fade from an old setting
    }
    for (int k = 1; k < pyra::kNumLayers; ++k)
        if (auto* c = layerPtr[(size_t) k].load())
        {
            c->setRateAndBufferSizeDetails (sampleRate, samplesPerBlock);
            c->prepareToPlay (sampleRate, samplesPerBlock);
        }
}

void PyraAudioProcessor::routeLayerMidi (int k, const juce::MidiBuffer& in, juce::MidiBuffer& out)
{
    out.clear();
    auto& rt = routes[(size_t) k];
    const auto& p = lp[(size_t) k];
    if (p.on == nullptr) { out.addEvents (in, 0, -1, 0); return; }

    const bool on = p.on->load() > 0.5f;
    if (! on)
    {
        // switched off: release whatever this layer was playing
        if (rt.wasOn)
            for (int i = 0; i < 16 * 128; ++i)
                if (rt.sounding[(size_t) i] > 0)
                {
                    out.addEvent (juce::MidiMessage::noteOff (i / 128 + 1, rt.sounding[(size_t) i] - 1), 0);
                    rt.sounding[(size_t) i] = 0;
                }
        rt.wasOn = false;
        return;
    }
    rt.wasOn = true;

    const int lo = (int) p.lo->load(), hi = (int) p.hi->load();
    const int vlo = (int) p.vlo->load(), vhi = (int) p.vhi->load();
    const int trans = (int) std::lround (p.trans->load());
    const int chan = (int) p.chan->load();   // 0 = omni

    for (const auto meta : in)
    {
        auto m = meta.getMessage();
        const int pos = meta.samplePosition;
        const int ch = m.getChannel();
        if (chan > 0 && ch > 0 && ch != chan) continue;
        const int slotBase = (juce::jlimit (1, 16, ch) - 1) * 128;

        if (m.isNoteOn())
        {
            const int n = m.getNoteNumber(), v = m.getVelocity();
            if (n < lo || n > hi || v < vlo || v > vhi) continue;
            const int outN = n + trans;
            if (outN < 0 || outN > 127) continue;
            auto& s = rt.sounding[(size_t) (slotBase + n)];
            if (s > 0) out.addEvent (juce::MidiMessage::noteOff (ch, s - 1), pos);   // same key again: release the old note first
            s = (uint8_t) (outN + 1);
            m.setNoteNumber (outN);
            out.addEvent (m, pos);
        }
        else if (m.isNoteOff())
        {
            auto& s = rt.sounding[(size_t) (slotBase + m.getNoteNumber())];
            if (s == 0) continue;   // this layer never played it
            m.setNoteNumber (s - 1);
            s = 0;
            out.addEvent (m, pos);
        }
        else if (m.isAftertouch())
        {
            const auto s = rt.sounding[(size_t) (slotBase + m.getNoteNumber())];
            if (s == 0) continue;
            m.setNoteNumber (s - 1);
            out.addEvent (m, pos);
        }
        else
        {
            if (m.isAllNotesOff() || m.isAllSoundOff())
                for (int i = 0; i < 128; ++i) rt.sounding[(size_t) (slotBase + i)] = 0;
            out.addEvent (m, pos);
        }
    }
}

void PyraAudioProcessor::mixLayers (juce::AudioBuffer<float>& out, const juce::MidiBuffer& input)
{
    const int n = out.getNumSamples();
    const int outCh = out.getNumChannels();
    if (n <= 0 || outCh <= 0) return;

    bool anySolo = false;
    for (int k = 0; k < pyra::kNumLayers; ++k)
        if (lp[(size_t) k].solo != nullptr && lp[(size_t) k].solo->load() > 0.5f && lp[(size_t) k].on->load() > 0.5f
            && (k == 0 || layerPtr[(size_t) k].load() != nullptr))
            anySolo = true;

    auto targetGains = [&] (int k, float& gl, float& gr)
    {
        const auto& p = lp[(size_t) k];
        const bool audible = p.on->load() > 0.5f && p.mute->load() < 0.5f && (! anySolo || p.solo->load() > 0.5f);
        const float db = p.level->load();
        const float g = (! audible || db <= -39.9f) ? 0.0f : juce::Decibels::decibelsToGain (db);
        const float pan = juce::jlimit (-1.0f, 1.0f, p.pan->load());
        gl = g * std::min (1.0f, 1.0f - pan);
        gr = g * std::min (1.0f, 1.0f + pan);
    };

    auto applyGains = [] (LayerRoute& rt, float* l, float* r, int num, float& peak)
    {
        for (int i = 0; i < num; ++i)
        {
            const float a = rt.gainL.getNextValue(), b = rt.gainR.getNextValue();
            l[i] *= a; r[i] *= b;
            peak = std::max (peak, std::max (std::abs (l[i]), std::abs (r[i])));
        }
    };

    // layer A: this sound
    {
        auto& rt = routes[0];
        float gl, gr;
        targetGains (0, gl, gr);
        if (rt.snap) { rt.gainL.setCurrentAndTargetValue (gl); rt.gainR.setCurrentAndTargetValue (gr); rt.snap = false; }
        rt.gainL.setTargetValue (gl);
        rt.gainR.setTargetValue (gr);
        float peak = 0.0f;
        if (rt.gainL.isSmoothing() || rt.gainR.isSmoothing() || std::abs (gl - 1.0f) > 1.0e-6f || std::abs (gr - 1.0f) > 1.0e-6f)
        {
            if (outCh >= 2) applyGains (rt, out.getWritePointer (0), out.getWritePointer (1), n, peak);
            else
            {
                auto* d = out.getWritePointer (0);
                for (int i = 0; i < n; ++i)
                {
                    d[i] *= 0.5f * (rt.gainL.getNextValue() + rt.gainR.getNextValue());
                    peak = std::max (peak, std::abs (d[i]));
                }
            }
        }
        else
            for (int ch = 0; ch < outCh; ++ch) peak = std::max (peak, out.getMagnitude (ch, 0, n));
        if (peak > layerPeak[0].load()) layerPeak[0].store (peak);
    }

    // layers B..H
    for (int k = 1; k < pyra::kNumLayers; ++k)
    {
        auto* child = layerPtr[(size_t) k].load();
        if (child == nullptr) continue;
        auto& rt = routes[(size_t) k];
        const bool on = lp[(size_t) k].on->load() > 0.5f;
        routeLayerMidi (k, input, layerMidi);
        if (! on && layerMidi.isEmpty() && ! rt.gainL.isSmoothing() && rt.gainL.getCurrentValue() == 0.0f)
            continue;   // off and silent: costs nothing

        if (layerBuf.getNumSamples() < n || layerBuf.getNumChannels() < 2)
            layerBuf.setSize (2, n, false, false, true);
        juce::AudioBuffer<float> view (layerBuf.getArrayOfWritePointers(), 2, n);
        view.clear();
        child->setPlayHead (getPlayHead());
        child->parentChordsOn.store (apvts.getRawParameterValue ("chord_on")->load() > 0.5f);
        child->parentArpOn.store (apvts.getRawParameterValue ("arp_on")->load() > 0.5f);
        child->processBlock (view, layerMidi);

        float gl, gr;
        targetGains (k, gl, gr);
        if (rt.snap) { rt.gainL.setCurrentAndTargetValue (gl); rt.gainR.setCurrentAndTargetValue (gr); rt.snap = false; }
        rt.gainL.setTargetValue (gl);
        rt.gainR.setTargetValue (gr);
        float peak = 0.0f;
        applyGains (rt, view.getWritePointer (0), view.getWritePointer (1), n, peak);
        if (peak > layerPeak[(size_t) k].load()) layerPeak[(size_t) k].store (peak);

        if (outCh >= 2)
        {
            out.addFrom (0, 0, view, 0, 0, n);
            out.addFrom (1, 0, view, 1, 0, n);
        }
        else
        {
            out.addFrom (0, 0, view, 0, 0, n, 0.5f);
            out.addFrom (0, 0, view, 1, 0, n, 0.5f);
        }
    }
}

int PyraAudioProcessor::totalVoices() const
{
    int v = engine.uiVoices.load();
    for (int k = 1; k < pyra::kNumLayers; ++k)
        if (auto* c = getLayer (k))
            if (lp[(size_t) k].on->load() > 0.5f) v += c->engine.uiVoices.load();
    return v;
}

//==============================================================================
juce::ValueTree PyraAudioProcessor::captureState (bool withLayers)
{
    auto state = apvts.copyState();
    state.setProperty ("pyraVersion", JucePlugin_VersionString, nullptr);
    for (auto* name : { "ARP_PATTERN", "CHORD_PATTERN", "GLITCH_PATTERN", "LFO_DRAWN", "LAYERS" })
        state.removeChild (state.getChildWithName (name), nullptr);
    state.appendChild (arpPattern.toTree(), nullptr);
    state.appendChild (chordPattern.toTree(), nullptr);
    state.appendChild (glitchPattern.toTree(), nullptr);
    state.appendChild (lfoDrawn.toTree(), nullptr);
    state.setProperty ("presetId", presetId, nullptr);
    state.setProperty ("presetName", presetName, nullptr);

    if (withLayers && ! isLayerChild)
    {
        juce::ValueTree layers ("LAYERS");
        for (int k = 1; k < pyra::kNumLayers; ++k)
            if (auto* c = getLayer (k))
            {
                auto t = c->captureState (false);
                t.setProperty ("layerIndex", k, nullptr);
                layers.appendChild (t, nullptr);
            }
        state.appendChild (layers, nullptr);
    }
    return state;
}

void PyraAudioProcessor::restoreState (const juce::ValueTree& source)
{
    auto tree = source.createCopy();
    const auto layers = tree.getChildWithName ("LAYERS");
    tree.removeChild (layers, nullptr);

    arpPattern.fromTree (tree.getChildWithName ("ARP_PATTERN"));
    chordPattern.fromTree (tree.getChildWithName ("CHORD_PATTERN"));
    glitchPattern.fromTree (tree.getChildWithName ("GLITCH_PATTERN"));
    lfoDrawn.fromTree (tree.getChildWithName ("LFO_DRAWN"));
    presetId = tree.getProperty ("presetId").toString();
    presetName = tree.getProperty ("presetName", "Init").toString();
    apvts.replaceState (tree);

    if (isLayerChild) return;
    for (int k = 1; k < pyra::kNumLayers; ++k)
    {
        juce::ValueTree lt;
        for (auto c : layers)
            if ((int) c.getProperty ("layerIndex", -1) == k) lt = c;
        if (lt.isValid())
            getOrCreateLayer (k).restoreState (lt);
        else if (getLayer (k) != nullptr)
            initLayerSound (k);
    }
}

void PyraAudioProcessor::copyLayerSound (int from, int to)
{
    auto* src = from == 0 ? this : getLayer (from);
    if (src == nullptr || from == to) return;
    auto& dst = getOrCreateLayer (to);
    auto state = src->captureState (false);
    if (to == 0)
    {
        // keep this instance's own layer settings (mix, zones): copy only the sound
        auto mine = captureState (false);
        for (auto p : mine)
            if (p.hasType ("PARAM") && p.getProperty ("id").toString().startsWith ("layer"))
            {
                auto q = state.getChildWithProperty ("id", p.getProperty ("id"));
                if (q.isValid()) q.setProperty ("value", p.getProperty ("value"), nullptr);
            }
        auto layersCopy = captureState (true).getChildWithName ("LAYERS").createCopy();
        state.appendChild (layersCopy, nullptr);
    }
    dst.restoreState (state);
    std::vector<float> now;
    for (auto* ap : dst.getParameters()) now.push_back (ap->getValue());
    dst.getPresetManager().restoreCurrent (src->presetId, src->presetName, now);   // a copy counts as "not modified"
    dst.history.clear();
}

void PyraAudioProcessor::initLayerSound (int k)
{
    auto& p = getOrCreateLayer (k);
    p.getPresetManager().loadInit();
}
