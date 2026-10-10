#include "Arp.h"
#include "Engine.h"
#include "../Params.h"

namespace pyra
{
//==============================================================================
void ArpPattern::clear()
{
    for (auto& s : steps)
    {
        s.degree = 0; s.velocity = 0.8f; s.gate = 1.0f;
        s.on = true; s.tie = false; s.lock = false;
    }
    ++changeCounter;
}

void ArpPattern::randomize (int numSteps, uint32_t seedValue)
{
    Rng r; r.seed (seedValue);
    static const int pool[] = { 0, 0, 0, 2, 2, 4, 4, 1, 3, 5, 7, 7, 6, -1, -3, -5, 9, 11, 14 };
    for (int k = 0; k < std::min (numSteps, kMaxArpSteps); ++k)
    {
        auto& s = steps[(size_t) k];
        if (s.lock.load()) continue;
        s.degree = k == 0 ? 0 : pool[r.next() % (uint32_t) std::size (pool)];
        s.on = k == 0 || r.uni() > 0.15f;
        s.velocity = std::min (1.0f, 0.45f + r.uni() * 0.5f + ((k % 4) == 0 ? 0.12f : 0.0f));
        s.gate = 0.3f + r.uni() * 0.7f;
        s.tie = r.uni() < 0.1f;
    }
    ++changeCounter;
}

juce::ValueTree ArpPattern::toTree() const
{
    juce::ValueTree t ("ARP_PATTERN");
    for (auto& s : steps)
    {
        juce::ValueTree c ("STEP");
        c.setProperty ("d", s.degree.load(), nullptr);
        c.setProperty ("v", s.velocity.load(), nullptr);
        c.setProperty ("g", s.gate.load(), nullptr);
        c.setProperty ("on", s.on.load(), nullptr);
        c.setProperty ("tie", s.tie.load(), nullptr);
        c.setProperty ("lock", s.lock.load(), nullptr);
        t.appendChild (c, nullptr);
    }
    return t;
}

void ArpPattern::fromTree (const juce::ValueTree& t)
{
    if (! t.hasType ("ARP_PATTERN")) return;
    for (int k = 0; k < std::min (kMaxArpSteps, t.getNumChildren()); ++k)
    {
        auto c = t.getChild (k);
        auto& s = steps[(size_t) k];
        s.degree = (int) c.getProperty ("d", 0);
        s.velocity = (float) c.getProperty ("v", 0.8f);
        s.gate = (float) c.getProperty ("g", 1.0f);
        s.on = (bool) c.getProperty ("on", true);
        s.tie = (bool) c.getProperty ("tie", false);
        s.lock = (bool) c.getProperty ("lock", false);
    }
    ++changeCounter;
}

//==============================================================================
int scaleShift (int note, int degrees, int scale, int key)
{
    const auto& steps = choices::scaleSteps (scale);
    const int n = (int) steps.size();
    if (n >= 12) return note + degrees;

    const int rel = ((note - key) % 12 + 12) % 12;
    int idx = 0;
    for (int i = 0; i < n; ++i) if (steps[(size_t) i] <= rel) idx = i;
    const int snapped = note - (rel - steps[(size_t) idx]);           // snap down into the scale
    const int total = idx + degrees;
    const int oct = (int) std::floor ((double) total / n);
    const int i2 = total - oct * n;
    return snapped - steps[(size_t) idx] + oct * 12 + steps[(size_t) i2];
}

std::vector<std::pair<int, float>> arpStepNotes (const std::vector<HeldNote>& heldIn, int fireIndex,
                                                 const ArpSettings& s, const ArpPattern& pattern, uint32_t randomSeed)
{
    std::vector<std::pair<int, float>> result;
    if (heldIn.empty()) return result;

    const int steps = juce::jlimit (1, kMaxArpSteps, s.steps);
    const auto& st = pattern.steps[(size_t) (fireIndex % steps)];
    if (! st.on.load()) return result;

    auto sorted = heldIn;
    std::sort (sorted.begin(), sorted.end(), [] (auto& a, auto& b) { return a.note < b.note; });
    auto byOrder = heldIn;
    std::sort (byOrder.begin(), byOrder.end(), [] (auto& a, auto& b) { return a.order < b.order; });

    const int oct = juce::jlimit (1, 4, s.octaves);
    auto expand = [oct] (const std::vector<HeldNote>& v)
    {
        std::vector<HeldNote> e;
        for (int o = 0; o < oct; ++o)
            for (auto h : v) { h.note += 12 * o; e.push_back (h); }
        return e;
    };

    std::vector<HeldNote> picks;
    switch (s.mode)
    {
        case 0: { auto up = expand (sorted); picks.push_back (up[(size_t) (fireIndex % (int) up.size())]); break; }
        case 1: { auto up = expand (sorted); std::reverse (up.begin(), up.end()); picks.push_back (up[(size_t) (fireIndex % (int) up.size())]); break; }
        case 2:
        case 3:
        {
            auto up = expand (sorted);
            if (s.mode == 3) std::reverse (up.begin(), up.end());
            std::vector<HeldNote> cycle (up);
            for (int i = (int) up.size() - 2; i >= 1; --i) cycle.push_back (up[(size_t) i]);
            picks.push_back (cycle[(size_t) (fireIndex % (int) cycle.size())]);
            break;
        }
        case 4:
        {
            auto up = expand (sorted);
            Rng r; r.seed (randomSeed * 2654435761u + 7u);
            picks.push_back (up[r.next() % (uint32_t) up.size()]);
            break;
        }
        case 5: { auto ord = expand (byOrder); picks.push_back (ord[(size_t) (fireIndex % (int) ord.size())]); break; }
        case 6:
        {
            const int o = (fireIndex % (steps * oct)) / steps;
            for (auto h : sorted) { h.note += 12 * o; picks.push_back (h); }
            break;
        }
        default:
        {   // sequencer: the pattern's degrees play a melody from the lowest held note
            auto h = sorted.front();
            h.note += 12 * ((fireIndex / steps) % oct);
            picks.push_back (h);
            break;
        }
    }

    for (auto& p : picks)
    {
        int note = scaleShift (p.note, st.degree.load(), s.scale, s.key);
        if (s.scale != 0) note = scaleShift (note, 0, s.scale, s.key);
        const float vel = p.velocity + (st.velocity.load() - p.velocity) * s.velAmount;
        result.push_back ({ juce::jlimit (0, 127, note), juce::jlimit (0.05f, 1.0f, vel) });
    }
    return result;
}

juce::MidiFile arpToMidiFile (const ArpSettings& s, const ArpPattern& pattern, std::vector<HeldNote> held, int loops)
{
    if (held.empty())
    {   // no chord played yet: use the triad of the chosen key and scale
        const int root = 60 + s.key;
        const int third = s.scale == 0 ? root + 4 : scaleShift (root, 2, s.scale, s.key);
        const int fifth = s.scale == 0 ? root + 7 : scaleShift (root, 4, s.scale, s.key);
        held = { { root, 0.8f, 0 }, { third, 0.8f, 1 }, { fifth, 0.8f, 2 } };
    }

    constexpr int ppq = 960;
    const double stepBeats = choices::syncDivisionBeats (s.rate);
    const int steps = juce::jlimit (1, kMaxArpSteps, s.steps);
    const int fires = steps * std::max (1, loops) * (s.mode == 6 || s.mode == 7 ? std::max (1, s.octaves) : 1);

    juce::MidiMessageSequence seq;
    for (int k = 0; k < fires; ++k)
    {
        const auto& step = pattern.steps[(size_t) (k % steps)];
        const double start = k * stepBeats + ((k & 1) ? s.swing * stepBeats * 0.5 : 0.0);
        const double len = stepBeats * (step.tie.load() ? 1.0 : juce::jlimit (0.05, 1.0, (double) s.gate * step.gate.load()));
        for (auto& [note, vel] : arpStepNotes (held, k, s, pattern, 1u + (uint32_t) k))
        {
            seq.addEvent (juce::MidiMessage::noteOn (1, note, vel), start * ppq);
            seq.addEvent (juce::MidiMessage::noteOff (1, note), (start + len) * ppq - 1.0);
        }
    }
    seq.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ppq);
    file.addTrack (seq);
    return file;
}

//==============================================================================
void Arpeggiator::init (juce::AudioProcessorValueTreeState& state, ArpPattern& pat)
{
    st = &state;
    pattern = &pat;
    auto p = [&state] (const char* id) { auto* v = state.getRawParameterValue (id); jassert (v != nullptr); return v; };
    pOn = p ("arp_on"); pMode = p ("arp_mode"); pRate = p ("arp_rate"); pOct = p ("arp_oct"); pGate = p ("arp_gate");
    pSwing = p ("arp_swing"); pLatch = p ("arp_latch"); pSteps = p ("arp_steps"); pScale = p ("arp_scale");
    pKey = p ("arp_key"); pVel = p ("arp_velamt");
}

void Arpeggiator::prepare (double sampleRate)
{
    sr = sampleRate;
    held.clear(); latched.clear(); playing.clear();
    held.reserve (128); latched.reserve (128); playing.reserve (256);
    physicallyDown = 0; internalBeat = 0.0; fireIndex = 0; lastStepIndex = -1;
    currentStep = -1;
}

ArpSettings Arpeggiator::readSettings() const
{
    auto i = [] (std::atomic<float>* a) { return (int) std::lround (a->load()); };
    ArpSettings s;
    s.mode = i (pMode); s.rate = i (pRate); s.octaves = i (pOct); s.steps = i (pSteps);
    s.scale = i (pScale); s.key = i (pKey);
    s.gate = pGate->load(); s.swing = pSwing->load(); s.velAmount = pVel->load();
    s.latch = pLatch->load() > 0.5f;
    return s;
}

std::vector<HeldNote> Arpeggiator::getLastChord() const
{
    const juce::SpinLock::ScopedLockType sl (chordLock);
    return lastChord;
}

void Arpeggiator::allOff (juce::MidiBuffer& out, int samplePos)
{
    for (auto& p : playing) out.addEvent (juce::MidiMessage::noteOff (1, p.note), samplePos);
    playing.clear();
}

void Arpeggiator::process (juce::MidiBuffer& midi, int numSamples, const HostInfo& host)
{
    const bool on = pOn->load() > 0.5f;
    const auto s = readSettings();

    // keep track of the keyboard even when the arp is off
    auto trackNote = [this, &s] (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            held.erase (std::remove_if (held.begin(), held.end(), [&] (auto& h) { return h.note == m.getNoteNumber(); }), held.end());
            held.push_back ({ m.getNoteNumber(), m.getFloatVelocity(), orderCounter++ });
            ++physicallyDown;
            latched = held;
        }
        else if (m.isNoteOff())
        {
            held.erase (std::remove_if (held.begin(), held.end(), [&] (auto& h) { return h.note == m.getNoteNumber(); }), held.end());
            physicallyDown = std::max (0, physicallyDown - 1);
            if (! s.latch) latched = held;
        }
        if (! held.empty())
        {
            const juce::SpinLock::ScopedLockType sl (chordLock);
            lastChord = held;
        }
    };

    if (! on)
    {
        if (wasOn)
        {
            juce::MidiBuffer extra;
            allOff (extra, 0);
            midi.addEvents (extra, 0, numSamples, 0);
            currentStep = -1;
        }
        wasOn = false;
        for (const auto meta : midi) trackNote (meta.getMessage());
        return;
    }
    wasOn = true;

    const double bpm = host.bpm > 1.0 ? host.bpm : 120.0;
    const double bps = bpm / 60.0 / sr;                       // beats per sample
    const bool hostRunning = host.playing && host.ppqValid;
    if (hostRunning && ! wasPlaying) lastStepIndex = -1;
    wasPlaying = hostRunning;

    double startBeat = hostRunning ? host.ppq : internalBeat;
    const double stepLen = choices::syncDivisionBeats (s.rate);
    const int steps = juce::jlimit (1, kMaxArpSteps, s.steps);

    juce::MidiBuffer out;
    auto active = [&] () -> const std::vector<HeldNote>& { return held.empty() && s.latch ? latched : held; };

    auto beatAt = [&] (int pos) { return startBeat + pos * bps; };
    auto posAt = [&] (double beat) { return juce::jlimit (0, numSamples - 1, (int) std::floor ((beat - startBeat) / bps)); };

    auto fire = [&] (long long k, int pos)
    {
        lastStepIndex = k;
        const auto& notes = active();
        if (notes.empty()) { fireIndex = 0; currentStep = -1; return; }

        const int stepIdx = (int) (fireIndex % steps);
        currentStep = stepIdx;
        const auto& step = pattern->steps[(size_t) stepIdx];
        const double len = stepLen * (step.tie.load() ? 1.05 : juce::jlimit (0.05, 1.0, (double) s.gate * step.gate.load()));
        const double begin = beatAt (pos);

        for (auto& [note, vel] : arpStepNotes (notes, (int) (fireIndex % 1000000), s, *pattern, seed + (uint32_t) fireIndex))
        {
            for (auto it = playing.begin(); it != playing.end();)
            {
                if (it->note == note) { out.addEvent (juce::MidiMessage::noteOff (1, note), pos); it = playing.erase (it); }
                else ++it;
            }
            out.addEvent (juce::MidiMessage::noteOn (1, note, vel), pos);
            playing.push_back ({ note, begin + len });
        }
        ++fireIndex;
    };

    auto runRange = [&] (int a, int b)
    {
        if (b <= a) return;
        const double ba = beatAt (a), bb = beatAt (b);

        // note-offs that fall in this range
        for (auto it = playing.begin(); it != playing.end();)
        {
            if (it->offBeat < bb) { out.addEvent (juce::MidiMessage::noteOff (1, it->note), std::max (a, posAt (it->offBeat))); it = playing.erase (it); }
            else ++it;
        }

        // step boundaries (odd steps pushed later by swing)
        const long long kFirst = (long long) std::floor (ba / stepLen) - 1;
        const long long kLast  = (long long) std::floor (bb / stepLen) + 1;
        for (long long k = kFirst; k <= kLast; ++k)
        {
            const double t = (double) k * stepLen + ((k & 1) ? s.swing * stepLen * 0.5 : 0.0);
            if (t >= ba && t < bb && k > lastStepIndex)
                fire (k, std::max (a, posAt (t)));
        }
    };

    int cursor = 0;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int pos = juce::jlimit (0, numSamples - 1, meta.samplePosition);
        if (m.isNoteOnOrOff())
        {
            runRange (cursor, pos);
            cursor = pos;
            const bool wasEmpty = active().empty();
            trackNote (m);
            if (m.isNoteOn() && wasEmpty)
            {
                fireIndex = 0;
                if (! hostRunning)
                {
                    // free-running: start the pattern right on the key press
                    const long long k = (long long) std::ceil (beatAt (pos) / stepLen);
                    startBeat = (double) k * stepLen - pos * bps;
                    lastStepIndex = k - 1;
                }
            }
        }
        else
            out.addEvent (m, pos);   // pitch bend, CCs etc. pass through
    }
    runRange (cursor, numSamples);

    if (! hostRunning) internalBeat = startBeat + numSamples * bps;
    if (active().empty() && playing.empty()) currentStep = -1;

    midi.swapWith (out);
}

} // namespace pyra
