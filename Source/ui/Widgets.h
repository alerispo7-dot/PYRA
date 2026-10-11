#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Params.h"

class PyraAudioProcessor;

namespace pyra
{
/** Set by the editor: tells the undo history that the user just did something in the window. */
inline std::function<void()>& userActivityHook() { static std::function<void()> f; return f; }

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
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    /** Opens a small text box to type an exact value. */
    void showValueEditor();
    /** Area under the knob that shows the name / value (click it to type a value). */
    juce::Rectangle<int> labelArea() const { return getLocalBounds().removeFromBottom (15); }

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
    float hover = 0.0f;   // animated 0..1 when the mouse is over the knob
    std::unique_ptr<juce::TextEditor> valueEditor;
};

/** Readable value text: "75%", "2.50 kHz", "120 ms", "-6.0 dB". */
juce::String formatParamValue (const juce::RangedAudioParameter&);
bool isPercentParam (const juce::RangedAudioParameter&);

/** Turns typed text ("2.5k", "-6 dB", "350ms", "75%") into a parameter value. */
float parseTypedValue (const juce::RangedAudioParameter&, const juce::String& text, bool& ok);

//==============================================================================
class ParamCombo : public juce::ComboBox
{
public:
    /** Groups of choices shown as submenus (like a browser): { "Low Pass", { 14, 0, 1 } }, ... */
    using Groups = std::vector<std::pair<juce::String, std::vector<int>>>;

    ParamCombo (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, bool translateItems = true,
                const Groups* groups = nullptr);

    /** Submenus for wavetables ("wt"), LFO shapes ("lfo"), filter types ("filter"), sub shapes ("sub"), noise types ("noise"). */
    static const Groups* groupsFor (const juce::String& kind);

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    std::unique_ptr<juce::ParameterAttachment> groupedAttachment;
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
/** Small dice button: random values for one section. Right-click = small variation. */
class DiceButton : public juce::Button
{
public:
    DiceButton();
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void mouseDown (const juce::MouseEvent& e) override { rightClick = e.mods.isPopupMenu(); juce::Button::mouseDown (e); }
    void clicked() override;
    std::function<void (bool variation)> onRoll;

private:
    bool rightClick = false;
};

/** Section frame with a title (and optionally a dice in the top-right corner). */
class Panel : public juce::Component, private juce::ComponentListener
{
public:
    explicit Panel (juce::String title = {}) : title (std::move (title)) {}
    ~Panel() override { removeComponentListener (this); }
    void paint (juce::Graphics&) override;
    juce::String title;
    int titleX = 10;
    static constexpr int headerH = 26;
    juce::Colour tint { 0xffff3a1c };   // header colour: sets sections apart (lava, blood, violet, molten orange)

    /** The section's on/off switch: the header lights up when it is on, dims when off. */
    void setPowerButton (juce::Button* b)
    {
        power = b;
        if (b != nullptr) b->onStateChange = [this] { if (power != nullptr && power->getToggleState() != lastPower) { lastPower = power->getToggleState(); repaint(); } };
        lastPower = b != nullptr && b->getToggleState();
    }

    /** A darker recessed area that groups controls inside a panel. */
    static void drawInset (juce::Graphics&, juce::Rectangle<float>);

    /** Shows the dice: `roll (variation)` randomizes this section only. */
    void enableDice (std::function<void (bool variation)> roll);
    static constexpr int diceSpace = 26;   // keep this much free at the right of the header

private:
    void componentMovedOrResized (juce::Component&, bool, bool) override { placeDice(); }
    void placeDice() { if (dice != nullptr) { dice->setBounds (getWidth() - 25, 4, 20, 18); dice->toFront (false); } }
    std::unique_ptr<DiceButton> dice;
    juce::Button* power = nullptr;
    bool lastPower = false;
    juce::Path headerCrack;
    int headerCrackWidth = -1;
};

//==============================================================================
/** One parameter changed by dragging inside a display (with a proper gesture, for automation and undo). */
struct ParamGesture
{
    juce::RangedAudioParameter* p = nullptr;
    float start = 0.0f;
    bool active = false;
    void begin()          { if (p != nullptr && ! active) { start = p->getValue(); p->beginChangeGesture(); active = true; } }
    void set (float norm) { if (p != nullptr) { norm = juce::jlimit (0.0f, 1.0f, norm); if (std::abs (norm - p->getValue()) > 1.0e-7f) p->setValueNotifyingHost (norm); } }
    void end()            { if (p != nullptr && active) p->endChangeGesture(); active = false; }
};

/** Wavetable screen: drag up / down = wavetable position, left / right = warp amount. */
class WavetableView : public juce::Component, public juce::SettableTooltipClient
{
public:
    WavetableView (juce::AudioProcessorValueTreeState& state, int oscIndex);
    void visibilityChanged() override { refresh(); }
    void paint (juce::Graphics&) override;
    void refresh();
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    std::atomic<float>* wt; std::atomic<float>* pos; std::atomic<float>* on;
    std::atomic<float>* warp; std::atomic<float>* warpAmt;
    ParamGesture posG, warpG;
    int lastWt = -1; float lastPos = -1, lastWarp = -1; bool lastOn = false;
};

/** Envelope screen: drag the points (attack, decay / sustain, release) and the middle of the curves (their bend). */
class EnvelopeView : public juce::Component, public juce::SettableTooltipClient
{
public:
    EnvelopeView (juce::AudioProcessorValueTreeState& state);
    void setEnvelope (int index) { env = index; refresh (true); }
    void setLive (float value) { if (std::abs (value - live) > 0.004f) { live = value; repaint(); } }
    void paint (juce::Graphics&) override;
    void refresh (bool force = false);
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    enum Handle { HAttack, HDecay, HRelease, CAttack, CDecay, CRelease, HCount };
    void layout (juce::Path& curve, juce::Point<float>* handles);
    int handleAt (juce::Point<float>);
    juce::AudioProcessorValueTreeState& st;
    int env = 0, hover = -1, dragging = -1;
    float live = 0;
    juce::String signature;
    ParamGesture gx, gy;
    juce::Point<float> downPos;
};

/** LFO screen: draw your own shape with the mouse (it becomes the "Custom (drawn)" shape). Right-click for tools. */
class LfoView : public juce::Component, public juce::SettableTooltipClient
{
public:
    LfoView (juce::AudioProcessorValueTreeState& state, LfoDrawnSet* drawn = nullptr);
    void setLfo (int index) { lfo = index; refresh (true); }
    void setLive (float value) { if (std::abs (value - live) > 0.004f) { live = value; repaint(); } }
    void paint (juce::Graphics&) override;
    void refresh (bool force = false);
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (8.0f, 10.0f); }
    void makeCustom();
    void drawAt (juce::Point<float>, bool connect);
    void showTools();
    juce::AudioProcessorValueTreeState& st;
    LfoDrawnSet* drawn;
    int lfo = 0, lastShape = -1, lastCounter = -1, lastIndex = -1;
    float live = 0, lastValue = 0;
};

} // namespace pyra
