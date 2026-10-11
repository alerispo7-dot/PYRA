#include "Widgets.h"
#include "PyraLookAndFeel.h"
#include "../Lang.h"
#include "../dsp/Wavetables.h"
#include "../dsp/Envelope.h"
#include "HellArt.h"

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

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
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
    setTooltip (param->getName (64) + (dest > 0 ? "\n" + tr ("Drag a mod source here to modulate. Right-click for options.") : juce::String()));
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
    const bool showValue = slider.isMouseButtonDown() || slider.isMouseOverOrDragging() || isMouseOver (true);
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
ParamCombo::ParamCombo (juce::AudioProcessorValueTreeState& state, const juce::String& id, bool translateItems)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
    {
        int i = 1;
        for (auto& c : p->choices)
            addItem (translateItems ? tr (c) : c, i++);
    }
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id, *this);
    if (auto* p = state.getParameter (id)) setTooltip (p->getName (64));
}

ParamToggle::ParamToggle (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& text)
{
    setButtonText (text);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id, *this);
    if (auto* p = state.getParameter (id)) setTooltip (p->getName (64));
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
    setInterceptsMouseClicks (false, false);
    refresh();
}

void WavetableView::refresh()
{
    const int w = (int) std::lround (wt->load());
    const float p = pos->load();
    const bool o = on->load() > 0.5f;
    if (w != lastWt || std::abs (p - lastPos) > 0.0005f || o != lastOn)
    {
        lastWt = w; lastPos = p; lastOn = o;
        repaint();
    }
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
}

//==============================================================================
EnvelopeView::EnvelopeView (juce::AudioProcessorValueTreeState& s) : st (s) { setInterceptsMouseClicks (false, false); }

void EnvelopeView::refresh (bool force)
{
    juce::String sig;
    for (auto* n : { "dly", "att", "hold", "dec", "sus", "rel", "aCrv", "dCrv", "rCrv" })
        sig << st.getRawParameterValue (pid::env (env, n))->load() << ",";
    if (force || sig != signature) { signature = sig; repaint(); }
}

void EnvelopeView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::display);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (colours::edge.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);

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

    juce::Path p;
    float u = 0.0f;
    p.startNewSubPath (X (0), Y (0));
    u += widths[0]; p.lineTo (X (u), Y (0));
    for (int i = 1; i <= 24; ++i) { const float t = (float) i / 24.0f; p.lineTo (X (u + widths[1] * t), Y (curveShape (t, aC))); }
    u += widths[1];
    u += widths[2]; p.lineTo (X (u), Y (1));
    for (int i = 1; i <= 24; ++i) { const float t = (float) i / 24.0f; p.lineTo (X (u + widths[3] * t), Y (1.0f - (1.0f - sus) * curveShape (t, dC))); }
    u += widths[3];
    u += widths[4]; p.lineTo (X (u), Y (sus));
    for (int i = 1; i <= 24; ++i) { const float t = (float) i / 24.0f; p.lineTo (X (u + widths[5] * t), Y (sus * (1.0f - curveShape (t, rC)))); }

    juce::Path fill (p);
    fill.lineTo (area.getRight(), area.getBottom());
    fill.lineTo (area.getX(), area.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.35f), 0, area.getY(),
                                             colours::accent.withAlpha (0.02f), 0, area.getBottom(), false));
    g.fillPath (fill);
    g.setColour (colours::lavaCore);
    g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (live > 0.001f)
    {
        g.setColour (colours::mod);
        g.fillEllipse (area.getRight() - 6.0f, Y (live) - 3.0f, 6.0f, 6.0f);
    }
}

//==============================================================================
LfoView::LfoView (juce::AudioProcessorValueTreeState& s) : st (s) { setInterceptsMouseClicks (false, false); }

void LfoView::refresh (bool force)
{
    const int shape = (int) std::lround (st.getRawParameterValue (pid::lfo (lfo, "shape"))->load());
    if (force || shape != lastShape) { lastShape = shape; repaint(); }
}

void LfoView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::display);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (colours::edge.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    auto area = r.reduced (8.0f, 10.0f);

    g.setColour (colours::edge);
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    const int shape = lastShape < 0 ? 0 : lastShape;
    juce::Path p;
    const int points = 200;
    Rng rng; rng.seed (99);
    float held = rng.bi(), prev = rng.bi(), next = rng.bi();
    const int cycles = (shape >= 9) ? 3 : 1;
    int lastCyc = 0;
    for (int i = 0; i <= points; ++i)
    {
        const float x = (float) i / (float) points * (float) cycles;
        const int cyc = (int) x;
        if (cyc != lastCyc) { lastCyc = cyc; held = rng.bi(); prev = next; next = rng.bi(); }
        const float ph = x - (float) cyc;
        const float v = Lfo::shapeValue (shape, ph, held, prev, next);
        const float px = area.getX() + (float) i / (float) points * area.getWidth();
        const float py = area.getCentreY() - v * area.getHeight() * 0.48f;
        if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
    }
    g.setColour (colours::mod.withAlpha (0.25f));
    g.strokePath (p, juce::PathStrokeType (4.0f));
    g.setColour (colours::mod);
    g.strokePath (p, juce::PathStrokeType (1.6f));

    const bool uni = st.getRawParameterValue (pid::lfo (lfo, "uni"))->load() > 0.5f;
    const float lv = uni ? live * 2.0f - 1.0f : live;
    g.setColour (colours::accent2);
    g.fillEllipse (area.getRight() - 6.0f, area.getCentreY() - lv * area.getHeight() * 0.48f - 3.0f, 6.0f, 6.0f);
}

} // namespace pyra
