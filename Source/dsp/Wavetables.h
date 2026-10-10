#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace pyra
{
/** Built-in wavetable bank.

    Every table holds 1..16 frames. Each frame is stored as a set of band-limited
    copies ("mip levels"): level k keeps at most 1024 >> k harmonics, so the
    oscillator can pick the richest copy that does not alias at the played pitch.
*/
class Wavetables
{
public:
    static constexpr int kBaseLen = 2048;
    static constexpr int kNumMips = 11;

    struct Table
    {
        juce::String name;
        int numFrames = 1;
        std::vector<float> data;   // frame-major, all mip levels, each with 1 guard sample

        const float* level (int frame, int mip) const noexcept
        {
            return data.data() + (size_t) frame * frameStride() + (size_t) mipOffset (mip);
        }
    };

    static const juce::StringArray& names();

    /** Lazily-built shared bank (built once, the first time it is needed). */
    static const Wavetables& get();

    const Table& table (int index) const noexcept
    {
        return tables[(size_t) juce::jlimit (0, (int) tables.size() - 1, index)];
    }

    int size() const noexcept { return (int) tables.size(); }

    static int mipLength (int mip) noexcept
    {
        if (mip <= 1) return kBaseLen;
        return juce::jmax (256, kBaseLen >> (mip - 1));
    }

    static int mipOffset (int mip) noexcept
    {
        int off = 0;
        for (int m = 0; m < mip; ++m) off += mipLength (m) + 1;
        return off;
    }

    static int frameStride() noexcept { return mipOffset (kNumMips); }

    /** Picks the band-limited copy for a fundamental of `hz` at sample rate `sr`. */
    static int mipFor (float hz, float sr) noexcept
    {
        const float allowed = 0.5f * sr / juce::jmax (1.0f, hz * 1.05f);
        if (allowed >= 1024.0f) return 0;
        int k = 0;
        while (k < kNumMips - 1 && (1024 >> k) > allowed) ++k;
        return k;
    }

    /** Reads a frame-interpolated, sample-interpolated value. phase in [0,1). */
    static float read (const Table& t, float pos, int mip, float phase) noexcept
    {
        const float fpos = pos * (float) (t.numFrames - 1);
        const int f0 = (int) fpos;
        const int f1 = juce::jmin (f0 + 1, t.numFrames - 1);
        const float ff = fpos - (float) f0;

        const int len = mipLength (mip);
        const float idx = phase * (float) len;
        int i0 = (int) idx;
        if (i0 >= len) i0 = len - 1;
        const float fr = idx - (float) i0;

        const float* a = t.level (f0, mip);
        const float va = a[i0] + fr * (a[i0 + 1] - a[i0]);
        if (f1 == f0 || ff <= 0.0f) return va;
        const float* b = t.level (f1, mip);
        const float vb = b[i0] + fr * (b[i0 + 1] - b[i0]);
        return va + ff * (vb - va);
    }

private:
    Wavetables();
    std::vector<Table> tables;
};

} // namespace pyra
