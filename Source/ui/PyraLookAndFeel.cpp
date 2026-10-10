#include "PyraLookAndFeel.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace pyra
{
PyraLookAndFeel::PyraLookAndFeel()
{
    using namespace colours;
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
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffd9d6d0));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff1b1b20));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff6a6870));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, accent.withAlpha (0.8f));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, accent2.withAlpha (0.3f));
    setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
}

juce::Font PyraLookAndFeel::font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

void PyraLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    using namespace colours;
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float track = std::max (2.0f, radius * 0.13f);
    const float arcR = radius - track * 0.5f;

    // track
    juce::Path bgArc;
    bgArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (edge);
    g.strokePath (bgArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc (bipolar sliders start at the centre)
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path valArc;
        valArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, std::min (from, angle), std::max (from, angle), true);
        const bool enabled = s.isEnabled();
        g.setColour (enabled ? accent : textDim);
        g.strokePath (valArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // body
    const float bodyR = radius - track * 2.0f;
    juce::ColourGradient grad (knobBody.brighter (0.25f), c.x, c.y - bodyR, knobBody.darker (0.4f), c.x, c.y + bodyR, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // pointer, glowing with the value
    const float glow = bipolar ? std::abs (pos - 0.5f) * 2.0f : pos;
    juce::Path p;
    const float pw = std::max (1.5f, bodyR * 0.14f);
    p.addRoundedRectangle (-pw * 0.5f, -bodyR + 2.0f, pw, bodyR * 0.55f, pw * 0.5f);
    g.setColour (text.interpolatedWith (accent2, glow * 0.8f));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void PyraLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    using namespace colours;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto base = on ? accent : knobBody;
    if (highlighted) base = base.brighter (0.12f);
    if (down) base = base.darker (0.2f);
    g.setColour (base);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? accent2 : edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void PyraLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    using namespace colours;
    auto r = b.getLocalBounds().toFloat();
    const float sz = std::min (14.0f, r.getHeight() - 2.0f);
    auto box = juce::Rectangle<float> (r.getX() + 2.0f, r.getCentreY() - sz * 0.5f, sz, sz);
    const bool on = b.getToggleState();

    g.setColour (on ? accent : knobBody);
    g.fillEllipse (box);
    if (on)
    {
        g.setColour (accent2.withAlpha (0.35f));
        g.drawEllipse (box.expanded (2.0f), 2.0f);
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
    g.setColour (knobBody);
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

juce::Font PyraLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return font (std::min (14.0f, (float) h * 0.55f), true);
}

juce::Label* PyraLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (12.0f));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

} // namespace pyra
