#include "Widgets.h"
#include "PyraLookAndFeel.h"
#include "../Lang.h"
#include "../dsp/Wavetables.h"
#include "../dsp/Envelope.h"
#include "HellArt.h"
#include "Prefs.h"
#include "../ParamHelp.h"

namespace pyra
{
using lang::tr;

//==============================================================================
ModMatrixAccess::ModMatrixAccess (juce::AudioProcessorValueTreeState& state) : st (state)
{
    for (int i = 0; i < kNumModSlots; ++i)
    {
        src[i] = st.getParameter (pid::mod (i, "src"));
        dst[i] = st.getParameter (pid::mod (i, "dst"));
        amt[i] = st.getParameter (pid::mod (i, "amt"));
    }
}

void ModMatrixAccess::getAssignments (int dest, juce::Array<Assignment>& out) const
{
    out.clearQuick();
    if (dest <= 0) return;
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int d = (int) std::lround (dst[i]->convertFrom0to1 (dst[i]->getValue()));
        const int s = (int) std::lround (src[i]->convertFrom0to1 (src[i]->getValue()));
        if (d == dest && s > 0)
            out.add ({ i, s, amt[i]->convertFrom0to1 (amt[i]->getValue()) });
    }
}

static void setParam (juce::RangedAudioParameter* p, float value)
{
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (value));
    p->endChangeGesture();
}

int ModMatrixAccess::assign (int s, int dest, float amount)
{
    // already assigned? just return that slot
    for (int i = 0; i < kNumModSlots; ++i)
        if ((int) std::lround (src[i]->convertFrom0to1 (src[i]->getValue())) == s
            && (int) std::lround (dst[i]->convertFrom0to1 (dst[i]->getValue())) == dest)
            return i;

    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int cs = (int) std::lround (src[i]->convertFrom0to1 (src[i]->getValue()));
        const int cd = (int) std::lround (dst[i]->convertFrom0to1 (dst[i]->getValue()));
        if (cs == 0 && cd == 0)
        {
            setParam (src[i], (float) s);
            setParam (dst[i], (float) dest);
            setParam (amt[i], amount);
            return i;
        }
    }
    return -1;
}

void ModMatrixAccess::clearSlot (int slot)
{
    setParam (src[slot], 0.0f);
    setParam (dst[slot], 0.0f);
    setParam (amt[slot], 0.0f);
}

void ModMatrixAccess::setAmount (int slot, float amount) { setParam (amt[slot], amount); }

int ModMatrixAccess::destForParam (const juce::String& paramId)
{
    for (int d = 1; d < DCount; ++d)
        if (destParamId (d) == paramId) return d;
    return -1;
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& lbl, ModMatrixAccess* m)
    : st (state), paramId (id), label (lbl), matrix (m)
{
    param = st.getParameter (paramId);
    jassert (param != nullptr);

    // SETTINGS > PERSONALIZE: drag up/down or around the knob, its speed, and the mouse wheel
    slider.setSliderStyle (prefs::get (prefs::KnobDrag) == 1 ? juce::Slider::Rotary : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    { const int sp = prefs::get (prefs::KnobSpeed); slider.setMouseDragSensitivity (sp == 0 ? 600 : (sp == 2 ? 110 : 220)); }
    slider.setScrollWheelEnabled (prefs::get (prefs::WheelKnobs) != 0);
    slider.setVelocityBasedMode (false);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.onValueChange = [this] { repaint(); };
    slider.onDragStart = [this] { repaint(); };
    slider.onDragEnd = [this] { repaint(); };
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, paramId, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    dest = matrix != nullptr ? ModMatrixAccess::destForParam (paramId) : -1;
    slider.addMouseListener (this, false);
    const auto help = paramHelp (paramId);
    setTooltip ((help.isNotEmpty() ? help : param->getName (64))
                + (dest > 0 ? "\n" + tr ("Drag a mod source here to modulate. Right-click for options.") : juce::String()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom ((bool) getProperties().getWithDefault ("bigLabel", false) ? 24 : 15);
    const int sz = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (sz, sz));
}

void Knob::paint (juce::Graphics& g)
{
    const bool hovering = slider.isMouseButtonDown() || slider.isMouseOverOrDragging() || isMouseOver (true);
    // SETTINGS > PERSONALIZE "values under the knobs": values all the time, the name when the mouse is over it
    const bool showValue = prefs::get (prefs::ShowValues) != 0 ? (slider.isMouseButtonDown() || ! hovering) : hovering;
    const bool big = (bool) getProperties().getWithDefault ("bigLabel", false);
    auto r = getLocalBounds().removeFromBottom (big ? 24 : 15);
    if (big && ! showValue)
    {   // CHAOS / FOCUS / MOTION: engraved letters with a burning glow
        g.setFont (PyraLookAndFeel::displayFont (19.0f));
        g.setColour (colours::accent.withAlpha (0.35f));
        g.drawText (tr (label), r.translated (0, 1), juce::Justification::centred);
        g.setColour (colours::accent2.interpolatedWith (colours::text, 0.3f));
        g.drawText (tr (label), r, juce::Justification::centred);
        return;
    }
    g.setFont (PyraLookAndFeel::font (big ? 14.0f : 11.5f, ! showValue));
    g.setColour (showValue ? colours::accent2 : colours::textDim.interpolatedWith (colours::text, 0.35f * hover));
    if (showValue && ! slider.isMouseButtonDown())
    {   // a hint that the value can be clicked and typed
        g.setColour (colours::knobBody);
        g.fillRoundedRectangle (r.toFloat().reduced (2.0f, 0.5f), 3.0f);
        g.setColour (colours::accent2);
    }
    g.drawFittedText (showValue ? formatParamValue (*param) : tr (label),
                      r, juce::Justification::centred, 1, 0.7f);
}

void Knob::paintOverChildren (juce::Graphics& g)
{
    const auto b = slider.getBounds().toFloat();
    if (hover > 0.01f)
    {   // hover: a soft ember halo fades in (and out)
        const float rr = std::min (b.getWidth(), b.getHeight()) * 0.5f;
        g.setColour (colours::accent2.withAlpha (0.22f * hover));
        g.drawEllipse (b.getCentreX() - rr + 1.0f, b.getCentreY() - rr + 1.0f, rr * 2.0f - 2.0f, rr * 2.0f - 2.0f, 1.5f);
    }
    if (dragOver)
    {
        g.setColour (colours::mod.withAlpha (0.35f));
        g.fillEllipse (b.reduced (2.0f));
    }
    if (mods.isEmpty()) return;

    const auto rp = slider.getRotaryParameters();
    const float start = rp.startAngleRadians, end = rp.endAngleRadians;
    const float p = (float) slider.valueToProportionOfLength (slider.getValue());
    const auto c = b.getCentre();
    float radius = std::min (b.getWidth(), b.getHeight()) * 0.5f + 1.0f;

    for (auto& m : mods)
    {
        const float q = juce::jlimit (0.0f, 1.0f, p + m.amt);
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, radius, radius, 0.0f, start + std::min (p, q) * (end - start),
                           start + std::max (p, q) * (end - start), true);
        g.setColour (colours::mod.withAlpha (0.9f));
        g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        radius += 3.0f;
    }
}

void Knob::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showMenu(); return; }
    // a click (not a drag) on the value under the knob: type a value
    if (e.eventComponent == this && labelArea().contains (e.getPosition()) && e.getDistanceFromDragStart() < 3)
        showValueEditor();
}

bool isPercentParam (const juce::RangedAudioParameter& p)
{
    if (dynamic_cast<const juce::AudioParameterFloat*> (&p) == nullptr || p.getLabel().isNotEmpty()) return false;
    const auto& r = p.getNormalisableRange();
    return std::abs (r.start) <= 1.5f && std::abs (r.end) <= 1.5f;
}

juce::String formatParamValue (const juce::RangedAudioParameter& p)
{
    if (dynamic_cast<const juce::AudioParameterFloat*> (&p) == nullptr) return lang::tr (p.getCurrentValueAsText());
    const float v = p.convertFrom0to1 (p.getValue());
    const auto unit = p.getLabel();
    auto num = [] (double x, int decimals) { return juce::String (x, decimals); };
    if (isPercentParam (p))
    {
        const bool bipolar = p.getNormalisableRange().start < 0.0f;
        const int pc = juce::roundToInt (v * 100.0f);
        return (bipolar && pc > 0 ? "+" : "") + juce::String (pc) + "%";
    }
    if (unit == "Hz")
    {
        const float a = std::abs (v);
        if (a >= 1000.0f) return num (v / 1000.0, a >= 10000.0f ? 1 : 2) + " kHz";
        return num (v, a < 10.0f ? 2 : (a < 100.0f ? 1 : 0)) + " Hz";
    }
    if (unit == "s")
    {
        if (v < 1.0f) return num (v * 1000.0, v < 0.01f ? 1 : 0) + " ms";
        return num (v, 2) + " s";
    }
    if (unit == "ms") return num (v, v < 10.0f ? 1 : 0) + " ms";
    if (unit == "dB") return num (v, 1) + " dB";
    if (unit == "st" || unit == "ct") return (v > 0.0f ? "+" : "") + num (v, std::abs (v) < 10.0f && unit == "ct" ? 1 : 0) + " " + unit;
    if (unit == ":1") return num (v, 1) + ":1";
    return num (v, std::abs (v) < 10.0f ? 2 : 1) + (unit.isNotEmpty() ? " " + unit : juce::String());
}

float parseTypedValue (const juce::RangedAudioParameter& p, const juce::String& textIn, bool& ok)
{
    auto text = textIn.trim().toLowerCase().replaceCharacter (',', '.');
    ok = false;
    if (text.isEmpty()) return 0.0f;

    // choices: accept the name
    if (auto* c = dynamic_cast<const juce::AudioParameterChoice*> (&p))
    {
        for (int i = 0; i < c->choices.size(); ++i)
            if (c->choices[i].toLowerCase() == text || lang::tr (c->choices[i]).toLowerCase() == text) { ok = true; return (float) i; }
    }

    const auto numberPart = text.initialSectionContainingOnly ("0123456789.-+");
    if (numberPart.isEmpty() || numberPart == "-" || numberPart == "+" || numberPart == ".") return 0.0f;
    double v = numberPart.getDoubleValue();
    const auto unit = text.substring (numberPart.length()).trim();
    if (isPercentParam (p)) v /= 100.0;                           // shown as %, typed as %
    else if (unit.startsWith ("k")) v *= 1000.0;                  // 2.5k = 2500 Hz
    else if (unit.startsWith ("ms") && p.getLabel() == "s") v *= 0.001;
    else if (unit.startsWith ("s") && p.getLabel() == "ms") v *= 1000.0;
    if (! std::isfinite (v)) return 0.0f;
    ok = true;
    const auto& r = p.getNormalisableRange();
    return r.snapToLegalValue (juce::jlimit (r.start, r.end, (float) v));
}

void Knob::showValueEditor()
{
    valueEditor = std::make_unique<juce::TextEditor>();
    auto& ed = *valueEditor;
    ed.setFont (PyraLookAndFeel::font (12.0f, true));
    ed.setJustification (juce::Justification::centred);
    ed.setColour (juce::TextEditor::backgroundColourId, colours::bg);
    ed.setColour (juce::TextEditor::outlineColourId, colours::accent);
    ed.setColour (juce::TextEditor::focusedOutlineColourId, colours::accent2);
    ed.setColour (juce::TextEditor::textColourId, colours::text);
    ed.setColour (juce::TextEditor::highlightColourId, colours::accent.withAlpha (0.4f));
    ed.setIndents (2, 2);
    ed.setText (formatParamValue (*param), false);
    ed.setSelectAllWhenFocused (true);
    const int w = juce::jmax (getWidth(), 64);
    auto area = juce::Rectangle<int> ((getWidth() - w) / 2, getHeight() - 20, w, 20);
    addAndMakeVisible (ed);
    ed.setBounds (area);
    ed.setTooltip (tr ("Type a value and press Enter (e.g. 2.5k, 120 ms, -6). Esc cancels."));

    juce::Component::SafePointer<Knob> safe (this);
    auto close = [safe] (bool apply)
    {
        if (safe == nullptr || safe->valueEditor == nullptr) return;
        if (apply)
        {
            bool ok = false;
            const float v = parseTypedValue (*safe->param, safe->valueEditor->getText(), ok);
            if (ok)
            {
                if (auto& hook = userActivityHook()) hook();
                safe->param->beginChangeGesture();
                safe->param->setValueNotifyingHost (safe->param->convertTo0to1 (v));
                safe->param->endChangeGesture();
            }
        }
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->valueEditor.reset(); safe->repaint(); } });
    };
    ed.onReturnKey = [close] { close (true); };
    ed.onEscapeKey = [close] { close (false); };
    ed.onFocusLost = [close] { close (true); };
    ed.grabKeyboardFocus();
}

void Knob::showMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader (param->getName (64));
    menu.addItem (2, tr ("Type a value..."));
    menu.addItem (1, tr ("Reset to default"));
    if (matrix != nullptr && ! mods.isEmpty())
    {
        menu.addSeparator();
        for (auto& m : mods)
            menu.addItem (100 + m.slot, tr ("Remove modulation") + ": " + choices::modSources()[m.src]
                                            + " (" + juce::String (juce::roundToInt (m.amt * 100.0f)) + "%)");
    }

    juce::Component::SafePointer<Knob> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 2) { safe->showValueEditor(); return; }
        if (r == 1)
        {
            safe->param->beginChangeGesture();
            safe->param->setValueNotifyingHost (safe->param->getDefaultValue());
            safe->param->endChangeGesture();
        }
        else if (r >= 100 && safe->matrix != nullptr)
        {
            safe->matrix->clearSlot (r - 100);
            safe->refreshModulation();
        }
    });
}

bool Knob::isInterestedInDragSource (const SourceDetails& d)
{
    return dest > 0 && d.description.toString().startsWith ("modsrc:");
}

void Knob::itemDropped (const SourceDetails& d)
{
    dragOver = false;
    const int s = d.description.toString().fromFirstOccurrenceOf ("modsrc:", false, false).getIntValue();
    if (matrix != nullptr && dest > 0 && s > 0)
        matrix->assign (s, dest, 0.5f);
    refreshModulation();
    repaint();
}

void Knob::refreshModulation()
{
    // hover animation (~200 ms), driven by the editor timer
    const float target = (isMouseOver (true) || slider.isMouseButtonDown()) ? 1.0f : 0.0f;
    if (std::abs (target - hover) > 0.001f)
    {
        hover = target > hover ? std::min (1.0f, hover + 0.2f) : std::max (0.0f, hover - 0.14f);
        repaint();
    }
    if (matrix == nullptr || dest <= 0) return;
    matrix->getAssignments (dest, mods);
    float sig = (float) mods.size() * 1000.0f;
    for (auto& m : mods) sig += m.amt * 7.0f + (float) m.src * 0.13f + (float) m.slot * 0.01f;
    if (sig != modSignature) { modSignature = sig; repaint(); }
}

//==============================================================================
ParamCombo::ParamCombo (juce::AudioProcessorValueTreeState& state, const juce::String& id, bool translateItems, const Groups* groups)
{
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id));
    if (choice != nullptr && groups != nullptr)
    {
        // a browser-like menu: one submenu per category (item id = choice index + 1, whatever the order in the menu)
        std::vector<bool> placed ((size_t) choice->choices.size(), false);
        auto* root = getRootMenu();
        for (auto& [name, items] : *groups)
        {
            juce::PopupMenu sub;
            for (int idx : items)
                if (juce::isPositiveAndBelow (idx, choice->choices.size()))
                {
                    sub.addItem (idx + 1, translateItems ? tr (choice->choices[idx]) : choice->choices[idx]);
                    placed[(size_t) idx] = true;
                }
            if (sub.getNumItems() > 0) root->addSubMenu (tr (name), sub);
        }
        juce::PopupMenu other;
        for (int i = 0; i < choice->choices.size(); ++i)
            if (! placed[(size_t) i]) other.addItem (i + 1, translateItems ? tr (choice->choices[i]) : choice->choices[i]);
        if (other.getNumItems() > 0) root->addSubMenu (tr ("Other"), other);

        groupedAttachment = std::make_unique<juce::ParameterAttachment> (*choice, [this] (float v)
        {
            setSelectedId (juce::roundToInt (v) + 1, juce::dontSendNotification);
        });
        onChange = [this] { if (getSelectedId() > 0) groupedAttachment->setValueAsCompleteGesture ((float) (getSelectedId() - 1)); };
        groupedAttachment->sendInitialUpdate();
    }
    else
    {
        if (choice != nullptr)
        {
            int i = 1;
            for (auto& c : choice->choices)
                addItem (translateItems ? tr (c) : c, i++);
        }
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id, *this);
    }
    if (auto* p = state.getParameter (id)) setTooltip (paramHelp (id).isNotEmpty() ? paramHelp (id) : p->getName (64));
}

const ParamCombo::Groups* ParamCombo::groupsFor (const juce::String& kind)
{
    static const Groups lfo {
        { "Basic", { 0, 1, 2, 3, 4, 5, 11, 12, 37 } },
        { "Curves", { 7, 8, 13, 14, 15, 16, 17, 18, 33, 36 } },
        { "Steps & Gates", { 6, 19, 20, 21, 22, 23, 24, 25, 26 } },
        { "Random", { 9, 10 } },
        { "Movement", { 27, 28, 29, 30, 31, 32, 34, 35 } },
        { "Custom", { kLfoShapeCustom } } };
    static const Groups filter {
        { "Low Pass", { FLp6, FLp12, FLp24, FLp36, FLadder, FDiode, FMs20 } },
        { "High Pass", { FHp6, FHp12, FHp24, FHp36 } },
        { "Band Pass", { FBp12, FBp24, FDualBp, FResonator } },
        { "Notch & EQ", { FNotch, FNotch24, FPeak, FLowShelf, FHighShelf, FAllpass } },
        { "Special", { FCombPlus, FCombMinus, FFormant, FPhaser } } };
    static const Groups sub {
        { "Basic", { 0, 1, 2, 3, 15 } },
        { "Pulse", { 4, 5, 6 } },
        { "Harmonic", { 7, 8, 9, 10, 11 } },
        { "Driven", { 12, 13, 14 } } };
    static const Groups noise {
        { "Colours", { 0, 1, 2, 5, 6 } },
        { "Textures", { 7, 8, 9, 10, 11, 12, 13 } },
        { "Digital", { 4, 14, 15, 3, 16 } } };
    static const Groups wt = []
    {
        Groups g;
        const auto& cats = Wavetables::categories();
        const auto& each = Wavetables::categoryOfEach();
        for (auto& c : cats)
        {
            std::vector<int> items;
            for (int i = 0; i < each.size(); ++i) if (each[i] == c) items.push_back (i);
            g.push_back ({ c, items });
        }
        return g;
    }();
    if (kind == "lfo") return &lfo;
    if (kind == "filter") return &filter;
    if (kind == "sub") return &sub;
    if (kind == "noise") return &noise;
    if (kind == "wt") return &wt;
    return nullptr;
}

ParamToggle::ParamToggle (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& text)
{
    setButtonText (text);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id, *this);
    if (auto* p = state.getParameter (id)) setTooltip (paramHelp (id).isNotEmpty() ? paramHelp (id) : p->getName (64));
}

//==============================================================================
ModSourceBadge::ModSourceBadge (int sourceIndex, const juce::String& t) : source (sourceIndex), text (t)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip (tr ("Click to show, drag onto a knob to modulate it"));
}

void ModSourceBadge::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (selected ? colours::panelHi.brighter (0.1f) : colours::knobBody);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (selected ? colours::mod : colours::edge);
    g.drawRoundedRectangle (r, 4.0f, selected ? 1.5f : 1.0f);

    // live activity bar
    const float lv = juce::jlimit (0.0f, 1.0f, std::abs (live));
    if (lv > 0.0f)
    {
        g.setColour (colours::mod.withAlpha (0.55f));
        g.fillRoundedRectangle (r.removeFromBottom (3.0f).withWidth (r.getWidth() * lv), 1.5f);
    }

    g.setColour (selected ? colours::text : colours::textDim);
    g.setFont (PyraLookAndFeel::font (12.0f, true));
    g.drawText (text, getLocalBounds(), juce::Justification::centred);
}

void ModSourceBadge::mouseDown (const juce::MouseEvent&) { dragging = false; }

void ModSourceBadge::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() > 4)
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragging = true;
            c->startDragging ("modsrc:" + juce::String (source), this);
        }
}

void ModSourceBadge::mouseUp (const juce::MouseEvent&)
{
    if (! dragging && onClick) onClick();
    dragging = false;
}

//==============================================================================
DiceButton::DiceButton() : juce::Button ("dice")
{
    setTooltip (tr ("Random for this section only. Right-click: small variation."));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void DiceButton::clicked()
{
    if (auto& hook = userActivityHook()) hook();
    if (onRoll) onRoll (rightClick || juce::ModifierKeys::currentModifiers.isShiftDown());
    rightClick = false;
}

void DiceButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().withSizeKeepingCentre (16.0f, 16.0f);
    if (down) r = r.reduced (0.8f);
    // a little die carved from bone-grey stone; it catches fire under the mouse
    if (highlighted)
    {
        g.setColour (colours::accent.withAlpha (0.3f));
        g.fillRoundedRectangle (r.expanded (2.0f), 5.0f);
    }
    g.setGradientFill (juce::ColourGradient (highlighted ? juce::Colour (0xffff5a30) : juce::Colour (0xff3a302c), r.getX(), r.getY(),
                                             highlighted ? juce::Colour (0xff9a0c08) : juce::Colour (0xff181210), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.5f);
    g.setColour (highlighted ? juce::Colour (0xffffb090) : juce::Colour (0xff6a5850));
    g.drawRoundedRectangle (r, 3.5f, 1.0f);
    g.setColour (highlighted ? juce::Colours::white : colours::text.withAlpha (0.85f));
    const float d = 2.6f;
    auto pip = [&] (float fx, float fy) { g.fillEllipse (r.getX() + r.getWidth() * fx - d * 0.5f, r.getY() + r.getHeight() * fy - d * 0.5f, d, d); };
    pip (0.28f, 0.28f); pip (0.72f, 0.28f); pip (0.5f, 0.5f); pip (0.28f, 0.72f); pip (0.72f, 0.72f);
}

void Panel::enableDice (std::function<void (bool)> roll)
{
    if (dice == nullptr)
    {
        dice = std::make_unique<DiceButton>();
        addAndMakeVisible (*dice);
        addComponentListener (this);
    }
    dice->onRoll = std::move (roll);
    placeDice();
}

//==============================================================================
void Panel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto seed = (uint32_t) title.hashCode();
    const bool powered = power == nullptr || power->getToggleState();
    juce::Path slab;
    slab.addRoundedRectangle (r, 7.0f);

    // body: a slab of dark cracked stone (each panel shows a different part of the rock)
    art::fillStone (g, slab, { -(float) (seed % 509u), -(float) ((seed >> 9) % 509u) }, 0.0f, 0.62f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.50f), 0.0f, r.getY(), juce::Colours::black.withAlpha (0.70f), 0.0f, r.getBottom(), false));
    g.fillPath (slab);

    // a lava vein seeping out of one corner (the section colour, brighter when the section is on)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (slab);
        const bool right = (seed & 1u) != 0;
        const float cx = right ? r.getRight() : r.getX();
        const float len = std::min (90.0f, r.getWidth() * 0.3f);
        auto vein = art::crackLine ({ cx, r.getBottom() - 4.0f - (float) (seed % 17u) },
                                    { cx + (right ? -len : len), r.getBottom() - 2.0f - (float) ((seed >> 5) % 30u) }, seed, 2.2f, true);
        art::drawLavaVein (g, vein, 1.0f, powered ? 0.5f : 0.12f, tint);
    }

    // header: darker band, the section colour glowing from the left, closed by a crack of lava
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (slab);
        auto head = r.withHeight ((float) headerH);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (head);
        g.setGradientFill (juce::ColourGradient (tint.withAlpha (powered ? 0.30f : 0.08f), head.getX(), 0.0f, tint.withAlpha (0.0f),
                                                 head.getX() + head.getWidth() * 0.75f, 0.0f, false));
        g.fillRect (head);
        if (headerCrack.isEmpty() || headerCrackWidth != getWidth())
        {
            headerCrackWidth = getWidth();
            headerCrack = art::crackLine ({ head.getX() + 4.0f, head.getBottom() }, { head.getRight() - 4.0f, head.getBottom() }, seed, 1.1f, true);
        }
        art::drawLavaVein (g, headerCrack, 0.9f, powered ? 0.75f : 0.15f, tint);
    }

    // chiselled rim: black outside, pale lip on top, section colour along the top edge
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawRoundedRectangle (r.expanded (0.5f), 7.5f, 1.4f);
    g.setColour (juce::Colour (0xff3e322e));
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (1.5f), 5.5f, 1.0f);
    g.setGradientFill (juce::ColourGradient (tint.withAlpha (powered ? 0.9f : 0.25f), r.getX(), 0.0f, tint.withAlpha (0.0f), r.getRight() * 0.8f, 0.0f, false));
    g.fillRoundedRectangle (r.getX() + 6.0f, r.getY(), r.getWidth() * 0.6f, 1.4f, 0.7f);

    if (title.isNotEmpty())
    {
        auto tr = getLocalBounds().removeFromTop (headerH).withTrimmedLeft (titleX).withTrimmedRight (10);
        g.setFont (PyraLookAndFeel::displayFont (15.5f));
        const auto t = title.toUpperCase();
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawText (t, tr.translated (1, 1), juce::Justification::centredLeft);
        if (powered && power != nullptr)
        {   // switched on: the letters burn
            g.setColour (tint.withAlpha (0.5f));
            g.drawText (t, tr.translated (0, 1).expanded (0, 1), juce::Justification::centredLeft);
        }
        g.setColour (powered ? tint.interpolatedWith (colours::text, 0.45f) : colours::textDim.withAlpha (0.8f));
        g.drawText (t, tr, juce::Justification::centredLeft);
    }
}

void Panel::drawInset (juce::Graphics& g, juce::Rectangle<float> r)
{
    // a polished obsidian window set into the stone
    g.setColour (juce::Colour (0xd0070404));
    g.fillRoundedRectangle (r, 6.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0.0f, r.getY(), juce::Colours::transparentBlack, 0.0f, r.getY() + 10.0f, false));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r.expanded (0.5f), 6.5f, 1.0f);
    g.setColour (juce::Colour (0xff4a3a35).withAlpha (0.8f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

//==============================================================================
WavetableView::WavetableView (juce::AudioProcessorValueTreeState& s, int o)
{
    wt = s.getRawParameterValue (pid::osc (o, "wt"));
    pos = s.getRawParameterValue (pid::osc (o, "pos"));
    on = s.getRawParameterValue (pid::osc (o, "on"));
    warp = s.getRawParameterValue (pid::osc (o, "warp"));
    warpAmt = s.getRawParameterValue (pid::osc (o, "warpAmt"));
    posG.p = s.getParameter (pid::osc (o, "pos"));
    warpG.p = s.getParameter (pid::osc (o, "warpAmt"));
    setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);
    setTooltip (tr ("Drag up / down to move through the wavetable, left / right to change the WARP amount. Double-click: back to the first frame."));
    refresh();
}

void WavetableView::refresh()
{
    const int w = (int) std::lround (wt->load());
    const float p = pos->load();
    const bool o = on->load() > 0.5f;
    const float wa = warpAmt->load() * (warp->load() > 0.5f ? 1.0f : 0.0f);
    if (w != lastWt || std::abs (p - lastPos) > 0.0005f || o != lastOn || std::abs (wa - lastWarp) > 0.001f)
    {
        lastWt = w; lastPos = p; lastOn = o; lastWarp = wa;
        repaint();
    }
}

void WavetableView::mouseDown (const juce::MouseEvent&)
{
    if (auto& hook = userActivityHook()) hook();
    posG.begin(); warpG.begin();
}

void WavetableView::mouseDrag (const juce::MouseEvent& e)
{
    const float dy = (float) -e.getDistanceFromDragStartY() / (float) std::max (40, getHeight()) * 0.8f;
    const float dx = (float) e.getDistanceFromDragStartX() / (float) std::max (60, getWidth()) * 0.8f;
    const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
    posG.set (posG.start + dy * fine);
    if (warp->load() > 0.5f) warpG.set (warpG.start + dx * fine);
    if (auto& hook = userActivityHook()) hook();
}

void WavetableView::mouseUp (const juce::MouseEvent&) { posG.end(); warpG.end(); }

void WavetableView::mouseDoubleClick (const juce::MouseEvent&)
{
    posG.begin(); posG.set (0.0f); posG.end();
}

void WavetableView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    posG.begin();
    posG.set (posG.p != nullptr ? posG.p->getValue() + w.deltaY * 0.15f : 0.0f);
    posG.end();
}

void WavetableView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::display);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (colours::edge.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);

    const auto& t = Wavetables::get().table (lastWt < 0 ? 0 : lastWt);
    const int frames = t.numFrames;
    const int mip = 3;
    const int len = Wavetables::mipLength (mip);
    const float depth = frames > 1 ? std::min (r.getWidth() * 0.18f, 46.0f) : 0.0f;
    auto area = r.reduced (8.0f, 10.0f);
    const float w = area.getWidth() - depth, h = area.getHeight() - depth * 0.5f;
    const juce::Colour wave = lastOn ? colours::accent : colours::textDim;

    auto pathFor = [&] (int frame, float ox, float oy, bool interpolated)
    {
        juce::Path p;
        const int points = 128;
        for (int i = 0; i <= points; ++i)
        {
            const float ph = (float) i / (float) points;
            float v;
            if (interpolated)
                v = Wavetables::read (t, lastPos, mip, std::min (ph, 0.9999f));
            else
            {
                const float* d = t.level (frame, mip);
                v = d[std::min ((int) (ph * (float) len), len - 1)];
            }
            const float x = area.getX() + ox + ph * w;
            const float y = area.getY() + depth * 0.5f + oy + (0.5f - 0.45f * v) * h;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    };

    if (frames > 1)
        for (int f = frames - 1; f >= 0; --f)
        {
            const float k = (float) f / (float) (frames - 1);
            g.setColour (wave.withAlpha (0.08f + 0.06f * (1.0f - std::abs (k - lastPos))));
            g.strokePath (pathFor (f, k * depth, -k * depth * 0.5f, false), juce::PathStrokeType (1.0f));
        }

    const float k = lastPos;
    auto cur = pathFor (0, k * depth, -k * depth * 0.5f, true);
    g.setColour (wave.withAlpha (0.25f));
    g.strokePath (cur, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (lastOn ? colours::lavaCore : colours::textDim);
    g.strokePath (cur, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (colours::textDim);
    g.setFont (PyraLookAndFeel::font (10.5f));
    if (frames > 1)
        g.drawText (juce::String (juce::roundToInt (lastPos * (float) (frames - 1)) + 1) + "/" + juce::String (frames),
                    r.reduced (6.0f, 3.0f), juce::Justification::topRight);
    if (isMouseOver (true))
        g.drawText (warp->load() > 0.5f ? tr ("drag: up/down = position, left/right = warp") : tr ("drag up/down = position"),
                    r.reduced (6.0f, 3.0f), juce::Justification::bottomLeft);
}

//==============================================================================
EnvelopeView::EnvelopeView (juce::AudioProcessorValueTreeState& s) : st (s)
{
    setTooltip (tr ("Drag the points: attack (top), decay and sustain (middle), release (end). Drag the middle of a curve up or down to bend it. Double-click a point to reset it."));
}

void EnvelopeView::refresh (bool force)
{
    juce::String sig;
    for (auto* n : { "dly", "att", "hold", "dec", "sus", "rel", "aCrv", "dCrv", "rCrv" })
        sig << st.getRawParameterValue (pid::env (env, n))->load() << ",";
    if (force || sig != signature) { signature = sig; repaint(); }
}

void EnvelopeView::layout (juce::Path& p, juce::Point<float>* hs)
{
    auto r = getLocalBounds().toFloat();
    auto val = [this] (const char* n) { return st.getRawParameterValue (pid::env (env, n))->load(); };
    const float times[] = { val ("dly"), val ("att"), val ("hold"), val ("dec"), 0.0f, val ("rel") };
    float widths[6];
    float total = 0.0f;
    for (int i = 0; i < 6; ++i)
    {
        widths[i] = i == 4 ? 0.35f : std::sqrt (times[i]) + (i == 1 || i == 3 || i == 5 ? 0.05f : 0.0f);
        if (i == 0 || i == 2) widths[i] = times[i] > 0.0005f ? std::sqrt (times[i]) : 0.0f;
        total += widths[i];
    }

    auto area = r.reduced (8.0f, 10.0f);
    const float sus = val ("sus");
    const float aC = val ("aCrv"), dC = val ("dCrv"), rC = val ("rCrv");
    auto X = [&] (float units) { return area.getX() + units / total * area.getWidth(); };
    auto Y = [&] (float v) { return area.getBottom() - v * area.getHeight(); };

    p.clear();
    float u = 0.0f;
    p.startNewSubPath (X (0), Y (0));
    u += widths[0]; p.lineTo (X (u), Y (0));
    for (int i = 1; i <= 24; ++i) { const float t = (float) i / 24.0f; p.lineTo (X (u + widths[1] * t), Y (curveShape (t, aC))); }
    hs[CAttack] = { X (u + widths[1] * 0.5f), Y (curveShape (0.5f, aC)) };
    u += widths[1];
    hs[HAttack] = { X (u), Y (1) };
    u += widths[2]; p.lineTo (X (u), Y (1));
    for (int i = 1; i <= 24; ++i) { const float t = (float) i / 24.0f; p.lineTo (X (u + widths[3] * t), Y (1.0f - (1.0f - sus) * curveShape (t, dC))); }
    hs[CDecay] = { X (u + widths[3] * 0.5f), Y (1.0f - (1.0f - sus) * curveShape (0.5f, dC)) };
    u += widths[3];
    hs[HDecay] = { X (u), Y (sus) };
    u += widths[4]; p.lineTo (X (u), Y (sus));
    for (int i = 1; i <= 24; ++i) { const float t = (float) i / 24.0f; p.lineTo (X (u + widths[5] * t), Y (sus * (1.0f - curveShape (t, rC)))); }
    hs[CRelease] = { X (u + widths[5] * 0.5f), Y (sus * (1.0f - curveShape (0.5f, rC))) };
    u += widths[5];
    hs[HRelease] = { X (u), Y (0) };
}

int EnvelopeView::handleAt (juce::Point<float> pt)
{
    juce::Path p; juce::Point<float> hs[HCount];
    layout (p, hs);
    int best = -1; float bestD = 12.0f;
    for (int i = 0; i < HCount; ++i)
    {
        const float d = hs[i].getDistanceFrom (pt) * (i >= CAttack ? 1.25f : 1.0f);   // points win over curves
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void EnvelopeView::mouseMove (const juce::MouseEvent& e)
{
    const int h = handleAt (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h < 0 ? juce::MouseCursor::NormalCursor : (h >= CAttack ? juce::MouseCursor::UpDownResizeCursor
                                                                             : juce::MouseCursor::DraggingHandCursor));
}

void EnvelopeView::mouseDown (const juce::MouseEvent& e)
{
    dragging = handleAt (e.position);
    downPos = e.position;
    gx = {}; gy = {};
    auto param = [this] (const char* n) { return st.getParameter (pid::env (env, n)); };
    switch (dragging)
    {
        case HAttack:  gx.p = param ("att"); break;
        case HDecay:   gx.p = param ("dec"); gy.p = param ("sus"); break;
        case HRelease: gx.p = param ("rel"); break;
        case CAttack:  gy.p = param ("aCrv"); break;
        case CDecay:   gy.p = param ("dCrv"); break;
        case CRelease: gy.p = param ("rCrv"); break;
        default: return;
    }
    gx.begin(); gy.begin();
    if (auto& hook = userActivityHook()) hook();
}

void EnvelopeView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
    const float dx = (e.position.x - downPos.x) / (float) std::max (60, getWidth()) * 1.2f * fine;
    const float dy = (downPos.y - e.position.y) / (float) std::max (40, getHeight() - 20) * fine;
    if (gx.p != nullptr) gx.set (gx.start + dx);
    if (dragging == HDecay && gy.p != nullptr)
    {   // sustain follows the mouse directly
        const float level = juce::jlimit (0.0f, 1.0f, (getHeight() - 10.0f - e.position.y) / (float) std::max (10, getHeight() - 20));
        gy.set (gy.p->convertTo0to1 (level));
    }
    else if (gy.p != nullptr)
    {   // bend: up = the curve bulges up
        const bool invert = dragging == CDecay || dragging == CRelease;
        gy.set (gy.start + (invert ? -dy : dy) * 0.8f);
    }
    if (auto& hook = userActivityHook()) hook();
}

void EnvelopeView::mouseUp (const juce::MouseEvent&) { gx.end(); gy.end(); dragging = -1; }

void EnvelopeView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = handleAt (e.position);
    auto reset = [this] (const char* n)
    {
        if (auto* p = st.getParameter (pid::env (env, n))) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
    };
    switch (h)
    {
        case HAttack: reset ("att"); break;
        case HDecay: reset ("dec"); reset ("sus"); break;
        case HRelease: reset ("rel"); break;
        case CAttack: reset ("aCrv"); break;
        case CDecay: reset ("dCrv"); break;
        case CRelease: reset ("rCrv"); break;
        default: break;
    }
}

void EnvelopeView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::display);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (colours::edge.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);

    auto area = r.reduced (8.0f, 10.0f);
    juce::Path p; juce::Point<float> hs[HCount];
    layout (p, hs);

    juce::Path fill (p);
    fill.lineTo (area.getRight(), area.getBottom());
    fill.lineTo (area.getX(), area.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.35f), 0, area.getY(),
                                             colours::accent.withAlpha (0.02f), 0, area.getBottom(), false));
    g.fillPath (fill);
    g.setColour (colours::lavaCore);
    g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // the points you can drag
    const bool over = isMouseOver (true) || dragging >= 0;
    for (int i = 0; i < HCount; ++i)
    {
        const bool curve = i >= CAttack;
        if (curve && ! over) continue;
        const bool hot = i == hover || i == dragging;
        const float rad = curve ? 3.0f : (hot ? 5.5f : 4.0f);
        g.setColour (hot ? colours::accent.withAlpha (0.35f) : juce::Colours::transparentBlack);
        g.fillEllipse (hs[i].x - rad * 2.0f, hs[i].y - rad * 2.0f, rad * 4.0f, rad * 4.0f);
        g.setColour (curve ? colours::textDim.withAlpha (0.8f) : juce::Colour (0xfff0e6df));
        g.fillEllipse (hs[i].x - rad, hs[i].y - rad, rad * 2.0f, rad * 2.0f);
        g.setColour (colours::accent);
        g.drawEllipse (hs[i].x - rad, hs[i].y - rad, rad * 2.0f, rad * 2.0f, 1.2f);
    }

    if (live > 0.001f)
    {
        g.setColour (colours::mod);
        g.fillEllipse (area.getRight() - 6.0f, area.getBottom() - live * area.getHeight() - 3.0f, 6.0f, 6.0f);
    }
}

//==============================================================================
LfoView::LfoView (juce::AudioProcessorValueTreeState& s, LfoDrawnSet* d) : st (s), drawn (d)
{
    if (drawn != nullptr)
    {
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        setTooltip (tr ("Draw your own LFO shape with the mouse (it becomes the Custom shape). Right-click: tools (smooth, flip, reverse, random...)."));
    }
    else setInterceptsMouseClicks (false, false);
}

void LfoView::refresh (bool force)
{
    const int shape = (int) std::lround (st.getRawParameterValue (pid::lfo (lfo, "shape"))->load());
    const int counter = drawn != nullptr ? drawn->changeCounter.load() : 0;
    if (force || shape != lastShape || counter != lastCounter) { lastShape = shape; lastCounter = counter; repaint(); }
}

void LfoView::makeCustom()
{
    if (drawn == nullptr) return;
    const int shape = (int) std::lround (st.getRawParameterValue (pid::lfo (lfo, "shape"))->load());
    if (shape == kLfoShapeCustom) return;
    // start from what is on the screen now, then switch the LFO to the drawn shape
    auto& t = drawn->lfo[(size_t) lfo];
    for (int i = 0; i < kLfoPoints; ++i)
        t.pts[(size_t) i].store (Lfo::shapeValue (shape, (float) i / (float) kLfoPoints, 0.3f, -0.4f, 0.6f));
    if (auto* p = st.getParameter (pid::lfo (lfo, "shape")))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) kLfoShapeCustom));
        p->endChangeGesture();
    }
    ++drawn->changeCounter;
}

void LfoView::drawAt (juce::Point<float> pt, bool connect)
{
    auto a = plotArea();
    const int idx = juce::jlimit (0, kLfoPoints - 1, (int) std::floor ((pt.x - a.getX()) / a.getWidth() * (float) kLfoPoints));
    const float v = juce::jlimit (-1.0f, 1.0f, (a.getCentreY() - pt.y) / (a.getHeight() * 0.48f));
    auto& t = drawn->lfo[(size_t) lfo];
    if (connect && lastIndex >= 0 && lastIndex != idx)
    {
        const int step = idx > lastIndex ? 1 : -1;
        for (int i = lastIndex; i != idx; i += step)
            t.pts[(size_t) i].store (lastValue + (v - lastValue) * (float) (i - lastIndex) / (float) (idx - lastIndex));
    }
    t.pts[(size_t) idx].store (v);
    lastIndex = idx; lastValue = v;
    ++drawn->changeCounter;
    if (auto& hook = userActivityHook()) hook();
    refresh();
}

void LfoView::mouseDown (const juce::MouseEvent& e)
{
    if (drawn == nullptr) return;
    if (e.mods.isPopupMenu()) { showTools(); return; }
    makeCustom();
    lastIndex = -1;
    drawAt (e.position, false);
}

void LfoView::mouseDrag (const juce::MouseEvent& e)
{
    if (drawn == nullptr || e.mods.isPopupMenu()) return;
    drawAt (e.position, true);
}

void LfoView::mouseUp (const juce::MouseEvent&) { lastIndex = -1; }

void LfoView::showTools()
{
    juce::PopupMenu m;
    m.addItem (1, tr ("Draw: start from the current shape"));
    m.addSeparator();
    m.addItem (2, tr ("Smooth"));
    m.addItem (3, tr ("Flip (upside down)"));
    m.addItem (4, tr ("Reverse (back to front)"));
    m.addItem (5, tr ("Random"));
    m.addItem (6, tr ("Reset to sine"));
    juce::Component::SafePointer<LfoView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto& self = *safe;
        self.makeCustom();
        auto& t = self.drawn->lfo[(size_t) self.lfo];
        float v[kLfoPoints];
        for (int i = 0; i < kLfoPoints; ++i) v[i] = t.pts[(size_t) i].load();
        juce::Random rnd;
        for (int i = 0; i < kLfoPoints; ++i)
        {
            float n = v[i];
            if (r == 2) n = (v[(i + kLfoPoints - 1) % kLfoPoints] + 2.0f * v[i] + v[(i + 1) % kLfoPoints]) * 0.25f;
            if (r == 3) n = -v[i];
            if (r == 4) n = v[kLfoPoints - 1 - i];
            if (r == 5) n = rnd.nextFloat() * 2.0f - 1.0f;
            if (r == 6) n = std::sin (6.2831853f * (float) i / (float) kLfoPoints);
            t.pts[(size_t) i].store (n);
        }
        ++self.drawn->changeCounter;
        if (auto& hook = userActivityHook()) hook();
        self.refresh (true);
    });
}

void LfoView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::display);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (colours::edge.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    auto area = plotArea();

    g.setColour (colours::edge);
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    const int shape = lastShape < 0 ? 0 : lastShape;
    const LfoDrawn* table = drawn != nullptr ? &drawn->lfo[(size_t) lfo] : nullptr;
    juce::Path p;
    const int points = 200;
    Rng rng; rng.seed (99);
    float held = rng.bi(), prev = rng.bi(), next = rng.bi();
    const int cycles = (shape == 9 || shape == 10) ? 3 : 1;
    int lastCyc = 0;
    for (int i = 0; i <= points; ++i)
    {
        const float x = (float) i / (float) points * (float) cycles;
        const int cyc = (int) x;
        if (cyc != lastCyc) { lastCyc = cyc; held = rng.bi(); prev = next; next = rng.bi(); }
        const float ph = x - (float) cyc;
        const float v = Lfo::shapeValue (shape, std::min (ph, 0.9999f), held, prev, next, table);
        const float px = area.getX() + (float) i / (float) points * area.getWidth();
        const float py = area.getCentreY() - v * area.getHeight() * 0.48f;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (colours::mod.withAlpha (0.25f));
    g.strokePath (p, juce::PathStrokeType (4.0f));
    g.setColour (colours::mod);
    g.strokePath (p, juce::PathStrokeType (1.6f));

    if (shape == kLfoShapeCustom && table != nullptr)
        for (int i = 0; i < kLfoPoints; ++i)
        {
            const float px = area.getX() + (float) i / (float) kLfoPoints * area.getWidth();
            const float py = area.getCentreY() - table->pts[(size_t) i].load() * area.getHeight() * 0.48f;
            g.setColour (juce::Colour (0xfff0e6df).withAlpha (0.85f));
            g.fillEllipse (px - 2.0f, py - 2.0f, 4.0f, 4.0f);
        }

    if (drawn != nullptr && isMouseOver (true))
    {
        g.setColour (colours::textDim);
        g.setFont (PyraLookAndFeel::font (10.5f));
        g.drawText (tr ("draw with the mouse - right-click: tools"), r.reduced (6.0f, 3.0f), juce::Justification::bottomLeft);
    }

    const bool uni = st.getRawParameterValue (pid::lfo (lfo, "uni"))->load() > 0.5f;
    const float lv = uni ? live * 2.0f - 1.0f : live;
    g.setColour (colours::accent2);
    g.fillEllipse (area.getRight() - 6.0f, area.getCentreY() - lv * area.getHeight() * 0.48f - 3.0f, 6.0f, 6.0f);
}

} // namespace pyra
