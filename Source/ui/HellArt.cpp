#include "HellArt.h"
#include "PyraLookAndFeel.h"
#include "../dsp/DspUtil.h"
#include <map>

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

    float fbm (float x, float y, uint32_t seed, int octaves)
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

    juce::Colour lavaRamp (float t)
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
    juce::Path smoothPath (const std::vector<juce::Point<float>>& p)
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

    juce::Path flameTongue (float cx, float baseY, float w, float h, float lean)
    {
        juce::Path f;
        f.startNewSubPath (cx - w * 0.5f, baseY);
        f.cubicTo (cx - w * 0.6f, baseY - h * 0.45f, cx - w * 0.1f + lean * 0.3f, baseY - h * 0.55f, cx + lean, baseY - h);
        f.cubicTo (cx + w * 0.15f + lean * 0.4f, baseY - h * 0.6f, cx + w * 0.65f, baseY - h * 0.4f, cx + w * 0.5f, baseY);
        f.closeSubPath();
        return f;
    }
}

//==============================================================================
const juce::Image& background (int width, int height)
{
    static std::map<std::pair<int, int>, juce::Image> cache;
    auto key = std::make_pair (width, height);
    if (auto it = cache.find (key); it != cache.end()) return it->second;

    juce::Image img (juce::Image::RGB, width, height, false);
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < height; ++y)
        {
            const float ny = (float) y / (float) height;
            for (int x = 0; x < width; ++x)
            {
                const float nx = (float) x / (float) width;
                // smoky sky: dark red-brown, darker at the top
                const float smoke = fbm (nx * 5.0f, ny * 4.0f + 3.0f, 7u, 5);
                const float swirl = fbm (nx * 2.0f + smoke, ny * 2.0f, 21u, 4);
                float r = 0.035f + 0.07f * smoke * swirl + 0.10f * ny * ny;
                float g = 0.012f + 0.018f * smoke + 0.025f * ny * ny;
                float b = 0.010f + 0.012f * smoke;
                // a lava glow rising from the bottom
                const float glow = std::pow (juce::jlimit (0.0f, 1.0f, (ny - 0.55f) / 0.45f), 2.2f);
                const float flicker = fbm (nx * 14.0f, ny * 6.0f, 55u, 3);
                r += glow * (0.30f + 0.25f * flicker);
                g += glow * (0.06f + 0.08f * flicker);
                // faint violet haze (from the eyes castle picture)
                const float haze = fbm (nx * 3.0f + 9.0f, ny * 3.0f, 91u, 3);
                b += 0.035f * haze * (1.0f - ny);
                r += 0.01f * haze;
                bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, r), juce::jlimit (0.0f, 1.0f, g),
                                                                      juce::jlimit (0.0f, 1.0f, b), 1.0f));
            }
        }
    }

    juce::Graphics g (img);
    // volcano silhouettes with a glowing crater
    Rng rng; rng.seed (4242u);
    const float horizon = (float) height * 0.72f;
    for (int layer = 0; layer < 2; ++layer)
    {
        juce::Path m;
        m.startNewSubPath (0.0f, (float) height);
        float x = 0.0f;
        m.lineTo (0.0f, horizon - 30.0f);
        while (x < (float) width)
        {
            x += 40.0f + rng.uni() * 90.0f;
            const float peak = horizon - (layer == 0 ? 60.0f : 25.0f) - rng.uni() * (layer == 0 ? 170.0f : 90.0f);
            m.lineTo (x, peak);
            x += 30.0f + rng.uni() * 70.0f;
            m.lineTo (x, horizon - rng.uni() * 40.0f);
        }
        m.lineTo ((float) width, (float) height);
        m.closeSubPath();
        g.setColour (layer == 0 ? juce::Colour (0xff120504) : juce::Colour (0xff1b0705));
        g.fillPath (m);
    }
    // embers
    for (int i = 0; i < 260; ++i)
    {
        const float ex = rng.uni() * (float) width, ey = std::pow (rng.uni(), 0.6f) * (float) height;
        const float sz = 0.6f + rng.uni() * 1.8f;
        const float a = 0.15f + 0.6f * rng.uni() * (ey / (float) height);
        g.setColour (juce::Colour (0xffffa030).withAlpha (a * 0.35f));
        g.fillEllipse (ex - sz * 2.0f, ey - sz * 2.0f, sz * 4.0f, sz * 4.0f);
        g.setColour (juce::Colour (0xffffd080).withAlpha (a));
        g.fillEllipse (ex - sz * 0.5f, ey - sz * 0.5f, sz, sz);
    }
    // vignette
    juce::ColourGradient vig (juce::Colours::transparentBlack, (float) width * 0.5f, (float) height * 0.45f,
                              juce::Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true);
    g.setGradientFill (vig);
    g.fillRect (0, 0, width, height);

    return cache[key] = img;
}

//==============================================================================
const juce::Image& lava (int size)
{
    static std::map<int, juce::Image> cache;
    if (auto it = cache.find (size); it != cache.end()) return it->second;
    juce::Image img (juce::Image::ARGB, size, size, true);
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const float c = (float) size * 0.5f;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                const float dx = ((float) x - c) / c, dy = ((float) y - c) / c;
                const float d = std::sqrt (dx * dx + dy * dy);
                if (d > 1.02f) continue;
                const float u = (float) x / (float) size * 6.0f, v = (float) y / (float) size * 6.0f;
                // domain-warped ridged noise = glowing cracks between dark crust plates
                const float wx = fbm (u * 0.7f, v * 0.7f, 3u, 3) * 2.2f, wy = fbm (u * 0.7f + 5.0f, v * 0.7f, 4u, 3) * 2.2f;
                const float n = fbm (u + wx, v + wy, 11u, 5);
                const float ridge = 1.0f - std::abs (n * 2.0f - 1.0f);
                float t = std::pow (ridge, 2.6f) * 1.18f;
                // hotter in the middle, like a molten core; rays from the centre
                const float ang = std::atan2 (dy, dx);
                const float rays = std::pow (0.5f + 0.5f * std::sin (ang * 9.0f + n * 6.0f), 6.0f) * (1.0f - d) * 0.6f;
                t += (1.0f - d) * (1.0f - d) * 0.55f + rays;
                t = t * (1.0f - 0.35f * std::pow (d, 4.0f));
                auto col = lavaRamp (t);
                const float edge = juce::jlimit (0.0f, 1.0f, (1.02f - d) * 25.0f);
                bd.setPixelColour (x, y, col.withAlpha (edge));
            }
    }
    return cache[size] = img;
}

//==============================================================================
void drawLogo (juce::Graphics& g, juce::Rectangle<float> a)
{
    // flames behind the clef
    const float cx = a.getCentreX(), base = a.getBottom() - a.getHeight() * 0.18f;
    struct F { float dx, w, h, lean; };
    const F flames[] = { { -0.22f, 0.42f, 0.78f, -0.10f }, { 0.20f, 0.40f, 0.86f, 0.08f }, { 0.0f, 0.55f, 1.02f, 0.02f },
                         { -0.08f, 0.32f, 0.62f, -0.12f }, { 0.12f, 0.30f, 0.66f, 0.1f } };
    for (int pass = 0; pass < 2; ++pass)
        for (auto& f : flames)
        {
            const float w = a.getWidth() * f.w * (pass == 0 ? 1.0f : 0.55f), h = a.getHeight() * f.h * (pass == 0 ? 1.0f : 0.7f);
            auto p = flameTongue (cx + a.getWidth() * f.dx, base, w, h, a.getWidth() * f.lean);
            if (pass == 0)
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffa020), cx, base, juce::Colour (0xffd8200a).withAlpha (0.9f),
                                                         cx, base - h, false));
            else
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff0a0), cx, base, juce::Colour (0xffffa020).withAlpha (0.0f),
                                                         cx, base - h, false));
            g.fillPath (p);
        }

    // the treble clef (one stroke: spiral, up to the top loop, down the stem to the curl)
    const std::vector<juce::Point<float>> pts {
        { 0.43f, 0.63f }, { 0.47f, 0.57f }, { 0.41f, 0.51f }, { 0.30f, 0.52f }, { 0.22f, 0.60f }, { 0.25f, 0.72f }, { 0.38f, 0.78f },
        { 0.53f, 0.73f }, { 0.58f, 0.61f }, { 0.52f, 0.47f }, { 0.40f, 0.38f }, { 0.31f, 0.27f }, { 0.32f, 0.14f }, { 0.40f, 0.04f },
        { 0.48f, 0.09f }, { 0.47f, 0.21f }, { 0.39f, 0.32f }, { 0.36f, 0.50f }, { 0.39f, 0.70f }, { 0.41f, 0.88f }, { 0.37f, 0.97f },
        { 0.28f, 0.98f }, { 0.25f, 0.92f } };
    std::vector<juce::Point<float>> scaled;
    const float side = a.getHeight();
    const float ox = cx - side * 0.40f, oy = a.getY();
    for (auto p : pts) scaled.push_back ({ ox + p.x * side, oy + p.y * side });
    auto clef = smoothPath (scaled);
    const float sw = side * 0.06f;
    g.setColour (juce::Colour (0xff1a0402).withAlpha (0.85f));
    g.strokePath (clef, juce::PathStrokeType (sw * 1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff4d0), cx, a.getY(), juce::Colour (0xffffb040), cx, a.getBottom(), false));
    g.strokePath (clef, juce::PathStrokeType (sw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xfffff4d0));
    g.fillEllipse (ox + 0.27f * side - sw * 1.1f, oy + 0.93f * side - sw * 1.1f, sw * 2.2f, sw * 2.2f);
}

//==============================================================================
void drawSkeleton (juce::Graphics& g, float x, float baseY, float h, float lean, float alpha, uint32_t seed)
{
    Rng r; r.seed (seed * 7919u + 3u);
    const auto bone = juce::Colour (0xffe8dcc4).withAlpha (alpha);
    const auto shade = juce::Colour (0xff2a0a04).withAlpha (alpha);
    const float skullR = h * 0.13f;
    const float headY = baseY - h * 0.55f;
    const float hx = x + lean * h * 0.15f;
    const float lw = std::max (1.0f, h * 0.035f);

    // ribs / spine just above the lava
    g.setColour (bone);
    g.drawLine (hx, headY + skullR, x, baseY, lw);
    for (int i = 0; i < 3; ++i)
    {
        const float yy = headY + skullR * 1.8f + (float) i * h * 0.075f;
        if (yy > baseY) break;
        juce::Path rib;
        rib.addCentredArc (x + (hx - x) * (1.0f - (yy - headY) / (baseY - headY)), yy, h * 0.12f, h * 0.04f, 0.0f,
                           -juce::MathConstants<float>::halfPi - 1.3f, -juce::MathConstants<float>::halfPi + 1.3f, true);
        g.strokePath (rib, juce::PathStrokeType (lw * 0.8f));
    }

    // arms raised, reaching out of the fire
    for (int side = -1; side <= 1; side += 2)
    {
        const float sx = hx + (float) side * h * 0.1f, sy = headY + skullR * 1.7f;
        const float ex = sx + (float) side * h * (0.14f + r.uni() * 0.08f), ey = sy - h * (0.12f + r.uni() * 0.1f);
        const float wx = ex + (float) side * h * (0.02f + r.uni() * 0.06f), wy = ey - h * (0.2f + r.uni() * 0.12f);
        g.drawLine (sx, sy, ex, ey, lw);
        g.drawLine (ex, ey, wx, wy, lw);
        for (int f = -2; f <= 2; ++f)
        {
            const float fa = -juce::MathConstants<float>::halfPi + (float) f * 0.3f + (float) side * 0.15f;
            g.drawLine (wx, wy, wx + std::cos (fa) * h * 0.07f, wy + std::sin (fa) * h * 0.07f, lw * 0.6f);
        }
    }

    // skull
    g.setColour (bone);
    g.fillEllipse (hx - skullR, headY - skullR, skullR * 2.0f, skullR * 2.0f);
    g.fillRoundedRectangle (hx - skullR * 0.6f, headY + skullR * 0.5f, skullR * 1.2f, skullR * 0.8f, skullR * 0.2f);
    g.setColour (shade);
    g.fillEllipse (hx - skullR * 0.62f, headY - skullR * 0.15f, skullR * 0.5f, skullR * 0.55f);
    g.fillEllipse (hx + skullR * 0.12f, headY - skullR * 0.15f, skullR * 0.5f, skullR * 0.55f);
    g.fillRect (hx - skullR * 0.08f, headY + skullR * 0.45f, skullR * 0.16f, skullR * 0.25f);
    for (int t = -2; t <= 2; ++t)
        g.drawLine (hx + (float) t * skullR * 0.22f, headY + skullR * 0.8f, hx + (float) t * skullR * 0.22f, headY + skullR * 1.25f,
                    std::max (0.6f, lw * 0.4f));
    // glowing eyes
    g.setColour (juce::Colour (0xffff8a20).withAlpha (alpha * 0.9f));
    g.fillEllipse (hx - skullR * 0.45f, headY + skullR * 0.02f, skullR * 0.16f, skullR * 0.16f);
    g.fillEllipse (hx + skullR * 0.29f, headY + skullR * 0.02f, skullR * 0.16f, skullR * 0.16f);
}

void drawLavaStrip (juce::Graphics& g, juce::Rectangle<float> area, float time)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffb030), 0.0f, area.getY(), juce::Colour (0xffa01808), 0.0f, area.getBottom(), false));
    g.fillRect (area);
    Rng r; r.seed (77u);
    for (float x = area.getX(); x < area.getRight(); x += 6.0f + r.uni() * 10.0f)
    {
        const float fh = area.getHeight() * (0.6f + r.uni() * 1.4f);
        auto f = flameTongue (x, area.getY() + 2.0f, 8.0f + r.uni() * 10.0f, fh, (r.bi() + std::sin (time + x)) * 3.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffc040), x, area.getY(), juce::Colour (0x00e03010), x, area.getY() - fh, false));
        g.fillPath (f);
    }
}

void drawHellScene (juce::Graphics& g, juce::Rectangle<float> a)
{
    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip;
    clip.addRoundedRectangle (a, 8.0f);
    g.reduceClipRegion (clip);
    g.drawImage (background (1280, 880), a, juce::RectanglePlacement::fillDestination);

    const float lavaY = a.getBottom() - a.getHeight() * 0.22f;
    // distant volcano erupting
    juce::Path v;
    const float vx = a.getCentreX() + a.getWidth() * 0.12f;
    v.startNewSubPath (vx - a.getWidth() * 0.32f, lavaY);
    v.lineTo (vx - a.getWidth() * 0.035f, lavaY - a.getHeight() * 0.45f);
    v.lineTo (vx + a.getWidth() * 0.035f, lavaY - a.getHeight() * 0.45f);
    v.lineTo (vx + a.getWidth() * 0.34f, lavaY);
    v.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a160c), vx, lavaY - a.getHeight() * 0.45f, juce::Colour (0xff120504), vx, lavaY, false));
    g.fillPath (v);
    const auto craterY = lavaY - a.getHeight() * 0.45f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xccffa030), vx, craterY, juce::Colour (0x00ff3010), vx, craterY - a.getHeight() * 0.25f, true));
    g.fillEllipse (vx - a.getHeight() * 0.25f, craterY - a.getHeight() * 0.25f, a.getHeight() * 0.5f, a.getHeight() * 0.5f);
    // lava running down
    juce::Path flow;
    flow.startNewSubPath (vx + 6.0f, craterY + 2.0f);
    flow.quadraticTo (vx + 30.0f, craterY + a.getHeight() * 0.15f, vx + 44.0f, lavaY - a.getHeight() * 0.05f);
    g.setColour (juce::Colour (0xffff7020).withAlpha (0.8f));
    g.strokePath (flow, juce::PathStrokeType (3.0f));

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.strokePath (v, juce::PathStrokeType (2.0f));

    // lava lake
    auto lake = juce::Rectangle<float> (a.getX(), lavaY, a.getWidth(), a.getBottom() - lavaY);
    for (int y = 0; y < (int) lake.getHeight(); y += 2)
    {
        const float t = 0.95f - (float) y / lake.getHeight() * 0.5f;
        g.setColour (lavaRamp (t * (0.85f + 0.15f * fbm ((float) y * 0.2f, 1.0f, 5u, 2))));
        g.fillRect (lake.getX(), lake.getY() + (float) y, lake.getWidth(), 2.0f);
    }

    // the damned
    Rng r; r.seed (666u);
    const int n = 7;
    for (int i = 0; i < n; ++i)
    {
        const float x = a.getX() + a.getWidth() * (0.08f + 0.84f * ((float) i + r.uni() * 0.6f) / (float) n);
        const float h = a.getHeight() * (0.28f + r.uni() * 0.2f);
        drawSkeleton (g, x, lavaY + lake.getHeight() * (0.25f + r.uni() * 0.4f), h, r.bi() * 0.5f, 0.85f, (uint32_t) i + 1u);
    }
    drawLavaStrip (g, lake.withTrimmedTop (lake.getHeight() * 0.55f), 0.0f);
}

//==============================================================================
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

} // namespace art
} // namespace pyra
