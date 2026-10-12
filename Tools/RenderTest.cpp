// Offline test: renders notes through PYRA and checks the audio (no host needed).
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "dsp/Wavetables.h"
#include "Lang.h"
#include "Presets.h"
#include "dsp/Samples.h"
#include "Randomizer.h"
#include <set>
#include "ui/Widgets.h"

using namespace pyra;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;
    int failures = 0;

    struct Note { double time, length; int note; float vel; int ch = 1; };

    /** Simulated DAW transport (playing at a fixed tempo). */
    struct FakePlayHead : juce::AudioPlayHead
    {
        double bpm = 128.0, ppq = 0.0; bool playing = true;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setBpm (bpm); p.setPpqPosition (ppq); p.setIsPlaying (playing);
            return p;
        }
    };
    FakePlayHead* transport = nullptr;

    void set (PyraAudioProcessor& p, const juce::String& id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        if (param == nullptr) { std::printf ("  !! unknown parameter %s\n", id.toRawUTF8()); ++failures; return; }
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    void resetAll (PyraAudioProcessor& p)
    {
        for (auto* param : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (param))
                r->setValueNotifyingHost (r->getDefaultValue());
    }

    struct Result { float peak = 0, rms = 0; bool finite = true; double cpuRatio = 0; };

    Result render (PyraAudioProcessor& p, const juce::String& name, double seconds, const std::vector<Note>& notes,
                   std::function<void (juce::MidiBuffer&, int start, int len)> extraMidi = {})
    {
        p.prepareToPlay (kSr, kBlock);
        const int total = (int) (seconds * kSr);
        juce::AudioBuffer<float> out (2, total);
        out.clear();
        juce::AudioBuffer<float> block (2, kBlock);

        const auto t0 = juce::Time::getHighResolutionTicks();
        for (int pos = 0; pos < total; pos += kBlock)
        {
            const int len = std::min (kBlock, total - pos);
            block.setSize (2, len, false, false, true);
            juce::MidiBuffer midi;
            for (auto& n : notes)
            {
                const int on = (int) (n.time * kSr), off = (int) ((n.time + n.length) * kSr);
                if (on >= pos && on < pos + len) midi.addEvent (juce::MidiMessage::noteOn (n.ch, n.note, n.vel), on - pos);
                if (off >= pos && off < pos + len) midi.addEvent (juce::MidiMessage::noteOff (n.ch, n.note), off - pos);
            }
            if (extraMidi) extraMidi (midi, pos, len);
            if (transport != nullptr) transport->ppq = pos / kSr * transport->bpm / 60.0 + 0.37;   // start mid-beat
            p.processBlock (block, midi);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, block, c, 0, len);
        }
        const auto t1 = juce::Time::getHighResolutionTicks();

        Result r;
        r.cpuRatio = juce::Time::highResolutionTicksToSeconds (t1 - t0) / seconds;
        double sum = 0;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < total; ++i)
            {
                const float v = out.getSample (c, i);
                if (! std::isfinite (v)) r.finite = false;
                r.peak = std::max (r.peak, std::abs (v));
                sum += (double) v * v;
            }
        r.rms = (float) std::sqrt (sum / (2.0 * total));

        auto file = juce::File::getCurrentWorkingDirectory().getChildFile ("renders").getChildFile (name + ".wav");
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::WavAudioFormat wav;
        if (auto stream = std::make_unique<juce::FileOutputStream> (file); stream->openedOk())
            if (auto* w = wav.createWriterFor (stream.get(), kSr, 2, 24, {}, 0))
            {
                stream.release();
                std::unique_ptr<juce::AudioFormatWriter> writer (w);
                writer->writeFromAudioSampleBuffer (out, 0, total);
            }

        const bool ok = r.finite && r.peak > 0.001f && r.peak < 4.0f;
        if (! ok) ++failures;
        std::printf ("  %-26s peak %6.3f  rms %6.3f  cpu %5.1f%%  %s\n", name.toRawUTF8(), r.peak, r.rms,
                     r.cpuRatio * 100.0, ok ? "ok" : "FAIL");
        return r;
    }

    std::vector<Note> chord (double t, double len, std::initializer_list<int> ns, float vel = 0.8f)
    {
        std::vector<Note> v;
        for (int n : ns) v.push_back ({ t, len, n, vel });
        return v;
    }
}

// Saves a PNG of every editor page (layout check without a DAW).
static void snapshots (pyra::lang::Language language, const juce::String& suffix)
{
    pyra::lang::set (language);
    PyraAudioProcessor p;
    p.prepareToPlay (kSr, kBlock);
    // a little modulation so rings are visible
    set (p, "mod1_src", SrcLfo1); set (p, "mod1_dst", dFilt (0, FCutoff)); set (p, "mod1_amt", 0.3f);
    set (p, "mod2_src", SrcEnv2); set (p, "mod2_dst", dOsc (0, OPos)); set (p, "mod2_amt", -0.25f);
    set (p, "f1_cutoff", 2500.0f); set (p, "f1_res", 0.45f);
    set (p, pid::osc (1, "on"), 1); set (p, pid::osc (1, "wt"), 10); set (p, pid::osc (1, "pos"), 0.4f);

    p.getPresetManager().load (p.getPresetManager().getAll()[75].id);
    p.getPresetManager().setFavourite (p.getPresetManager().getAll()[75].id, true);
    set (p, "chaos_amt", 0.65f); set (p, "focus_amt", 0.4f); set (p, "motion_amt", 0.5f); set (p, "motion_shape", 3);
    {   // run a little audio so the activity displays have something to show
        juce::AudioBuffer<float> b (2, kBlock); juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        for (int k = 0; k < 60; ++k) { p.processBlock (b, m); m.clear(); }
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1280, 880);
    auto* root = ed->getChildComponent (0);
    juce::Array<juce::TextButton*> tabs;
    for (auto* c : root->getChildren())
        if (auto* b = dynamic_cast<juce::TextButton*> (c))
        {
            bool isLang = false;
            for (int l = 0; l < pyra::lang::NumLanguages; ++l) isLang = isLang || b->getButtonText() == pyra::lang::code ((pyra::lang::Language) l);
            if (! isLang) tabs.add (b);
        }

    for (int i = 0; i < tabs.size(); ++i)
    {
        if (tabs[i]->onClick) tabs[i]->onClick();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = juce::File::getCurrentWorkingDirectory().getChildFile ("ui").getChildFile (
            "page" + juce::String (i) + "_" + tabs[i]->getButtonText().removeCharacters (" ") + suffix + ".png");
        f.getParentDirectory().createDirectory();
        f.deleteFile();
        {
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        }

        // the FX page has a second view
        std::function<juce::TextButton* (juce::Component*)> findGlitchTab = [&] (juce::Component* c) -> juce::TextButton*
        {
            for (auto* ch : c->getChildren())
            {
                if (auto* b = dynamic_cast<juce::TextButton*> (ch))
                    if (b->getButtonText().startsWith ("GLITCH /") && b->getParentComponent()->isVisible()) return b;
                if (auto* r = findGlitchTab (ch)) return r;
            }
            return nullptr;
        };
        if (auto* gt = findGlitchTab (root))
        {
            p.glitchPattern.randomize (16, 5);
            if (gt->onClick) gt->onClick();
            auto img2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f2 = f.getSiblingFile (f.getFileNameWithoutExtension() + "_creative.png");
            f2.deleteFile();
            juce::FileOutputStream os2 (f2);
            juce::PNGImageFormat().writeImageToStream (img2, os2);
        }
    }
    // ANALOG with the knobs up (the carved pictures wake up) and LAYERS with a few layers in use
    auto snap = [&] (const juce::String& tabText, const juce::String& name)
    {
        for (auto* t : tabs)
            if (t->getButtonText() == tabText && t->onClick) t->onClick();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = juce::File::getCurrentWorkingDirectory().getChildFile ("ui").getChildFile (name + suffix + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
    };
    set (p, "an_age", 0.7f); set (p, "drift", 0.5f); set (p, "spread", 0.4f); set (p, "an_tolerance", 0.3f); set (p, "an_circuit", 0.6f);
    set (p, "an_noise", 0.35f); set (p, "an_hum", 0.3f); set (p, "an_bits", 0.55f); set (p, "an_rate", 0.4f);
    snap ("ANALOG", "x_analog_active");
    set (p, "chaos_amt", 1.0f); set (p, "focus_amt", 0.15f); set (p, "motion_amt", 0.85f);
    snap ("CHAOS", "x_chaos_full");
    p.chordPattern.slots[2].empty = true;   // a rest in the progression
    snap (pyra::lang::tr ("CHORDS"), "x_chords_rest");
    p.chordPattern.slots[2].empty = false;
    for (int k = 1; k < 4; ++k)
    {
        auto& l = p.getOrCreateLayer (k);
        l.getPresetManager().load (l.getPresetManager().getAll()[k * 37].id);
        set (p, pyra::layerParam (k, "on"), 1);
    }
    set (p, pyra::layerParam (1, "level"), -6.0f); set (p, pyra::layerParam (1, "pan"), -0.4f);
    set (p, pyra::layerParam (2, "lo"), 36); set (p, pyra::layerParam (2, "hi"), 59); set (p, pyra::layerParam (2, "trans"), -12);
    set (p, pyra::layerParam (3, "vlo"), 90); set (p, pyra::layerParam (3, "mute"), 1);
    snap (pyra::lang::tr ("LAYERS"), "x_layers_active");
    std::printf ("UI snapshots written (%s)\n", suffix.toRawUTF8());
}

// Renders every preset of a bank, measures loudness and writes a normalised copy.
static int normalizeBank (const juce::File& in, const juce::File& out, const juce::File& auditionDir)
{
    auto bank = juce::JSON::parse (in);
    auto* arr = bank.getArray();
    if (arr == nullptr) { std::printf ("cannot read %s\n", in.getFullPathName().toRawUTF8()); return 1; }

    PyraAudioProcessor p;
    int problems = 0;
    juce::StringArray report;
    juce::Array<juce::var> features;
    std::map<juce::String, std::vector<float>> catLevels;

    for (int idx = 0; idx < arr->size(); ++idx)
    {
        auto& preset = arr->getReference (idx);
        const auto cat = preset["category"].toString();
        presetdata::apply (p, preset);
        if (p.apvts.getRawParameterValue ("smp_on")->load() > 0.5f)
        {
            const int k = (int) std::lround (p.apvts.getRawParameterValue ("smp_inst")->load());
            SampleBank::get().acquire (k);
            for (int w = 0; w < 1000 && SampleBank::get().getStatus (k) != SampleBank::Ready; ++w) juce::Thread::sleep (10);
        }

        // test performance by category (and sub-category)
        const auto sub = preset["sub"].toString();
        std::vector<Note> notes; double len = 2.5;
        const bool kitLoop = sub == "Drum Loop", percLoop = sub == "Percs Loop";
        const bool singleNote = cat == "Bass" || sub.contains ("Lead") || sub == "Chords" || sub.startsWith ("Solo") || sub == "Scat"
                                || sub == "Trumpet" || sub == "Trombone" || sub == "Saxophone" || sub == "Tuba" || sub == "Horn"
                                || cat == "Winds" || sub == "Accordion";
        if (cat == "Bass" || cat == "Lead")        notes = chord (0.0, 1.5, { 36 });
        else if (kitLoop)                          { notes = chord (0.0, 3.0, { 36 }); len = 3.5; }
        else if (percLoop)                         { notes = chord (0.0, 3.0, { 48 }); len = 3.5; }
        else if (cat == "Drums")
        {
            if (sub == "Tonal Drums") notes = chord (0.0, 0.4, { 48 });
            else { notes = { { 0.0, 0.15, 36, 0.9f }, { 0.4, 0.15, 38, 0.9f }, { 0.8, 0.15, 42, 0.8f }, { 1.2, 0.15, 48, 0.9f } }; }
            len = 2.0;
        }
        else if (cat == "FX")                      { notes = chord (0.0, 4.0, { 60 }); len = 6.0; }
        else if (cat == "Arps" || cat == "Arp" || cat == "Sequence") { notes = chord (0.0, 3.0, { 48, 55, 60, 63 }); len = 3.5; }
        else if (cat == "Pad")                     { notes = chord (0.0, 3.0, { 48, 55, 60, 63 }); len = 4.5; }
        else if (singleNote)                       { notes = chord (0.0, 2.0, { 60 }); len = 2.8; }
        else if (cat == "Strings" || cat == "Brass" || cat == "Vox") { notes = chord (0.0, 2.5, { 48, 55, 60, 64 }); len = 3.5; }
        else                                       notes = chord (0.0, 1.5, { 48, 55, 60, 64 });

        // render
        p.prepareToPlay (kSr, kBlock);
        const int total = (int) (len * kSr);
        juce::AudioBuffer<float> outBuf (2, total), block (2, kBlock);
        for (int pos = 0; pos < total; pos += kBlock)
        {
            const int n = std::min (kBlock, total - pos);
            block.setSize (2, n, false, false, true);
            juce::MidiBuffer midi;
            for (auto& nt : notes)
            {
                const int on = (int) (nt.time * kSr), off = (int) ((nt.time + nt.length) * kSr);
                if (on >= pos && on < pos + n) midi.addEvent (juce::MidiMessage::noteOn (1, nt.note, nt.vel), on - pos);
                if (off >= pos && off < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, nt.note), off - pos);
            }
            p.processBlock (block, midi);
            for (int c = 0; c < 2; ++c) outBuf.copyFrom (c, pos, block, c, 0, n);
        }

        // measure: loudest 300 ms window (RMS) and sample peak
        float peak = 0; bool finite = true; double dc = 0;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < total; ++i)
            {
                const float v = outBuf.getSample (c, i);
                if (! std::isfinite (v)) finite = false;
                peak = std::max (peak, std::abs (v)); dc += v;
            }
        dc /= 2.0 * total;
        const int win = (int) (0.3 * kSr);
        double best = 0;
        for (int start = 0; start + win <= total; start += win / 4)
        {
            double sum = 0;
            for (int c = 0; c < 2; ++c)
                for (int i = start; i < start + win; ++i) { const double v = outBuf.getSample (c, i); sum += v * v; }
            best = std::max (best, sum / (2.0 * win));
        }
        const float rmsDb = (float) (10.0 * std::log10 (best + 1e-12));
        const float peakDb = juce::Decibels::gainToDecibels (peak, -120.0f);

        const float targetRms = cat == "Drums" ? -14.0f : -16.0f;

        // sound fingerprint (for finding presets that sound alike): 12 moments x 16 bands, in dB
        {
            constexpr int order = 11, N = 1 << order, frames = 12, bands = 16;
            juce::dsp::FFT fft (order);
            std::vector<float> buf ((size_t) N * 2);
            juce::Array<juce::var> fp;
            for (int f = 0; f < frames; ++f)
            {
                const int start = (int) ((double) f / frames * std::max (0, total - N));
                std::fill (buf.begin(), buf.end(), 0.0f);
                for (int k = 0; k < N && start + k < total; ++k)
                {
                    const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) k / (float) N);
                    buf[(size_t) k] = 0.5f * (outBuf.getSample (0, start + k) + outBuf.getSample (1, start + k)) * w;
                }
                fft.performFrequencyOnlyForwardTransform (buf.data());
                for (int b = 0; b < bands; ++b)
                {
                    const double f0 = 40.0 * std::pow (2.0, b * 9.0 / bands), f1 = 40.0 * std::pow (2.0, (b + 1) * 9.0 / bands);
                    const int k0 = std::max (1, (int) (f0 / kSr * N)), k1 = std::max (k0 + 1, (int) (f1 / kSr * N));
                    double e = 0; for (int k = k0; k < k1 && k < N / 2; ++k) e += buf[(size_t) k] * buf[(size_t) k];
                    fp.add (std::round (10.0 * std::log10 (e / (k1 - k0) + 1e-12) * 10.0) / 10.0);
                }
            }
            double side = 0, mid = 0;
            for (int i = 0; i < total; ++i) { const float l = outBuf.getSample (0, i), r = outBuf.getSample (1, i); side += (l - r) * (l - r); mid += (l + r) * (l + r); }
            auto* fo = new juce::DynamicObject();
            fo->setProperty ("i", idx); fo->setProperty ("name", preset["name"]); fo->setProperty ("cat", cat); fo->setProperty ("sub", sub);
            fo->setProperty ("fp", fp); fo->setProperty ("width", std::sqrt (side / (mid + 1e-12)));
            features.add (juce::var (fo));
        }
        const float gainDb = std::min (targetRms - rmsDb, -1.5f - peakDb);
        auto* params = preset["params"].getDynamicObject();
        const float oldMaster = params->hasProperty ("master") ? (float) params->getProperty ("master") : -4.0f;
        float newMaster = oldMaster + gainDb;
        bool clamped = false;
        if (newMaster > 24.0f) { newMaster = 24.0f; clamped = true; }
        if (newMaster < -36.0f) { newMaster = -36.0f; clamped = true; }
        params->setProperty ("master", std::round (newMaster * 10.0f) / 10.0f);

        const bool silent = peakDb < -60.0f;
        const bool bad = ! finite || silent || clamped || std::abs (dc) > 0.02;
        if (bad) ++problems;
        catLevels[cat].push_back (rmsDb - peakDb);   // crest factor
        report.add (juce::String::formatted ("%-4d %-14s %-26s rms %6.1f  peak %6.1f  master %5.1f -> %5.1f %s%s%s%s",
            idx, cat.toRawUTF8(), preset["name"].toString().toRawUTF8(), rmsDb, peakDb, oldMaster, newMaster,
            finite ? "" : " NAN", silent ? " SILENT" : "", clamped ? " CLAMPED" : "", std::abs (dc) > 0.02 ? " DC" : ""));

        // audition file (normalised) for listening checks
        if (auditionDir != juce::File())
        {
            outBuf.applyGain (juce::Decibels::decibelsToGain (newMaster - oldMaster));
            auto f = auditionDir.getChildFile (juce::String (idx).paddedLeft ('0', 3) + "_"
                                               + juce::File::createLegalFileName (preset["name"].toString()).replace (" ", "_") + ".wav");
            f.deleteFile();
            juce::WavAudioFormat wav;
            if (auto stream = std::make_unique<juce::FileOutputStream> (f); stream->openedOk())
                if (auto* w = wav.createWriterFor (stream.get(), kSr, 2, 16, {}, 0))
                {
                    stream.release();
                    std::unique_ptr<juce::AudioFormatWriter> writer (w);
                    writer->writeFromAudioSampleBuffer (outBuf, 0, total);
                }
        }
    }

    out.replaceWithText (juce::JSON::toString (bank, true));
    juce::File::getCurrentWorkingDirectory().getChildFile ("features.json").replaceWithText (juce::JSON::toString (juce::var (features), true));
    report.add (juce::String (problems) + " presets with problems");
    juce::File::getCurrentWorkingDirectory().getChildFile ("normalize_report.txt").replaceWithText (report.joinIntoString ("\n"));
    std::printf ("normalised %d presets, %d with problems (see normalize_report.txt)\n", arr->size(), problems);
    return problems == 0 ? 0 : 2;
}

// Converts every audio file listed in a text file ("in<TAB>out" per line). Output format from the extension:
// .wav = 32-bit float, .ogg = Ogg Vorbis (quality index argv[3], default 6).
static int convertList (const juce::File& list, int oggQuality)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    juce::StringArray lines;
    list.readLines (lines);
    int done = 0, failed = 0;
    for (auto& line : lines)
    {
        if (line.trim().isEmpty()) continue;
        const juce::File in (line.upToFirstOccurrenceOf ("\t", false, false));
        const juce::File out (line.fromFirstOccurrenceOf ("\t", false, false));
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (in));
        if (reader == nullptr) { std::printf ("cannot read %s\n", in.getFullPathName().toRawUTF8()); ++failed; continue; }
        juce::AudioBuffer<float> buf ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&buf, 0, (int) reader->lengthInSamples, 0, true, true);

        out.getParentDirectory().createDirectory();
        out.deleteFile();
        std::unique_ptr<juce::AudioFormat> format;
        int bits = 32;
        if (out.hasFileExtension ("ogg")) { format = std::make_unique<juce::OggVorbisAudioFormat>(); bits = 16; }
        else format = std::make_unique<juce::WavAudioFormat>();
        auto stream = std::make_unique<juce::FileOutputStream> (out);
        std::unique_ptr<juce::AudioFormatWriter> writer (format->createWriterFor (stream.get(), reader->sampleRate,
                                                           (unsigned int) buf.getNumChannels(), bits, {}, oggQuality));
        if (writer == nullptr) { ++failed; continue; }
        stream.release();
        writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
        ++done;
    }
    std::printf ("converted %d files, %d failed\n", done, failed);
    return failed == 0 ? 0 : 1;
}

/** Renders short musical phrases of chosen presets (for the website's sound demos).
    jobs: [{ "name", "preset": {...}, "bpm", "seconds", "notes": [[time s, length s, note, velocity 0..1], ...] }] */
static int renderDemos (const juce::File& jobsFile, const juce::File& outDir)
{
    auto jobs = juce::JSON::parse (jobsFile);
    auto* arr = jobs.getArray();
    if (arr == nullptr) { std::printf ("cannot read %s\n", jobsFile.getFullPathName().toRawUTF8()); return 1; }
    outDir.createDirectory();
    PyraAudioProcessor p;
    FakePlayHead ph;
    p.setPlayHead (&ph);
    for (auto& job : *arr)
    {
        presetdata::apply (p, job["preset"]);
        if (p.apvts.getRawParameterValue ("smp_on")->load() > 0.5f)
        {
            const int k = (int) std::lround (p.apvts.getRawParameterValue ("smp_inst")->load());
            SampleBank::get().acquire (k);
            for (int w = 0; w < 1000 && SampleBank::get().getStatus (k) != SampleBank::Ready; ++w) juce::Thread::sleep (10);
        }
        ph.bpm = (double) job["bpm"]; ph.ppq = 0.0; ph.playing = true;
        std::vector<Note> notes;
        if (auto* ns = job["notes"].getArray())
            for (auto& n : *ns) notes.push_back ({ (double) n[0], (double) n[1], (int) n[2], (float) (double) n[3] });
        const int total = (int) ((double) job["seconds"] * kSr);
        p.prepareToPlay (kSr, kBlock);
        juce::AudioBuffer<float> out (2, total), block (2, kBlock);
        for (int pos = 0; pos < total; pos += kBlock)
        {
            const int len = std::min (kBlock, total - pos);
            block.setSize (2, len, false, false, true);
            juce::MidiBuffer midi;
            for (auto& n : notes)
            {
                const int on = (int) (n.time * kSr), off = (int) ((n.time + n.length) * kSr);
                if (on >= pos && on < pos + len) midi.addEvent (juce::MidiMessage::noteOn (1, n.note, n.vel), on - pos);
                if (off >= pos && off < pos + len) midi.addEvent (juce::MidiMessage::noteOff (1, n.note), off - pos);
            }
            ph.ppq = pos / kSr * ph.bpm / 60.0;
            p.processBlock (block, midi);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, block, c, 0, len);
        }
        auto f = outDir.getChildFile (job["name"].toString() + ".wav");
        f.deleteFile();
        juce::WavAudioFormat wav;
        if (auto stream = std::make_unique<juce::FileOutputStream> (f); stream->openedOk())
            if (auto* w = wav.createWriterFor (stream.get(), kSr, 2, 24, {}, 0))
            {
                stream.release();
                std::unique_ptr<juce::AudioFormatWriter> writer (w);
                writer->writeFromAudioSampleBuffer (out, 0, total);
            }
        std::printf ("  %s  peak %.3f\n", job["name"].toString().toRawUTF8(), out.getMagnitude (0, total));
    }
    p.setPlayHead (nullptr);
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 2 && juce::String (argv[1]) == "--convert")
        return convertList (juce::File (argv[2]), argc > 3 ? juce::String (argv[3]).getIntValue() : 6);
    if (argc > 3 && juce::String (argv[1]) == "--demo")
        return renderDemos (juce::File (argv[2]), juce::File (argv[3]));
    if (argc > 3 && juce::String (argv[1]) == "--normalize")
    {
        juce::File audition;
        if (argc > 4) { audition = juce::File (argv[4]); audition.createDirectory(); }
        return normalizeBank (juce::File (argv[2]), juce::File (argv[3]), audition);
    }
    if (argc > 1 && juce::String (argv[1]) == "--choices")
    {   // every parameter as JSON: range, default and the names of its choices (for the preset generator)
        PyraAudioProcessor p;
        auto* all = new juce::DynamicObject();
        for (auto* ap : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (ap))
            {
                auto* o = new juce::DynamicObject();
                const auto& rng = r->getNormalisableRange();
                o->setProperty ("min", rng.start); o->setProperty ("max", rng.end);
                o->setProperty ("def", r->convertFrom0to1 (r->getDefaultValue()));
                if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (r)) o->setProperty ("choices", c->choices);
                if (dynamic_cast<juce::AudioParameterBool*> (r) != nullptr) o->setProperty ("bool", true);
                if (dynamic_cast<juce::AudioParameterInt*> (r) != nullptr) o->setProperty ("int", true);
                all->setProperty (r->getParameterID(), juce::var (o));
            }
        std::printf ("%s\n", juce::JSON::toString (juce::var (all), true).toRawUTF8());
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "--params")
    {
        PyraAudioProcessor p;
        for (auto* ap : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (ap))
                std::printf ("%s\t%s\n", r->getParameterID().toRawUTF8(), r->getName (100).toRawUTF8());
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "--ui")
    {
        snapshots (pyra::lang::English, "_en");
        snapshots (pyra::lang::Italian, "_it");
        snapshots (pyra::lang::Spanish, "_es");
        snapshots (pyra::lang::French, "_fr");
        snapshots (pyra::lang::German, "_de");
        snapshots (pyra::lang::Portuguese, "_pt");
        pyra::lang::set (pyra::lang::English);
        return 0;
    }
    auto start = juce::Time::getMillisecondCounterHiRes();
    const auto& bank = Wavetables::get();
    std::printf ("Wavetable bank: %d tables built in %.0f ms\n", bank.size(), juce::Time::getMillisecondCounterHiRes() - start);

    PyraAudioProcessor p;
    std::printf ("Parameters: %d\n\n", p.getParameters().size());

    std::printf ("Scenarios\n");
    resetAll (p);
    render (p, "01_default_saw_chord", 3.0, chord (0.0, 2.0, { 48, 60, 64, 67 }));

    // Supersaw pad with filter envelope
    resetAll (p);
    for (int o = 0; o < 3; ++o)
    {
        set (p, pid::osc (o, "on"), 1);
        set (p, pid::osc (o, "voices"), 9);
        set (p, pid::osc (o, "detune"), 0.3f);
        set (p, pid::osc (o, "level"), 0.5f);
    }
    set (p, pid::osc (1, "oct"), -1);
    set (p, pid::osc (2, "wt"), 5); // Analog Saw
    set (p, "f1_type", FLp24); set (p, "f1_cutoff", 600); set (p, "f1_res", 0.4f); set (p, "f1_env", 0.45f);
    set (p, "env2_att", 0.4f); set (p, "env2_dec", 1.5f);
    set (p, "env1_att", 0.2f); set (p, "env1_rel", 1.0f);
    render (p, "02_supersaw_pad", 4.0, chord (0.0, 2.5, { 48, 55, 60, 63, 67 }));

    // Every filter type with an LFO sweeping the cutoff
    for (int t = 0; t < FTypeCount; ++t)
    {
        resetAll (p);
        set (p, "f1_type", (float) t); set (p, "f1_cutoff", 800); set (p, "f1_res", 0.7f); set (p, "f1_drive", 0.3f);
        set (p, "mod1_src", SrcLfo1); set (p, "mod1_dst", dFilt (0, FCutoff)); set (p, "mod1_amt", 0.35f);
        set (p, "lfo1_sync", 0); set (p, "lfo1_rate", 1.5f);
        render (p, "03_filter_" + juce::String (t).paddedLeft ('0', 2) + "_" + choices::filterTypes()[t].replace (" ", ""),
                1.5, chord (0.0, 1.2, { 45, 57 }));
    }

    // Warp modes + FM
    for (int w = 0; w < WarpCount; ++w)
    {
        resetAll (p);
        set (p, pid::osc (0, "warp"), (float) w);
        set (p, pid::osc (0, "warpAmt"), 0.6f);
        set (p, pid::osc (1, "on"), 1);
        set (p, pid::osc (1, "wt"), 1);  // sine as FM carrier
        set (p, pid::osc (1, "fmSrc"), FmOscA);
        set (p, pid::osc (1, "fmAmt"), 0.4f);
        set (p, "mod1_src", SrcEnv2); set (p, "mod1_dst", dOsc (0, OWarp)); set (p, "mod1_amt", 0.4f);
        render (p, "04_warp_" + choices::warpModes()[w].replace (" ", "").replace ("+", "P").replace ("-", "M"), 1.2,
                chord (0.0, 1.0, { 52 }));
    }

    // Mono legato glide bass with sub + noise
    resetAll (p);
    set (p, "voiceMode", ModeLegato); set (p, "glide", 0.12f);
    set (p, "sub_on", 1); set (p, "noise_on", 1); set (p, "noise_type", 1); set (p, "noise_level", 0.1f);
    set (p, "f1_type", FLadder); set (p, "f1_cutoff", 400); set (p, "f1_res", 0.5f);
    {
        std::vector<Note> line { { 0.0, 0.5, 36, 0.9f }, { 0.4, 0.5, 43, 0.9f }, { 0.8, 0.5, 39, 0.9f }, { 1.2, 0.8, 48, 0.9f } };
        render (p, "05_mono_legato_glide", 2.6, line);
    }

    // All wavetables, middle position, with a WT-position LFO
    resetAll (p);
    {
        std::vector<Note> seq;
        for (int i = 0; i < bank.size(); ++i) seq.push_back ({ i * 0.35, 0.3, 60 + (i % 5) * 2, 0.8f });
        set (p, "mod1_src", SrcLfo1); set (p, "mod1_dst", dOsc (0, OPos)); set (p, "mod1_amt", 0.5f);
        set (p, pid::osc (0, "pos"), 0.25f);
        // switch tables while playing
        int idx = 0;
        render (p, "06_all_wavetables", bank.size() * 0.35 + 0.5, seq, [&] (juce::MidiBuffer&, int pos, int)
        {
            const int want = std::min (bank.size() - 1, (int) (pos / kSr / 0.35));
            if (want != idx) { idx = want; set (p, pid::osc (0, "wt"), (float) idx); }
        });
    }

    // MPE: per-note bend on channel 2
    resetAll (p);
    set (p, "mpe", 1);
    {
        std::vector<Note> n { { 0.0, 1.5, 60, 0.8f, 2 }, { 0.0, 1.5, 64, 0.8f, 3 } };
        render (p, "07_mpe_bend", 2.0, n, [] (juce::MidiBuffer& m, int pos, int len)
        {
            for (int i = 0; i < len; i += 64)
            {
                const double t = (pos + i) / kSr;
                if (t > 0.2 && t < 1.2)
                    m.addEvent (juce::MidiMessage::pitchWheel (2, 8192 + (int) (4000 * std::sin (t * 6.0))), i);
            }
        });
    }

    // Stress: 32 notes, 3 oscillators x 16 unison, 2 filters
    resetAll (p);
    set (p, "polyphony", 32);
    for (int o = 0; o < 3; ++o) { set (p, pid::osc (o, "on"), 1); set (p, pid::osc (o, "voices"), 16); set (p, pid::osc (o, "level"), 0.2f); }
    set (p, "f2_on", 1); set (p, "f2_type", FLadder); set (p, "filterRouting", 0);
    {
        std::vector<Note> many;
        for (int i = 0; i < 32; ++i) many.push_back ({ 0.0, 2.0, 36 + i * 2, 0.5f });
        auto r = render (p, "08_stress_32x3x16", 2.5, many);
        std::printf ("     (worst case: 1536 oscillators at once, %.1f%% of one CPU core)\n", r.cpuRatio * 100.0);
    }

    // ---- Phase 3: every FX module on a plucky chord
    {
        const char* mods[] = { "dist", "res", "flt", "shf", "grn", "cho", "fla", "pha", "tape", "dly", "rev", "glt", "eq", "comp" };
        for (auto* m : mods)
        {
            resetAll (p);
            set (p, "env1_dec", 0.4f); set (p, "env1_sus", 0.2f); set (p, "env1_rel", 0.3f);
            set (p, juce::String ("fx_") + m + "_on", 1);
            if (juce::String (m) == "eq") { set (p, "fx_eq_lowg", 6); set (p, "fx_eq_midg", -6); set (p, "fx_eq_highg", 4); }
            render (p, "20_fx_" + juce::String (m), 3.0, chord (0.0, 0.6, { 48, 60, 63, 67 }));
        }
        // delay timing: short blip, dry off
        resetAll (p);
        set (p, "env1_dec", 0.02f); set (p, "env1_sus", 0.0f); set (p, "env1_rel", 0.01f);
        set (p, "fx_dly_on", 1); set (p, "fx_dly_mix", 1.0f); set (p, "fx_dly_fb", 0.5f);
        render (p, "26_delay_blip", 2.0, chord (0.0, 0.03, { 72 }));

        // distortion types
        for (int t = 0; t < 8; ++t)
        {
            resetAll (p);
            set (p, "fx_dist_on", 1); set (p, "fx_dist_type", (float) t); set (p, "fx_dist_drive", 0.6f);
            render (p, "21_dist_type" + juce::String (t), 1.0, chord (0.0, 0.8, { 40 }));
        }
        // stability: delay feedback above 1, huge shimmer reverb, everything on at once
        resetAll (p);
        set (p, "fx_dly_on", 1); set (p, "fx_dly_fb", 1.05f); set (p, "fx_dly_mode", 1);
        set (p, "fx_rev_on", 1); set (p, "fx_rev_type", 2); set (p, "fx_rev_decay", 20.0f); set (p, "fx_rev_shim", 1.0f); set (p, "fx_rev_mix", 0.6f);
        auto r1 = render (p, "22_runaway_test", 12.0, chord (0.0, 0.5, { 60, 64, 67 }));
        std::printf ("     (12 s with delay feedback 105%% + 20 s shimmer: peak %.2f, must stay bounded)\n", r1.peak);

        resetAll (p);
        for (auto* m : mods) set (p, juce::String ("fx_") + m + "_on", 1);
        set (p, "polyphony", 16);
        auto r2 = render (p, "23_all_fx_on", 4.0, chord (0.0, 2.0, { 36, 48, 55, 60, 64, 67, 72 }));
        std::printf ("     (all 14 effects: %.1f%% CPU)\n", r2.cpuRatio * 100.0);
    }

    // ---- ANALOG: full AGE vs clean
    resetAll (p);
    set (p, "an_age", 1.0f); set (p, "an_bits", 0.5f); set (p, "an_rate", 0.4f);
    render (p, "24_analog_age_full", 3.0, chord (0.0, 2.0, { 48, 55, 60, 64 }));

    // ---- ARP: every mode, free running (no host transport)
    for (int m = 0; m < 8; ++m)
    {
        resetAll (p);
        set (p, "arp_on", 1); set (p, "arp_mode", (float) m); set (p, "arp_oct", 2);
        set (p, "env1_dec", 0.2f); set (p, "env1_sus", 0.3f);
        if (m == 7) { set (p, "arp_scale", 2); p.arpPattern.randomize (16, 42); }
        int noteOns = 0;
        render (p, "25_arp_" + choices::arpModes()[m].replace (" ", "").replace ("-", ""), 2.2, chord (0.0, 2.0, { 60, 64, 67 }));
        (void) noteOns;
    }
    p.arpPattern.clear();

    // ARP locked to a playing DAW at 128 BPM (notes must land on the 1/16 grid)
    {
        FakePlayHead ph;
        transport = &ph;
        p.setPlayHead (&ph);
        resetAll (p);
        set (p, "arp_on", 1); set (p, "env1_dec", 0.1f); set (p, "env1_sus", 0.0f);
        render (p, "27_arp_host_128bpm", 2.0, chord (0.05, 1.8, { 60, 67 }));
        p.setPlayHead (nullptr);
        transport = nullptr;
    }

    // Aliasing check material: single high saw notes (analysed outside)
    resetAll (p);
    set (p, "f1_on", 0);
    render (p, "10_saw_C7", 1.0, chord (0.0, 0.9, { 96 }));
    set (p, pid::osc (0, "voices"), 7);
    render (p, "11_unison_C6", 1.0, chord (0.0, 0.9, { 84 }));

    // State save / restore round trip
    {
        resetAll (p);
        set (p, "f1_cutoff", 1234.0f);
        set (p, "mod3_src", SrcMacro2); set (p, "mod3_dst", dOsc (2, OPos)); set (p, "mod3_amt", -0.42f);
        p.arpPattern.steps[3].degree = 5; p.arpPattern.steps[3].lock = true;
        set (p, "fx_rev_on", 1); set (p, "an_age", 0.4f);
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        PyraAudioProcessor q;
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        const float c = q.apvts.getRawParameterValue ("f1_cutoff")->load();
        const float a = q.apvts.getRawParameterValue ("mod3_amt")->load();
        const bool ok = std::abs (c - 1234.0f) < 1.0f && std::abs (a + 0.42f) < 0.001f
                        && q.arpPattern.steps[3].degree.load() == 5 && q.arpPattern.steps[3].lock.load()
                        && q.apvts.getRawParameterValue ("fx_rev_on")->load() > 0.5f
                        && std::abs (q.apvts.getRawParameterValue ("an_age")->load() - 0.4f) < 0.001f;
        if (! ok) ++failures;
        std::printf ("  %-26s %s (cutoff %.1f, amount %.3f)\n", "09_state_roundtrip", ok ? "ok" : "FAIL", c, a);
    }

    // ---- Sampler
    {
        auto& bank = SampleBank::get();
        std::printf ("  samples folder: %s (%s)\n", bank.getFolder().getFullPathName().toRawUTF8(), bank.isInstalled() ? "found" : "MISSING");
        for (int k = 1; k < SampleBank::ids().size(); ++k)
        {
            resetAll (p);
            set (p, pid::osc (0, "on"), 0);
            set (p, "smp_on", 1); set (p, "smp_inst", (float) k);
            set (p, "env1_rel", 0.4f);
            bank.acquire (k);
            for (int w = 0; w < 3000 && bank.getStatus (k) != SampleBank::Ready && bank.getStatus (k) != SampleBank::Failed; ++w)
                juce::Thread::sleep (10);
            render (p, "50_sample_" + SampleBank::ids()[k], 4.0, chord (0.0, 3.0, { 48, 55, 60, 64 }));
        }
        // one long note to check the loops
        resetAll (p);
        set (p, pid::osc (0, "on"), 0); set (p, "smp_on", 1); set (p, "smp_inst", 3);
        render (p, "51_violins_long_A4", 9.0, chord (0.0, 8.5, { 69 }));
        set (p, "smp_inst", 1);
        render (p, "52_piano_A4", 4.0, chord (0.0, 3.5, { 69 }));
    }

    // ---- Phase 5: CHAOS / FOCUS / MOTION
    {
        resetAll (p);
        for (int o = 0; o < 3; ++o) { set (p, pid::osc (o, "on"), 1); set (p, pid::osc (o, "voices"), 5); set (p, pid::osc (o, "level"), 0.4f); }
        set (p, "f1_cutoff", 2000.0f);
        set (p, "chaos_amt", 1.0f); set (p, "chaos_bite", 1.0f); set (p, "chaos_space", 1.0f); set (p, "chaos_speed", 3.0f);
        render (p, "40_chaos_full", 4.0, chord (0.0, 3.5, { 48, 55, 60, 64 }));
        set (p, "chaos_amt", 0.0f);
        set (p, "focus_amt", 1.0f);
        render (p, "41_focus_full", 3.0, chord (0.0, 2.5, { 48, 55, 60, 64 }));
        set (p, "focus_amt", 0.0f);

        FakePlayHead ph; ph.bpm = 120.0;
        transport = &ph; p.setPlayHead (&ph);
        for (int sh = 0; sh < 9; ++sh)
        {
            set (p, "motion_amt", 1.0f); set (p, "motion_shape", (float) sh);
            render (p, "42_motion_" + choices::motionShapes()[sh].replace (" ", "").replace ("-", ""), 2.5, chord (0.0, 2.4, { 48, 60, 67 }));
        }
        p.setPlayHead (nullptr); transport = nullptr;
        set (p, "motion_amt", 0.0f);

        // every factory preset with all three macros at maximum must stay bounded
        auto& pm = p.getPresetManager();
        float worst = 0; juce::String worstName; int bad = 0;
        for (int k = 0; k < pm.getAll().size(); k += 1)
        {
            const auto& info = pm.getAll()[k];
            pm.load (info.id);
            set (p, "chaos_amt", 1.0f); set (p, "chaos_bite", 1.0f); set (p, "chaos_space", 1.0f);
            set (p, "focus_amt", 1.0f); set (p, "motion_amt", 1.0f);
            p.prepareToPlay (kSr, kBlock);
            juce::AudioBuffer<float> b (2, kBlock);
            float pk = 0; bool fin = true;
            for (int blk = 0; blk < (int) (1.5 * kSr / kBlock); ++blk)
            {
                juce::MidiBuffer m;
                if (blk == 0) for (int nn : { 48, 55, 60 }) m.addEvent (juce::MidiMessage::noteOn (1, nn, 0.8f), 0);
                if (blk == (int) (1.0 * kSr / kBlock)) for (int nn : { 48, 55, 60 }) m.addEvent (juce::MidiMessage::noteOff (1, nn), 0);
                p.processBlock (b, m);
                for (int c = 0; c < 2; ++c) for (int i = 0; i < kBlock; ++i) { const float v = b.getSample (c, i); if (! std::isfinite (v)) fin = false; pk = std::max (pk, std::abs (v)); }
            }
            if (! fin || pk > 4.0f) ++bad;
            if (pk > worst) { worst = pk; worstName = info.name; }
        }
        if (bad > 0) ++failures;
        std::printf ("  %-26s %s (loudest: %s, peak %.2f)\n", "43_all_presets_maxed", bad == 0 ? "ok" : "FAIL", worstName.toRawUTF8(), worst);
    }

    // ---- Preset system: factory load, save, rename keeps id, favourites, tags
    {
        PyraAudioProcessor q;
        auto& pm = q.getPresetManager();
        const int factoryCount = pm.getAll().size();
        bool ok = factoryCount >= 500;
        const auto& first = pm.getAll()[100];
        ok = ok && pm.load (first.id) && q.presetName == first.name && ! pm.isModified();
        set (q, "f1_cutoff", 777.0f);
        ok = ok && pm.isModified();
        const auto id = pm.saveUser ("Test Sound", "Bass", { "mytag" });
        ok = ok && id.startsWith ("user:") && pm.find (id) != nullptr;
        pm.setFavourite (id, true);
        pm.setPersonalTags (first.id, { "Marco" });
        ok = ok && pm.renameUser (id, "Paolo") && pm.find (id) != nullptr && pm.find (id)->name == "Paolo";
        pm.loadInit();
        ok = ok && pm.load (id) && std::abs (q.apvts.getRawParameterValue ("f1_cutoff")->load() - 777.0f) < 1.0f;
        ok = ok && pm.isFavourite (id) && pm.getPersonalTags (first.id).contains ("Marco");
        // a project saved with the old name still reports the current one
        juce::MemoryBlock mb; q.getStateInformation (mb);
        PyraAudioProcessor r2; r2.setStateInformation (mb.getData(), (int) mb.getSize());
        ok = ok && r2.getPresetManager().getCurrentName() == "Paolo";
        ok = ok && pm.deleteUser (id) && pm.find (id) == nullptr;
        pm.setPersonalTags (first.id, {});
        if (! ok) ++failures;
        std::printf ("  %-26s %s (%d presets)\n", "30_preset_system", ok ? "ok" : "FAIL", factoryCount);
    }

    // ---- Phase 6: GLITCH / GRANULAR / SHIFTER, MANGLE, CHORDS, RANDOM, UNDO, typed values
    {
        FakePlayHead ph; ph.bpm = 124.0;
        transport = &ph; p.setPlayHead (&ph);

        // every glitch effect on every step
        for (int fx = 1; fx < GCount; ++fx)
        {
            resetAll (p);
            set (p, "fx_glt_on", 1);
            for (int k = 0; k < kMaxGlitchSteps; ++k) p.glitchPattern.steps[(size_t) k].fx = (fx == GGate && (k & 1)) ? (int) GOff : fx;
            render (p, "60_glitch_" + choices::glitchEffects()[fx].replace (" ", ""), 2.0, chord (0.0, 1.9, { 48, 55, 60, 64 }));
        }
        p.glitchPattern.clear();
        resetAll (p);
        set (p, "fx_glt_on", 1); set (p, "fx_glt_chance", 1.0f); set (p, "fx_glt_rate", 16);
        render (p, "61_glitch_all_random", 3.0, chord (0.0, 2.9, { 48, 55, 60, 64 }));

        // granular
        resetAll (p);
        set (p, "fx_grn_on", 1); set (p, "fx_grn_mix", 1.0f);
        render (p, "62_granular_default", 2.5, chord (0.0, 2.0, { 48, 60, 67 }));
        set (p, "fx_grn_pitch", 12); set (p, "fx_grn_density", 60); set (p, "fx_grn_size", 500); set (p, "fx_grn_spray", 1.0f);
        render (p, "62_granular_dense_up", 2.5, chord (0.0, 2.0, { 48, 60, 67 }));
        set (p, "fx_grn_pitch", -24); set (p, "fx_grn_rev", 1.0f); set (p, "fx_grn_size", 10); set (p, "fx_grn_density", 1);
        render (p, "62_granular_sparse_rev", 2.5, chord (0.0, 2.0, { 48, 60, 67 }));
        set (p, "fx_grn_freeze", 1); set (p, "fx_grn_pitch", 0); set (p, "fx_grn_density", 20); set (p, "fx_grn_size", 120);
        set (p, "fx_grn_rev", 0.0f); set (p, "fx_grn_spray", 0.5f);
        render (p, "62_granular_freeze", 3.0, chord (0.0, 1.0, { 48, 60, 67 }));   // keeps sounding after the notes stop

        // shifter
        resetAll (p);
        set (p, "fx_shf_on", 1); set (p, "fx_shf_mix", 1.0f); set (p, "fx_shf_freq", 300.0f); set (p, "fx_shf_fb", 0.9f);
        render (p, "63_shifter_freq_fb", 3.0, chord (0.0, 2.5, { 48, 60 }));
        set (p, "fx_shf_mode", 1); set (p, "fx_shf_freq", -700.0f);
        render (p, "63_shifter_ring", 2.0, chord (0.0, 1.5, { 48, 60 }));

        // CHAOS at the top with MANGLE: must change the sound a lot and stay bounded
        auto renderBuf = [&] (juce::AudioBuffer<float>& outBuf, double seconds)
        {
            p.prepareToPlay (kSr, kBlock);
            const int total = (int) (seconds * kSr);
            outBuf.setSize (2, total); outBuf.clear();
            juce::AudioBuffer<float> b (2, kBlock);
            for (int pos = 0; pos < total; pos += kBlock)
            {
                juce::MidiBuffer m;
                if (pos == 0) for (int nn : { 48, 55, 60, 64 }) m.addEvent (juce::MidiMessage::noteOn (1, nn, 0.8f), 0);
                ph.ppq = pos / kSr * ph.bpm / 60.0;
                p.processBlock (b, m);
                for (int c = 0; c < 2; ++c) outBuf.copyFrom (c, pos, b, c, 0, std::min (kBlock, total - pos));
            }
        };
        resetAll (p);
        juce::AudioBuffer<float> calm, wild;
        renderBuf (calm, 3.0);
        set (p, "chaos_amt", 1.0f); set (p, "chaos_mangle", 1.0f); set (p, "chaos_seed", 7);
        renderBuf (wild, 3.0);
        double diff = 0, ref = 0; float pk = 0; bool fin = true;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < calm.getNumSamples(); ++i)
            {
                const float a = calm.getSample (c, i), w = wild.getSample (c, i);
                if (! std::isfinite (w)) fin = false;
                pk = std::max (pk, std::abs (w));
                diff += (a - w) * (a - w); ref += a * a;
            }
        const double ratio = std::sqrt (diff / std::max (1e-9, ref));
        const bool mangleOk = fin && pk < 1.01f && ratio > 0.6;
        if (! mangleOk) ++failures;
        std::printf ("  %-26s %s (difference to the calm sound: %.0f%%, peak %.2f)\n", "64_chaos_mangle_full", mangleOk ? "ok" : "FAIL", ratio * 100.0, pk);
        set (p, "chaos_mangle", 0.0f);
        renderBuf (wild, 3.0);
        diff = 0;
        for (int c = 0; c < 2; ++c) for (int i = 0; i < calm.getNumSamples(); ++i) { const float d = calm.getSample (c, i) - wild.getSample (c, i); diff += d * d; }
        std::printf ("     (same CHAOS with MANGLE at 0: difference %.0f%%)\n", std::sqrt (diff / std::max (1e-9, ref)) * 100.0);

        // CHORDS: theory
        {
            ChordPattern cp;
            ChordSettings cs; cs.key = 0; cs.scale = 1; cs.size = 0;
            const int degs[] = { 0, 4, 5, 3 };
            juce::StringArray names, romans;
            for (int k = 0; k < 4; ++k) { cp.slots[(size_t) k].degree = degs[k]; names.add (chordName (cp, k, cs)); romans.add (chordRoman (cp, k, cs)); }
            cs.scale = 2;   // C minor: i VI III VII
            cp.clear();
            juce::StringArray minorNames;
            for (int k = 0; k < 4; ++k) minorNames.add (chordName (cp, k, cs));
            cs.size = 1; cs.scale = 1;
            ChordPattern seventh; seventh.slots[0].degree = 4;
            const auto g7 = chordName (seventh, 0, cs);
            const bool ok = names.joinIntoString (" ") == "C G Am F" && romans.joinIntoString (" ") == "I V vi IV"
                            && minorNames.joinIntoString (" ") == "Cm Ab Eb Bb" && g7 == "G7";
            if (! ok) ++failures;
            std::printf ("  %-26s %s (%s | %s | %s | %s)\n", "65_chord_theory", ok ? "ok" : "FAIL", names.joinIntoString (" ").toRawUTF8(),
                         romans.joinIntoString (" ").toRawUTF8(), minorNames.joinIntoString (" ").toRawUTF8(), g7.toRawUTF8());

            // random progressions are always made of valid chords
            int badRandom = 0;
            for (uint32_t sd = 1; sd < 300; ++sd)
            {
                ChordPattern rp;
                rp.randomize (1 + (int) (sd % 8), (int) (sd % 20), (float) (sd % 10) / 9.0f, 0, sd);
                ChordSettings rs; rs.scale = (int) (sd % 20); rs.key = (int) (sd % 12); rs.size = (int) (sd % 6); rs.voicing = (int) (sd % 4);
                rs.inversion = (int) (sd % 4); rs.count = 1 + (int) (sd % 8);
                std::vector<int> prev;
                for (int k = 0; k < rs.count; ++k)
                {
                    auto n = chordNotes (rp, k, rs, prev);
                    if (n.size() < 2 || n.front() < 20 || n.back() > 110) ++badRandom;
                    prev = n;
                }
            }
            if (badRandom > 0) ++failures;
            std::printf ("  %-26s %s (300 random progressions, %d bad chords)\n", "65_chord_random", badRandom == 0 ? "ok" : "FAIL", badRandom);
        }

        // CHORDS: progression with the DAW playing (one chord per bar at 124 BPM), counted at the MIDI level
        {
            resetAll (p);
            p.chordPattern.clear();
            set (p, "chord_on", 1); set (p, "chord_count", 4); set (p, "chord_len", 2);
            set (p, "env1_rel", 0.05f);
            int noteOns = 0, firstBarOns = 0;
            p.prepareToPlay (kSr, kBlock);
            juce::AudioBuffer<float> b (2, kBlock);
            const double barSec = 4.0 * 60.0 / ph.bpm;
            const int total = (int) (barSec * 4.0 * kSr);
            std::set<int> chordStarts;
            for (int pos = 0; pos < total; pos += kBlock)
            {
                juce::MidiBuffer m;
                if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
                ph.ppq = pos / kSr * ph.bpm / 60.0;
                juce::MidiBuffer copy (m);
                p.chords.process (copy, kBlock, HostInfo { ph.bpm, ph.ppq, true, true });
                for (const auto meta : copy)
                    if (meta.getMessage().isNoteOn())
                    {
                        ++noteOns;
                        if (pos + meta.samplePosition < (int) (barSec * kSr)) ++firstBarOns;
                        chordStarts.insert ((int) std::lround ((pos + meta.samplePosition) / (barSec * kSr) * 4.0));
                    }
            }
            const bool ok = noteOns >= 4 * 4 && firstBarOns >= 4 && firstBarOns <= 6 && chordStarts.size() >= 4;
            if (! ok) ++failures;
            std::printf ("  %-26s %s (%d note-ons in 4 bars, %d in the first bar)\n", "66_chords_progression", ok ? "ok" : "FAIL", noteOns, firstBarOns);
            p.chords.prepare (kSr);
            render (p, "66_chords_audio", 4.0, chord (0.0, 3.9, { 60 }));
            set (p, "chord_rhythm", 6); set (p, "chord_strum", 0.5f); set (p, "chord_human", 1.0f); set (p, "chord_size", 5);
            render (p, "66_chords_stabs_strum", 4.0, chord (0.0, 3.9, { 48 }));
            set (p, "chord_rhythm", 0); set (p, "chord_strum", 0.0f); set (p, "chord_human", 0.1f);
            set (p, "arp_on", 1); set (p, "arp_mode", 2);
            render (p, "67_chords_into_arp", 4.0, chord (0.0, 3.9, { 60 }));
            set (p, "arp_on", 0);
            set (p, "chord_mode", 1); set (p, "chord_scale", 1);
            render (p, "68_chord_per_key", 3.0, { { 0.0, 0.8, 60, 0.8f }, { 0.9, 0.8, 62, 0.8f }, { 1.8, 0.8, 67, 0.8f } });
            set (p, "chord_on", 0);
        }
        p.setPlayHead (nullptr); transport = nullptr;

        // RANDOM on every page: always playable, never silent, never deafening
        {
            int bad = 0;
            float loudest = 0;
            for (int page = 0; page <= (int) randomizer::Settings; ++page)
                for (uint32_t sd = 1; sd <= 6; ++sd)
                {
                    resetAll (p);
                    randomizer::randomize (p, (randomizer::Page) page, false, sd * 977u + (uint32_t) page);
                    if (page == randomizer::Chords) set (p, "chord_on", 1);
                    if (page == randomizer::Arp) set (p, "arp_on", 1);
                    randomizer::randomize (p, (randomizer::Page) page, true, sd);
                    p.prepareToPlay (kSr, kBlock);
                    juce::AudioBuffer<float> b (2, kBlock);
                    float pk = 0; bool fin = true;
                    for (int blk = 0; blk < (int) (2.0 * kSr / kBlock); ++blk)
                    {
                        juce::MidiBuffer m;
                        if (blk == 0) for (int nn : { 48, 55, 60, 64 }) m.addEvent (juce::MidiMessage::noteOn (1, nn, 0.8f), 0);
                        if (blk == (int) (1.5 * kSr / kBlock)) for (int nn : { 48, 55, 60, 64 }) m.addEvent (juce::MidiMessage::noteOff (1, nn), 0);
                        p.processBlock (b, m);
                        for (int c = 0; c < 2; ++c) for (int i = 0; i < kBlock; ++i) { const float v = b.getSample (c, i); if (! std::isfinite (v)) fin = false; pk = std::max (pk, std::abs (v)); }
                    }
                    loudest = std::max (loudest, pk);
                    if (! fin || pk < 0.003f || pk > 1.01f) { ++bad; std::printf ("     random page %d seed %u: peak %.3f\n", page, sd, pk); }
                }
            set (p, "chord_on", 0); set (p, "arp_on", 0);
            if (bad > 0) ++failures;
            std::printf ("  %-26s %s (8 pages x 6 seeds, loudest %.2f)\n", "69_random_every_page", bad == 0 ? "ok" : "FAIL", loudest);
        }

        // section dice: only that section changes
        {
            const char* secs[] = { "osc0", "osc1", "osc2", "subnoise", "filter0", "filter1", "env1", "env2", "lfo1", "lfo4", "voice", "macro",
                                   "fx:dist", "fx:res", "fx:flt", "fx:shf", "fx:grn", "fx:cho", "fx:fla", "fx:pha", "fx:tape", "fx:dly",
                                   "fx:rev", "fx:glt", "fx:eq", "fx:comp", "chaos", "focus", "motion", "age", "movement", "circuit",
                                   "noise", "converter", "mod1", "mod16", "mpe" };
            int bad = 0;
            for (auto* sec : secs)
            {
                resetAll (p);
                std::vector<float> before;
                for (auto* ap : p.getParameters()) before.push_back (ap->getValue());
                randomizer::randomizeSection (p, sec, false, 1234u);
                randomizer::randomizeSection (p, sec, true, 99u);
                int changed = 0, outside = 0;
                juce::String outsideId;
                const juce::String sc (sec);
                for (int i = 0; i < p.getParameters().size(); ++i)
                {
                    auto* q = dynamic_cast<juce::RangedAudioParameter*> (p.getParameters()[i]);
                    if (std::abs (q->getValue() - before[(size_t) i]) < 1.0e-6f) continue;
                    ++changed;
                    const auto id = q->getParameterID();
                    bool mine = false;
                    if (sc.startsWith ("fx:")) mine = id.startsWith ("fx_" + sc.substring (3) + "_");
                    else if (sc.startsWith ("osc")) mine = id.startsWith (pid::osc (sc.getTrailingIntValue(), ""));
                    else if (sc.startsWith ("filter")) mine = id.startsWith (pid::filt (sc.getTrailingIntValue(), "")) || id == "filterRouting";
                    else if (sc.startsWith ("env")) mine = id.startsWith (pid::env (sc.getTrailingIntValue() - 1, ""));
                    else if (sc.startsWith ("lfo")) mine = id.startsWith (pid::lfo (sc.getTrailingIntValue() - 1, ""));
                    else if (sc.startsWith ("mod")) mine = id.startsWith ("mod" + juce::String (sc.getTrailingIntValue()) + "_");
                    else if (sc == "subnoise") mine = id.startsWith ("sub_") || id.startsWith ("noise_");
                    else if (sc == "voice") mine = id == "voiceMode" || id.startsWith ("glide") || id.startsWith ("bend") || id == "velAmp";
                    else if (sc == "macro") mine = id.startsWith ("macro");
                    else if (sc == "chaos") mine = id.startsWith ("chaos_");
                    else if (sc == "focus") mine = id.startsWith ("focus");
                    else if (sc == "motion") mine = id.startsWith ("motion_");
                    else if (sc == "mpe") mine = id == "mpeRange";
                    else mine = id.startsWith ("an_") || id == "drift" || id == "spread";
                    if (! mine) { ++outside; outsideId = id; }
                }
                if (changed == 0 || outside > 0) { ++bad; std::printf ("     dice %s: %d changed, %d outside (%s)\n", sec, changed, outside, outsideId.toRawUTF8()); }
            }
            p.glitchPattern.clear();
            if (bad > 0) ++failures;
            std::printf ("  %-26s %s (%d sections)\n", "69_section_dice", bad == 0 ? "ok" : "FAIL", (int) std::size (secs));
        }

        // UNDO / REDO
        {
            PyraAudioProcessor q;
            auto& h = q.history;
            h.tick();
            h.noteUserActivity();
            set (q, "f1_cutoff", 500.0f);
            h.tick(); juce::Thread::sleep (420); h.tick();
            h.noteUserActivity();
            set (q, "f1_cutoff", 900.0f); set (q, "f1_res", 0.8f);
            q.glitchPattern.steps[2].fx = GReverse; ++q.glitchPattern.changeCounter;
            h.tick(); juce::Thread::sleep (420); h.tick();
            auto cut = [&] { return q.apvts.getRawParameterValue ("f1_cutoff")->load(); };
            const float c2 = cut();
            h.undo();
            const float c1 = cut(); const bool glitchUndone = q.glitchPattern.steps[2].fx.load() == GOff;
            h.undo();
            const float c0 = cut();
            h.redo(); h.redo();
            const float c3 = cut();
            const bool ok = std::abs (c2 - 900.0f) < 1 && std::abs (c1 - 500.0f) < 1 && std::abs (c0 - 20000.0f) < 1 && std::abs (c3 - 900.0f) < 1
                            && glitchUndone && q.glitchPattern.steps[2].fx.load() == GReverse;
            if (! ok) ++failures;
            std::printf ("  %-26s %s (%.0f -> undo %.0f -> undo %.0f -> redo x2 %.0f)\n", "70_undo_redo", ok ? "ok" : "FAIL", c2, c1, c0, c3);
        }

        // typed values
        {
            auto parse = [&] (const char* id, const char* text)
            {
                bool okp = false;
                const float v = parseTypedValue (*p.apvts.getParameter (id), text, okp);
                return okp ? v : -99999.0f;
            };
            const bool ok = std::abs (parse ("f1_cutoff", "2.5k") - 2500.0f) < 0.5f
                            && std::abs (parse ("env1_att", "120 ms") - 0.12f) < 0.0001f
                            && std::abs (parse ("env1_att", "1.5 s") - 1.5f) < 0.0001f
                            && std::abs (parse ("oscA_level", "75") - 0.75f) < 0.0001f
                            && std::abs (parse ("oscA_pan", "-50%") + 0.5f) < 0.0001f
                            && std::abs (parse ("master", "-6 dB") + 6.0f) < 0.0001f
                            && std::abs (parse ("f1_cutoff", "99999") - 20000.0f) < 0.5f
                            && parse ("f1_cutoff", "abc") < -9999.0f
                            && std::abs (parse ("oscA_semi", "7") - 7.0f) < 0.0001f;
            auto* lvl = p.apvts.getParameter ("oscA_level");
            lvl->setValueNotifyingHost (lvl->convertTo0to1 (0.75f));
            auto* cf = p.apvts.getParameter ("f1_cutoff");
            cf->setValueNotifyingHost (cf->convertTo0to1 (2500.0f));
            const auto t1 = formatParamValue (*lvl), t2 = formatParamValue (*cf);
            const bool ok2 = t1 == "75%" && t2 == "2.50 kHz";
            if (! ok || ! ok2) ++failures;
            std::printf ("  %-26s %s (shows %s, %s)\n", "71_typed_values", ok && ok2 ? "ok" : "FAIL", t1.toRawUTF8(), t2.toRawUTF8());
        }

        // state + presets keep the chord and glitch patterns
        {
            resetAll (p);
            p.chordPattern.slots[2].degree = 4; p.chordPattern.slots[2].quality = QDom7; p.chordPattern.slots[1].lock = true;
            p.glitchPattern.steps[5].fx = GTapeStop;
            juce::MemoryBlock mb; p.getStateInformation (mb);
            PyraAudioProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
            bool ok = q.chordPattern.slots[2].degree.load() == 4 && q.chordPattern.slots[2].quality.load() == QDom7
                      && q.chordPattern.slots[1].lock.load() && q.glitchPattern.steps[5].fx.load() == GTapeStop;
            auto data = presetdata::capture (p);
            q.chordPattern.clear(); q.glitchPattern.clear();
            presetdata::apply (q, data);
            ok = ok && q.chordPattern.slots[2].quality.load() == QDom7 && q.glitchPattern.steps[5].fx.load() == GTapeStop;
            presetdata::apply (q, juce::var (new juce::DynamicObject()));
            ok = ok && q.chordPattern.isDefault() && q.glitchPattern.isEmpty();
            if (! ok) ++failures;
            std::printf ("  %-26s %s\n", "72_patterns_saved", ok ? "ok" : "FAIL");
            p.chordPattern.clear(); p.glitchPattern.clear();
        }

        // CHORDS emotions: every mood makes valid chords in each of its scales; RANDOM moves the scale into the mood
        {
            resetAll (p);
            bool ok = true;
            juce::String sample;
            for (int mood = 1; mood < chordMoodNames().size(); ++mood)
            {
                const auto& st = chordMoodStyle (mood);
                if (st.progressions.empty() || st.scales.empty()) { ok = false; std::printf ("    mood %d has no style\n", mood); continue; }
                for (int scale : st.scales)
                    for (uint32_t seed = 1; seed < 12; ++seed)
                    {
                        ChordPattern pat;
                        pat.randomizeMood (mood, seed % 2 ? 4 : 8, scale, 0.4f, seed * 77u);
                        ChordSettings cs; cs.scale = scale; cs.key = 0; cs.count = seed % 2 ? 4 : 8;
                        std::vector<int> prev;
                        for (int k = 0; k < cs.count; ++k)
                        {
                            auto notes = chordNotes (pat, k, cs, prev);
                            if (notes.size() < 2) { ok = false; std::printf ("    mood %d scale %d slot %d: %d notes\n", mood, scale, k, (int) notes.size()); }
                            prev = notes;
                        }
                    }
                ChordPattern pat;
                pat.randomizeMood (mood, 4, st.scales[0], 0.0f, 5u);
                ChordSettings cs; cs.scale = st.scales[0]; cs.key = 9;
                sample << "    " << chordMoodNames()[mood] << ": ";
                for (int k = 0; k < 4; ++k) sample << chordName (pat, k, cs) << " ";
                sample << "\n";
            }
            // angry = power chords somewhere; RANDOM puts the scale inside the mood
            {
                ChordPattern pat; int power = 0;
                for (uint32_t seed = 1; seed < 20; ++seed)
                {
                    pat.randomizeMood (18, 4, 6, 0.1f, seed);
                    for (int k = 0; k < 4; ++k) power += pat.slots[(size_t) k].quality.load() == QPower ? 1 : 0;
                }
                ok = ok && power > 0;
            }
            set (p, "chord_mood", 4.0f); set (p, "chord_scale", 1.0f);   // Sad in Major -> RANDOM picks a minor-ish scale
            randomizer::randomize (p, randomizer::Chords, false, 99u);
            const int sc = (int) p.apvts.getRawParameterValue ("chord_scale")->load();
            const auto& sadScales = chordMoodStyle (4).scales;
            ok = ok && std::find (sadScales.begin(), sadScales.end(), sc) != sadScales.end();
            randomizer::randomizeChordsAll (p, 1234u);
            if (! ok) ++failures;
            std::printf ("  %-26s %s\n%s", "73_chord_emotions", ok ? "ok" : "FAIL", sample.toRawUTF8());
            p.chordPattern.clear();
        }

        // LAYERS: extra sounds play, zones split the keyboard, mute / solo, transpose with clean note-offs, state and presets
        {
            PyraAudioProcessor m;
            resetAll (m);
            auto& b = m.getOrCreateLayer (1);
            resetAll (b);
            set (b, pid::osc (0, "wt"), 1);
            auto peaks = [&] (std::initializer_list<int> notes, double secs = 0.6)
            {
                for (auto& pk : m.layerPeak) pk.store (0.0f);
                std::vector<Note> v;
                for (int n : notes) v.push_back ({ 0.0, secs * 0.6, n, 0.8f });
                auto r = render (m, "74_layers_tmp", secs, v);
                return std::make_tuple (m.layerPeak[0].load(), m.layerPeak[1].load(), r.rms);
            };
            auto [a0, b0, r0] = peaks ({ 60 });
            set (m, layerParam (1, "on"), 1);
            auto [a1, b1, r1] = peaks ({ 60 });
            bool ok = b0 == 0.0f && b1 > 0.01f && a1 > 0.01f && a0 > 0.01f;
            // split: A below C4, B from C4 up
            set (m, layerParam (0, "hi"), 59); set (m, layerParam (1, "lo"), 60);
            auto [a2, b2, r2] = peaks ({ 48 });
            auto [a3, b3, r3] = peaks ({ 72 });
            ok = ok && a2 > 0.01f && b2 < 1.0e-4f && a3 < 1.0e-4f && b3 > 0.01f;
            set (m, layerParam (0, "hi"), 127); set (m, layerParam (1, "lo"), 0);
            // solo B: A goes quiet; mute B: B goes quiet
            set (m, layerParam (1, "solo"), 1);
            auto [a4, b4, r4] = peaks ({ 60 });
            set (m, layerParam (1, "solo"), 0); set (m, layerParam (1, "mute"), 1);
            auto [a5, b5, r5] = peaks ({ 60 });
            ok = ok && a4 < 1.0e-4f && b4 > 0.01f && b5 < 1.0e-4f && a5 > 0.01f;
            set (m, layerParam (1, "mute"), 0);
            if (! ok) ++failures;
            std::printf ("  %-26s %s (B off %.3f on %.3f | split %.3f/%.3f %.3f/%.3f | solo %.3f/%.3f mute %.3f/%.3f)\n", "74_layers_play",
                         ok ? "ok" : "FAIL", b0, b1, a2, b2, a3, b3, a4, b4, a5, b5);

            // transpose changed while a key is held: the note still stops
            {
                set (m, "env1_rel", 0.05f); set (b, "env1_rel", 0.05f);
                set (m, layerParam (1, "trans"), 12);
                m.prepareToPlay (kSr, kBlock);
                juce::AudioBuffer<float> buf (2, kBlock);
                juce::MidiBuffer mid;
                mid.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
                m.processBlock (buf, mid); mid.clear();
                const int during = b.engine.uiVoices.load();
                set (m, layerParam (1, "trans"), -7);
                for (int i = 0; i < 20; ++i) m.processBlock (buf, mid);
                mid.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
                m.processBlock (buf, mid); mid.clear();
                for (int i = 0; i < 200; ++i) m.processBlock (buf, mid);
                const int after = b.engine.uiVoices.load();
                const bool ok2 = during > 0 && after == 0;
                if (! ok2) ++failures;
                std::printf ("  %-26s %s (voices %d -> %d)\n", "75_layers_noteoff", ok2 ? "ok" : "FAIL", during, after);
                set (m, layerParam (1, "trans"), 0);
            }

            // state: the layers come back with the project; presets never carry the layer mix
            {
                auto& pm = b.getPresetManager();
                pm.load (pm.getAll()[42].id);
                const auto name = b.presetName;
                set (m, layerParam (1, "level"), -9.0f); set (m, layerParam (3, "on"), 1);
                m.getOrCreateLayer (3).getPresetManager().load (pm.getAll()[100].id);
                juce::MemoryBlock mb; m.getStateInformation (mb);
                PyraAudioProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
                auto* qb = q.getLayer (1);
                auto* qd = q.getLayer (3);
                bool ok3 = qb != nullptr && qd != nullptr && qb->presetName == name
                           && std::abs (q.apvts.getRawParameterValue (layerParam (1, "level"))->load() + 9.0f) < 0.05f
                           && qb->apvts.getRawParameterValue (pid::osc (0, "wt"))->load() == b.apvts.getRawParameterValue (pid::osc (0, "wt"))->load();
                auto data = presetdata::capture (m);
                if (auto* obj = data["params"].getDynamicObject())
                    for (auto& prop : obj->getProperties())
                        if (prop.name.toString().startsWith ("layer")) ok3 = false;
                // loading a preset into A keeps the layer mix
                m.getPresetManager().load (pm.getAll()[7].id);
                ok3 = ok3 && std::abs (m.apvts.getRawParameterValue (layerParam (1, "level"))->load() + 9.0f) < 0.05f;
                // copy a layer's sound
                m.copyLayerSound (1, 2);
                ok3 = ok3 && m.getLayer (2) != nullptr && m.getLayer (2)->presetName == name;
                // RANDOM on LAYERS makes a multi
                randomizer::randomizeLayers (m, false, 77u);
                int onCount = 0;
                for (int k = 0; k < kNumLayers; ++k) onCount += m.apvts.getRawParameterValue (layerParam (k, "on"))->load() > 0.5f ? 1 : 0;
                ok3 = ok3 && onCount >= 2;
                auto rr = render (m, "76_layers_random_multi", 2.0, chord (0.0, 1.5, { 48, 60, 64, 67 }));
                ok3 = ok3 && rr.peak > 0.001f;
                if (! ok3) ++failures;
                std::printf ("  %-26s %s (%d layers on after RANDOM)\n", "76_layers_state", ok3 ? "ok" : "FAIL", onCount);
            }
        }

        // the layers follow the CHORDS (and ARP) of the main sound
        {
            PyraAudioProcessor m;
            resetAll (m);
            auto& b = m.getOrCreateLayer (1);
            resetAll (b);
            set (m, layerParam (1, "on"), 1);
            set (m, "chord_on", 1);
            m.prepareToPlay (kSr, kBlock);
            juce::AudioBuffer<float> buf (2, kBlock);
            juce::MidiBuffer mid;
            mid.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            int maxB = 0;
            for (int i = 0; i < 200; ++i) { m.processBlock (buf, mid); mid.clear(); maxB = std::max (maxB, b.engine.uiVoices.load()); }
            const bool okc = maxB >= 3;
            set (m, "chord_on", 0); set (m, "arp_on", 1);
            mid.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            for (int i = 0; i < 50; ++i) { m.processBlock (buf, mid); mid.clear(); }
            mid.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            mid.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 0);
            mid.addEvent (juce::MidiMessage::noteOn (1, 67, 0.8f), 0);
            int changes = 0, last = -1;
            for (int i = 0; i < 400; ++i)
            {
                m.processBlock (buf, mid); mid.clear();
                const int v = b.engine.uiVoices.load();
                if (v != last) { ++changes; last = v; }
            }
            const bool oka = changes >= 4 && b.apvts.getRawParameterValue ("arp_on")->load() < 0.5f;
            if (! (okc && oka)) ++failures;
            std::printf ("  %-26s %s (layer B voices with CHORDS: %d, ARP changes: %d)\n", "77_layers_follow", okc && oka ? "ok" : "FAIL", maxB, changes);
        }

        // every wavetable: finite, loud enough, never clipping
        {
            const auto& bank = Wavetables::get();
            bool okw = bank.size() == Wavetables::names().size() && Wavetables::categoryOfEach().size() == bank.size();
            int bad = 0;
            for (int t = 0; t < bank.size(); ++t)
            {
                const auto& tab = bank.table (t);
                for (int f = 0; f < tab.numFrames; ++f)
                {
                    const float* d = tab.level (f, 0);
                    float pk = 0.0f;
                    for (int i = 0; i < Wavetables::kBaseLen; ++i) { if (! std::isfinite (d[i])) pk = 99.0f; pk = std::max (pk, std::abs (d[i])); }
                    if (pk < 0.3f || pk > 1.01f) { ++bad; if (bad < 6) std::printf ("    table %s frame %d peak %.3f\n", tab.name.toRawUTF8(), f, pk); }
                }
                if (! Wavetables::categories().contains (Wavetables::categoryOfEach()[t])) okw = false;
            }
            okw = okw && bad == 0;
            if (! okw) ++failures;
            std::printf ("  %-26s %s (%d tables)\n", "78_wavetables", okw ? "ok" : "FAIL", bank.size());
        }

        // new sub shapes, noise types and LFO shapes all make sound
        {
            for (int sh = 0; sh < choices::subShapes().size(); ++sh)
            {
                resetAll (p);
                set (p, pid::osc (0, "on"), 0); set (p, "sub_on", 1); set (p, "sub_shape", (float) sh); set (p, "sub_level", 0.8f);
                render (p, "79_sub_" + juce::String (sh).paddedLeft ('0', 2) + "_" + choices::subShapes()[sh].removeCharacters (" %+-"), 0.4,
                        chord (0.0, 0.3, { 48 }));
            }
            for (int nt = 0; nt < choices::noiseTypes().size(); ++nt)
            {
                resetAll (p);
                set (p, pid::osc (0, "on"), 0); set (p, "noise_on", 1); set (p, "noise_type", (float) nt); set (p, "noise_level", 0.8f);
                render (p, "80_noise_" + juce::String (nt).paddedLeft ('0', 2) + "_" + choices::noiseTypes()[nt].removeCharacters (" -"), 1.5,
                        chord (0.0, 1.4, { 60 }));
            }
            for (int ls = 0; ls < choices::lfoShapes().size(); ++ls)
            {
                resetAll (p);
                set (p, "f1_cutoff", 800); set (p, "mod1_src", SrcLfo1); set (p, "mod1_dst", dFilt (0, FCutoff)); set (p, "mod1_amt", 0.4f);
                set (p, "lfo1_shape", (float) ls); set (p, "lfo1_sync", 0); set (p, "lfo1_rate", 3.0f);
                render (p, "81_lfo_" + juce::String (ls).paddedLeft ('0', 2) + "_" + choices::lfoShapes()[ls].removeCharacters (" %+-/()&"), 0.8,
                        chord (0.0, 0.7, { 48 }));
            }
        }
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
