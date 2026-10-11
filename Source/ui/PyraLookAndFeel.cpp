#include "PyraLookAndFeel.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include "HellArt.h"
#include "BinaryData.h"

namespace pyra
{
namespace
{
    struct Faces
    {
        juce::Typeface::Ptr medium, semi, condSemi, condBold, display, logo;
        Faces()
        {
            auto load = [] (const char* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); };
            medium   = load (BinaryData::BarlowMedium_ttf, BinaryData::BarlowMedium_ttfSize);
            semi     = load (BinaryData::BarlowSemiBold_ttf, BinaryData::BarlowSemiBold_ttfSize);
            condSemi = load (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
            condBold = load (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
            display  = load (BinaryData::NewRockerRegular_ttf, BinaryData::NewRockerRegular_ttfSize);
            logo     = load (BinaryData::UnifrakturCookBold_ttf, BinaryData::UnifrakturCookBold_ttfSize);
        }
    };
    const Faces& faces() { static Faces f; return f; }
}

PyraLookAndFeel::PyraLookAndFeel()
{
    using namespace colours;
    setDefaultSansSerifTypeface (faces().medium);
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, knobBody);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, edge);
    setColour (juce::ComboBox::arrowColourId, accent);
    setColour (juce::PopupMenu::backgroundColourId, panelHi);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::TextButton::buttonColourId, knobBody);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xfffff4ec));
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.4f));
    setColour (juce::TooltipWindow::backgroundColourId, panelHi);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, edge);
    setColour (juce::TextEditor::backgroundColourId, knobBody);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, edge);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::ScrollBar::thumbColourId, edge);
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffd9cfc8));      // pale stone keys
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff100b0a));      // obsidian
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff5a4a44));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, accent.withAlpha (0.85f));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, accent.withAlpha (0.25f));
    setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colour (0xff3a2e2a));
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
}


juce::Font PyraLookAndFeel::font (float height, bool bold)
{
    // Barlow runs a little small: +6% keeps the old sizes readable (nothing under ~11 px on screen)
    return juce::Font (juce::FontOptions().withTypeface (bold ? faces().semi : faces().medium).withHeight (std::max (11.0f, height * 1.06f)));
}

juce::Font PyraLookAndFeel::titleFont (float height)
{
    return juce::Font (juce::FontOptions().withTypeface (faces().condBold).withHeight (height).withKerningFactor (0.06f));
}

juce::Font PyraLookAndFeel::logoFont (float height)
{
    return juce::Font (juce::FontOptions().withTypeface (faces().logo).withHeight (height));
}

juce::Font PyraLookAndFeel::displayFont (float height)
{
    return juce::Font (juce::FontOptions().withTypeface (faces().display).withHeight (height));
}

namespace
{
    juce::Colour lavaAt (float t)
    {
        // dark blood -> lava red -> molten orange (the veins of the logo)
        t = juce::jlimit (0.0f, 1.0f, t);
        if (t < 0.55f) return juce::Colour (0xff6e0806).interpolatedWith (colours::accent, t / 0.55f);
        return colours::accent.interpolatedWith (juce::Colour (0xffff8a3a), (t - 0.55f) / 0.45f);
    }

    /** Texture offset that differs from knob to knob (so no two stones look the same). */
    juce::Point<float> stoneOffset (const void* p)
    {
        auto h = (uint32_t) (reinterpret_cast<uintptr_t> (p) >> 4);
        h = (h ^ (h >> 13)) * 1274126177u;
        return { (float) (h & 511u), (float) ((h >> 9) & 511u) };
    }
}

void PyraLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    using namespace colours;
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float radius0 = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float track = std::max (2.0f, radius0 * 0.12f);
    const float radius = radius0 - track * 0.6f;   // leaves room for the glow
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcR = radius - track * 0.5f;
    const bool enabled = s.isEnabled();
    const bool lavaFace = (bool) s.getProperties().getWithDefault ("lava", false);
    // bipolar = centred ranges like pan or ±24 st (not -40..+6 dB)
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0
                         && std::abs (s.getMinimum() + s.getMaximum()) <= 0.05 * (s.getMaximum() - s.getMinimum());
    const float heat = bipolar ? std::abs (pos - 0.5f) * 2.0f : pos;

    // track: a groove cut into the stone
    juce::Path bgArc;
    bgArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff080505));
    g.strokePath (bgArc, juce::PathStrokeType (track + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xff2e2421));
    g.strokePath (bgArc, juce::PathStrokeType (track * 0.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc: lava flowing in the groove, hotter towards the tip
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        const float a0 = std::min (from, angle), a1 = std::max (from, angle);
        juce::Path glowArc;
        glowArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
        g.setColour ((enabled ? accent : textDim).withAlpha (0.10f));
        g.strokePath (glowArc, juce::PathStrokeType (track * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour ((enabled ? accent : textDim).withAlpha (0.22f));
        g.strokePath (glowArc, juce::PathStrokeType (track * 1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const int segs = juce::jlimit (2, 28, (int) ((a1 - a0) * 9.0f));
        for (int k = 0; k < segs; ++k)
        {
            const float s0 = a0 + (a1 - a0) * (float) k / (float) segs, s1 = a0 + (a1 - a0) * (float) (k + 1) / (float) segs;
            juce::Path seg;
            seg.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, s0, s1 + 0.01f, true);
            const float t = bipolar ? 0.35f + 0.65f * (float) (k + 1) / (float) segs : 0.15f + 0.85f * (float) (k + 1) / (float) segs;
            g.setColour (enabled ? lavaAt (t * (0.6f + 0.4f * heat)) : textDim);
            g.strokePath (seg, juce::PathStrokeType (track, juce::PathStrokeType::curved,
                                                     k == 0 || k == segs - 1 ? juce::PathStrokeType::rounded : juce::PathStrokeType::butt));
        }
        // a thin molten core along the arc
        if (enabled)
        {
            g.setColour (juce::Colour (0xffffc4a0).withAlpha (0.25f + 0.35f * heat));
            g.strokePath (glowArc, juce::PathStrokeType (std::max (0.8f, track * 0.25f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // body: a round of cracked stone; its veins glow as the value goes up
    const float bodyR = radius - track * 2.0f;
    const auto body = juce::Rectangle<float> (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (body.translated (0.0f, bodyR * 0.08f).expanded (1.5f));
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addEllipse (body);
        g.reduceClipRegion (clip);
        const float t = (float) (double) s.getProperties().getWithDefault ("time", 0.0);
        const auto off = stoneOffset (&s);
        if (lavaFace)
        {
            // molten core like the ear cups of the logo: turns with the knob and burns brighter as the value goes up
            const auto& tex = art::lava();
            g.drawImageTransformed (tex, juce::AffineTransform::translation (-256.0f, -256.0f)
                                             .rotated (angle * 0.35f + t * 0.03f)
                                             .scaled (bodyR * 2.0f / 512.0f * 1.25f)
                                             .translated (c.x, c.y));
            g.setColour (juce::Colours::black.withAlpha (juce::jlimit (0.0f, 0.8f, 0.55f * (1.0f - pos) + 0.06f * std::sin (t * 2.3f))));   // breathing
            g.fillEllipse (body);
        }
        else
        {
            const float sc = juce::jlimit (0.12f, 0.4f, bodyR / 110.0f);
            g.drawImageTransformed (art::stone(), juce::AffineTransform::translation (-off.x, -off.y).rotated (angle * 0.5f).scaled (sc).translated (c.x, c.y));
            if (enabled && heat > 0.02f)
            {
                g.setOpacity (juce::jlimit (0.0f, 1.0f, 0.15f + 0.75f * heat));
                g.drawImageTransformed (art::lava(), juce::AffineTransform::translation (-off.y * 0.5f, -off.x * 0.5f).rotated (angle * 0.5f).scaled (sc * 0.9f).translated (c.x, c.y));
                g.setOpacity (1.0f);
            }
        }
        // round it: light from above, dark rim
        juce::ColourGradient shade (juce::Colours::white.withAlpha (0.10f), c.x, c.y - bodyR, juce::Colours::black.withAlpha (0.55f), c.x, c.y + bodyR, false);
        g.setGradientFill (shade);
        g.fillEllipse (body);
        juce::ColourGradient rim (juce::Colours::transparentBlack, c.x, c.y, juce::Colours::black.withAlpha (0.8f), c.x + bodyR, c.y, true);
        rim.addColour (0.72, juce::Colours::transparentBlack);
        g.setGradientFill (rim);
        g.fillEllipse (body);
    }
    g.setColour (lavaFace ? accent.withAlpha (0.35f + 0.55f * pos) : juce::Colour (0xff5a4842).withAlpha (0.9f));
    g.drawEllipse (body.reduced (0.5f), lavaFace ? 1.5f : 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (body.expanded (0.6f), 1.0f);

    // pointer: a white-hot shard
    juce::Path p;
    const float pw = std::max (1.6f, bodyR * (lavaFace ? 0.09f : 0.14f));
    p.addRoundedRectangle (-pw * 0.5f, -bodyR + 2.0f, pw, bodyR * (lavaFace ? 0.38f : 0.55f), pw * 0.5f);
    const auto tr = juce::AffineTransform::rotation (angle).translated (c.x, c.y);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (p, juce::PathStrokeType (1.2f), tr);
    if (enabled && heat > 0.05f)
    {
        g.setColour (accent.withAlpha (0.45f * heat));
        g.strokePath (p, juce::PathStrokeType (pw * 1.4f), tr);
    }
    g.setColour (lavaFace ? juce::Colour (0xfffff0e6) : text.interpolatedWith (juce::Colour (0xffffb08a), heat * 0.8f));
    g.fillPath (p, tr);
}

void PyraLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                                        juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
        return;
    }
    const bool horiz = style == juce::Slider::LinearHorizontal;
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float th = 5.0f;
    auto groove = horiz ? r.withSizeKeepingCentre (r.getWidth(), th) : r.withSizeKeepingCentre (th, r.getHeight());
    // a groove cut into the stone, filled with the colour of the slider (lava, or violet for modulation)
    g.setColour (juce::Colour (0xff080505));
    g.fillRoundedRectangle (groove.expanded (1.0f), 3.0f);
    g.setColour (juce::Colour (0xff2e2421));
    g.drawRoundedRectangle (groove.expanded (1.0f), 3.0f, 1.0f);
    const auto c = s.findColour (juce::Slider::trackColourId);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0
                         && std::abs (s.getMinimum() + s.getMaximum()) <= 0.05 * (s.getMaximum() - s.getMinimum());
    const float zero = bipolar ? (horiz ? groove.getCentreX() : groove.getCentreY()) : (horiz ? groove.getX() : groove.getBottom());
    auto fill = horiz ? juce::Rectangle<float>::leftTopRightBottom (std::min (zero, sliderPos), groove.getY(), std::max (zero, sliderPos), groove.getBottom())
                      : juce::Rectangle<float>::leftTopRightBottom (groove.getX(), std::min (zero, sliderPos), groove.getRight(), std::max (zero, sliderPos));
    if (s.isEnabled())
    {
        g.setColour (c.withAlpha (0.25f));
        g.fillRoundedRectangle (fill.expanded (2.0f), 3.0f);
        g.setGradientFill (juce::ColourGradient (c.darker (0.5f), fill.getX(), fill.getY(), c.brighter (0.25f), fill.getRight(), fill.getBottom(), false));
        g.fillRoundedRectangle (fill, 2.5f);
    }
    // thumb: a little stone with a hot rim
    const float tr = 7.0f;
    const auto tc = horiz ? juce::Point<float> (sliderPos, r.getCentreY()) : juce::Point<float> (r.getCentreX(), sliderPos);
    auto thumb = juce::Rectangle<float> (tr * 2.0f, tr * 2.0f).withCentre (tc);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (thumb.translated (0.0f, 1.0f).expanded (1.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9dfd8), tc.x, thumb.getY(), juce::Colour (0xff7a6c66), tc.x, thumb.getBottom(), false));
    g.fillEllipse (thumb);
    g.setColour (s.isMouseOverOrDragging() ? colours::accent : c.withAlpha (0.8f));
    g.drawEllipse (thumb, 1.2f);
}

void PyraLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool highlighted, bool down)
{
    using namespace colours;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    juce::Path shape;
    shape.addRoundedRectangle (r, 4.0f);
    const bool on = b.getToggleState();
    const auto origin = juce::Point<float> (-(float) b.getX() * 0.7f, -(float) b.getY() * 0.7f);
    if (on)
    {
        // active: a slab of stone split open by lava
        g.setColour (accent.withAlpha (0.28f));
        g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (shape);
            g.setFillType (juce::FillType (art::lava(), juce::AffineTransform::scale (0.45f).translated (origin)));
            g.fillPath (shape);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xd0ff4a22), r.getX(), r.getY(), juce::Colour (0xe08a0a08), r.getX(), r.getBottom(), false));
            g.fillPath (shape);
        }
        g.setColour (juce::Colour (0xffffb090).withAlpha (0.85f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        return;
    }
    // custom colours (e.g. RANDOM buttons) keep their tint, glowing through the stone
    const bool custom = b.findColour (juce::TextButton::buttonColourId) != knobBody;
    art::fillStone (g, shape, origin, custom ? 0.25f : (down ? 0.55f : 0.42f), 0.35f);
    if (custom)
    {
        g.setGradientFill (juce::ColourGradient (base.withAlpha (0.75f), r.getX(), r.getY(), base.darker (0.6f).withAlpha (0.85f), r.getX(), r.getBottom(), false));
        g.fillPath (shape);
    }
    if (highlighted)
    {
        g.setColour (accent.withAlpha (0.10f));
        g.fillPath (shape);
    }
    // chiselled edge: pale lip on top, dark below
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r.expanded (0.5f), 4.5f, 1.0f);
    g.setColour (highlighted ? accent.withAlpha (0.9f) : juce::Colour (0xff4e3e38));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3.0f, r.getRight() - 3.0f);
}

void PyraLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    using namespace colours;
    auto r = b.getLocalBounds().toFloat();
    const float sz = std::min (14.0f, r.getHeight() - 2.0f);
    auto box = juce::Rectangle<float> (r.getX() + 2.0f, r.getCentreY() - sz * 0.5f, sz, sz);
    const bool on = b.getToggleState();

    // a little socket in the stone; when on, an ember burns in it
    g.setColour (juce::Colour (0xff0a0707));
    g.fillEllipse (box);
    if (on)
    {
        g.setColour (accent.withAlpha (0.3f));
        g.fillEllipse (box.expanded (3.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffa070), box.getCentreX(), box.getY() + sz * 0.3f,
                                                 juce::Colour (0xffb00c0a), box.getCentreX(), box.getBottom(), true));
        g.fillEllipse (box.reduced (1.5f));
    }
    g.setColour (highlighted ? accent : juce::Colour (0xff5a4842));
    g.drawEllipse (box, 1.0f);

    if (b.getButtonText().isNotEmpty())
    {
        g.setColour (on ? text : textDim);
        g.setFont (font (std::min (13.0f, r.getHeight() * 0.8f), true));
        g.drawText (b.getButtonText(), r.withLeft (box.getRight() + 5.0f), juce::Justification::centredLeft, true);
    }
}

void PyraLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb)
{
    using namespace colours;
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    juce::Path shape;
    shape.addRoundedRectangle (r, 4.0f);
    art::fillStone (g, shape, { -(float) cb.getX() * 0.7f, -(float) cb.getY() * 0.7f }, 0.62f, 0.35f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r.expanded (0.5f), 4.5f, 1.0f);
    g.setColour (cb.hasKeyboardFocus (false) || cb.isMouseOver (true) ? accent.withAlpha (0.85f) : juce::Colour (0xff463833));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    const float a = (float) h * 0.22f;
    juce::Path arrow;
    const float ax = (float) w - (float) h * 0.55f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - a, ay - a * 0.5f, ax + a, ay - a * 0.5f, ax, ay + a * 0.6f);
    g.setColour (accent.withAlpha (0.35f));
    g.strokePath (arrow, juce::PathStrokeType (3.0f));
    g.setColour (accent);
    g.fillPath (arrow);
}

juce::Font PyraLookAndFeel::getComboBoxFont (juce::ComboBox& cb)
{
    return font (std::min (14.0f, (float) cb.getHeight() * 0.62f));
}

void PyraLookAndFeel::positionComboBoxText (juce::ComboBox& cb, juce::Label& l)
{
    l.setBounds (4, 1, cb.getWidth() - cb.getHeight() - 2, cb.getHeight() - 2);
    l.setFont (getComboBoxFont (cb));
}

juce::Font PyraLookAndFeel::getPopupMenuFont() { return font (15.0f); }

void PyraLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    juce::Path shape;
    shape.addRectangle (0.0f, 0.0f, (float) w, (float) h);
    art::fillStone (g, shape, {}, 0.62f, 0.45f);
    g.setColour (colours::accent.withAlpha (0.55f));
    g.drawRect (0, 0, w, h);
}

void PyraLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                         bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                                         const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        auto r = area.toFloat().reduced (8.0f, 0.0f);
        art::drawLavaVein (g, art::crackLine ({ r.getX(), r.getCentreY() }, { r.getRight(), r.getCentreY() }, (uint32_t) area.getY(), 0.8f, false), 0.8f, 0.25f);
        return;
    }
    if (isHighlighted && isActive)
    {
        auto r = area.toFloat().reduced (2.0f, 1.0f);
        g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.85f), r.getX(), r.getY(), juce::Colour (0xd08a0a08), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
    }
    LookAndFeel_V4::drawPopupMenuItem (g, area, false, isActive, false, isTicked, hasSubMenu, text, shortcutKeyText, icon, textColour);
}

void PyraLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize,
                                     bool mouseOver, bool)
{
    auto track = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (vertical ? (float) w * 0.3f : 0.0f, vertical ? 0.0f : (float) h * 0.3f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (track, 2.0f);
    auto thumb = vertical ? juce::Rectangle<float> (track.getX(), (float) thumbStart, track.getWidth(), (float) thumbSize)
                          : juce::Rectangle<float> ((float) thumbStart, track.getY(), (float) thumbSize, track.getHeight());
    g.setColour ((mouseOver ? colours::accent : juce::Colour (0xff6a524a)).withAlpha (0.9f));
    g.fillRoundedRectangle (thumb.reduced (0.5f), 2.0f);
}

void PyraLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (juce::Colour (0xff0c0908));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 3.0f);
}

void PyraLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& te)
{
    g.setColour (te.hasKeyboardFocus (true) ? colours::accent : colours::edge.brighter (0.2f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 3.0f, 1.0f);
}

void PyraLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& t, int w, int h)
{
    juce::Path shape;
    shape.addRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 4.0f);
    art::fillStone (g, shape, {}, 0.7f, 0.4f);
    g.setColour (colours::accent.withAlpha (0.75f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 4.0f, 1.0f);
    juce::AttributedString s;
    s.setJustification (juce::Justification::centredLeft);
    s.append (t, font (13.0f), colours::text);
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, (float) w - 16.0f);
    tl.draw (g, { 8.0f, 0.0f, (float) w - 16.0f, (float) h });
}

juce::Font PyraLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return titleFont (std::min (16.0f, (float) h * 0.62f));
}

juce::Label* PyraLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (12.0f));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

} // namespace pyra
