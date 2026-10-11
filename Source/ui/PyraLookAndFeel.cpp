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
        juce::Typeface::Ptr medium, semi, condSemi, condBold, display;
        Faces()
        {
            auto load = [] (const char* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); };
            medium   = load (BinaryData::BarlowMedium_ttf, BinaryData::BarlowMedium_ttfSize);
            semi     = load (BinaryData::BarlowSemiBold_ttf, BinaryData::BarlowSemiBold_ttfSize);
            condSemi = load (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
            condBold = load (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
            display  = load (BinaryData::CinzelDecorativeBlack_ttf, BinaryData::CinzelDecorativeBlack_ttfSize);
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
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::TextButton::buttonColourId, knobBody);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::Slider::textBoxTextColourId, text);
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
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe6d8c4));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff170806));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff6a4a3e));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, accent.withAlpha (0.8f));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, accent2.withAlpha (0.3f));
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

juce::Font PyraLookAndFeel::displayFont (float height)
{
    return juce::Font (juce::FontOptions().withTypeface (faces().display).withHeight (height));
}

namespace
{
    juce::Colour lavaAt (float t)
    {
        // deep red -> lava orange -> molten yellow
        t = juce::jlimit (0.0f, 1.0f, t);
        if (t < 0.5f) return juce::Colour (0xffa8180a).interpolatedWith (colours::accent, t * 2.0f);
        return colours::accent.interpolatedWith (colours::accent2, (t - 0.5f) * 2.0f);
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

    // track: a dark groove
    juce::Path bgArc;
    bgArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff140605));
    g.strokePath (bgArc, juce::PathStrokeType (track + 1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (edge.withAlpha (0.8f));
    g.strokePath (bgArc, juce::PathStrokeType (track * 0.55f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc: flowing lava (dark red at the start, molten yellow at the tip) with a soft glow
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        const float a0 = std::min (from, angle), a1 = std::max (from, angle);
        juce::Path glowArc;
        glowArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
        g.setColour ((enabled ? accent : textDim).withAlpha (0.22f));
        g.strokePath (glowArc, juce::PathStrokeType (track * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const int segs = juce::jlimit (2, 28, (int) ((a1 - a0) * 9.0f));
        for (int k = 0; k < segs; ++k)
        {
            const float s0 = a0 + (a1 - a0) * (float) k / (float) segs, s1 = a0 + (a1 - a0) * (float) (k + 1) / (float) segs;
            juce::Path seg;
            seg.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, s0, s1 + 0.01f, true);
            const float t = bipolar ? 0.35f + 0.65f * (float) (k + 1) / (float) segs : 0.15f + 0.85f * (float) (k + 1) / (float) segs;
            g.setColour (enabled ? lavaAt (t * (0.55f + 0.45f * (bipolar ? std::abs (pos - 0.5f) * 2.0f : pos))) : textDim);
            g.strokePath (seg, juce::PathStrokeType (track, juce::PathStrokeType::curved,
                                                     k == 0 || k == segs - 1 ? juce::PathStrokeType::rounded : juce::PathStrokeType::butt));
        }
    }

    // body
    const float bodyR = radius - track * 2.0f;
    const auto body = juce::Rectangle<float> (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (body.translated (0.0f, bodyR * 0.08f).expanded (1.0f));
    if (lavaFace)
    {
        // molten core (like a magma ball): it turns with the knob and burns brighter as the value goes up
        const auto& tex = art::lava (512);
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip; clip.addEllipse (body);
            g.reduceClipRegion (clip);
            const float t = (float) (double) s.getProperties().getWithDefault ("time", 0.0);
            g.drawImageTransformed (tex, juce::AffineTransform::translation (-256.0f, -256.0f)
                                             .rotated (angle * 0.35f + t * 0.05f)
                                             .scaled (bodyR * 2.0f / 512.0f * 1.04f)
                                             .translated (c.x, c.y));
            g.setColour (juce::Colours::black.withAlpha (juce::jlimit (0.0f, 0.8f, 0.5f * (1.0f - pos) + 0.07f * std::sin (t * 2.3f))));   // breathing glow
            g.fillEllipse (body);
            const int doodle = (int) s.getProperties().getWithDefault ("doodle", -1);
            if (doodle >= 0)
                art::drawDoodle (g, (art::Doodle) doodle, body.withSizeKeepingCentre (bodyR * 1.05f, bodyR * 1.05f),
                                 juce::Colour (0xff1a0402).withAlpha (0.72f), std::max (1.5f, bodyR * 0.035f), 3u);
            // dark rim, like a crust
            juce::ColourGradient rim (juce::Colours::transparentBlack, c.x, c.y, juce::Colours::black.withAlpha (0.75f), c.x + bodyR, c.y, true);
            rim.addColour (0.75, juce::Colours::transparentBlack);
            g.setGradientFill (rim);
            g.fillEllipse (body);
        }
        g.setColour (accent2.withAlpha (0.35f + 0.5f * pos));
        g.drawEllipse (body, 1.5f);
    }
    else
    {
        juce::ColourGradient grad (juce::Colour (0xff47302a), c.x, c.y - bodyR, juce::Colour (0xff120807), c.x, c.y + bodyR, false);
        g.setGradientFill (grad);
        g.fillEllipse (body);
        // a faint ember glow inside, growing with the value
        const float v = bipolar ? std::abs (pos - 0.5f) * 2.0f : pos;
        if (enabled && v > 0.01f)
        {
            juce::ColourGradient ember (accent.withAlpha (0.20f * v), c.x, c.y + bodyR * 0.6f, juce::Colours::transparentBlack, c.x, c.y - bodyR * 0.4f, false);
            g.setGradientFill (ember);
            g.fillEllipse (body);
        }
        g.setColour (juce::Colour (0xff5a3a30).withAlpha (0.8f));
        g.drawEllipse (body.reduced (0.5f), 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (body.expanded (0.5f), 1.0f);
    }

    // pointer: molten metal
    const float glow = bipolar ? std::abs (pos - 0.5f) * 2.0f : pos;
    juce::Path p;
    const float pw = std::max (1.6f, bodyR * (lavaFace ? 0.09f : 0.14f));
    p.addRoundedRectangle (-pw * 0.5f, -bodyR + 2.0f, pw, bodyR * (lavaFace ? 0.38f : 0.55f), pw * 0.5f);
    const auto tr = juce::AffineTransform::rotation (angle).translated (c.x, c.y);
    if (enabled && glow > 0.05f)
    {
        g.setColour (accent.withAlpha (0.35f * glow));
        g.strokePath (p, juce::PathStrokeType (pw * 1.6f), tr);
    }
    g.setColour (lavaFace ? juce::Colour (0xfffff2d8) : text.interpolatedWith (accent2, glow * 0.85f));
    g.fillPath (p, tr);
}

void PyraLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool highlighted, bool down)
{
    using namespace colours;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    if (on)
    {
        // active: glowing lava
        g.setColour (accent.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (1.5f), 6.0f);
        juce::ColourGradient lavaG (accent2, r.getX(), r.getY(), juce::Colour (0xffc8300c), r.getX(), r.getBottom(), false);
        g.setGradientFill (lavaG);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colour (0xffffe0a0).withAlpha (0.8f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        return;
    }
    // custom colours (e.g. RANDOM buttons) keep their tint, as dark embers
    const bool custom = b.findColour (juce::TextButton::buttonColourId) != knobBody;
    auto top = custom ? base.brighter (0.15f) : juce::Colour (0xff311a15);
    auto bottom = custom ? base.darker (0.35f) : juce::Colour (0xff1a0c0a);
    if (highlighted) { top = top.brighter (0.15f); bottom = bottom.brighter (0.12f); }
    if (down) { std::swap (top, bottom); }
    g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (highlighted ? accent.withAlpha (0.8f) : edge.brighter (0.15f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3.0f, r.getRight() - 3.0f);
}

void PyraLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    using namespace colours;
    auto r = b.getLocalBounds().toFloat();
    const float sz = std::min (14.0f, r.getHeight() - 2.0f);
    auto box = juce::Rectangle<float> (r.getX() + 2.0f, r.getCentreY() - sz * 0.5f, sz, sz);
    const bool on = b.getToggleState();

    if (on)
    {
        g.setColour (accent.withAlpha (0.3f));
        g.fillEllipse (box.expanded (3.0f));
        g.setGradientFill (juce::ColourGradient (accent2, box.getCentreX(), box.getY(), juce::Colour (0xffc02a0a), box.getCentreX(), box.getBottom(), false));
        g.fillEllipse (box);
    }
    else
    {
        g.setColour (juce::Colour (0xff140806));
        g.fillEllipse (box);
    }
    g.setColour (highlighted ? text : edge.brighter (0.3f));
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
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a1612), 0.0f, 0.0f, juce::Colour (0xff160a08), 0.0f, (float) h, false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (cb.hasKeyboardFocus (false) ? accent : edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    const float a = (float) h * 0.22f;
    juce::Path arrow;
    const float ax = (float) w - (float) h * 0.55f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - a, ay - a * 0.5f, ax + a, ay - a * 0.5f, ax, ay + a * 0.6f);
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
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff261310), 0.0f, 0.0f, juce::Colour (0xff120807), 0.0f, (float) h, false));
    g.fillAll();
    g.setColour (colours::edge.brighter (0.2f));
    g.drawRect (0, 0, w, h);
}

void PyraLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& t, int w, int h)
{
    g.setColour (juce::Colour (0xf0180a08));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 4.0f);
    g.setColour (colours::accent.withAlpha (0.7f));
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
