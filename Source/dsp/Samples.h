#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <vector>

namespace pyra
{
/** One recorded note of a sampled instrument. Audio is kept as 16-bit stereo in memory. */
struct SampleZone
{
    int root = 60, lo = 0, hi = 127, velLo = 0, velHi = 127;
    float tune = 0;                   // cents
    int loopStart = 0, loopEnd = 0;   // loopEnd > loopStart => sustained loop
    std::vector<int16_t> data;        // interleaved L/R
    int length = 0;                   // frames

    inline float frameL (int i) const noexcept { return data[(size_t) i * 2] * (1.0f / 32768.0f); }
    inline float frameR (int i) const noexcept { return data[(size_t) i * 2 + 1] * (1.0f / 32768.0f); }
};

struct SampleInstrument
{
    juce::String id, name;
    double sampleRate = 44100.0;
    bool sustained = false;
    std::vector<SampleZone> zones;

    const SampleZone* find (int note, int velocity) const noexcept;
};

/** Shared, lazily loaded sample library. Instruments load on a background thread the first time a
    voice asks for them; the audio thread never waits and never allocates. */
class SampleBank : private juce::Thread
{
public:
    static SampleBank& get();

    /** Instrument ids, in parameter order (index 0 = "Off"). Append-only. */
    static const juce::StringArray& ids();
    static const juce::StringArray& displayNames();

    /** Audio thread: returns the instrument if loaded, otherwise asks for it and returns nullptr. */
    const SampleInstrument* acquire (int index) noexcept;

    enum Status { NotFound, Idle, Loading, Ready, Failed };
    Status getStatus (int index) const noexcept;
    juce::File getFolder() const { const juce::ScopedLock sl (folderLock); return folder; }
    bool isInstalled() const { return getFolder().isDirectory(); }

    ~SampleBank() override;

private:
    SampleBank();
    void run() override;
    std::unique_ptr<SampleInstrument> load (int index);
    static juce::File locateFolder();

    juce::File folder;
    juce::CriticalSection folderLock;
    static constexpr int kMax = 64;
    std::atomic<const SampleInstrument*> loaded[kMax] {};
    std::atomic<int> status[kMax] {};
    std::atomic<bool> requested[kMax] {};
    std::vector<std::unique_ptr<SampleInstrument>> owned;
    juce::WaitableEvent wake;
};

} // namespace pyra
