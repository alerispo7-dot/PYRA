#include "Samples.h"

namespace pyra
{
//==============================================================================
const SampleZone* SampleInstrument::find (int note, int velocity) const noexcept
{
    const SampleZone* best = nullptr;
    int bestDist = 1 << 30;
    for (auto& z : zones)
    {
        const int keyDist = note < z.lo ? z.lo - note : (note > z.hi ? note - z.hi : 0);
        const int velDist = velocity < z.velLo ? z.velLo - velocity : (velocity > z.velHi ? velocity - z.velHi : 0);
        const int d = keyDist * 1000 + velDist;
        if (d < bestDist) { bestDist = d; best = &z; if (d == 0) break; }
    }
    return best;
}

//==============================================================================
const juce::StringArray& SampleBank::ids()
{
    static juce::StringArray a { "", "grand_piano", "upright_piano", "violins", "violas", "cellos", "contrabass", "solo_violin",
                                 "pizzicato", "harp", "trumpet", "french_horn", "trombone", "tuba", "flute", "clarinet", "oboe",
                                 "bassoon", "marimba", "glockenspiel" };
    return a;
}

const juce::StringArray& SampleBank::displayNames()
{
    static juce::StringArray a { "Off", "Grand Piano", "Upright Piano", "Violins", "Violas", "Cellos", "Contrabass", "Solo Violin",
                                 "Pizzicato Strings", "Harp", "Trumpet", "French Horn", "Trombone", "Tuba", "Flute", "Clarinet", "Oboe",
                                 "Bassoon", "Marimba", "Glockenspiel" };
    return a;
}

SampleBank& SampleBank::get()
{
    static SampleBank bank;
    return bank;
}

juce::File SampleBank::locateFolder()
{
    auto ok = [] (const juce::File& f) { return f.getChildFile ("grand_piano").getChildFile ("instrument.json").existsAsFile(); };

    // 1. the user's own copy (Documents/PYRA/Samples) wins
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("PYRA").getChildFile ("Samples");
    if (ok (docs)) return docs;

    // 2. inside this plugin's own bundle (PYRA.vst3/Contents/Resources/Samples)
    auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    for (auto dir = exe.getParentDirectory(); dir.getParentDirectory() != dir; dir = dir.getParentDirectory())
    {
        if (ok (dir.getChildFile ("Resources").getChildFile ("Samples"))) return dir.getChildFile ("Resources").getChildFile ("Samples");
        // e.g. the standalone app sitting next to PYRA.vst3 in the downloaded folder
        const auto nextTo = dir.getChildFile ("PYRA.vst3").getChildFile ("Contents").getChildFile ("Resources").getChildFile ("Samples");
        if (ok (nextTo)) return nextTo;
        if (dir.getFileName().endsWithIgnoreCase (".vst3") || dir.getFileName().endsWithIgnoreCase (".component")
            || dir.getFileName().endsWithIgnoreCase (".app"))
        {
            const auto sibling = dir.getSiblingFile ("PYRA.vst3").getChildFile ("Contents").getChildFile ("Resources").getChildFile ("Samples");
            if (ok (sibling)) return sibling;
            break;
        }
    }

    // 3. the installed VST3 bundle (used by the AU, the standalone app and other hosts)
    juce::Array<juce::File> bundles;
   #if JUCE_MAC
    bundles.add (juce::File ("~/Library/Audio/Plug-Ins/VST3/PYRA.vst3"));
    bundles.add (juce::File ("/Library/Audio/Plug-Ins/VST3/PYRA.vst3"));
   #elif JUCE_WINDOWS
    bundles.add (juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory)
                    .getChildFile ("Common Files").getChildFile ("VST3").getChildFile ("PYRA.vst3"));
    bundles.add (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                    .getChildFile ("VST3").getChildFile ("PYRA.vst3"));
   #else
    bundles.add (juce::File ("~/.vst3/PYRA.vst3"));
   #endif
    for (auto& b : bundles)
        if (ok (b.getChildFile ("Contents").getChildFile ("Resources").getChildFile ("Samples")))
            return b.getChildFile ("Contents").getChildFile ("Resources").getChildFile ("Samples");

    return docs;   // not installed (yet)
}

SampleBank::SampleBank() : juce::Thread ("PYRA sample loader")
{
    folder = locateFolder();
    for (int i = 0; i < kMax; ++i) { loaded[i] = nullptr; status[i] = isInstalled() ? Idle : NotFound; requested[i] = false; }
    startThread (juce::Thread::Priority::low);
}

SampleBank::~SampleBank()
{
    stopThread (4000);
}

SampleBank::Status SampleBank::getStatus (int index) const noexcept
{
    if (index <= 0 || index >= kMax) return Idle;
    return (Status) status[index].load();
}

const SampleInstrument* SampleBank::acquire (int index) noexcept
{
    if (index <= 0 || index >= kMax || index >= ids().size()) return nullptr;
    if (auto* inst = loaded[index].load (std::memory_order_acquire)) return inst;
    requested[index].store (true, std::memory_order_relaxed);
    return nullptr;
}

void SampleBank::run()
{
    while (! threadShouldExit())
    {
        for (int i = 1; i < ids().size() && i < kMax && ! threadShouldExit(); ++i)
        {
            if (! requested[i].load() || loaded[i].load() != nullptr || status[i].load() == Failed)
                continue;
            if (! isInstalled())
            {
                // the user may have installed the samples while the plugin was open
                { const juce::ScopedLock sl (folderLock); folder = locateFolder(); }
                if (! isInstalled()) continue;
            }
            status[i] = Loading;
            if (auto inst = load (i))
            {
                owned.push_back (std::move (inst));
                loaded[i].store (owned.back().get(), std::memory_order_release);
                status[i] = Ready;
            }
            else status[i] = Failed;
        }
        wait (40);
    }
}

std::unique_ptr<SampleInstrument> SampleBank::load (int index)
{
    const auto dir = getFolder().getChildFile (ids()[index]);
    const auto meta = juce::JSON::parse (dir.getChildFile ("instrument.json"));
    auto* zones = meta["zones"].getArray();
    if (zones == nullptr) return nullptr;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    auto inst = std::make_unique<SampleInstrument>();
    inst->id = ids()[index];
    inst->name = meta["name"].toString();
    inst->sustained = (bool) meta["sustained"];
    inst->sampleRate = (double) meta["sampleRate"];

    for (auto& zv : *zones)
    {
        if (threadShouldExit()) return nullptr;
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (dir.getChildFile (zv["file"].toString())));
        if (reader == nullptr) continue;
        const int len = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> buf (2, len);
        buf.clear();
        reader->read (&buf, 0, len, 0, true, true);
        if (reader->numChannels == 1) buf.copyFrom (1, 0, buf, 0, 0, len);

        SampleZone z;
        z.root = (int) zv["root"]; z.lo = (int) zv["lo"]; z.hi = (int) zv["hi"];
        z.velLo = (int) zv["velLo"]; z.velHi = (int) zv["velHi"];
        z.tune = (float) zv["tune"];
        z.loopStart = (int) zv["loopStart"]; z.loopEnd = std::min ((int) zv["loopEnd"], len);
        if (z.loopEnd - z.loopStart < 64) z.loopStart = z.loopEnd = 0;
        z.length = len;
        z.data.resize ((size_t) len * 2 + 8, 0);   // guard frames for interpolation
        for (int i = 0; i < len; ++i)
        {
            z.data[(size_t) i * 2]     = (int16_t) juce::jlimit (-32768, 32767, (int) std::lround (buf.getSample (0, i) * 32767.0f));
            z.data[(size_t) i * 2 + 1] = (int16_t) juce::jlimit (-32768, 32767, (int) std::lround (buf.getSample (1, i) * 32767.0f));
        }
        inst->zones.push_back (std::move (z));
    }
    if (inst->zones.empty()) return nullptr;
    return inst;
}

} // namespace pyra
