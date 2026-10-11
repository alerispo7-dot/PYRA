#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pyra
{
namespace colours
{
    // the imprint of the logo: cracked dark stone, red lava veins, bone-grey stone highlights
    const juce::Colour bg        { 0xff070505 };
    const juce::Colour panel     { 0xff161211 };   // dark stone
    const juce::Colour panelHi   { 0xff221c1a };
    const juce::Colour edge      { 0xff3d302c };   // stone edge
    const juce::Colour text      { 0xffefe6df };   // pale stone / bone
    const juce::Colour textDim   { 0xffad9f98 };   // weathered stone
    const juce::Colour accent    { 0xffff3a1c };   // lava red (main colour)
    const juce::Colour accent2   { 0xffff9a3c };   // molten orange (voice, macros, highlights)
    const juce::Colour hot       { 0xffd8142a };   // blood red (filters, CHAOS)
    const juce::Colour mod       { 0xffb46cf0 };   // modulation: cursed violet, easy to tell apart from the lava
    const juce::Colour knobBody  { 0xff241d1b };
    const juce::Colour display   { 0xff0a0606 };   // obsidian screens
    const juce::Colour lavaCore  { 0xffff6236 };   // the bright line of lava drawn in the screens
}

class PyraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PyraLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
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
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize,
                        bool mouseOver, bool mouseDown) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
};

} // namespace pyra
