#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pyra
{
namespace colours
{
    const juce::Colour bg        { 0xff111114 };
    const juce::Colour panel     { 0xff19191e };
    const juce::Colour panelHi   { 0xff202027 };
    const juce::Colour edge      { 0xff2b2b33 };
    const juce::Colour text      { 0xffe9e6e1 };
    const juce::Colour textDim   { 0xff8c8994 };
    const juce::Colour accent    { 0xffff6a2b };   // ember orange
    const juce::Colour accent2   { 0xffffb04a };   // glow
    const juce::Colour hot       { 0xffe8402a };
    const juce::Colour mod       { 0xff46c8ff };   // modulation (cyan)
    const juce::Colour knobBody  { 0xff26262d };
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

    static juce::Font font (float height, bool bold = false);
};

} // namespace pyra
