#include "HellArt.h"
#include "PyraLookAndFeel.h"
#include "../dsp/DspUtil.h"
#include <map>
#include "BinaryData.h"

namespace pyra
{
namespace art
{
namespace
{
    //==========================================================================
    // value noise
    float hash2 (int x, int y, uint32_t seed)
    {
        uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u + seed * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float) ((h ^ (h >> 16)) & 0xffffff) / 16777215.0f;
    }

    float smooth (float t) { return t * t * (3.0f - 2.0f * t); }

    float valueNoise (float x, float y, uint32_t seed)
    {
        const int xi = (int) std::floor (x), yi = (int) std::floor (y);
        const float fx = smooth (x - (float) xi), fy = smooth (y - (float) yi);
        const float a = hash2 (xi, yi, seed), b = hash2 (xi + 1, yi, seed);
        const float c = hash2 (xi, yi + 1, seed), d = hash2 (xi + 1, yi + 1, seed);
        return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
    }

    [[maybe_unused]] float fbm (float x, float y, uint32_t seed, int octaves)
    {
        float sum = 0, amp = 0.5f, norm = 0;
        for (int o = 0; o < octaves; ++o)
        {
            sum += valueNoise (x, y, seed + (uint32_t) o * 101u) * amp;
            norm += amp;
            x *= 2.03f; y *= 2.03f; amp *= 0.5f;
        }
        return sum / norm;
    }

    [[maybe_unused]] juce::Colour lavaRamp (float t)
    {
        // black crust -> deep red -> orange -> yellow -> almost white
        t = juce::jlimit (0.0f, 1.0f, t);
        struct S { float p; juce::uint32 c; };
        static const S stops[] = { { 0.0f, 0xff070202 }, { 0.25f, 0xff3a0702 }, { 0.5f, 0xffa81d04 }, { 0.7f, 0xffff5a0a },
                                   { 0.86f, 0xffffa21e }, { 1.0f, 0xfffff0b0 } };
        for (int i = 1; i < (int) std::size (stops); ++i)
            if (t <= stops[i].p)
            {
                const float k = (t - stops[i - 1].p) / (stops[i].p - stops[i - 1].p);
                return juce::Colour (stops[i - 1].c).interpolatedWith (juce::Colour (stops[i].c), k);
            }
        return juce::Colour (stops[std::size (stops) - 1].c);
    }

    /** A wobbly hand-drawn line through points (child drawing). */
    juce::Path shaky (const std::vector<juce::Point<float>>& pts, float wobble, uint32_t seed, bool closed = false)
    {
        juce::Path p;
        Rng r; r.seed (seed * 2654435761u + 17u);
        for (size_t i = 0; i < pts.size(); ++i)
        {
            const auto q = pts[i] + juce::Point<float> (r.bi() * wobble, r.bi() * wobble);
            if (i == 0) p.startNewSubPath (q);
            else
            {
                const auto prev = pts[i - 1];
                const auto mid = (prev + pts[i]) * 0.5f + juce::Point<float> (r.bi() * wobble * 1.5f, r.bi() * wobble * 1.5f);
                p.quadraticTo (mid, q);
            }
        }
        if (closed) p.closeSubPath();
        return p;
    }

    std::vector<juce::Point<float>> ellipsePts (juce::Rectangle<float> r, int n, float start = 0.0f, float sweep = 1.0f)
    {
        std::vector<juce::Point<float>> v;
        for (int i = 0; i <= n; ++i)
        {
            const float a = (start + sweep * (float) i / (float) n) * juce::MathConstants<float>::twoPi;
            v.push_back ({ r.getCentreX() + std::cos (a) * r.getWidth() * 0.5f, r.getCentreY() + std::sin (a) * r.getHeight() * 0.5f });
        }
        return v;
    }

    /** Catmull-Rom smooth path through points. */
    [[maybe_unused]] juce::Path smoothPath (const std::vector<juce::Point<float>>& p)
    {
        juce::Path path;
        if (p.size() < 2) return path;
        path.startNewSubPath (p[0]);
        for (size_t i = 0; i + 1 < p.size(); ++i)
        {
            const auto p0 = i == 0 ? p[i] : p[i - 1];
            const auto p1 = p[i], p2 = p[i + 1];
            const auto p3 = i + 2 < p.size() ? p[i + 2] : p[i + 1];
            path.cubicTo (p1 + (p2 - p0) / 6.0f, p2 - (p3 - p1) / 6.0f, p2);
        }
        return path;
    }

}

//==============================================================================
namespace
{
    juce::Image asset (const char* data, int size) { return juce::ImageCache::getFromMemory (data, size); }
}

const juce::Image& logo()       { static auto i = asset (BinaryData::logo_png, BinaryData::logo_pngSize); return i; }
const juce::Image& wordmark()   { static auto i = asset (BinaryData::wordmark_png, BinaryData::wordmark_pngSize); return i; }
const juce::Image& ballroom()   { static auto i = asset (BinaryData::ballroom_jpg, BinaryData::ballroom_jpgSize); return i; }
const juce::Image& stone()      { static auto i = asset (BinaryData::stone_jpg, BinaryData::stone_jpgSize); return i; }
const juce::Image& lava (int)   { static auto i = asset (BinaryData::magma_jpg, BinaryData::magma_jpgSize); return i; }
const juce::Image& basalt()     { static auto i = asset (BinaryData::bg_jpg, BinaryData::bg_jpgSize); return i; }

void drawBackground (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& img = basalt();
    g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
    g.drawImage (img, area, juce::RectanglePlacement::fillDestination);
}

void fillStone (juce::Graphics& g, const juce::Path& shape, juce::Point<float> origin, float darkness, float scale)
{
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (shape);
    const auto& img = stone();
    g.setFillType (juce::FillType (img, juce::AffineTransform::scale (scale).translated (origin.x, origin.y)));
    g.fillPath (shape);
    if (darkness > 0.0f)
    {
        g.setColour (juce::Colours::black.withAlpha (juce::jlimit (0.0f, 1.0f, darkness)));
        g.fillPath (shape);
    }
}

juce::Path crackLine (juce::Point<float> a, juce::Point<float> b, uint32_t seed, float rough, bool branches)
{
    juce::Path p;
    Rng r; r.seed (seed * 2246822519u + 3u);
    const auto d = b - a;
    const float len = d.getDistanceFromOrigin();
    if (len < 1.0f) return p;
    const juce::Point<float> n (-d.y / len, d.x / len);
    const int segs = std::max (3, (int) (len / 9.0f));
    p.startNewSubPath (a);
    float off = 0.0f;
    for (int i = 1; i <= segs; ++i)
    {
        const float t = (float) i / (float) segs;
        off = i == segs ? 0.0f : juce::jlimit (-rough * 2.0f, rough * 2.0f, off * 0.6f + r.bi() * rough);
        const auto q = a + d * t + n * off;
        p.lineTo (q);
        if (branches && i < segs && r.uni() < 0.16f)
        {   // a short side crack
            juce::Path br;
            br.startNewSubPath (q);
            const float side = r.uni() < 0.5f ? -1.0f : 1.0f;
            auto e = q + n * side * (4.0f + r.uni() * rough * 4.0f) + d * ((r.uni() - 0.3f) * 0.04f);
            br.lineTo ((q + e) * 0.5f + n * r.bi() * 1.5f);
            br.lineTo (e);
            p.addPath (br);
            p.startNewSubPath (q);
        }
    }
    return p;
}

void drawLavaVein (juce::Graphics& g, const juce::Path& p, float width, float heat, juce::Colour tint)
{
    heat = juce::jlimit (0.0f, 1.0f, heat);
    const auto core = tint.interpolatedWith (juce::Colour (0xffffd2a0), 0.35f * heat);
    // dark gouge first, so a cold crack still reads as a crack in the stone
    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.strokePath (p, juce::PathStrokeType (width + 1.2f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    if (heat <= 0.01f) return;
    g.setColour (tint.withAlpha (0.10f * heat));
    g.strokePath (p, juce::PathStrokeType (width * 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (tint.withAlpha (0.25f * heat));
    g.strokePath (p, juce::PathStrokeType (width * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (core.withAlpha (0.55f + 0.45f * heat));
    g.strokePath (p, juce::PathStrokeType (width * 0.8f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
}

void engrave (juce::Graphics& g, const juce::Path& p, float width, float alpha)
{
    // carved into the stone: a dark groove with a pale lip catching the light below it
    g.setColour (juce::Colours::black.withAlpha (0.7f * alpha));
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xffcbb8ad).withAlpha (0.16f * alpha));
    g.strokePath (p, juce::PathStrokeType (width * 0.55f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (0.0f, width * 0.6f));
}

void drawSpikeWave (juce::Graphics& g, juce::Rectangle<float> a, float level, float time)
{
    // the spiky red waveform from the middle of the logo, beating with the sound
    const float cx = a.getCentreX(), cy = a.getCentreY();
    const float hh = a.getHeight() * 0.5f;
    const float lv = juce::jlimit (0.0f, 1.0f, level);
    juce::Path wave;
    const int n = 13;
    wave.startNewSubPath (a.getX(), cy);
    for (int k = 0; k < n; ++k)
    {
        const float t = ((float) k + 0.5f) / (float) n;               // 0..1 across
        const float x = a.getX() + t * a.getWidth();
        const float env = std::pow (1.0f - std::abs (t - 0.5f) * 2.0f, 1.6f);
        const float jit = 0.65f + 0.35f * std::sin (time * (5.0f + (float) k * 1.7f) + (float) k * 2.1f);
        const float amp = hh * (0.10f + 0.9f * lv * jit) * env;
        const float w = a.getWidth() / (float) n * 0.5f;
        const float up = (k % 2 == 0 ? 1.0f : 0.72f) * amp;
        wave.lineTo (x - w * 0.55f, cy);
        wave.lineTo (x - w * 0.12f, cy - up);
        wave.lineTo (x, cy + up * 0.8f);
        wave.lineTo (x + w * 0.12f, cy - up * 0.25f);
        wave.lineTo (x + w * 0.55f, cy);
    }
    wave.lineTo (a.getRight(), cy);
    const auto red = juce::Colour (0xffff2a14);
    g.setColour (red.withAlpha (0.12f + 0.2f * lv));
    g.strokePath (wave, juce::PathStrokeType (5.0f, juce::PathStrokeType::mitered));
    g.setColour (red.withAlpha (0.35f + 0.3f * lv));
    g.strokePath (wave, juce::PathStrokeType (2.2f, juce::PathStrokeType::mitered));
    g.setColour (juce::Colour (0xffffb08a).withAlpha (0.5f + 0.5f * lv));
    g.strokePath (wave, juce::PathStrokeType (0.9f, juce::PathStrokeType::mitered));
}

void drawStoneSlab (juce::Graphics& g, juce::Rectangle<float> r, float radius, uint32_t seed, float darkness)
{
    juce::Path slab;
    slab.addRoundedRectangle (r, radius);
    fillStone (g, slab, { -(float) (seed % 509u), -(float) ((seed >> 9) % 509u) }, 0.0f, 0.6f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (darkness), 0.0f, r.getY(),
                                             juce::Colours::black.withAlpha (std::min (0.92f, darkness + 0.15f)), 0.0f, r.getBottom(), false));
    g.fillPath (slab);
    drawCarvedFrame (g, r, radius);
}

void drawCarvedFrame (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawRoundedRectangle (r.expanded (1.0f), radius + 1.0f, 2.0f);
    g.setColour (juce::Colour (0xff4b3c36));
    g.drawRoundedRectangle (r, radius, 1.0f);
    g.setColour (juce::Colour (0xffcbb8ad).withAlpha (0.08f));
    g.drawRoundedRectangle (r.reduced (1.5f), std::max (0.0f, radius - 1.5f), 1.0f);
}

//==============================================================================
juce::Path cracks (juce::Rectangle<float> a, uint32_t seed)
{
    juce::Path p;
    Rng r; r.seed (seed * 7u + 1u);
    for (int k = 0; k < 3; ++k)
    {
        float x = a.getRight(), y = a.getBottom() - r.uni() * a.getHeight() * 0.5f;
        p.startNewSubPath (x, y);
        const int segs = 4 + (int) (r.uni() * 3.0f);
        for (int s = 0; s < segs; ++s)
        {
            x -= a.getWidth() / (float) segs * (0.6f + r.uni() * 0.6f);
            y -= a.getHeight() / (float) segs * (r.uni() * 1.2f - 0.2f);
            p.lineTo (x, y);
        }
    }
    return p;
}

void drawDoodle (juce::Graphics& g, Doodle d, juce::Rectangle<float> a, juce::Colour c, float th, uint32_t seed)
{
    g.setColour (c);
    const juce::PathStrokeType stroke (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    const float w = a.getWidth(), h = a.getHeight(), x = a.getX(), y = a.getY();
    const float wob = std::max (0.6f, std::min (w, h) * 0.025f);
    auto P = [&] (float fx, float fy) { return juce::Point<float> (x + fx * w, y + fy * h); };
    auto line = [&] (std::vector<juce::Point<float>> pts, uint32_t s) { g.strokePath (shaky (pts, wob, seed + s), stroke); };
    auto oval = [&] (float fx, float fy, float fw, float fh, uint32_t s)
    {
        g.strokePath (shaky (ellipsePts ({ x + (fx - fw * 0.5f) * w, y + (fy - fh * 0.5f) * h, fw * w, fh * h }, 14), wob, seed + s, true), stroke);
    };
    auto dot = [&] (float fx, float fy, float r)
    {
        g.fillEllipse (x + fx * w - r * w, y + fy * h - r * w, 2.0f * r * w, 2.0f * r * w);
    };

    switch (d)
    {
        case DoodleSkull:
            oval (0.5f, 0.42f, 0.78f, 0.7f, 1);
            line ({ P (0.3f, 0.7f), P (0.32f, 0.92f), P (0.68f, 0.92f), P (0.7f, 0.7f) }, 2);
            oval (0.34f, 0.45f, 0.2f, 0.22f, 3); oval (0.66f, 0.45f, 0.2f, 0.22f, 4);
            dot (0.34f, 0.46f, 0.035f); dot (0.66f, 0.46f, 0.035f);
            line ({ P (0.47f, 0.66f), P (0.5f, 0.58f), P (0.53f, 0.66f) }, 5);
            for (int t = 0; t < 4; ++t) line ({ P (0.38f + 0.08f * (float) t, 0.8f), P (0.38f + 0.08f * (float) t, 0.92f) }, 6u + (uint32_t) t);
            line ({ P (0.2f, 0.25f), P (0.3f, 0.32f), P (0.26f, 0.4f) }, 11);   // a crack
            break;

        case DoodleDemon:
            oval (0.5f, 0.55f, 0.62f, 0.66f, 1);
            line ({ P (0.26f, 0.38f), P (0.12f, 0.12f), P (0.18f, 0.0f), P (0.34f, 0.27f) }, 2);   // horns
            line ({ P (0.74f, 0.38f), P (0.88f, 0.12f), P (0.82f, 0.0f), P (0.66f, 0.27f) }, 3);
            line ({ P (0.32f, 0.45f), P (0.45f, 0.52f), P (0.33f, 0.55f), P (0.32f, 0.45f) }, 4);  // slanted eyes
            line ({ P (0.68f, 0.45f), P (0.55f, 0.52f), P (0.67f, 0.55f), P (0.68f, 0.45f) }, 5);
            dot (0.39f, 0.51f, 0.025f); dot (0.61f, 0.51f, 0.025f);
            line ({ P (0.32f, 0.7f), P (0.4f, 0.76f), P (0.5f, 0.73f), P (0.6f, 0.76f), P (0.68f, 0.7f) }, 6);   // grin
            line ({ P (0.4f, 0.75f), P (0.43f, 0.81f), P (0.46f, 0.74f) }, 7);   // fangs
            line ({ P (0.54f, 0.74f), P (0.57f, 0.81f), P (0.6f, 0.75f) }, 8);
            line ({ P (0.5f, 0.88f), P (0.47f, 0.98f), P (0.52f, 0.95f) }, 9);   // pointy beard
            break;

        case DoodleEyes:
        {
            Rng r; r.seed (seed);
            for (int i = 0; i < 3; ++i)
            {
                const float ex = 0.25f + 0.5f * r.uni(), ey = 0.15f + 0.7f * r.uni(), ew = 0.4f + 0.2f * r.uni();
                line ({ P (ex - ew * 0.5f, ey), P (ex, ey - ew * 0.32f), P (ex + ew * 0.5f, ey) }, 20u + (uint32_t) i * 3u);
                line ({ P (ex - ew * 0.5f, ey), P (ex, ey + ew * 0.3f), P (ex + ew * 0.5f, ey) }, 21u + (uint32_t) i * 3u);
                dot (ex + r.bi() * ew * 0.12f, ey, ew * 0.11f);
            }
            break;
        }

        case DoodleGhost:
            line ({ P (0.22f, 0.95f), P (0.2f, 0.5f), P (0.3f, 0.15f), P (0.5f, 0.05f), P (0.7f, 0.15f), P (0.8f, 0.5f), P (0.78f, 0.95f),
                    P (0.68f, 0.86f), P (0.6f, 0.97f), P (0.5f, 0.86f), P (0.4f, 0.97f), P (0.32f, 0.86f), P (0.22f, 0.95f) }, 1);
            oval (0.4f, 0.38f, 0.1f, 0.16f, 2); oval (0.6f, 0.38f, 0.1f, 0.16f, 3);
            oval (0.5f, 0.6f, 0.12f, 0.18f, 4);   // screaming mouth
            break;

        case DoodleHand:
            line ({ P (0.42f, 1.0f), P (0.44f, 0.62f) }, 1);
            line ({ P (0.56f, 1.0f), P (0.56f, 0.62f) }, 2);
            oval (0.5f, 0.55f, 0.36f, 0.16f, 3);
            for (int f = 0; f < 4; ++f)
            {
                const float fx = 0.36f + 0.09f * (float) f;
                line ({ P (fx, 0.5f), P (fx - 0.02f, 0.32f), P (fx + 0.01f, 0.14f - 0.04f * (float) (f == 1 || f == 2)) }, 10u + (uint32_t) f);
            }
            line ({ P (0.32f, 0.58f), P (0.2f, 0.45f), P (0.16f, 0.36f) }, 20);   // thumb
            break;
    }
}

//==============================================================================
void drawWallScratches (juce::Graphics& g, juce::Rectangle<float> area, uint32_t seed, float strength)
{
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (area.toNearestInt());
    Rng r; r.seed (seed * 2654435761u + 1013u);

    // one scratch = a dark gouge with a pale, dusty edge (like plaster scraped with a nail)
    auto scratch = [&] (const juce::Path& p, float width, float alpha)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f * alpha));
        g.strokePath (p, juce::PathStrokeType (width + 0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (0.7f, 0.9f));
        g.setColour (juce::Colour (0xffe9d9c0).withAlpha (alpha));
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    auto line = [&] (juce::Point<float> a, juce::Point<float> b, float wob, uint32_t s)
    {
        const auto m = (a + b) * 0.5f;
        return shaky ({ a, m, b }, wob, s);
    };
    auto pick = [&] (float lo, float hi) { return lo + (hi - lo) * r.uni(); };

    const int items = 18 + (int) (r.uni() * 10.0f);
    for (int it = 0; it < items; ++it)
    {
        const float x = area.getX() + area.getWidth() * r.uni();
        const float y = area.getY() + area.getHeight() * r.uni();
        const float alpha = strength * (r.uni() < 0.25f ? pick (0.05f, 0.1f) : pick (0.12f, 0.3f));   // some almost rubbed out
        const float rot = r.bi() * 0.25f;
        const float sc = pick (0.7f, 1.3f);
        const auto tr = juce::AffineTransform::rotation (rot, x, y);
        const float kind = r.uni();

        if (kind < 0.42f)
        {   // counting days: groups of four bars crossed by a fifth
            const int groups = 1 + (int) (r.uni() * 5.0f);
            const float h = 15.0f * sc, gap = 4.5f * sc;
            float gx = x;
            const bool lastPartial = r.uni() < 0.6f;
            for (int gi = 0; gi < groups; ++gi)
            {
                const int bars = (gi == groups - 1 && lastPartial) ? 1 + (int) (r.uni() * 4.0f) : 5;
                juce::Path p;
                for (int k = 0; k < std::min (4, bars); ++k)
                {
                    const float bx = gx + (float) k * gap + r.bi() * 0.8f;
                    p.addPath (line ({ bx + r.bi(), y + r.bi() * 1.5f }, { bx + r.bi() * 1.5f, y + h + r.bi() * 1.5f }, 0.5f, seed + (uint32_t) (it * 50 + gi * 7 + k)));
                }
                if (bars == 5)
                    p.addPath (line ({ gx - 2.0f * sc, y + h * 0.8f }, { gx + 3.0f * gap + 2.0f * sc, y + h * 0.15f }, 0.6f, seed + (uint32_t) (it * 50 + gi)));
                p.applyTransform (tr);
                scratch (p, 1.0f, alpha);
                gx += 4.0f * gap + 5.0f * sc;
            }
            if (r.uni() < 0.2f)
            {   // the whole row struck out
                auto p = line ({ x - 4.0f, y + h * 0.5f + r.bi() * 3.0f }, { gx, y + h * 0.5f + r.bi() * 4.0f }, 1.2f, seed + (uint32_t) it * 3u);
                p.applyTransform (tr);
                scratch (p, 1.2f, alpha);
            }
        }
        else if (kind < 0.58f)
        {   // illegible scribble
            std::vector<juce::Point<float>> pts;
            float px = x, py = y, ang = r.uni() * 6.28f;
            const int n = 6 + (int) (r.uni() * 10.0f);
            for (int k = 0; k < n; ++k)
            {
                pts.push_back ({ px, py });
                ang += r.bi() * 2.2f;
                px += std::cos (ang) * 7.0f * sc;
                py += std::sin (ang) * 5.0f * sc;
            }
            auto p = shaky (pts, 0.8f, seed + (uint32_t) it);
            p.applyTransform (tr);
            scratch (p, 0.9f, alpha);
        }
        else if (kind < 0.76f)
        {   // a half-legible word carved with a nail
            static const char* words[] = { "HELP", "666", "NO", "WHY", "LET ME OUT", "DAY 13", "HE SEES", "SIN", "RUN", "NEVER", "BURN", "FORGIVE" };
            const juce::String w (words[r.next() % (uint32_t) std::size (words)]);
            juce::GlyphArrangement ga;
            ga.addLineOfText (PyraLookAndFeel::titleFont (13.0f * sc), w, x, y);
            juce::Path glyphs;
            ga.createPath (glyphs);
            // scratch only the outline of each letter, roughly
            juce::Path rough;
            juce::PathFlatteningIterator fi (glyphs, {}, 1.2f);
            bool started = false;
            while (fi.next())
            {
                const juce::Point<float> a { fi.x1 + r.bi() * 0.7f, fi.y1 + r.bi() * 0.7f };
                if (! started || fi.subPathIndex == 0) { rough.startNewSubPath (a); started = true; }
                rough.lineTo (fi.x2 + r.bi() * 0.7f, fi.y2 + r.bi() * 0.7f);
                if (fi.closesSubPath) rough.closeSubPath();
            }
            rough.applyTransform (tr);
            scratch (rough, 0.7f, alpha * 1.1f);
        }
        else if (kind < 0.86f)
        {   // crude sketch: skull, demon, eyes, ghost or hand, scratched small and crooked
            const auto d = (Doodle) (r.next() % 5u);
            juce::Graphics::ScopedSaveState s2 (g);
            g.addTransform (tr);
            const float sz = 24.0f * sc;
            g.setColour (juce::Colours::black.withAlpha (0.5f * alpha));
            drawDoodle (g, d, { x + 0.8f, y + 0.9f, sz, sz * 1.15f }, juce::Colours::black.withAlpha (0.5f * alpha), 1.6f, seed + (uint32_t) it);
            drawDoodle (g, d, { x, y, sz, sz * 1.15f }, juce::Colour (0xffe9d9c0).withAlpha (alpha), 1.0f, seed + (uint32_t) it);
        }
        else if (kind < 0.93f)
        {   // X marks / crosses
            juce::Path p;
            const float s3 = 6.0f * sc;
            p.addPath (line ({ x - s3, y - s3 }, { x + s3, y + s3 }, 0.6f, seed + (uint32_t) it));
            p.addPath (line ({ x + s3, y - s3 }, { x - s3, y + s3 }, 0.6f, seed + (uint32_t) it + 1u));
            p.applyTransform (tr);
            scratch (p, 1.1f, alpha);
        }
        else
        {   // a spiral, scratched round and round
            std::vector<juce::Point<float>> pts;
            for (int k = 0; k < 28; ++k)
            {
                const float t = (float) k / 27.0f, a2 = t * 15.0f;
                pts.push_back ({ x + std::cos (a2) * t * 10.0f * sc, y + std::sin (a2) * t * 10.0f * sc });
            }
            auto p = shaky (pts, 0.4f, seed + (uint32_t) it);
            p.applyTransform (tr);
            scratch (p, 0.8f, alpha);
        }
    }
}

} // namespace art
} // namespace pyra
