// Offline test: renders notes through PYRA and checks the audio (no host needed).
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "dsp/Wavetables.h"
#include "Lang.h"
#include "Presets.h"

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
            if (b->getButtonText() != "EN" && b->getButtonText() != "IT") tabs.add (b);

    for (int i = 0; i < tabs.size(); ++i)
    {
        if (tabs[i]->onClick) tabs[i]->onClick();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = juce::File::getCurrentWorkingDirectory().getChildFile ("ui").getChildFile (
            "page" + juce::String (i) + "_" + tabs[i]->getButtonText().removeCharacters (" ") + suffix + ".png");
        f.getParentDirectory().createDirectory();
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
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
    std::map<juce::String, std::vector<float>> catLevels;

    for (int idx = 0; idx < arr->size(); ++idx)
    {
        auto& preset = arr->getReference (idx);
        const auto cat = preset["category"].toString();
        presetdata::apply (p, preset);

        // test performance by category
        std::vector<Note> notes; double len = 2.5;
        if (cat == "Bass")                         notes = chord (0.0, 1.5, { 36 });
        else if (cat == "Lead")                    notes = chord (0.0, 1.5, { 60 });
        else if (cat == "Drums")                   { notes = chord (0.0, 0.15, { 48 }); len = 1.5; }
        else if (cat == "FX")                      { notes = chord (0.0, 4.0, { 60 }); len = 6.0; }
        else if (cat == "Arp" || cat == "Sequence") { notes = chord (0.0, 3.0, { 48, 55, 60, 63 }); len = 3.5; }
        else if (cat == "Pad" || cat == "Strings") { notes = chord (0.0, 3.0, { 48, 55, 60, 63 }); len = 4.5; }
        else if (cat == "Chords")                  notes = chord (0.0, 1.5, { 48 });
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
    report.add (juce::String (problems) + " presets with problems");
    juce::File::getCurrentWorkingDirectory().getChildFile ("normalize_report.txt").replaceWithText (report.joinIntoString ("\n"));
    std::printf ("normalised %d presets, %d with problems (see normalize_report.txt)\n", arr->size(), problems);
    return problems == 0 ? 0 : 2;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 3 && juce::String (argv[1]) == "--normalize")
    {
        juce::File audition;
        if (argc > 4) { audition = juce::File (argv[4]); audition.createDirectory(); }
        return normalizeBank (juce::File (argv[2]), juce::File (argv[3]), audition);
    }
    if (argc > 1 && juce::String (argv[1]) == "--ui")
    {
        snapshots (pyra::lang::English, "_en");
        snapshots (pyra::lang::Italian, "_it");
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
        const char* mods[] = { "dist", "res", "flt", "cho", "fla", "pha", "tape", "dly", "rev", "eq", "comp" };
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
        std::printf ("     (all 11 effects: %.1f%% CPU)\n", r2.cpuRatio * 100.0);
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

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
