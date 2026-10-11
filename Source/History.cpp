#include "History.h"
#include "PluginProcessor.h"

namespace pyra
{
UndoHistory::UndoHistory (PyraAudioProcessor& p) : proc (p) {}

int UndoHistory::patternSignature() const
{
    return proc.arpPattern.changeCounter.load() * 92821 + proc.chordPattern.changeCounter.load() * 7919
           + proc.glitchPattern.changeCounter.load() * 31 + proc.lfoDrawn.changeCounter.load() * 3;
}

UndoHistory::Snapshot UndoHistory::capture() const
{
    Snapshot s;
    for (auto* p : proc.getParameters()) s.params.push_back (p->getValue());
    s.patternSig = patternSignature();
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("arp", juce::var (proc.arpPattern.toTree().createXml()->toString()));
    obj->setProperty ("chords", proc.chordPattern.toVar());
    obj->setProperty ("glitch", proc.glitchPattern.toVar());
    obj->setProperty ("lfoDraw", proc.lfoDrawn.toVar());
    s.patterns = juce::var (obj);
    s.presetId = proc.presetId;
    s.presetName = proc.presetName;
    s.presetLoaded = proc.getPresetManager().getLoadedSnapshot();
    return s;
}

void UndoHistory::apply (const Snapshot& s)
{
    const auto& params = proc.getParameters();
    for (int i = 0; i < params.size() && i < (int) s.params.size(); ++i)
    {
        auto* p = params[i];
        if (std::abs (p->getValue() - s.params[(size_t) i]) > 1.0e-7f)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (s.params[(size_t) i]);
            p->endChangeGesture();
        }
    }
    if (auto xml = juce::parseXML (s.patterns["arp"].toString()))
    {
        // keep the locks the user has now: they are a working tool, not part of the sound
        bool locks[kMaxArpSteps];
        for (int k = 0; k < kMaxArpSteps; ++k) locks[k] = proc.arpPattern.steps[(size_t) k].lock.load();
        proc.arpPattern.fromTree (juce::ValueTree::fromXml (*xml));
        for (int k = 0; k < kMaxArpSteps; ++k) proc.arpPattern.steps[(size_t) k].lock = locks[k];
    }
    {
        bool cl[kMaxChordSlots], gl[kMaxGlitchSteps];
        for (int k = 0; k < kMaxChordSlots; ++k) cl[k] = proc.chordPattern.slots[(size_t) k].lock.load();
        for (int k = 0; k < kMaxGlitchSteps; ++k) gl[k] = proc.glitchPattern.steps[(size_t) k].lock.load();
        proc.chordPattern.fromVar (s.patterns["chords"]);
        proc.glitchPattern.fromVar (s.patterns["glitch"]);
        proc.lfoDrawn.fromVar (s.patterns["lfoDraw"]);
        for (int k = 0; k < kMaxChordSlots; ++k) proc.chordPattern.slots[(size_t) k].lock = cl[k];
        for (int k = 0; k < kMaxGlitchSteps; ++k) proc.glitchPattern.steps[(size_t) k].lock = gl[k];
    }
    proc.getPresetManager().restoreCurrent (s.presetId, s.presetName, s.presetLoaded);
}

void UndoHistory::clear()
{
    entries.clear();
    index = -1;
    pending = false;
}

void UndoHistory::commit()
{
    pending = false;
    if (index + 1 < (int) entries.size()) entries.erase (entries.begin() + index + 1, entries.end());
    entries.push_back (capture());
    if ((int) entries.size() > maxEntries) entries.erase (entries.begin());
    index = (int) entries.size() - 1;
}

void UndoHistory::tick()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (entries.empty())
    {
        entries.push_back (capture());
        index = 0;
        lastSeen = entries.back().params;
        lastSeenSig = entries.back().patternSig;
        return;
    }

    std::vector<float> cur;
    cur.reserve (lastSeen.size());
    for (auto* p : proc.getParameters()) cur.push_back (p->getValue());
    const int sig = patternSignature();

    if (cur != lastSeen || sig != lastSeenSig)
    {
        lastSeen = cur;
        lastSeenSig = sig;
        lastChange = now;
        if (now - lastActivity < 3000.0) pending = true;
        else if (! pending) entries[(size_t) index] = capture();   // host automation: follow it, don't record a step
    }

    // commit once things have settled and the mouse is up (a whole knob drag = one step)
    if (pending && now - lastChange > 350.0 && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        commit();
}

bool UndoHistory::canUndo() const { return index > 0 || pending; }

void UndoHistory::undo()
{
    if (pending) commit();
    if (index <= 0) return;
    --index;
    apply (entries[(size_t) index]);
    lastSeen.clear();
    for (auto* p : proc.getParameters()) lastSeen.push_back (p->getValue());
    lastSeenSig = patternSignature();
    entries[(size_t) index].patternSig = lastSeenSig;
    pending = false;
}

void UndoHistory::redo()
{
    if (pending) commit();
    if (index + 1 >= (int) entries.size()) return;
    ++index;
    apply (entries[(size_t) index]);
    lastSeen.clear();
    for (auto* p : proc.getParameters()) lastSeen.push_back (p->getValue());
    lastSeenSig = patternSignature();
    entries[(size_t) index].patternSig = lastSeenSig;
    pending = false;
}

} // namespace pyra
