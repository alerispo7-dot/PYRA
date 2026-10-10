#include "Engine.h"

namespace pyra
{
void Engine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    scratchR.assign ((size_t) std::max (maxBlockSize, 8192), 0.0f);
    for (auto& v : voices) v.prepare (sampleRate);
    held.clear();
    held.reserve (128);
    master.reset (sampleRate, 0.02);
    master.setCurrentAndTargetValue (1.0f);
    for (auto& a : uiEnv) a = 0.0f;
    for (auto& a : uiLfo) a = 0.0f;
}

void Engine::allNotesOff()
{
    for (auto& v : voices) v.kill();
    held.clear();
    sustainPedal = false;
}

int Engine::getActiveVoiceCount() const noexcept
{
    int c = 0;
    for (auto& v : voices) if (v.isActive()) ++c;
    return c;
}

//==============================================================================
void Engine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const Settings& s, const HostInfo& host)
{
    buffer.clear();
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 1 || numSamples == 0) return;

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    // hosts with a mono output: render stereo into scratch, then fold down
    if (r == nullptr)
    {
        if ((int) scratchR.size() < numSamples) scratchR.resize ((size_t) numSamples);
        std::fill (scratchR.begin(), scratchR.begin() + numSamples, 0.0f);
    }
    float* rr = r != nullptr ? r : scratchR.data();

    int pos = 0;
    for (const auto meta : midi)
    {
        const int evPos = juce::jlimit (0, numSamples, meta.samplePosition);
        if (evPos > pos)
        {
            renderChunk (l + pos, rr + pos, evPos - pos, s, host, pos);
            pos = evPos;
        }
        handleMidi (meta.getMessage(), s);
    }
    if (pos < numSamples)
        renderChunk (l + pos, rr + pos, numSamples - pos, s, host, pos);

    applyAnalog (l, rr, numSamples, s);

    if (r == nullptr)
        for (int i = 0; i < numSamples; ++i) l[i] = 0.5f * (l[i] + rr[i]);

    // master volume + safety
    master.setTargetValue (s.masterGain);
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = master.getNextValue();
        float a = l[i] * g;
        if (! std::isfinite (a)) a = 0.0f;
        l[i] = a;
        if (r != nullptr)
        {
            float b = r[i] * g;
            if (! std::isfinite (b)) b = 0.0f;
            r[i] = b;
        }
    }
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // UI snapshot from the newest sounding voice
    const Voice* newest = nullptr;
    int count = 0;
    for (auto& v : voices)
        if (v.isActive())
        {
            ++count;
            if (newest == nullptr || v.age > newest->age) newest = &v;
        }
    uiVoices = count;
    if (newest != nullptr)
    {
        for (int e = 0; e < kNumEnv; ++e) uiEnv[e] = newest->lastEnv[e];
        for (int k = 0; k < kNumLfo; ++k) uiLfo[k] = newest->lastLfo[k];
    }
    else
        for (int e = 0; e < kNumEnv; ++e) uiEnv[e] = 0.0f;
}

void Engine::applyAnalog (float* l, float* r, int n, const Settings& s)
{
    // vintage converter: lower bit depth and sample rate (with a soft anti-alias low-pass, like old samplers)
    if (s.bits > 0.0f || s.rateReduce > 0.0f)
    {
        const int factor = 1 + (int) (s.rateReduce * 15.0f);
        const float lpA = factor > 1 ? std::min (1.0f, 2.0f * kPi * (float) (sr * 0.42 / factor) / (float) sr) : 1.0f;
        const float q = s.bits > 0.0f ? std::exp2 (15.0f - s.bits * 11.0f) : 0.0f;
        for (int i = 0; i < n; ++i)
        {
            convLp[0] += lpA * (l[i] - convLp[0]);
            convLp[1] += lpA * (r[i] - convLp[1]);
            if (--convCount <= 0) { convHold[0] = convLp[0]; convHold[1] = convLp[1]; convCount = factor; }
            float a = factor > 1 ? convHold[0] : convLp[0], b = factor > 1 ? convHold[1] : convLp[1];
            if (q > 0.0f) { a = std::round (a * q) / q; b = std::round (b * q) / q; }
            l[i] = a; r[i] = b;
        }
    }

    // noise floor + mains hum, only while the synth is sounding (no hiss in silent parts of a song)
    if (s.noiseFloor <= 0.0f && s.hum <= 0.0f) { activity = 0.0f; return; }
    const bool sounding = getActiveVoiceCount() > 0;
    const float noiseGain = s.noiseFloor > 0.0f ? juce::Decibels::decibelsToGain (-88.0f + s.noiseFloor * 46.0f) : 0.0f;
    const float humGain = s.hum > 0.0f ? juce::Decibels::decibelsToGain (-80.0f + s.hum * 44.0f) : 0.0f;
    const float humInc = (s.humFreq == 1 ? 60.0f : 50.0f) / (float) sr;
    const float actUp = 1.0f / (0.005f * (float) sr), actDown = 1.0f / (0.4f * (float) sr);
    for (int i = 0; i < n; ++i)
    {
        activity = sounding ? std::min (1.0f, activity + actUp) : std::max (0.0f, activity - actDown);
        if (activity <= 0.0f) continue;
        const float wl = anRng.bi(), wr = anRng.bi();
        pinkL[0] = 0.997f * pinkL[0] + wl * 0.029f; pinkL[1] = 0.985f * pinkL[1] + wl * 0.032f; pinkL[2] = 0.95f * pinkL[2] + wl * 0.048f;
        pinkR[0] = 0.997f * pinkR[0] + wr * 0.029f; pinkR[1] = 0.985f * pinkR[1] + wr * 0.032f; pinkR[2] = 0.95f * pinkR[2] + wr * 0.048f;
        humPhase += humInc; if (humPhase >= 1.0f) humPhase -= 1.0f;
        const float hum = std::sin (kTwoPi * humPhase) + 0.5f * std::sin (2.0f * kTwoPi * humPhase) + 0.25f * std::sin (3.0f * kTwoPi * humPhase);
        l[i] += activity * (noiseGain * (pinkL[0] + pinkL[1] + pinkL[2] + wl * 0.02f) * 4.0f + humGain * hum * 0.6f);
        r[i] += activity * (noiseGain * (pinkR[0] + pinkR[1] + pinkR[2] + wr * 0.02f) * 4.0f + humGain * hum * 0.6f);
    }
}

void Engine::renderChunk (float* l, float* r, int n, const Settings& s, const HostInfo& host, int offsetInBlock)
{
    VoiceContext ctx;
    ctx.s = &s;
    ctx.sampleRate = (float) sr;
    ctx.modWheel = modWheel;
    ctx.channelPressure = channelPressure;
    ctx.pitchBend = pitchBend;
    ctx.globalBendSemis = pitchBend >= 0.0f ? pitchBend * (float) s.bendUp : pitchBend * (float) s.bendDown;

    const double bpm = host.bpm > 1.0 ? host.bpm : 120.0;

    int done = 0;
    while (done < n)
    {
        const int len = std::min (kChunk, n - done);

        for (int k = 0; k < kNumLfo; ++k)
        {
            const auto& lf = s.lfo[k];
            const double beats = choices::syncDivisionBeats (lf.div);
            ctx.lfoSyncHz[k] = (float) (bpm / 60.0 / beats);

            if (lf.sync && host.playing && host.ppqValid)
            {
                const double ppqNow = host.ppq + (double) (offsetInBlock + done) / sr * bpm / 60.0;
                const double ph = ppqNow / beats + lf.phase;
                lfoFreePhase[k] = ph - std::floor (ph);
            }
            else
            {
                const double hz = lf.sync ? ctx.lfoSyncHz[k] : lf.rate;
                lfoFreePhase[k] += hz * len / sr;
                lfoFreePhase[k] -= std::floor (lfoFreePhase[k]);
            }
            ctx.lfoGlobalPhase[k] = (float) std::fmod (lfoFreePhase[k] + (lf.sync && host.playing ? 0.0 : lf.phase), 1.0);
        }

        for (auto& v : voices)
            if (v.isActive())
                v.render (l + done, r + done, len, ctx);

        done += len;
    }
}

//==============================================================================
void Engine::handleMidi (const juce::MidiMessage& m, const Settings& s)
{
    const int ch = juce::jlimit (1, 16, m.getChannel());

    if (m.isNoteOn())
    {
        noteOn (ch, m.getNoteNumber(), m.getFloatVelocity(), s);
    }
    else if (m.isNoteOff())
    {
        noteOff (ch, m.getNoteNumber(), s);
    }
    else if (m.isPitchWheel())
    {
        const float b = juce::jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8192.0f);
        if (s.mpe && ch != 1)
        {
            mpeBend[ch] = b * (float) s.mpeRange;
            for (auto& v : voices) if (v.isActive() && v.channel == ch) v.mpeBendSemis = mpeBend[ch];
        }
        else pitchBend = b;
    }
    else if (m.isChannelPressure())
    {
        const float p = (float) m.getChannelPressureValue() / 127.0f;
        if (s.mpe && ch != 1)
        {
            mpePressure[ch] = p;
            for (auto& v : voices) if (v.isActive() && v.channel == ch) v.mpePressure = p;
        }
        else channelPressure = p;
    }
    else if (m.isAftertouch())
    {
        const float p = (float) m.getAfterTouchValue() / 127.0f;
        for (auto& v : voices)
            if (v.isActive() && v.note == m.getNoteNumber() && v.channel == ch) v.polyPressure = p;
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const float val = (float) m.getControllerValue() / 127.0f;
        if (cc == 1) modWheel = val;
        else if (cc == 74)
        {
            mpeSlide[ch] = val;
            for (auto& v : voices) if (v.isActive() && (v.channel == ch || ! s.mpe)) v.mpeSlide = val;
        }
        else if (cc == 64)
        {
            const bool down = m.getControllerValue() >= 64;
            if (sustainPedal && ! down)
                for (auto& v : voices)
                    if (v.isActive() && v.sustained && ! v.keyDown) { v.sustained = false; v.release(); }
            sustainPedal = down;
        }
        else if (cc == 120) { for (auto& v : voices) v.kill(); held.clear(); }
        else if (cc == 123) { for (auto& v : voices) if (v.isActive()) v.release(); held.clear(); }
    }
    else if (m.isAllNotesOff()) { for (auto& v : voices) if (v.isActive()) v.release(); held.clear(); }
    else if (m.isAllSoundOff()) { allNotesOff(); }
}

Voice* Engine::findVoiceToUse (const Settings& s)
{
    int active = 0;
    Voice* freeVoice = nullptr;
    for (auto& v : voices)
    {
        if (v.isActive()) ++active;
        else if (freeVoice == nullptr) freeVoice = &v;
    }
    if (active < s.polyphony && freeVoice != nullptr) return freeVoice;

    // steal: quietest released voice first, otherwise the oldest one
    Voice* best = nullptr;
    for (auto& v : voices)
        if (v.isActive() && v.isReleasing() && (best == nullptr || v.getLevel() < best->getLevel()))
            best = &v;
    if (best != nullptr) return best;

    for (auto& v : voices)
        if (v.isActive() && (best == nullptr || v.age < best->age))
            best = &v;
    return best != nullptr ? best : &voices[0];
}

void Engine::noteOn (int ch, int note, float vel, const Settings& s)
{
    if (vel <= 0.0f) { noteOff (ch, note, s); return; }

    if (s.voiceMode != ModePoly)
    {
        auto& v = voices[0];
        for (int i = 1; i < kMaxVoices; ++i)
            if (voices[i].isActive()) voices[i].release();

        const bool overlapping = ! held.empty() && v.isActive() && v.keyDown;
        held.erase (std::remove_if (held.begin(), held.end(), [&] (const Held& h) { return h.note == note && h.ch == ch; }), held.end());
        held.push_back ({ note, ch, vel });

        if (overlapping)
        {
            v.legatoTo (note, vel, s.voiceMode == ModeMono, true, s);
            v.channel = ch;
        }
        else
        {
            const float from = (s.glide > 0.0f && ! s.glideLegato && lastNote >= 0.0f) ? lastNote : -1.0f;
            v.start (note, ch, vel, from, s, seedCounter++ * 2654435761u);
            v.age = ++ageCounter;
            if (s.mpe) { v.mpeBendSemis = mpeBend[ch]; v.mpePressure = mpePressure[ch]; v.mpeSlide = mpeSlide[ch]; }
        }
        lastNote = (float) note;
        return;
    }

    // poly: a repeated key releases its previous voice
    bool anyHeld = false;
    for (auto& v : voices)
    {
        if (v.isActive() && v.keyDown) anyHeld = true;
        if (v.isActive() && v.note == note && v.channel == ch && (v.keyDown || v.sustained))
        { v.sustained = false; v.release(); }
    }

    auto* v = findVoiceToUse (s);
    const float from = (s.glide > 0.0f && lastNote >= 0.0f && (! s.glideLegato || anyHeld)) ? lastNote : -1.0f;
    v->start (note, ch, vel, from, s, seedCounter++ * 2654435761u);
    v->age = ++ageCounter;
    if (s.mpe) { v->mpeBendSemis = mpeBend[ch]; v->mpePressure = mpePressure[ch]; v->mpeSlide = mpeSlide[ch]; }
    lastNote = (float) note;
}

void Engine::noteOff (int ch, int note, const Settings& s)
{
    if (s.voiceMode != ModePoly)
    {
        held.erase (std::remove_if (held.begin(), held.end(), [&] (const Held& h) { return h.note == note && h.ch == ch; }), held.end());
        auto& v = voices[0];
        if (v.isActive() && v.note == note && v.keyDown)
        {
            if (! held.empty())
            {
                const auto& h = held.back();
                v.legatoTo (h.note, h.vel, s.voiceMode == ModeMono, true, s);
                v.channel = h.ch;
                lastNote = (float) h.note;
            }
            else if (sustainPedal) { v.keyDown = false; v.sustained = true; }
            else v.release();
        }
        // in case a voice was left over after switching modes
        for (int i = 1; i < kMaxVoices; ++i)
            if (voices[i].isActive() && voices[i].note == note && voices[i].keyDown) voices[i].release();
        return;
    }

    for (auto& v : voices)
    {
        if (v.isActive() && v.note == note && v.channel == ch && v.keyDown)
        {
            if (sustainPedal) { v.keyDown = false; v.sustained = true; }
            else v.release();
        }
    }
}

} // namespace pyra
