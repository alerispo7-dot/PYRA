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
const juce::Image& stone()
{
    static juce::Image img = []
    {
        const int n = 192;
        juce::Image im (juce::Image::ARGB, n, n, true);
        juce::Image::BitmapData bd (im, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
            {
                // tileable: sample noise on a torus
                const float u = (float) x / (float) n * juce::MathConstants<float>::twoPi, v = (float) y / (float) n * juce::MathConstants<float>::twoPi;
                const float nx = 3.0f + std::cos (u) * 1.6f, ny = 3.0f + std::sin (u) * 1.6f, nz = 3.0f + std::cos (v) * 1.6f + std::sin (v) * 0.9f;
                const float a = fbm (nx * 2.0f + nz, ny * 2.0f - nz * 0.5f, 13u, 4);
                const float speck = hash2 (x, y, 5u) > 0.985f ? 0.6f : 0.0f;
                const float l = juce::jlimit (0.0f, 1.0f, (a - 0.35f) * 1.6f + speck);
                bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (0.55f, 0.32f, 0.25f, l));
            }
        return im;
    }();
    return img;
}

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

namespace
{
    /** Polished chrome: bright sky, dark horizon line, warm reflection of the fire underneath. */
    juce::ColourGradient chrome (float top, float bottom)
    {
        juce::ColourGradient cg (juce::Colour (0xfffdfbf8), 0.0f, top, juce::Colour (0xffffd9a0), 0.0f, bottom, false);
        cg.addColour (0.18, juce::Colour (0xffd9d6d8));
        cg.addColour (0.42, juce::Colour (0xff6b6870));
        cg.addColour (0.50, juce::Colour (0xff1c1a1e));   // horizon
        cg.addColour (0.53, juce::Colour (0xfff2efe9));
        cg.addColour (0.72, juce::Colour (0xffa59a98));
        cg.addColour (0.88, juce::Colour (0xffe07a38));   // fire reflected in the metal
        return cg;
    }

    void fireBehind (juce::Graphics& g, juce::Rectangle<float> a, float time, int tongues, uint32_t seed, float intensity)
    {
        Rng r; r.seed (seed);
        for (int pass = 0; pass < 3; ++pass)
            for (int i = 0; i < tongues; ++i)
            {
                const float u = ((float) i + 0.5f) / (float) tongues + r.bi() * 0.04f;
                const float x = a.getX() + a.getWidth() * u;
                const float flick = 1.0f + 0.16f * std::sin (time * (6.0f + (float) i * 1.7f) + (float) i)
                                         + 0.08f * std::sin (time * 15.0f + (float) i * 3.1f);
                const float h = a.getHeight() * (0.55f + 0.45f * r.uni()) * flick * (pass == 0 ? 1.0f : (pass == 1 ? 0.72f : 0.42f));
                const float w = a.getWidth() / (float) tongues * (pass == 0 ? 2.4f : 1.6f);
                auto f = flameTongue (x, a.getBottom(), w, h, std::sin (time * 3.0f + (float) i) * w * 0.35f);
                const juce::Colour c0 = pass == 0 ? juce::Colour (0xffb01206) : (pass == 1 ? juce::Colour (0xffff5a10) : juce::Colour (0xffffd070));
                g.setGradientFill (juce::ColourGradient (c0.withAlpha (intensity), x, a.getBottom(), c0.withAlpha (0.0f), x, a.getBottom() - h, false));
                g.fillPath (f);
            }
    }
}

void drawLogo (juce::Graphics& g, juce::Rectangle<float> a, float time)
{
    // hellfire behind a chrome treble clef (think biker from hell / hell priest)
    fireBehind (g, a.expanded (4.0f, 0.0f).withTrimmedBottom (2.0f), time, 5, 31u, 0.95f);

    const std::vector<juce::Point<float>> pts {
        { 0.43f, 0.63f }, { 0.47f, 0.57f }, { 0.41f, 0.51f }, { 0.30f, 0.52f }, { 0.22f, 0.60f }, { 0.25f, 0.72f }, { 0.38f, 0.78f },
        { 0.53f, 0.73f }, { 0.58f, 0.61f }, { 0.52f, 0.47f }, { 0.40f, 0.38f }, { 0.31f, 0.27f }, { 0.32f, 0.14f }, { 0.40f, 0.04f },
        { 0.48f, 0.09f }, { 0.47f, 0.21f }, { 0.39f, 0.32f }, { 0.36f, 0.50f }, { 0.39f, 0.70f }, { 0.41f, 0.88f }, { 0.37f, 0.97f },
        { 0.28f, 0.98f }, { 0.25f, 0.92f } };
    std::vector<juce::Point<float>> scaled;
    const float side = a.getHeight();
    const float ox = a.getCentreX() - side * 0.40f, oy = a.getY();
    for (auto p : pts) scaled.push_back ({ ox + p.x * side, oy + p.y * side });
    auto clef = smoothPath (scaled);
    const float sw = side * 0.085f;
    juce::Path solid;
    juce::PathStrokeType (sw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (solid, clef);
    solid.addEllipse (ox + 0.27f * side - sw * 1.2f, oy + 0.93f * side - sw * 1.2f, sw * 2.4f, sw * 2.4f);

    g.setColour (juce::Colour (0xff0a0303));
    g.strokePath (solid, juce::PathStrokeType (2.2f));
    g.setGradientFill (chrome (a.getY(), a.getBottom()));
    g.fillPath (solid);
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.strokePath (solid, juce::PathStrokeType (0.6f), juce::AffineTransform::translation (-0.4f, -0.5f));
}

void drawChromeText (juce::Graphics& g, const juce::String& text, const juce::Font& font, float x, float baseline, float time)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (font, text, x, baseline);
    juce::Path letters;
    ga.createPath (letters);
    const auto b = letters.getBounds();

    // flames licking up from behind the letters
    fireBehind (g, { b.getX(), b.getY() - b.getHeight() * 0.35f, b.getWidth(), b.getHeight() * 1.05f }, time, 9, 77u, 0.75f);

    // deep shadow, dark bevel, chrome, highlight edge, red-hot rim at the bottom
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.fillPath (letters, juce::AffineTransform::translation (1.5f, 2.0f));
    g.setColour (juce::Colour (0xff120404));
    g.strokePath (letters, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (chrome (b.getY(), b.getBottom()));
    g.fillPath (letters);
    g.setColour (juce::Colours::white.withAlpha (0.75f));
    g.strokePath (letters, juce::PathStrokeType (0.6f), juce::AffineTransform::translation (-0.4f, -0.6f));
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (letters);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x00ff4010), 0.0f, b.getCentreY(), juce::Colour (0x88ff3a0a), 0.0f, b.getBottom(), false));
        g.fillRect (b);
    }
}

//==============================================================================
void drawSkeleton (juce::Graphics& g, float x, float baseY, float h, float lean, float alpha, uint32_t seed)
{
    // drawn like a child's pencil sketch: wobbly lines, no shading
    Rng r; r.seed (seed * 7919u + 3u);
    const auto c = juce::Colour (0xffeee0c8).withAlpha (alpha);
    const juce::PathStrokeType st (juce::jlimit (1.0f, 1.8f, h * 0.014f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded);   // pencil-thin
    const float wob = std::max (0.5f, h * 0.02f);
    const float headR = h * 0.12f, hx = x + lean * h * 0.15f, headY = baseY - h * 0.62f;
    g.setColour (c);
    g.strokePath (shaky (ellipsePts ({ hx - headR, headY - headR, headR * 2.0f, headR * 2.1f }, 10), wob, seed, true), st);
    // eyes: two scribbled holes, a crooked grin
    for (int e = -1; e <= 1; e += 2)
        g.strokePath (shaky ({ { hx + (float) e * headR * 0.45f - headR * 0.18f, headY - headR * 0.1f }, { hx + (float) e * headR * 0.45f + headR * 0.18f, headY + headR * 0.15f },
                               { hx + (float) e * headR * 0.45f - headR * 0.18f, headY + headR * 0.15f }, { hx + (float) e * headR * 0.45f + headR * 0.15f, headY - headR * 0.1f } },
                             wob * 0.5f, seed + (uint32_t) e + 9u), st);
    g.strokePath (shaky ({ { hx - headR * 0.5f, headY + headR * 0.55f }, { hx, headY + headR * 0.7f }, { hx + headR * 0.5f, headY + headR * 0.5f } }, wob * 0.5f, seed + 3u), st);
    // spine and a few ribs
    g.strokePath (shaky ({ { hx, headY + headR }, { (hx + x) * 0.5f, (headY + baseY) * 0.5f }, { x, baseY } }, wob, seed + 4u), st);
    for (int k = 0; k < 3; ++k)
    {
        const float yy = headY + headR * 1.6f + (float) k * h * 0.08f, w = h * (0.13f - (float) k * 0.02f);
        const float cx = hx + (x - hx) * ((yy - headY) / (baseY - headY));
        g.strokePath (shaky ({ { cx - w, yy + h * 0.03f }, { cx, yy }, { cx + w, yy + h * 0.03f } }, wob, seed + 10u + (uint32_t) k), st);
    }
    // arms up, stick fingers
    for (int side = -1; side <= 1; side += 2)
    {
        const float sx = hx, sy = headY + headR * 1.5f;
        const float ex = sx + (float) side * h * (0.18f + r.uni() * 0.08f), ey = sy - h * (0.05f + r.uni() * 0.12f);
        const float wx = ex + (float) side * h * 0.05f, wy = ey - h * (0.2f + r.uni() * 0.1f);
        g.strokePath (shaky ({ { sx, sy }, { ex, ey }, { wx, wy } }, wob, seed + 20u + (uint32_t) side), st);
        for (int f = -1; f <= 1; ++f)
            g.strokePath (shaky ({ { wx, wy }, { wx + (float) f * h * 0.04f + (float) side * h * 0.01f, wy - h * 0.07f } }, wob * 0.4f, seed + 30u + (uint32_t) (f + 2 * side)), st);
    }
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

    // the sky is a wall too: someone kept count up here
    drawWallScratches (g, a.withHeight (a.getHeight() * 0.45f).reduced (10.0f, 6.0f), 999u, 0.8f);

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
        const float h = a.getHeight() * (0.2f + r.uni() * 0.14f);
        drawSkeleton (g, x, lavaY + lake.getHeight() * (0.25f + r.uni() * 0.4f), h, r.bi() * 0.5f, 0.7f, (uint32_t) i + 1u);
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
