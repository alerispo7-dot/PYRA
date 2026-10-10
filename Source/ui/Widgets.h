#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Params.h"

class PyraAudioProcessor;

namespace pyra
{
//==============================================================================
/** Reads the modulation matrix so knobs can draw their modulation rings and
    so sources can be dropped onto knobs. */
class ModMatrixAccess
{
public:
    explicit ModMatrixAccess (juce::AudioProcessorValueTreeState& state);

    struct Assignment { int slot, src; float amt; };
    void getAssignments (int dest, juce::Array<Assignment>& out) const;

    /** Adds src -> dest in the first free slot. Returns the slot or -1 if full. */
    int assign (int src, int dest, float amount = 0.5f);
    void clearSlot (int slot);
    void setAmount (int slot, float amount);

    static int destForParam (const juce::String& paramId);

private:
    juce::AudioProcessorValueTreeState& st;
    juce::RangedAudioParameter* src[kNumModSlots] {}, *dst[kNumModSlots] {}, *amt[kNumModSlots] {};
};

//==============================================================================
/** Rotary knob bound to a parameter. Accepts dropped modulation sources. */
class Knob : public juce::Component,
             public juce::DragAndDropTarget,
             public juce::SettableTooltipClient
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& label,
          ModMatrixAccess* matrix);

    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dragOver = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    /** Called by the editor's timer; repaints only when modulation changed. */
    void refreshModulation();

    juce::Slider slider;

private:
    void showMenu();

    juce::AudioProcessorValueTreeState& st;
    juce::String paramId, label;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    ModMatrixAccess* matrix;
    int dest = -1;
    bool dragOver = false;
    juce::Array<ModMatrixAccess::Assignment> mods;
    float modSignature = 0.0f;
};

//==============================================================================
class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, bool translateItems = true);

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class ParamToggle : public juce::ToggleButton
{
public:
    ParamToggle (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text = {});

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

//==============================================================================
/** Small label you can drag onto any knob to modulate it. Also acts as a tab. */
class ModSourceBadge : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModSourceBadge (int sourceIndex, const juce::String& text);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void setSelected (bool s) { if (selected != s) { selected = s; repaint(); } }
    void setLiveValue (float v) { if (std::abs (v - live) > 0.01f) { live = v; repaint(); } }

    std::function<void()> onClick;
    const int source;

private:
    juce::String text;
    bool selected = false, dragging = false;
    float live = 0.0f;
};

//==============================================================================
/** Section frame with a title. */
class Panel : public juce::Component
{
public:
    explicit Panel (juce::String title = {}) : title (std::move (title)) {}
    void paint (juce::Graphics&) override;
    juce::String title;
    int titleX = 10;
    static constexpr int headerH = 26;
};

//==============================================================================
class WavetableView : public juce::Component
{
public:
    WavetableView (juce::AudioProcessorValueTreeState& state, int oscIndex);
    void visibilityChanged() override { refresh(); }
    void paint (juce::Graphics&) override;
    void refresh();

private:
    std::atomic<float>* wt; std::atomic<float>* pos; std::atomic<float>* on;
    std::atomic<float>* warp; std::atomic<float>* warpAmt;
    int lastWt = -1; float lastPos = -1; bool lastOn = false;
};

class EnvelopeView : public juce::Component
{
public:
    EnvelopeView (juce::AudioProcessorValueTreeState& state);
    void setEnvelope (int index) { env = index; refresh (true); }
    void setLive (float value) { if (std::abs (value - live) > 0.004f) { live = value; repaint(); } }
    void paint (juce::Graphics&) override;
    void refresh (bool force = false);

private:
    juce::AudioProcessorValueTreeState& st;
    int env = 0;
    float live = 0;
    juce::String signature;
};

class LfoView : public juce::Component
{
public:
    LfoView (juce::AudioProcessorValueTreeState& state);
    void setLfo (int index) { lfo = index; refresh (true); }
    void setLive (float value) { if (std::abs (value - live) > 0.004f) { live = value; repaint(); } }
    void paint (juce::Graphics&) override;
    void refresh (bool force = false);

private:
    juce::AudioProcessorValueTreeState& st;
    int lfo = 0, lastShape = -1;
    float live = 0;
};

} // namespace pyra
