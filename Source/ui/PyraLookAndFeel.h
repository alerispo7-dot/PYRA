#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pyra
{
namespace colours
{
    // "inferno" palette: soot, embers, lava, bone; violet for modulation (the eyes-castle sky)
    const juce::Colour bg        { 0xff0c0606 };
    const juce::Colour panel     { 0xff1b0e0c };
    const juce::Colour panelHi   { 0xff28140f };
    const juce::Colour edge      { 0xff4a1d14 };
    const juce::Colour text      { 0xfff2e4d2 };   // bone
    const juce::Colour textDim   { 0xffbea595 };   // brighter than before: readable on soot
    const juce::Colour accent    { 0xffff5a1a };   // lava
    const juce::Colour accent2   { 0xffffac32 };   // molten glow
    const juce::Colour hot       { 0xffe0281a };   // blood red
    const juce::Colour mod       { 0xffb578ff };   // modulation (hell violet)
    const juce::Colour knobBody  { 0xff2a1713 };
    const juce::Colour display   { 0xff0d0504 };   // screens (wavetable, envelope, filter...)
}

class PyraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PyraLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    static juce::Font font (float height, bool bold = false);      // Barlow: labels and values
    static juce::Font titleFont (float height);                    // Barlow Condensed Bold: titles, tabs, buttons
    static juce::Font displayFont (float height);                  // New Rocker: big knob labels
    static juce::Font logoFont (float height);                     // Unifraktur Cook: the PYRA name (gothic)

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
};

} // namespace pyra
