#include <array>
#include <complex>
// Aeola — editor implementation. Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later.
#include "PluginEditor.h"
#include "Land.h"
#include <cmath>

using namespace aeola;
using juce::Colour; using juce::Graphics; using juce::Rectangle; using juce::Point;

// Type: the system sans-serif, as in modern synth UIs. Section titles are small tracked capitals.
static juce::Font F(float h, int style = 0) { return juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), std::max(9.5f, h), style)); }
static juce::Font Sans(float h, const char* weight = "Light") { return juce::Font(juce::FontOptions("Helvetica Neue", h, 0).withStyle(weight)); }
static juce::Font CapsFont(float h = 9.5f) { return juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), h, juce::Font::bold).withKerningFactor(0.14f)); }
static void caps(Graphics& g, const juce::String& t, Rectangle<float> r, Colour c = col::dim, juce::Justification j = juce::Justification::centredLeft)
{ g.setColour(c); g.setFont(CapsFont()); g.drawText(t.toUpperCase(), r.toNearestInt(), j); }

// Flat panel: charcoal fill, one hairline, small radius.
static void glassPanel(Graphics& g, Rectangle<float> r, float radius, Colour line, float fillAlpha = 1.0f)
{
    g.setColour(col::panel.withAlpha(fillAlpha)); g.fillRoundedRectangle(r, radius);
    g.setColour(line); g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
}

// ============================== look & feel ==============================================
AeolaLookAndFeel::AeolaLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, col::bg0);
    setColour(juce::Slider::textBoxTextColourId, col::text);
    setColour(juce::Slider::textBoxOutlineColourId, Colour(0x00000000));
    setColour(juce::Slider::textBoxBackgroundColourId, Colour(0x00000000));
    setColour(juce::Slider::textBoxHighlightColourId, col::accent.withAlpha(0.4f));
    setColour(juce::Label::textColourId, col::text);
    setColour(juce::ComboBox::textColourId, col::text);
    setColour(juce::PopupMenu::backgroundColourId, Colour(0xff1b1d20));
    setColour(juce::PopupMenu::textColourId, col::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha(0.28f));
    setColour(juce::PopupMenu::headerTextColourId, col::accent);
    setColour(juce::TooltipWindow::backgroundColourId, Colour(0xf00e1012));
    setColour(juce::TooltipWindow::textColourId, col::text);
    setColour(juce::TooltipWindow::outlineColourId, col::panelEdge);
    setColour(juce::TextEditor::backgroundColourId, Colour(0xff141618));
    setColour(juce::TextEditor::textColourId, col::text);
    setColour(juce::AlertWindow::backgroundColourId, Colour(0xff1b1d20));
    setColour(juce::AlertWindow::textColourId, col::text);
}
juce::Font AeolaLookAndFeel::getComboBoxFont(juce::ComboBox&) { return F(12.0f); }
juce::Font AeolaLookAndFeel::getPopupMenuFont() { return F(13.0f); }

juce::Label* AeolaLookAndFeel::createSliderTextBox(juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox(s);
    l->setFont(F(11.0f)); l->setJustificationType(juce::Justification::centred);
    l->setColour(juce::Label::textColourId, col::text.withAlpha(0.92f));
    l->setColour(juce::Label::outlineWhenEditingColourId, col::accent);
    return l;
}

void AeolaLookAndFeel::drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    auto b = Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(3.0f);
    const float r = std::min(20.0f, std::min(b.getWidth(), b.getHeight()) / 2.0f); auto c = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);
    const bool hot = s.isMouseOverOrDragging();
    juce::Path track; track.addCentredArc(c.x, c.y, r, r, 0, a0, a1, true);
    g.setColour(Colour(0xff2d3035)); g.strokePath(track, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val; val.addCentredArc(c.x, c.y, r, r, 0, a0, ang, true);
    g.setColour(col::accent.withAlpha(hot ? 1.0f : 0.85f)); g.strokePath(val, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // body: dark disc with a soft top light
    const float br = std::max(2.0f, r - 4.0f); auto body = Rectangle<float>(br * 2.0f, br * 2.0f).withCentre(c);
    g.setColour(Colour(0x66000000)); g.fillEllipse(body.translated(0.0f, 1.2f));
    g.setGradientFill(juce::ColourGradient(Colour(hot ? 0xff3b4045 : 0xff33373c), body.getX(), body.getY(), Colour(0xff1c1e21), body.getX(), body.getBottom(), false));
    g.fillEllipse(body);
    g.setColour(Colour(0xff0d0e10)); g.drawEllipse(body, 1.0f);
    g.setColour(col::text); g.drawLine(c.x + std::sin(ang) * 5.0f, c.y - std::cos(ang) * 5.0f, c.x + std::sin(ang) * (br - 2.0f), c.y - std::cos(ang) * (br - 2.0f), 1.8f);
    if (s.hasKeyboardFocus(true)) { g.setColour(col::accent); g.drawEllipse(Rectangle<float>(r * 2.0f + 6.0f, r * 2.0f + 6.0f).withCentre(c), 1.4f); }
}

void AeolaLookAndFeel::drawToggleButton(Graphics& g, juce::ToggleButton& b, bool hl, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = b.getToggleState();
    const bool isS = b.getButtonText() == "S", isM = b.getButtonText() == "M";
    const Colour onCol = isS ? col::accent2 : (isM ? col::bad : col::accent);
    g.setColour(on ? onCol.withAlpha(0.20f) : (hl ? Colour(0xff25282c) : Colour(0xff141618))); g.fillRoundedRectangle(r, 5.0f);
    g.setColour(on ? onCol : (hl ? Colour(0xff4a4f55) : col::panelEdge)); g.drawRoundedRectangle(r, 5.0f, 1.0f);
    if (isS || isM) { g.setColour(on ? onCol : col::dim); g.setFont(F(11.0f, juce::Font::bold)); g.drawText(b.getButtonText(), r, juce::Justification::centred); return; }
    auto tr = r; if (on) { g.setColour(col::accent); g.fillEllipse(r.getX() + 9.0f, r.getCentreY() - 2.5f, 5.0f, 5.0f); tr = r.withTrimmedLeft(8.0f); }
    g.setColour(on ? col::text : col::dim); g.setFont(F(11.5f)); g.drawText(b.getButtonText(), tr, juce::Justification::centred);
    if (b.hasKeyboardFocus(true)) { g.setColour(col::accent); g.drawRoundedRectangle(r.expanded(1.5f), 6.0f, 1.5f); }
}
void AeolaLookAndFeel::drawButtonBackground(Graphics& g, juce::Button& b, const Colour&, bool hl, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    if (b.getComponentID() == "tab")
    {   // page tabs: no box, an accent underline under the active one (drawn in drawButtonText)
        return;
    }
    const bool on = b.getToggleState();
    g.setColour(on ? col::accent.withAlpha(0.22f) : (down ? Colour(0xff30343a) : (hl ? Colour(0xff25282c) : Colour(0xff202327)))); g.fillRoundedRectangle(r, 5.0f);
    g.setColour(on ? col::accent : (hl ? Colour(0xff4a4f55) : col::panelEdge)); g.drawRoundedRectangle(r, 5.0f, 1.0f);
    if (b.hasKeyboardFocus(true)) { g.setColour(col::accent); g.drawRoundedRectangle(r.expanded(1.5f), 6.0f, 1.5f); }
}
void AeolaLookAndFeel::drawButtonText(Graphics& g, juce::TextButton& b, bool hl, bool)
{
    auto r = b.getLocalBounds().toFloat();
    if (b.getComponentID() == "tab")
    {
        const bool on = b.getToggleState();
        g.setColour(on ? col::text : (hl ? col::dim.brighter(0.2f) : Colour(0xff6a7279))); g.setFont(CapsFont(10.0f));
        g.drawText(b.getButtonText().toUpperCase(), r.toNearestInt(), juce::Justification::centredLeft);
        if (on) { g.setColour(col::accent); g.fillRect(r.getX(), r.getBottom() - 2.0f, juce::GlyphArrangement::getStringWidth(CapsFont(10.0f), b.getButtonText().toUpperCase()) + 2.0f, 2.0f); }
        return;
    }
    g.setColour(b.getToggleState() ? col::text : (b.isEnabled() ? col::text : col::dim)); g.setFont(F(11.5f));
    g.drawText(b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
}
void AeolaLookAndFeel::drawComboBox(Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb)
{
    auto r = Rectangle<float>(0, 0, (float) w, (float) h).reduced(0.5f);
    g.setColour(cb.isMouseOver() ? Colour(0xff191b1e) : Colour(0xff141618)); g.fillRoundedRectangle(r, 5.0f);
    g.setColour(cb.isPopupActive() ? col::accent.withAlpha(0.8f) : (cb.isMouseOver() ? Colour(0xff4a4f55) : col::panelEdge)); g.drawRoundedRectangle(r, 5.0f, 1.0f);
    if (cb.hasKeyboardFocus(true)) { g.setColour(col::accent); g.drawRoundedRectangle(r.expanded(1.5f), 6.0f, 1.5f); }
    juce::Path p; float cx = w - 12.0f, cy = h / 2.0f; p.startNewSubPath(cx - 3.5f, cy - 1.5f); p.lineTo(cx, cy + 2.0f); p.lineTo(cx + 3.5f, cy - 1.5f);
    g.setColour(cb.isPopupActive() ? col::accent : col::dim); g.strokePath(p, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
void AeolaLookAndFeel::drawLinearSlider(Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle style, juce::Slider& sl)
{
    if (style == juce::Slider::LinearBar)
    {   // value bar: a dark slot filled to the value, the value written in it
        auto r = Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(0.5f);
        const bool on = sl.isEnabled(), hot = sl.isMouseOverOrDragging();
        g.setColour(Colour(0xff141618)); g.fillRoundedRectangle(r, 5.0f);
        { juce::Graphics::ScopedSaveState ss(g); juce::Path clip; clip.addRoundedRectangle(r, 5.0f); g.reduceClipRegion(clip);
          g.setColour(col::accent.withAlpha(on ? (hot ? 0.40f : 0.28f) : 0.10f)); g.fillRect(r.withRight(std::max(r.getX() + 2.0f, pos))); }
        g.setColour(hot ? Colour(0xff4a4f55) : col::panelEdge); g.drawRoundedRectangle(r, 5.0f, 1.0f);
        g.setColour(on ? col::text : col::dim); g.setFont(F(10.5f)); g.drawText(sl.getTextFromValue(sl.getValue()), r.toNearestInt(), juce::Justification::centred);
        return;
    }
    if (style == juce::Slider::LinearVertical)
    {   // mixer fader: a slot, filled from the bottom, and a flat cap
        const float cx = x + w / 2.0f, bot = (float) (y + h);
        g.setColour(Colour(0xff2d3035)); g.fillRoundedRectangle(cx - 2.0f, (float) y, 4.0f, (float) h, 2.0f);
        g.setColour(col::accent.withAlpha(0.8f)); g.fillRoundedRectangle(cx - 2.0f, pos, 4.0f, std::max(0.0f, bot - pos), 2.0f);
        g.setColour(Colour(0xff3a3e44)); g.fillRoundedRectangle(cx - 11.0f, pos - 5.0f, 22.0f, 10.0f, 2.5f);
        g.setColour(col::text); g.fillRect(cx - 8.0f, pos - 0.5f, 16.0f, 1.0f);
        return;
    }
    float cy = y + h / 2.0f;
    g.setColour(Colour(0xff2d3035)); g.fillRoundedRectangle((float) x, cy - 2, (float) w, 4.0f, 2.0f);
    g.setColour(col::accent); g.fillRoundedRectangle((float) x, cy - 2, pos - x, 4.0f, 2.0f);
    g.setColour(col::text); g.fillEllipse(pos - 6, cy - 6, 12, 12);
}

// ================================ knob ===================================================
static std::unique_ptr<juce::Component> makeModRing(Knob& k);
Knob::Knob(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& label, float dialSize) : paramId(id), dial(dialSize), labelText(label)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);               // the value shows in place of the name while the pointer is over the knob
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    addAndMakeVisible(slider);
    name.setText(label, juce::dontSendNotification); name.setJustificationType(juce::Justification::centred);
    name.setFont(F(10.5f)); name.setColour(juce::Label::textColourId, col::dim); name.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(name);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, id, slider);
    if (auto* p = apvts.getParameter(id)) slider.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
    baseTip = tooltipFor(id); slider.setTooltip(baseTip + "  (right-click for options)");
    ring = makeModRing(*this); addAndMakeVisible(*ring);
    slider.addMouseListener(this, false);
    slider.onValueChange = [this] { refreshValueAbove(); if (over || slider.isMouseButtonDown()) showValue(); };
    vlab.setJustificationType(juce::Justification::centred); vlab.setFont(F(10.5f)); vlab.setColour(juce::Label::textColourId, col::text); vlab.setInterceptsMouseClicks(false, false);
    setSize(60, 66);
}
void Knob::refreshValueAbove() { if (above) vlab.setText(slider.getTextFromValue(slider.getValue()), juce::dontSendNotification); }
void Knob::setValueAbove(bool on)
{
    if (on == above) { refreshValueAbove(); return; }
    above = on; if (on) addAndMakeVisible(vlab); else removeChildComponent(&vlab);
    resized(); refreshValueAbove();
}
void Knob::showValue() { name.setText(slider.getTextFromValue(slider.getValue()), juce::dontSendNotification); name.setColour(juce::Label::textColourId, col::text); }
void Knob::showName() { name.setText(labelText, juce::dontSendNotification); name.setColour(juce::Label::textColourId, col::dim); }
void Knob::mouseEnter(const juce::MouseEvent&) { over = true; showValue(); }
void Knob::mouseExit(const juce::MouseEvent&) { over = false; if (!slider.isMouseButtonDown()) showName(); }
void Knob::mouseUp(const juce::MouseEvent&) { if (!over) showName(); }
// 1.13: the outer ring of a modulated knob. Drag it up / down to change the amount of the knob's first connection.
struct ModRing : juce::Component
{
    explicit ModRing(Knob& k) : knob(k) { setMouseCursor(juce::MouseCursor::UpDownResizeCursor); }
    bool hitTest(int x, int y) override
    {
        if (knob.proc == nullptr || knob.nConn == 0 || knob.activeSlot < 0) return false;
        const float d = juce::Point<float>((float) x, (float) y).getDistanceFrom(knob.dialCentre()), r = knob.dialRadius();
        return d >= r - 0.5f && d <= r + 6.0f;
    }
    void mouseDown(const juce::MouseEvent&) override
    {
        slot = knob.activeSlot; if (slot < 0) return;
        if (auto* p = knob.proc->apvts.getParameter(ID::modSlotId(slot, 2))) { p->beginChangeGesture(); start = p->convertFrom0to1(p->getValue()); }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (slot < 0) return;
        if (auto* p = knob.proc->apvts.getParameter(ID::modSlotId(slot, 2)))
        {
            const float a = juce::jlimit(-1.0f, 1.0f, start - (float) e.getDistanceFromDragStartY() / 120.0f);
            p->setValueNotifyingHost(p->convertTo0to1(a));
            knob.name.setText(juce::String(modSrcNames()[(int) knob.proc->P(ID::modSlotId(slot, 0))]) + " " + juce::String(juce::roundToInt(a * 100.0f)) + "%", juce::dontSendNotification);
            knob.name.setColour(juce::Label::textColourId, col::warm); knob.repaint();
        }
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        if (slot >= 0) if (auto* p = knob.proc->apvts.getParameter(ID::modSlotId(slot, 2))) p->endChangeGesture();
        slot = -1; knob.showName(); knob.refreshMod();
    }
    void mouseDoubleClick(const juce::MouseEvent&) override
    {   // back to 25 %
        const int s = knob.activeSlot; if (s < 0) return;
        if (auto* p = knob.proc->apvts.getParameter(ID::modSlotId(s, 2))) p->setValueNotifyingHost(p->convertTo0to1(0.25f));
    }
    Knob& knob; int slot = -1; float start = 0.0f;
};

static std::unique_ptr<juce::Component> makeModRing(Knob& k) { return std::make_unique<ModRing>(k); }
juce::Point<float> Knob::dialCentre() const { return slider.getBounds().toFloat().reduced(3.0f).getCentre(); }
float Knob::dialRadius() const { auto b = slider.getBounds().toFloat().reduced(3.0f); return std::min(20.0f, std::min(b.getWidth(), b.getHeight()) / 2.0f) + 3.5f; }
void Knob::refreshMod()
{
    int n = 0, first = -1; juce::StringArray parts;
    if (proc != nullptr && modDst > 0)
        for (int i = 0; i < kModSlots; ++i)
            if ((int) proc->P(ID::modSlotId(i, 1)) == modDst && proc->P(ID::modSlotId(i, 0)) > 0.5f)
            {   ++n; if (first < 0) first = i;
                const float a = proc->P(ID::modSlotId(i, 2));
                parts.add(juce::String(modSrcNames()[(int) proc->P(ID::modSlotId(i, 0))]) + " " + (a >= 0 ? "+" : "") + juce::String(juce::roundToInt(a * 100.0f)) + "%"); }
    nConn = n; activeSlot = first;
    juce::String tip = baseTip + "  (right-click for options)";
    if (n > 0) tip << "\nModulated by " << parts.joinIntoString(", ") << ". Drag the orange ring to change " << (n > 1 ? "the first amount." : "the amount.");
    if (slider.getTooltip() != tip) slider.setTooltip(tip);
}

void Knob::mouseDown(const juce::MouseEvent& e)
{   // right-click (or ctrl-click): the same menu on every knob: type a value, connect a modulation source, the connections it has (1.13)
    if (!e.mods.isPopupMenu()) return;
    if (proc != nullptr)
    {
        juce::PopupMenu m, src, cnt, alt; m.addItem(1, "Type a value...");
        for (int s = 1; s < SrcClassCount; ++s) src.addItem(100 + s, modSrcNames()[s]);
        for (int c = 0; c < kNumClasses; ++c) { cnt.addItem(100 + SrcClassCount + c, modSrcNames()[SrcClassCount + c]); alt.addItem(100 + SrcClassAlt + c, modSrcNames()[SrcClassAlt + c]); }
        src.addSubMenu("Count per class", cnt); src.addSubMenu("Altitude per class", alt);
        if (modDst > 0)
        {
            m.addSubMenu("Modulate by", src);
            bool has = false;
            for (int i = 0; i < kModSlots; ++i)
                if ((int) proc->P(ID::modSlotId(i, 1)) == modDst && proc->P(ID::modSlotId(i, 0)) > 0.5f)
                {   if (!has) { m.addSeparator(); m.addSectionHeader("Connections"); } has = true;
                    const float a = proc->P(ID::modSlotId(i, 2));
                    m.addItem(200 + i, "Remove  " + juce::String(modSrcNames()[(int) proc->P(ID::modSlotId(i, 0))]) + "  " + (a >= 0 ? "+" : "") + juce::String(juce::roundToInt(a * 100.0f)) + "%"); }
            if (has) { m.addItem(2, "Remove all"); m.addItem(3, "Show in the Matrix"); }
        }
        else m.addItem(4, "Can't be modulated", false);
        juce::Component::SafePointer<Knob> safe(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [safe](int r) {
            if (safe == nullptr || r == 0) return;
            auto* p = safe->proc; auto set = [p](const char* id, float v) { if (auto* q = p->apvts.getParameter(id)) q->setValueNotifyingHost(q->convertTo0to1(v)); };
            if (r == 1) { safe->typeValue(); return; }
            if (r == 3) { if (safe->onShowMatrix) safe->onShowMatrix(safe->modDst); return; }
            if (r >= 200 && r < 200 + kModSlots) { const int i = r - 200; set(ID::modSlotId(i, 0), 0); set(ID::modSlotId(i, 1), 0); set(ID::modSlotId(i, 2), 0); return; }
            if (r == 2) { for (int i = 0; i < kModSlots; ++i) if ((int) p->P(ID::modSlotId(i, 1)) == safe->modDst) { set(ID::modSlotId(i, 0), 0); set(ID::modSlotId(i, 1), 0); set(ID::modSlotId(i, 2), 0); } return; }
            const int s = r - 100;
            for (int i = 0; i < kModSlots; ++i) if ((int) p->P(ID::modSlotId(i, 1)) == safe->modDst && (int) p->P(ID::modSlotId(i, 0)) == s) return;     // already connected
            for (int i = 0; i < kModSlots; ++i)
                if (p->P(ID::modSlotId(i, 0)) < 0.5f && p->P(ID::modSlotId(i, 1)) < 0.5f)
                { set(ID::modSlotId(i, 0), (float) s); set(ID::modSlotId(i, 1), (float) safe->modDst); set(ID::modSlotId(i, 2), 0.25f); return; }
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "All connections are in use", "Aeola has eight connections. Remove one on the Motion tab first.");
        });
        return;
    }
    typeValue();
}
void Knob::typeValue()
{
    auto* w = new juce::AlertWindow(labelText, "Type a value:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor("v", slider.getTextFromValue(slider.getValue())); w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<Knob> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w](int res) {
        if (res == 1 && safe != nullptr) safe->slider.setValue(safe->slider.getValueFromText(w->getTextEditorContents("v")), juce::sendNotificationSync);
        delete w; }), false);
}
void MixMeter::paint(Graphics& g)
{
    const float pk = proc.classPeak(cls); shown = std::max(pk, shown * 0.8f);
    auto b = getLocalBounds().toFloat(); g.setColour(Colour(0xff0e1012)); g.fillRoundedRectangle(b, 2.0f);
    const float db = shown > 1e-6f ? 20.0f * std::log10(shown) : -100.0f, f = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 66.0f);
    auto bar = b.reduced(1.0f); bar = bar.removeFromBottom(bar.getHeight() * f);
    g.setColour(db > -1.0f ? col::bad : (db > -9.0f ? col::accent2 : col::good)); g.fillRoundedRectangle(bar, 1.5f);
}

void Knob::paintOverChildren(Graphics& g)
{
    if (proc == nullptr) return;
    const auto c = dialCentre(); const float r = dialRadius();
    if (juce::Time::getMillisecondCounterHiRes() * 0.001 < flashUntil)
    {   // "show me this control" (from the Matrix)
        g.setColour(col::accent2.withAlpha(0.9f)); g.drawEllipse(c.x - r - 5.0f, c.y - r - 5.0f, (r + 5.0f) * 2.0f, (r + 5.0f) * 2.0f, 2.0f);
    }
    if (!modDst) return;
    const double v0 = slider.getValue(), lo = slider.getMinimum(), hi = slider.getMaximum();
    auto target = [&](float d) {
        double v1;
        switch (modDst)
        {
            case DstCutoff: v1 = v0 * std::pow(2.0, 4.0 * d); break;
            case DstBeat:   v1 = v0 + d * 6.0; break;
            case DstDrive:  v1 = v0 + d * 24.0; break;
            case DstWidth:  v1 = v0 + d * 1.5; break;
            default:        v1 = v0 + d * (hi - lo); break;
        }
        return std::max(lo, std::min(hi, v1)); };
    const float a0 = juce::MathConstants<float>::pi * 1.2f, a1 = juce::MathConstants<float>::pi * 2.8f;
    auto angOf = [&](double v) { return a0 + (float) slider.valueToProportionOfLength(v) * (a1 - a0); };
    // one thin arc per connection: the range it can move the control over (both ways for sources that swing both ways)
    int n = 0;
    for (int i = 0; i < kModSlots && n < 3; ++i)
    {
        if ((int) proc->P(ID::modSlotId(i, 1)) != modDst || proc->P(ID::modSlotId(i, 0)) < 0.5f) continue;
        const int s = (int) proc->P(ID::modSlotId(i, 0)); const float amt = proc->P(ID::modSlotId(i, 2));
        const bool bip = s == SrcLfo1 || s == SrcLfo2 || s == SrcWalk || s == SrcPitchBend || (s >= SrcTension && s <= SrcSpace);
        const double vA = bip ? target(-amt) : v0, vB = target(amt); const float rr = r + 1.0f + (float) n * 2.6f;
        juce::Path arc; arc.addCentredArc(c.x, c.y, rr, rr, 0, angOf(vA), angOf(vB), true);
        g.setColour(col::warm.withAlpha(0.5f)); g.strokePath(arc, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        ++n;
    }
    if (n == 0) return;
    {   // badge: this knob is connected (with the count when there are several)
        const float bx = c.x + r * 0.86f, by = c.y - r * 0.86f; int total = 0;
        for (int i = 0; i < kModSlots; ++i) if ((int) proc->P(ID::modSlotId(i, 1)) == modDst && proc->P(ID::modSlotId(i, 0)) > 0.5f) ++total;
        g.setColour(col::warm); g.fillEllipse(bx - 4.5f, by - 4.5f, 9.0f, 9.0f);
        if (total > 1) { g.setColour(Colour(0xff141618)); g.setFont(F(8.5f, juce::Font::bold)); g.drawText(juce::String(total), juce::Rectangle<float>(bx - 4.5f, by - 4.5f, 9.0f, 9.0f), juce::Justification::centred); }
    }
    // where the control is right now
    const float d = proc->modDestShown(modDst);
    if (std::fabs(d) < 1e-4f) return;
    const double v1 = target(d);
    juce::Path live; live.addCentredArc(c.x, c.y, r, r, 0, angOf(v0), angOf(v1), true);
    g.setColour(col::warm.withAlpha(0.9f)); g.strokePath(live, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float ang = angOf(v1);
    g.setColour(col::warm); g.fillEllipse(c.x + r * std::sin(ang) - 2.2f, c.y - r * std::cos(ang) - 2.2f, 4.4f, 4.4f);
}
void Knob::resized()
{
    auto r = getLocalBounds();
    if (above) { vlab.setBounds(r.removeFromTop(17).withHeight(14)); }
    slider.setBounds(r.removeFromTop(std::max(24, r.getHeight() - 15)));
    name.setBounds(r.withTrimmedTop(0));
    if (ring) ring->setBounds(getLocalBounds());
}

// ================================ sky view ===============================================
// Two views of the same traffic. "Sound": one column per note of the scale, height = distance (near = loud and bright), so you see which
// aircraft plays what and why. "Sky": the geography (north-up map with coastline). Hover or click an aircraft for a card; the class
// chips above are Solo (click) and Mute (alt-click) for that class's voice.
static juce::Path classPath(int cat, float x, float y, float r)
{
    juce::Path p;
    switch (cat)
    {
        case kHeavy: p.addRectangle(x - r, y - r, 2 * r, 2 * r); break;
        case kCargo: p.addTriangle(x, y - r - 1, x + r + 1, y + r, x - r - 1, y + r); break;
        case kHeli:  p.addQuadrilateral(x, y - r - 1, x + r + 1, y, x, y + r + 1, x - r - 1, y); break;
        case kBiz:   p.addPolygon({ x, y }, 6, r + 0.8f); break;
        case kProp:  p.addTriangle(x - r - 1, y - r, x + r + 1, y - r, x, y + r + 1); break;
        case kMil:   p.addStar({ x, y }, 4, r * 0.55f, r * 1.5f); break;
        case kLight: p.addEllipse(x - r * 0.75f, y - r * 0.75f, r * 1.5f, r * 1.5f); break;
        default:     p.addEllipse(x - r, y - r, 2 * r, 2 * r); break;
    }
    return p;
}
// map glyph: an arrow-head pointing along the track (helicopters are a diamond)
static juce::Path mapGlyph(int cat, float x, float y, float r, float trackDeg)
{
    juce::Path p = classPath(cat, 0.0f, 0.0f, r);                    // the same shape as in the chips and the Sound view
    if (cat == kHeli || cat == kLight || cat == kSmall || cat == kLarge) { p.applyTransform(juce::AffineTransform::translation(x, y)); return p; }
    p.applyTransform(juce::AffineTransform::rotation(trackDeg * 0.0174533f).translated(x, y)); return p;
}
static void classMarker(Graphics& g, int cat, float x, float y, float r) { g.fillPath(classPath(cat, x, y, r)); }

static Colour catColour(int c)
{
    switch (c) { case kCargo: return Colour(0xffff8f5a); case kHeavy: return Colour(0xffffffff); case kHeli: return Colour(0xffb58cff);
                 case kLight: return Colour(0xff8fc4e8); case kSmall: return Colour(0xff9fb0ff); case kBiz: return Colour(0xff5fe0e6);
                 case kProp: return Colour(0xffd8e070); case kMil: return Colour(0xffff6fb0); default: return Colour(0xffc4ccd2); }
}
static const char* classCode(int c) { static const char* n[] = { "LGT", "SML", "NAR", "HVY", "HEL", "FRT", "BIZ", "TBP", "MIL" }; return n[std::max(0, std::min(8, c))]; }   // 1.13: FRT Freight, TBP Turboprop return n[std::max(0, std::min(8, c))]; }
static float classRadius(int c)
{
    switch (c) { case kHeavy: case kCargo: return 5.5f; case kLarge: case kMil: return 5.0f; case kSmall: case kProp: return 4.2f; case kBiz: return 4.4f; default: return 3.8f; }
}
static juce::String noteLabel(int note) { return juce::String(noteNames()[((note % 12) + 12) % 12]) + juce::String(note / 12 - 1); }
static const char* bearingName(double dE, double dN)
{
    static const char* n[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    double a = std::atan2(dE, dN) * 180.0 / M_PI; if (a < 0) a += 360.0; return n[(int) std::lround(a / 45.0) % 8];
}

// ---- land layer (1.13): Natural Earth land and lakes as filled shapes cut into tiles, culled by bounding box ----
// Land is one path (non-zero winding, so the tiles join without seams and the holes stay holes), lakes a second one, and the shoreline a
// third that leaves out the straight cuts along tile borders. Outlines are smoothed (curves through the points' midpoints), except at cuts.
struct LandPaths { juce::Path land, lakes, shore; };
template <typename Proj>
static void buildLand(double apLat, double apLon, double cLat, double cLon, double reachLat, double reachLon, bool coarse, Proj proj, LandPaths& out)
{
    out.land.clear(); out.lakes.clear(); out.shore.clear(); out.land.setUsingNonZeroWinding(true); out.lakes.setUsingNonZeroWinding(true);
    const double cosLat = std::cos(apLat * M_PI / 180.0);
    auto wrap = [](double d) { while (d > 180.0) d -= 360.0; while (d < -180.0) d += 360.0; return d; };
    const int lod = coarse ? 1 : 0;
    std::vector<juce::Point<float>> pts, run; std::vector<int> la, lo;
    auto flush = [&]() {
        if (run.size() >= 2) { out.shore.startNewSubPath(run[0]); for (size_t i = 1; i + 1 < run.size(); ++i) out.shore.quadraticTo(run[i], (run[i] + run[i + 1]) * 0.5f); out.shore.lineTo(run.back()); }
        run.clear(); };
    for (int k = 0; k < kLandPieceCount; ++k)
    {
        const LandPiece& lp = kLandPieces[k]; if (lp.lod != lod) continue;
        if (lp.latMax / 100.0 < cLat - reachLat || lp.latMin / 100.0 > cLat + reachLat) continue;
        const double lc = 0.5 * (lp.lonMin + lp.lonMax) / 100.0, half = 0.5 * (lp.lonMax - lp.lonMin) / 100.0;
        if (std::fabs(wrap(lc - cLon)) - half > reachLon) continue;
        const int tileU = lp.tile * 1000; int a = lp.lat0, o = lp.lon0;
        pts.clear(); la.clear(); lo.clear();
        for (int i = 0; i < lp.n; ++i)
        {
            if (i > 0) { a += kLandSteps[lp.off + 2 * (i - 1)]; o += kLandSteps[lp.off + 2 * (i - 1) + 1]; }
            const double dN = (a / 1000.0 - apLat) * 60.0, dE = wrap(o / 1000.0 - apLon) * 60.0 * cosLat;
            pts.push_back(proj(dE, dN)); la.push_back(a); lo.push_back(o);
        }
        const int n = (int) pts.size(); if (n < 3) continue;
        auto onCut = [&](int i) { return la[(size_t) i] % tileU == 0 || lo[(size_t) i] % tileU == 0; };
        auto mid = [&](int i, int j) { return (pts[(size_t) i] + pts[(size_t) j]) * 0.5f; };
        juce::Path& fill = lp.kind == 0 ? out.land : out.lakes;
        fill.startNewSubPath(mid(n - 1, 0));
        for (int i = 0; i < n; ++i) { const int j = (i + 1) % n; if (onCut(i)) { fill.lineTo(pts[(size_t) i]); fill.lineTo(mid(i, j)); } else fill.quadraticTo(pts[(size_t) i], mid(i, j)); }
        fill.closeSubPath();
        for (int i = 0; i < n; ++i)
        {
            const int j = (i + 1) % n;
            const bool cut = (la[(size_t) i] == la[(size_t) j] && la[(size_t) i] % tileU == 0) || (lo[(size_t) i] == lo[(size_t) j] && lo[(size_t) i] % tileU == 0);
            if (cut) { flush(); continue; }
            if (run.empty()) run.push_back(pts[(size_t) i]);
            run.push_back(pts[(size_t) j]);
        }
        flush();
    }
}
// for the tests: how much land there is near a place (path area in square nm, 0 = open sea)
float aeola::landAreaNear(double lat, double lon, double reachDeg, bool coarse)
{
    LandPaths lp; buildLand(lat, lon, lat, lon, reachDeg, reachDeg, coarse, [](double dE, double dN) { return juce::Point<float>((float) dE, (float) -dN); }, lp);
    auto b = lp.land.getBounds().getIntersection({ -(float) reachDeg * 30.0f, -(float) reachDeg * 30.0f, (float) reachDeg * 60.0f, (float) reachDeg * 60.0f });
    return lp.land.isEmpty() ? 0.0f : b.getWidth() * b.getHeight();
}

SkyView::SkyView(AeolaDroneProcessor& p) : proc(p)
{
    setWantsKeyboardFocus(true);
    startTimerHz(24);
}

// local layout (the view is 496 x 430)
juce::Rectangle<float> SkyView::segRect(int i) const { return { (float) getWidth() - 12.0f - 186.0f + (float) i * 62.0f, 7.0f, 62.0f, 24.0f }; }
juce::Rectangle<float> SkyView::chipRect(int c) const { const float w = std::max(51.0f, std::min(84.0f, ((float) getWidth() - 24.0f) / 9.0f - 2.0f)); return { 12.0f + (float) c * (w + 2.0f), 38.0f, w, 22.0f }; }
juce::Rectangle<float> SkyView::plotRect() const
{
    if (tab >= 1 && getHeight() < 450) return { 46.0f, 84.0f, (float) getWidth() - 46.0f - 14.0f, (float) getHeight() - 84.0f - 14.0f };   // compact view: the map takes all the free height
    return { 46.0f, 72.0f, (float) getWidth() - 46.0f - 14.0f, (float) getHeight() - 156.0f };
}
juce::Rectangle<float> SkyView::zoomRect(int i) const { return { 14.0f, plotRect().getCentreY() - 26.0f + (float) i * 30.0f, 24.0f, 24.0f }; }
juce::Rectangle<float> SkyView::landRect() const { return { 12.0f, plotRect().getCentreY() + 40.0f, 44.0f, 20.0f }; }

void SkyView::mouseMove(const juce::MouseEvent& e)
{
    uint32_t h = 0; float best = 11.0f;
    for (auto& k : hits) { float d = k.p.getDistanceFrom(e.position); if (d < best) { best = d; h = k.id; } }
    if (h != hoverId) { hoverId = h; repaint(); }
    const bool hp = placeHit.contains(e.position); if (hp != hoverPlace) { hoverPlace = hp; setMouseCursor(hp ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); repaint(); }
    int chip = -1; for (int c = 0; c < kNumClasses; ++c) if (chipRect(c).contains(e.position)) chip = c;
    if (tab == 1)
    {   // 1.12 map: the dashed circle can be dragged; aircraft beyond it show where they are going
        const bool ne = std::fabs(e.position.getDistanceFrom(mapC) - (float) std::max(5.0, radiusShown) * mapS) < 7.0f && h == 0;
        if (ne != nearEdge) { nearEdge = ne; repaint(); }
        uint32_t ho = 0; float bo = 10.0f; if (h == 0) for (auto& k : outHits) { float d = k.p.getDistanceFrom(e.position); if (d < bo) { bo = d; ho = k.id; } }
        if (ho != hoverOut) { hoverOut = ho; repaint(); }
        if (!hp) setMouseCursor(nearEdge ? juce::MouseCursor::UpDownLeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }
    if (chip != hoverChip) { hoverChip = chip; setTooltip(chip >= 0 ? juce::String(className(chip)) + ":  click = solo,  alt-click = mute,  double-click = clear all" : juce::String()); repaint(); }
}
void SkyView::mouseExit(const juce::MouseEvent&) { hoverId = 0; hoverChip = -1; repaint(); }
void SkyView::mouseDoubleClick(const juce::MouseEvent& e)
{
    for (int c = 0; c < kNumClasses; ++c) if (chipRect(c).contains(e.position)) { proc.clearClassFlags(); return; }
    if (tab == 2) { proc.setProp("earthYaw", 0.0); proc.setProp("earthTilt", 28.0); repaint(); return; }
    if (tab == 1) { proc.setProp("mapZoomNm", 0.0); proc.setProp("mapPanE", 0.0); proc.setProp("mapPanN", 0.0); repaint(); return; }     // map: back to the circle
    if (plotRect().contains(e.position) && hoverId == 0) proc.setRadius(40.0f);
}
void SkyView::mouseUp(const juce::MouseEvent&) { dragMode = 0; }
void SkyView::mouseDrag(const juce::MouseEvent& e)
{
    if (tab == 1 && dragMode != 0 && mapS > 0.0f)
    {
        if (dragMode == 1) proc.setRadius((float) std::max(5.0, std::min(250.0, (double) e.position.getDistanceFrom(mapC) / mapS)));
        else
        {   const auto d = e.position - lastDrag;
            proc.setProp("mapPanE", (double) proc.getProp("mapPanE", 0.0) - d.x / mapS); proc.setProp("mapPanN", (double) proc.getProp("mapPanN", 0.0) + d.y / mapS); }
        lastDrag = e.position; repaint(); return;
    }
    if (tab != 2) return;
    const auto d = e.position - lastDrag; lastDrag = e.position;
    proc.setProp("earthYaw", std::fmod((double) proc.getProp("earthYaw", 0.0) + d.x * 0.4, 360.0));
    proc.setProp("earthTilt", std::max(12.0, std::min(70.0, (double) proc.getProp("earthTilt", 28.0) - d.y * 0.25))); repaint();
}
void SkyView::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus(); lastDrag = e.position;
    if (placeHit.contains(e.position) && onPlaceClick) { onPlaceClick(); return; }
    for (int i = 0; i < 3; ++i) if (segRect(i).contains(e.position)) { proc.setProp("skyTab", i); repaint(); return; }
    for (int c = 0; c < kNumClasses; ++c)
        if (chipRect(c).contains(e.position))
        {
            if (e.mods.isAltDown() || e.mods.isRightButtonDown()) proc.setClassFlag(c, 6, !proc.classFlag(c, 6));
            else proc.setClassFlag(c, 5, !proc.classFlag(c, 5));
            repaint(); return;
        }
    if (tab == 1)
    {
        if (landRect().contains(e.position)) { proc.setProp("mapLand", !(bool) proc.getProp("mapLand", true)); repaint(); return; }
        for (int i = 0; i < 2; ++i) if (zoomRect(i).expanded(3.0f).contains(e.position)) { proc.setProp("mapZoomNm", std::max(2.0, std::min(400.0, mapZ * (i ? 1.0 / 0.8 : 0.8)))); repaint(); return; }
        if (nearEdge) { proc.setProp("mapZoomNm", mapZ); dragMode = 1; return; }       // the scale stays put while the circle is dragged
        if (hoverId == 0 && plotRect().contains(e.position)) dragMode = 2;
    }
    if (hoverId) pinnedId = (pinnedId == hoverId ? 0 : hoverId); else pinnedId = 0;
    repaint();
}
void SkyView::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    double r = proc.getRadius(), f = std::exp(-(double) w.deltaY * (w.isReversed ? -1.0 : 1.0) * 1.2);
    if (tab == 1) { proc.setProp("mapZoomNm", std::max(2.0, std::min(400.0, mapZ * f))); repaint(); return; }       // 1.12: the map zooms; the radius has its own knob and the dashed circle
    proc.setRadius((float) std::max(5.0, std::min(250.0, r * f)));
}
bool SkyView::keyPressed(const juce::KeyPress& k)
{
    const auto c = k.getTextCharacter();
    auto classOfSel = [&]() { uint32_t id = hoverId ? hoverId : pinnedId; auto it = acs.find(id); return it == acs.end() ? -1 : it->second.a.category; };
    if (c == '1') { proc.setProp("skyTab", 0); return true; }
    if (c == '2') { proc.setProp("skyTab", 1); return true; }
    if (c == '3') { proc.setProp("skyTab", 2); return true; }
    if (c == '+' || c == '=') { proc.setRadius((float) std::max(5.0, proc.getRadius() * 0.8)); return true; }
    if (c == '-') { proc.setRadius((float) std::min(250.0, proc.getRadius() / 0.8)); return true; }
    if (c == '0') { proc.setRadius(40.0f); return true; }
    if (c == 'l' || c == 'L') { proc.setProp("mapLand", !(bool) proc.getProp("mapLand", true)); return true; }
    if (k == juce::KeyPress::escapeKey) { pinnedId = 0; repaint(); return true; }
    if ((c == 's' || c == 'S') && classOfSel() >= 0) { proc.setClassFlag(classOfSel(), 5, !proc.classFlag(classOfSel(), 5)); return true; }
    if ((c == 'm' || c == 'M') && classOfSel() >= 0) { proc.setClassFlag(classOfSel(), 6, !proc.classFlag(classOfSel(), 6)); return true; }
    return false;
}

// greedy, flicker-free label placement: keep the previous slot when it is still free
static bool placeLabel(juce::Point<float> p, float w, float h, int& slot, std::vector<Rectangle<float>>& used, Rectangle<float> bounds, Rectangle<float>& out, float gap = 7.0f)
{
    static const juce::Point<float> off[8] = { { 1, 0 }, { -1, 0 }, { 0, -1 }, { 0, 1 }, { 1, -1 }, { -1, -1 }, { 1, 1 }, { -1, 1 } };
    for (int k = 0; k < 8; ++k)
    {
        int i = (slot + k) % 8; Rectangle<float> r(w, h);
        r.setCentre(p.x + off[i].x * (gap + w * 0.5f), p.y + off[i].y * (gap + h * 0.5f));
        if (!bounds.contains(r)) continue;
        bool hit = false; for (auto& u : used) if (u.intersects(r.expanded(1.0f))) { hit = true; break; }
        if (!hit) { out = r; slot = i; return true; }
    }
    return false;
}

void SkyView::paint(Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001; const float dt = (float) std::max(0.001, std::min(0.1, now - lastT)); lastT = now;
    glassPanel(g, b, 8.0f, col::panelEdge);

    UiSnapshot u; const bool have = proc.getUi(u);
    tab = std::max(0, std::min(2, (int) proc.getProp("skyTab", 0)));
    // ---- shared aircraft states: smoothed position, fade in / out ----
    radiusShown += (std::max(5.0, u.radiusNm) - radiusShown) * (1.0 - std::exp(-dt / 0.09));
    for (auto& kv : acs) kv.second.present = false;
    for (int i = 0; have && i < u.nAc; ++i)
    {
        const auto& a = u.ac[i]; auto& s = acs[a.id];
        if (s.alpha <= 0.0f && s.lat == 0.0 && s.lon == 0.0) { s.lat = a.lat; s.lon = a.lon; }
        const double k = 1.0 - std::exp(-dt / 0.15); s.lat += (a.lat - s.lat) * k; s.lon += (a.lon - s.lon) * k;
        s.a = a; s.present = true;
        if (now - s.lastTrail > 1.0) { s.lastTrail = now; s.trail.push_back({ s.lat, s.lon }); if (s.trail.size() > 24) s.trail.pop_front(); }
    }
    for (auto it = acs.begin(); it != acs.end();)
    {
        auto& s = it->second; s.alpha += ((s.present ? 1.0f : 0.0f) - s.alpha) * (1.0f - std::exp(-dt / (s.present ? 0.12f : 0.18f)));
        if (!s.present && s.alpha < 0.02f) it = acs.erase(it); else ++it;
    }
    {   // 1.12: aircraft beyond the radius (map only), and crossings of the circle
        for (auto& kv : outs) kv.second.present = false;
        for (int i = 0; have && i < u.nOut; ++i)
        {
            const auto& a = u.out[i]; auto& s = outs[a.id];
            if (s.alpha <= 0.0f && s.lat == 0.0 && s.lon == 0.0) { s.lat = a.lat; s.lon = a.lon; }
            const double k = 1.0 - std::exp(-dt / 0.15); s.lat += (a.lat - s.lat) * k; s.lon += (a.lon - s.lon) * k; s.a = a; s.present = true;
            if (now - s.lastTrail > 2.0) { s.lastTrail = now; s.trail.push_back({ s.lat, s.lon }); if (s.trail.size() > 12) s.trail.pop_front(); }
        }
        for (auto it = outs.begin(); it != outs.end();)
        {   auto& s = it->second; s.alpha += ((s.present ? 1.0f : 0.0f) - s.alpha) * (1.0f - std::exp(-dt / 0.2f));
            if (!s.present && s.alpha < 0.02f) it = outs.erase(it); else ++it; }
        std::set<uint32_t> nowIn, nowOut;
        for (auto& kv : acs) if (kv.second.present) nowIn.insert(kv.first);
        for (auto& kv : outs) if (kv.second.present) nowOut.insert(kv.first);
        if (have && (!lastIn.empty() || !lastOut.empty()))
        {
            for (auto id : nowIn) if (lastOut.count(id) && crosses.size() < 24) crosses.push_back({ acs[id].lat, acs[id].lon, now, true });
            for (auto id : nowOut) if (lastIn.count(id) && crosses.size() < 24) crosses.push_back({ outs[id].lat, outs[id].lon, now, false });
        }
        lastIn = nowIn; lastOut = nowOut;
        while (!crosses.empty() && now - crosses.front().t > 3.0) crosses.pop_front();
    }
    uiK = std::max(1.0f, std::min(1.6f, (float) getHeight() / 430.0f));
    hits.clear(); levelOf.clear();
    for (int v = 0; have && v < u.nVoices; ++v) if (u.v[v].kind == 1 && (u.v[v].id & kAcVoiceBit)) { const uint32_t aid = u.v[v].id & ~kAcVoiceBit; levelOf[aid] = std::max(levelOf[aid], u.v[v].level); }
    const int nGnd = have ? u.nGround : 0;
    uint32_t sel = hoverId ? hoverId : pinnedId;
    if (sel && !(sel >= kGroundId ? (int) (sel - kGroundId) < nGnd : acs.count(sel) > 0)) { if (pinnedId == sel) pinnedId = 0; sel = 0; }

    // ---- header: source badge, audition, view tabs ----
    {
        const int sIdx = std::min(4, std::max(0, u.source));
        const bool stale = sIdx == 1 && u.ageSec > 45.0;
        const char* names[] = { "Waiting", "Live", "Cached", "Replay", "Simulated" };
        Colour bc[] = { col::dim, stale ? col::warn : col::good, col::warn, col::accent2, col::bad };
        const float pulse = sIdx == 0 ? 0.55f + 0.45f * (float) std::sin(now * 4.0) : 1.0f;
        const juce::String nameTxt = stale ? juce::String("Live, delayed") : juce::String(names[sIdx]);
        g.setColour(bc[sIdx].withAlpha(pulse)); g.fillEllipse(14.0f, 15.0f, 7.0f, 7.0f);
        g.setFont(F(12.5f, juce::Font::bold)); g.setColour(col::text); g.drawText(nameTxt, 26, 8, 120, 22, juce::Justification::centredLeft);
        juce::String place = proc.isCustomPlace() ? proc.placeName().upToFirstOccurrenceOf(",", false, false).substring(0, 18) : juce::String(iataOf(proc.makePlace().code));
        const float sx = 26.0f + juce::GlyphArrangement::getStringWidth(F(12.5f, juce::Font::bold), nameTxt) + 12.0f;
        g.setFont(F(12.0f)); g.setColour(col::text.withAlpha(0.9f));
        const float pw = juce::GlyphArrangement::getStringWidth(F(12.0f), place) + 18.0f;
        placeHit = Rectangle<float>(sx - 4.0f, 7.0f, pw + 6.0f, 24.0f);
        if (hoverPlace) { g.setColour(Colour(0xff25282c)); g.fillRoundedRectangle(placeHit, 5.0f); }
        g.drawText(place, (int) sx, 8, (int) pw, 22, juce::Justification::centredLeft);
        { juce::Path ch; const float cx = sx + pw - 8.0f, cy = 19.0f; ch.startNewSubPath(cx - 3, cy - 1.5f); ch.lineTo(cx, cy + 1.5f); ch.lineTo(cx + 3, cy - 1.5f); g.setColour(col::dim); g.strokePath(ch, juce::PathStrokeType(1.3f)); }
        if (u.audition)
        {
            auto ab = Rectangle<float>(0, 9.0f, 0, 20.0f);
            ab.setWidth(juce::GlyphArrangement::getStringWidth(F(10.5f, juce::Font::bold), "AUDITION - virtual aircraft") + 18.0f); ab.setX(segRect(0).getX() - ab.getWidth() - 8.0f);
            g.setColour(col::accent2.withAlpha(0.16f)); g.fillRoundedRectangle(ab, 10.0f); g.setColour(col::accent2); g.drawRoundedRectangle(ab.reduced(0.5f), 10.0f, 1.0f);
            g.setFont(F(10.5f, juce::Font::bold)); g.drawText("AUDITION - virtual aircraft", ab.toNearestInt(), juce::Justification::centred);
        }
        static const char* labels[] = { "Sound", "Sky", "Earth" };
        for (int i = 0; i < 3; ++i)
        {
            auto r = segRect(i); const bool on = i == tab;
            if (on) { g.setColour(Colour(0xff25282c)); g.fillRoundedRectangle(r, 5.0f); g.setColour(col::panelEdge); g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 1.0f); }
            g.setColour(on ? col::text : col::dim); g.setFont(F(12.0f, on ? juce::Font::bold : 0)); g.drawText(labels[i], r.toNearestInt(), juce::Justification::centred);
        }
    }
    // ---- class chips: click = Solo, alt-click = Mute ----
    {
        int count[kNumClasses] = {}; for (auto& kv : acs) if (kv.second.present) ++count[std::max(0, std::min(kNumClasses - 1, kv.second.a.category))];
        bool anySolo = false; for (int c = 0; c < kNumClasses; ++c) anySolo = anySolo || proc.classFlag(c, 5);
        for (int c = 0; c < kNumClasses; ++c)
        {
            auto r = chipRect(c); const bool solo = proc.classFlag(c, 5), mute = proc.classFlag(c, 6); const float al = mute ? 0.35f : (anySolo && !solo ? 0.4f : (count[c] ? 1.0f : 0.5f));
            if (hoverChip == c) { g.setColour(Colour(0xff25282c)); g.fillRoundedRectangle(r, 5.0f); }
            if (solo) { g.setColour(col::accent2.withAlpha(0.18f)); g.fillRoundedRectangle(r, 5.0f); g.setColour(col::accent2); g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 1.0f); }
            g.setColour(catColour(c).withAlpha(al)); classMarker(g, c, r.getX() + 9.0f, r.getCentreY(), 3.6f);
            g.setColour((solo ? col::accent2 : (count[c] ? col::text : col::dim)).withAlpha(al)); g.setFont(F(10.5f, solo ? juce::Font::bold : 0));
            g.drawText(juce::String(classCode(c)) + (count[c] ? " " + juce::String(count[c]) : ""), r.withTrimmedLeft(17).toNearestInt(), juce::Justification::centredLeft);
            if (mute) { g.setColour(col::bad.withAlpha(0.8f)); g.drawLine(r.getX() + 6, r.getCentreY(), r.getRight() - 6, r.getCentreY(), 1.2f); }
        }
    }
    if (tab == 0) paintSound(g, u, have, dt, now, sel); else if (tab == 1) paintMap(g, u, have, dt, now, sel); else paintEarth(g, u, have, dt, now, sel);

    // ---- footer: counts, then the ground traffic line ----
    {
        int nSnd = 0, nMoved = 0, nClash = 0, nOver = 0, nMute = 0;
        for (auto& kv : acs) { if (!kv.second.present) continue; switch (kv.second.a.status) { case 1: ++nSnd; break; case 2: ++nSnd; ++nMoved; break; case 3: ++nClash; break; case 4: ++nOver; break; case 5: ++nMute; break; default: break; } }
        struct Pl { int n; const char* t; Colour c; bool always; };
        const Pl ps[] = { { nSnd, "sounding", col::good, true }, { nMoved, "moved by Consonance", col::accent2, true }, { nClash, "silenced", col::bad, true }, { nOver, "over limit", col::dim, false }, { nMute, "muted", col::dim, false } };
        float x = 14.0f; const float y = (float) getHeight() - 14.0f; g.setFont(F(11.0f));
        for (auto& p : ps)
        {
            if (!p.always && !p.n) continue; const float al = p.n ? 1.0f : 0.45f; juce::String t = juce::String(p.n) + (p.n == nSnd && p.c == col::good ? " / " + juce::String(std::min((int) proc.P(ID::maxac), (int) proc.P(ID::voices))) : juce::String()) + " " + p.t; const float w = juce::GlyphArrangement::getStringWidth(F(11.0f), t);
            g.setColour(p.c.withAlpha(al)); g.fillEllipse(x, y - 3.0f, 6.0f, 6.0f); g.setColour((p.n ? col::text : col::dim).withAlpha(al)); g.drawText(t, (int) x + 10, (int) y - 8, (int) w + 6, 16, juce::Justification::centredLeft); x += w + 24.0f;
        }
        // trust line: where the aircraft come from, how fresh, and what the count means, so it can be checked against any tracker
        {
            const int sIdx = std::min(4, std::max(0, u.source)); juce::String t; Colour tc = col::dim;
            const juce::String where = (proc.isCustomPlace() ? proc.placeName().upToFirstOccurrenceOf(",", false, false).substring(0, 18) : juce::String(iataOf(proc.makePlace().code))) + " (" + juce::String(proc.getRadius(), 0) + " nm)";
            auto ago = [](double s2) { return s2 < 90.0 ? juce::String((int) std::lround(s2)) + " s" : (s2 < 7200.0 ? juce::String((int) std::lround(s2 / 60.0)) + " min" : juce::String((int) std::lround(s2 / 3600.0)) + " h"); };
            if (sIdx == 1) { t = juce::String(u.feedName) + "  -  " + juce::String(u.nReal) + (u.capped ? " nearest" : "") + " aircraft in the air within " + juce::String((int) std::lround(u.radiusNm)) + " nm  -  updated " + ago(u.ageSec) + " ago"; tc = u.ageSec > 45.0 ? col::warn : col::dim;
                             if (u.feedErr[0]) { t << "  -  " << (std::strncmp(u.feedErr, "Daily", 5) == 0 ? "" : "last update failed: ") << u.feedErr; tc = col::warn; } }
            else if (sIdx == 2) { t = "Saved snapshot from " + ago(u.ageSec) + " ago. Not live." + (u.feedErr[0] ? juce::String("  -  ") + juce::String(u.feedErr) : juce::String()); tc = col::warn; }
            else if (sIdx == 3) { t = "Replay of recorded traffic. Not live."; tc = col::accent2; }
            else if (sIdx == 4) { t = "Simulated traffic. These aircraft are not real."; tc = col::bad; }
            else if (u.feedErr[0]) { t = "No traffic data: " + juce::String(u.feedErr) + (u.retryIn > 1800.0 ? ". Only the drone bed plays until then." : (u.retryIn > 1.0 ? ". Trying again in " + ago(u.retryIn) + "." : ". Trying again.")); tc = col::warn; }
            else t = "Connecting to adsb.lol...";
            g.setFont(F(10.5f)); g.setColour(tc); g.drawText(t, 14, getHeight() - 42, getWidth() - 28, 16, juce::Justification::centredLeft, true);
        }
        // ground traffic and the drone bed
        const float ry = (float) getHeight() - 62.0f; g.setFont(F(10.5f)); g.setColour(col::dim);
        int gc[kNumClasses] = {}; for (int i = 0; i < nGnd; ++i) ++gc[std::max(0, std::min(kNumClasses - 1, u.gnd[i].category))];
        float gx = 14.0f; const juce::String head = nGnd ? "On the ground (silent):" : juce::String();
        if (nGnd) g.drawText(head, (int) gx, (int) ry, 150, 16, juce::Justification::centredLeft); gx += juce::GlyphArrangement::getStringWidth(F(10.5f), head) + 8.0f;
        for (int c = 0; c < kNumClasses; ++c) if (gc[c])
        {
            g.setColour(catColour(c).withAlpha(0.8f)); g.strokePath(classPath(c, gx + 4.0f, ry + 8.0f, 3.6f), juce::PathStrokeType(1.1f));
            g.setColour(col::dim); g.drawText(juce::String(classCode(c)) + (gc[c] > 1 ? " x" + juce::String(gc[c]) : juce::String()), (int) gx + 11, (int) ry, 50, 16, juce::Justification::centredLeft); gx += 52.0f;
        }
        juce::String bed; std::vector<std::pair<float, int>> bn; int ghosts = 0;
        for (int v = 0; v < u.nVoices; ++v) { if (u.v[v].kind == 2) bn.push_back({ u.v[v].level, u.v[v].note }); else if (u.v[v].kind == 3) ++ghosts; }
        std::sort(bn.begin(), bn.end(), [](auto& x2, auto& y2) { return x2.first > y2.first; });
        for (size_t i = 0; i < std::min<size_t>(3, bn.size()); ++i) bed += noteLabel(bn[i].second) + "  ";
        if (ghosts) bed += "+" + juce::String(ghosts);
        if (bed.isNotEmpty()) { g.setColour(col::dim); g.drawText("Drone bed  " + bed, Rectangle<int>(getWidth() - 160, (int) ry, 148, 16), juce::Justification::centredRight); }
    }
    // ---- readout card for the hovered / pinned aircraft ----
    if (sel) paintCard(g, u, sel);
    if (hasKeyboardFocus(false)) { g.setColour(col::accent.withAlpha(0.6f)); g.drawRoundedRectangle(b.reduced(1.5f), 7.0f, 1.0f); }
}

// ------------------------------------------------------------------------- readout card ----------------------------------------------
static juce::String minutesText(double m) { return m < 1.0 ? juce::String("under a minute") : "~" + juce::String((int) std::lround(m)) + " min"; }

void SkyView::paintCard(Graphics& g, const UiSnapshot& u, uint32_t sel)
{
    const bool isGround = sel >= kGroundId;
    UiAircraft a; double lat = 0, lon = 0;
    if (isGround) { a = u.gnd[sel - kGroundId]; lat = a.lat; lon = a.lon; } else { auto& s = acs[sel]; a = s.a; lat = s.lat; lon = s.lon; }
    const Colour cc = catColour(a.category); const double cosLat = std::cos(u.apLat * M_PI / 180.0);
    const double dE = (lon - u.apLon) * 60.0 * cosLat, dN = (lat - u.apLat) * 60.0, dist = std::hypot(dE, dN);
    double fhz = 0; for (int v = 0; v < u.nVoices; ++v) if (u.v[v].id == (kAcVoiceBit | a.id)) fhz = u.v[v].freq;
    juce::String why; Colour wc = col::dim;
    if (isGround) why = "on the ground: silent";
    else switch (a.status) { case 1: why = "sounding"; wc = col::good; break; case 2: why = "moved off " + noteLabel(a.wantNote) + " by Consonance (it would clash)"; wc = col::accent2; break;
                             case 3: why = "silent: would clash"; wc = col::bad; break; case 4: why = "over the voice limit"; break; case 5: why = "muted / not soloed"; break; default: why = "-"; }
    const bool aud = !isGround && (a.id & 0xFF000000u) == 0x03000000u;
    juce::StringArray lines;
    if (aud) lines.add("virtual - not real traffic");
    else if (isGround) lines.add(juce::String(a.type));
    else lines.add(juce::String(a.type) + "  " + juce::String(a.alt / 1000.0f, 1) + "k ft  " + juce::String((int) std::lround(a.gs)) + " kt");
    if (!aud) lines.add(juce::String((int) std::lround(dist)) + " nm " + bearingName(dE, dN) + (isGround ? juce::String() : juce::String(dist / std::max(1.0, u.radiusNm) < 0.34 ? "  - near: bright" : (dist / std::max(1.0, u.radiusNm) < 0.67 ? "  - mid" : "  - far: dark"))));
    if (!isGround && a.note > 0) lines.add("Note " + noteLabel(a.note) + (fhz > 0 ? "  " + juce::String(fhz, 1) + " Hz" : juce::String()));
    if (!isGround && !aud && a.gs > 30.0f) { const double m = crossEtaMin(dE, dN, a.track, a.gs, std::max(1.0, u.radiusNm), true); if (m > 0) lines.add("leaves the circle in " + minutesText(m)); }
    lines.add(why);
    const float cw = 176.0f, ch = 24.0f + 14.0f * (float) lines.size();
    // pick the corner of the pointer that hides the fewest other aircraft
    juce::Point<float> at; for (auto& k : hits) if (k.id == sel) at = k.p;
    const juce::Point<float> offs[4] = { { 14, 12 }, { -cw - 14, 12 }, { 14, -ch - 12 }, { -cw - 14, -ch - 12 } };
    Rectangle<float> best; int bestScore = 1 << 30; const auto limit = Rectangle<float>(6.0f, 64.0f, (float) getWidth() - 12.0f, (float) getHeight() - 72.0f);
    for (auto& o : offs)
    {
        Rectangle<float> r(at.x + o.x, at.y + o.y, cw, ch); int score = 0;
        if (r.getX() < limit.getX()) { r.setX(limit.getX()); score += 2; } if (r.getRight() > limit.getRight()) { r.setX(limit.getRight() - cw); score += 2; }
        if (r.getY() < limit.getY()) { r.setY(limit.getY()); score += 2; } if (r.getBottom() > limit.getBottom()) { r.setY(limit.getBottom() - ch); score += 2; }
        for (auto& k : hits) if (k.id != sel && r.expanded(6.0f).contains(k.p)) score += 4; if (r.expanded(4.0f).contains(at)) score += 6;
        if (score < bestScore) { bestScore = score; best = r; }
    }
    g.setColour(Colour(0xf00e1012)); g.fillRoundedRectangle(best, 7.0f); g.setColour(cc.withAlpha(0.55f)); g.drawRoundedRectangle(best.reduced(0.5f), 7.0f, 1.0f);
    auto t = best.reduced(10.0f, 6.0f);
    g.setColour(cc); classMarker(g, a.category, t.getX() + 5.0f, t.getY() + 8.0f, 4.0f);
    g.setColour(col::text); g.setFont(F(11.5f, juce::Font::bold)); g.drawText(juce::String(a.callsign[0] ? a.callsign : "(no callsign)") + "  " + className(a.category), t.removeFromTop(16).withTrimmedLeft(15).toNearestInt(), juce::Justification::centredLeft, true);
    g.setFont(F(10.5f));
    for (int i = 0; i < lines.size(); ++i) { g.setColour(i == lines.size() - 1 ? wc : col::dim); g.drawText(lines[i], t.removeFromTop(14).toNearestInt(), juce::Justification::centredLeft, true); }
}

// ------------------------------------------------------------------------ sound view ---------------------------------------------------
void SkyView::paintSound(Graphics& g, const UiSnapshot& u, bool have, float dt, double now, uint32_t sel)
{
    auto P = plotRect();
    g.setColour(Colour(0xff111315)); g.fillRoundedRectangle(P, 6.0f);
    const double D = std::max(5.0, radiusShown), cosLat = std::cos(u.apLat * M_PI / 180.0);
    // one column per scale note across the playing range
    int lo = u.baseNote - 2, hi = u.baseNote + (int) std::lround(u.rangeOct * 12.0f) + 2;
    for (auto& kv : acs) for (int nn : { kv.second.a.note, kv.second.a.wantNote }) if (nn > 0) { lo = std::min(lo, nn - 1); hi = std::max(hi, nn + 1); }     // class octaves can leave the core range
    std::vector<int> cols; for (int n = lo; n <= hi; ++n) if (inScale(n, u.rootPc, u.scale)) cols.push_back(n);
    if (cols.size() < 4) { cols.clear(); for (int n = lo; n <= hi; ++n) cols.push_back(n); }
    const float cw = P.getWidth() / (float) cols.size();
    auto colX = [&](int i) { return P.getX() + ((float) i + 0.5f) * cw; };
    auto colOf = [&](int note) { int best = 0, bd = 1 << 30; for (int i = 0; i < (int) cols.size(); ++i) { int d = std::abs(cols[(size_t) i] - note); if (d < bd) { bd = d; best = i; } } return best; };
    const float top = P.getY() + 16.0f, bot = P.getBottom() - 28.0f;
    auto rowY = [&](double d01) { return top + (float) std::max(0.0, std::min(1.0, d01)) * (bot - top); };
    // which columns are in use (smoothed), so the notes being played light up
    float target[128] = {}; for (auto& kv : acs) { const auto& a = kv.second.a; if (kv.second.present && (a.status == 1 || a.status == 2) && a.note > 0 && a.note < 128) target[a.note] = 1.0f; }
    for (int n = 0; n < 128; ++n) occ[n] += (target[n] - occ[n]) * std::min(1.0f, dt * 6.0f);
    for (int i = 0; i < (int) cols.size(); ++i)
    {
        const int n = cols[(size_t) i], pc = ((n % 12) + 12) % 12; const bool root = pc == u.rootPc, fifth = pc == (u.rootPc + 7) % 12;
        if (occ[n] > 0.01f) { g.setColour(col::accent.withAlpha(0.07f * occ[n])); g.fillRect(Rectangle<float>(P.getX() + (float) i * cw, P.getY(), cw, P.getHeight())); }
        else if (i % 2 == 0) { g.setColour(Colour(0x06ffffff)); g.fillRect(Rectangle<float>(P.getX() + (float) i * cw, P.getY(), cw, P.getHeight())); }
        g.setColour(root ? col::accent2.withAlpha(0.45f + 0.3f * occ[n]) : (fifth ? col::accent.withAlpha(0.22f + 0.3f * occ[n]) : Colour(0x0dffffff)));
        g.drawLine(colX(i), P.getY() + 6.0f, colX(i), P.getBottom() - 24.0f, root ? 1.2f : 1.0f);
        if (cw >= 21.0f || root || fifth || i % 2 == 0)
        { g.setColour(root ? col::accent2 : (occ[n] > 0.3f ? col::text : Colour(0xff6a7279))); g.setFont(F(9.5f, root ? juce::Font::bold : 0)); g.drawText(noteLabel(n), (int) (colX(i) - 14.0f), (int) P.getBottom() - 20, 28, 12, juce::Justification::centred); }
    }
    for (int v = 0; v < u.nVoices; ++v) if (u.v[v].kind == 2 || u.v[v].kind == 3)
    {   // the drone bed: a faint blue-grey column under the notes it holds
        const int i = colOf(u.v[v].note); const float a2 = std::min(0.12f, 0.03f + u.v[v].level * 0.5f);
        g.setGradientFill(juce::ColourGradient(Colour(0xff8fa6c8).withAlpha(a2), 0, P.getBottom(), Colour(0xff8fa6c8).withAlpha(0.0f), 0, P.getY(), false)); g.fillRect(Rectangle<float>(P.getX() + (float) i * cw, P.getY(), cw, P.getHeight()));
    }
    for (double f : { 0.0, 0.5, 1.0 }) { g.setColour(Colour(0x0fffffff)); g.drawLine(P.getX(), rowY(f), P.getRight(), rowY(f), 1.0f); }
    g.setFont(F(9.5f)); g.setColour(Colour(0xff6a7279));
    g.drawText("0", (int) P.getX() - 34, (int) rowY(0) - 6, 28, 12, juce::Justification::right);
    g.drawText(juce::String((int) std::lround(D * 0.5)), (int) P.getX() - 34, (int) rowY(0.5) - 6, 28, 12, juce::Justification::right);
    g.drawText(juce::String((int) std::lround(D)) + " nm", (int) P.getX() - 40, (int) rowY(1.0) - 6, 34, 12, juce::Justification::right);
    // axis captions
    g.setColour(Colour(0xff6a7279)); g.setFont(F(10.0f));
    g.drawText(juce::CharPointer_UTF8("\xe2\x86\x90 lower note"), (int) P.getX(), (int) P.getBottom() + 4, 130, 14, juce::Justification::left);
    g.drawText(juce::CharPointer_UTF8("higher note \xe2\x86\x92"), (int) P.getRight() - 130, (int) P.getBottom() + 4, 130, 14, juce::Justification::right);
    g.drawText("higher flight = higher note (some classes an octave up or down)   -   near = loud and bright", (int) P.getCentreX() - 230, (int) P.getBottom() + 4, 460, 14, juce::Justification::centred);
    g.setColour(col::dim); g.setFont(F(9.5f)); g.drawText("loudest", (int) P.getX() + 4, (int) P.getY() + 1, 60, 12, juce::Justification::left);

    std::vector<uint32_t> ids; for (auto& kv : acs) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end(), [&](uint32_t x, uint32_t y) { return acs[x].a.alt > acs[y].a.alt; });
    std::vector<Rectangle<float>> used; struct Lab { uint32_t id; juce::Point<float> p; int prio; }; std::vector<Lab> labs;
    bool anyAc = false;
    for (uint32_t id : ids)
    {
        auto& s = acs[id]; const auto& a = s.a; if (a.status == 0) continue; if (s.present) anyAc = true;
        const double dE = (s.lon - u.apLon) * 60.0 * cosLat, dN = (s.lat - u.apLat) * 60.0, d01 = std::hypot(dE, dN) / D;
        const float y = rowY(d01); const float xNow = colX(colOf(a.note > 0 ? a.note : a.wantNote)), xWant = colX(colOf(a.wantNote));
        const Colour cc = catColour(a.category); const bool isSel = id == sel; const float al = s.alpha * (sel && !isSel ? 0.65f : 1.0f) * (focusClass >= 0 && a.category != focusClass ? 0.4f : 1.0f); const float rad = classRadius(a.category) * uiK;
        if (a.status == 1 || a.status == 2)
        {
            if (a.status == 2 && std::fabs(xWant - xNow) > 2.0f)
            {   // where altitude wanted it (dashed outline) and the Consonance move (dashed amber line + arrow head)
                const float dir = xNow > xWant ? 1.0f : -1.0f;
                juce::Path ln; ln.startNewSubPath(xWant + dir * (rad + 3), y); ln.lineTo(xNow - dir * (rad + 9), y); juce::Path dl; float dsh[] = { 3.0f, 3.0f }; juce::PathStrokeType(1.2f).createDashedStroke(dl, ln, dsh, 2);
                g.setColour(col::accent2.withAlpha(0.9f * al)); g.fillPath(dl);
                used.push_back(Rectangle<float>(std::min(xWant, xNow), y - 3.0f, std::fabs(xNow - xWant), 6.0f));      // labels keep off the arrow
                juce::Path ah; ah.addTriangle(xNow - dir * (rad + 4), y, xNow - dir * (rad + 10), y - 3.2f, xNow - dir * (rad + 10), y + 3.2f); g.fillPath(ah);
                g.setColour(cc.withAlpha(0.5f * al)); g.strokePath(classPath(a.category, xWant, y, rad), juce::PathStrokeType(1.2f));
            }
            const float lv = levelOf.count(id) ? levelOf[id] : 0.0f, pul = std::min(1.0f, std::sqrt(lv) * 1.6f), gr = (14.0f + 10.0f * pul) * uiK;
            if (s.trail.size() > 2)
            {   // where it has been in the last seconds: a fading streak along its column (approaching = it falls from above)
                float yo = y; { const auto& t0 = s.trail.front(); const double tE = (t0.y - u.apLon) * 60.0 * cosLat, tN = (t0.x - u.apLat) * 60.0; yo = rowY(std::hypot(tE, tN) / D); }
                g.setGradientFill(juce::ColourGradient(cc.withAlpha(0.0f), xNow, yo, cc.withAlpha(0.5f * al), xNow, y, false)); g.drawLine(xNow, yo, xNow, y, 2.0f);
            }
            g.setGradientFill(juce::ColourGradient(cc.withAlpha((0.14f + 0.34f * pul) * al), xNow, y, cc.withAlpha(0.0f), xNow + gr, y, true)); g.fillEllipse(xNow - gr, y - gr, gr * 2, gr * 2);
            g.setColour(cc.withAlpha(al)); classMarker(g, a.category, xNow, y, rad);
            if (a.status == 2) { g.setColour(col::accent2.withAlpha(al)); g.drawEllipse(xNow - 9.0f, y - 9.0f, 18.0f, 18.0f, 1.3f); }
            used.push_back(Rectangle<float>(xNow - rad - 2, y - rad - 2, rad * 2 + 4, rad * 2 + 4)); if (s.present) hits.push_back({ id, { xNow, y }, 9.0f });
            labs.push_back({ id, { xNow, y }, isSel ? 0 : 1 });
        }
        else
        {
            g.setColour((a.status == 3 ? col::bad : cc).withAlpha((a.status == 3 ? 0.9f : (a.status == 5 ? 0.3f : 0.45f)) * al));
            if (a.status == 3) { g.drawEllipse(xWant - 6, y - 6, 12, 12, 1.0f); g.drawLine(xWant - 3.5f, y - 3.5f, xWant + 3.5f, y + 3.5f, 1.4f); g.drawLine(xWant - 3.5f, y + 3.5f, xWant + 3.5f, y - 3.5f, 1.4f); }
            else { g.strokePath(classPath(a.category, xWant, y, rad * 0.85f), juce::PathStrokeType(1.0f)); if (a.status == 5) g.drawLine(xWant - rad, y + rad, xWant + rad, y - rad, 1.0f); }
            used.push_back(Rectangle<float>(xWant - 7, y - 7, 14, 14)); if (s.present) hits.push_back({ id, { xWant, y }, 9.0f });
            if (a.status == 3 || isSel) labs.push_back({ id, { xWant, y }, isSel ? 0 : 2 });
        }
        if (isSel) { const float sx = (a.status == 1 || a.status == 2) ? xNow : xWant; g.setColour(col::text.withAlpha(0.85f)); g.drawEllipse(sx - 10, y - 10, 20, 20, 1.0f); }
    }
    std::sort(labs.begin(), labs.end(), [](const Lab& x, const Lab& y) { return x.prio < y.prio; });
    int nl = 0; for (auto& l : labs)
    {
        if (nl >= 16 && l.prio > 0) break; auto& s = acs[l.id]; const auto& a = s.a;
        juce::String t = juce::String(a.callsign).substring(0, 7); if (t.isEmpty()) t = juce::String(a.type);
        g.setFont(F(10.0f * std::min(1.25f, uiK))); const float w = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), t) + 4.0f; Rectangle<float> r;
        if (placeLabel(l.p, w, 12.0f * std::min(1.25f, uiK), s.slotProf, used, P.expanded(0.0f, 2.0f), r, classRadius(a.category) * uiK + 9.0f)) { used.push_back(r); g.setColour((a.status == 3 ? col::bad : col::text).withAlpha(0.92f * s.alpha)); g.drawText(t, r.toNearestInt(), juce::Justification::centred); ++nl; }
    }
    if (!anyAc)
    {
        g.setColour(col::text.withAlpha(0.85f)); g.setFont(F(12.5f));
        g.drawText(!have ? "No data" : (u.source == 0 ? "Waiting for traffic data..." : "The sky is empty"), P.withSizeKeepingCentre(300, 16).translated(0, -6).toNearestInt(), juce::Justification::centred);
        g.setColour(col::dim); g.setFont(F(10.5f)); g.drawText(u.source == 0 ? "nothing is shown until real data arrives" : "no aircraft within " + juce::String((int) std::lround(u.radiusNm)) + " nm: the drone bed plays alone. Raise the radius.", P.withSizeKeepingCentre(420, 14).translated(0, 12).toNearestInt(), juce::Justification::centred);
    }
}

// --------------------------------------------------------------------------- map ---------------------------------------------------------
// 1.12: a real map. It fills the view, zooms and pans on its own (wheel, +/-, drag; double-click = back to the circle), and the listening
// radius is one dashed circle that can be dragged. Aircraft beyond the circle are drawn dim and silent, with where they are heading.
void SkyView::paintMap(Graphics& g, const UiSnapshot& u, bool have, float dt, double now, uint32_t sel)
{
    juce::ignoreUnused(dt);
    const auto PR = plotRect();
    const double Rn = std::max(5.0, radiusShown), cosLat = std::cos(u.apLat * M_PI / 180.0);
    const double zp = (double) proc.getProp("mapZoomNm", 0.0);
    const double Z = zp > 0.0 ? zp : Rn * 1.5;                         // half of the map's shorter side, in nm
    const float S = (float) (std::min(PR.getWidth(), PR.getHeight()) * 0.5 / Z);
    const double panE = (double) proc.getProp("mapPanE", 0.0), panN = (double) proc.getProp("mapPanN", 0.0);
    const juce::Point<float> c(PR.getCentreX() - (float) (panE * S), PR.getCentreY() + (float) (panN * S));
    mapS = S; mapC = c; mapZ = Z;
    auto rel = [&](double lat, double lon, double& dE, double& dN) { double dl = lon - u.apLon; while (dl > 180.0) dl -= 360.0; while (dl < -180.0) dl += 360.0; dE = dl * 60.0 * cosLat; dN = (lat - u.apLat) * 60.0; };
    auto toPx = [&](double lat, double lon) { double dE, dN; rel(lat, lon, dE, dN); return juce::Point<float>(c.x + (float) dE * S, c.y - (float) dN * S); };
    const float Rpx = (float) Rn * S;

    {
        juce::Graphics::ScopedSaveState ss(g);
        juce::Path frame; frame.addRoundedRectangle(PR, 6.0f);
        g.setColour(Colour(0xff0e1114)); g.fillPath(frame); g.reduceClipRegion(frame);

        // land: grey shapes on the dark sea, lakes cut out, a faint shoreline (Natural Earth, 1.13); cached as paths in pixels
        if ((bool) proc.getProp("mapLand", true))
        {
            const juce::String landKey = juce::String(u.apLat, 4) + "|" + juce::String(u.apLon, 4) + "|" + juce::String(Z, 3) + "|" + juce::String(panE, 3) + "|" + juce::String(panN, 3) + "|" + PR.toString();
            if (landKey != landCacheKey)
            {
                const double hw = PR.getWidth() * 0.5 / S * 1.2, hh = PR.getHeight() * 0.5 / S * 1.2, cl = std::max(0.05, cosLat);
                LandPaths lp;
                buildLand(u.apLat, u.apLon, u.apLat + panN / 60.0, u.apLon + panE / (60.0 * cl), hh / 60.0 + 0.1, std::min(180.0, hw / (60.0 * cl) + 0.1), Z > 110.0,
                          [&](double dE, double dN) { return juce::Point<float>(c.x + (float) dE * S, c.y - (float) dN * S); }, lp);
                landFill = lp.land; lakeFill = lp.lakes; landCache = lp.shore; landCacheKey = landKey;
            }
            g.setColour(Colour(0xff21262b)); g.fillPath(landFill); g.setColour(Colour(0xff0e1114)); g.fillPath(lakeFill);
            g.setColour(Colour(0x12ffffff)); g.strokePath(landCache, juce::PathStrokeType(0.8f));
        }
        // airports from the list
        {
            int na; const Airport* aps = airports(na);
            for (int i = 0; i < na; ++i)
            {
                const auto q = toPx(aps[i].lat, aps[i].lon); if (!PR.expanded(20.0f).contains(q)) continue;
                g.setColour(col::dim.withAlpha(0.7f)); g.drawEllipse(q.x - 2.5f, q.y - 2.5f, 5, 5, 1.0f);
                if (Z <= 200.0) { g.setFont(F(9.5f)); g.setColour(col::dim.withAlpha(0.75f)); g.drawText(iataOf(aps[i].icao), (int) q.x + 4, (int) q.y - 5, 30, 11, juce::Justification::left); }
            }
        }
        // where the data ends (live data is fetched a little beyond the circle; the simulator only fills the circle)
        if (have && (u.source == 1 || u.source == 2))
        {
            const float A = (float) mapAreaNm(Rn) * S;
            if (A < std::hypot(PR.getWidth(), PR.getHeight()))
            {
                g.setColour(Colour(0x14ffffff)); g.drawEllipse(c.x - A, c.y - A, A * 2, A * 2, 1.0f);
                g.setColour(Colour(0xff5a6168)); g.setFont(F(9.5f)); g.drawText("data ends", (int) c.x - 30, (int) (c.y + A) - 13, 60, 11, juce::Justification::centred);
            }
        }
        // the listening circle: dashed; inside it aircraft sound
        {
            g.setColour(col::accent.withAlpha(0.035f)); g.fillEllipse(c.x - Rpx, c.y - Rpx, Rpx * 2, Rpx * 2);
            juce::Path circ; circ.addEllipse(c.x - Rpx, c.y - Rpx, Rpx * 2, Rpx * 2);
            juce::Path dashed; const float dl[] = { 6.0f, 5.0f }; juce::PathStrokeType(nearEdge || dragMode == 1 ? 2.0f : 1.4f).createDashedStroke(dashed, circ, dl, 2);
            g.setColour(col::accent.withAlpha(nearEdge || dragMode == 1 ? 0.95f : 0.6f)); g.fillPath(dashed);
            const juce::String rl = (Rn < 10 ? juce::String(Rn, 1) : juce::String((int) std::lround(Rn))) + " nm";
            g.setColour(col::accent.withAlpha(0.9f)); g.setFont(F(10.5f, juce::Font::bold));
            g.drawText(rl, (int) (c.x + Rpx * 0.707f) + 4, (int) (c.y - Rpx * 0.707f) - 16, 60, 12, juce::Justification::left);
            if (nearEdge && dragMode == 0) { g.setColour(col::dim); g.setFont(F(9.5f)); g.drawText("drag to change the radius", (int) (c.x + Rpx * 0.707f) + 4, (int) (c.y - Rpx * 0.707f) - 4, 140, 11, juce::Justification::left); }
        }
        // listener
        g.setColour(col::text.withAlpha(0.14f)); g.fillEllipse(c.x - 7, c.y - 7, 14, 14); g.setColour(col::text); g.fillEllipse(c.x - 2, c.y - 2, 4, 4);
        // crossings of the circle: a short ring (green coming in, grey going out)
        for (auto& x : crosses)
        {
            const float k = (float) juce::jlimit(0.0, 1.0, (now - x.t) / 2.5); const auto q = toPx(x.lat, x.lon); const float r = 6.0f + 22.0f * k;
            g.setColour((x.in ? col::accent : col::dim).withAlpha(0.8f * (1.0f - k))); g.drawEllipse(q.x - r, q.y - r, r * 2, r * 2, 1.4f);
        }
        // ground traffic: dashed hollow diamonds, silent
        for (int i = 0; have && i < u.nGround; ++i)
        {
            const auto& a = u.gnd[i]; auto q = toPx(a.lat, a.lon); const Colour cc = catColour(a.category); const bool isSel = sel == kGroundId + (uint32_t) i;
            juce::Path d; d.addQuadrilateral(q.x, q.y - 5, q.x + 5, q.y, q.x, q.y + 5, q.x - 5, q.y); juce::Path dl2; float dsh[] = { 2.0f, 2.0f }; juce::PathStrokeType(1.1f).createDashedStroke(dl2, d, dsh, 2);
            g.setColour(cc.withAlpha(isSel ? 1.0f : 0.7f)); g.fillPath(dl2); hits.push_back({ kGroundId + (uint32_t) i, q, 8.0f });
            if (isSel) { g.setColour(col::text.withAlpha(0.8f)); g.drawEllipse(q.x - 9, q.y - 9, 18, 18, 1.0f); }
        }
        // aircraft beyond the circle: dim outlines with a short trail, silent
        outHits.clear();
        for (auto& kv : outs)
        {
            const auto& s = kv.second; const auto p = toPx(s.lat, s.lon); if (!PR.expanded(12.0f).contains(p)) continue;
            const Colour cc = catColour(s.a.category); const float al = s.alpha * (kv.first == hoverOut ? 0.95f : 0.7f);
            for (size_t i = 0; i < s.trail.size(); ++i) { auto q = toPx(s.trail[i].x, s.trail[i].y); g.setColour(cc.withAlpha(al * 0.5f * (float) (i + 1) / (float) s.trail.size())); g.fillEllipse(q.x - 1.0f, q.y - 1.0f, 2.0f, 2.0f); }
            const float rad = classRadius(s.a.category) * uiK * 0.85f;
            g.setColour(cc.withAlpha(al)); g.strokePath(mapGlyph(s.a.category, p.x, p.y, rad, s.a.track), juce::PathStrokeType(1.0f));
            if (s.present) outHits.push_back({ kv.first, p, 9.0f });
            if (s.a.gs > 30.0f && kv.first != hoverOut)
            {   // coming in soon: say so next to it
                double dE, dN; rel(s.lat, s.lon, dE, dN); const double m = crossEtaMin(dE, dN, s.a.track, s.a.gs, Rn, false);
                if (m > 0.0 && m < 15.0) { g.setColour(col::dim.withAlpha(0.85f * s.alpha)); g.setFont(F(9.5f)); g.drawText(juce::String(s.a.callsign).substring(0, 7) + "  in " + juce::String((int) std::max(1.0, std::round(m))) + " min", (int) p.x + 8, (int) p.y - 13, 110, 11, juce::Justification::left); }
            }
        }
        bool anyAc = false; for (auto& kv : acs) anyAc = anyAc || kv.second.present;
        if (!anyAc)
        {
            g.setColour(col::dim.withAlpha(0.9f)); g.setFont(F(11.5f));
            g.drawText(!have ? "No data" : (u.source == 0 ? "Waiting for traffic data..." : "No aircraft inside the circle"), Rectangle<float>(c.x - 130, c.y + 24, 260, 14).toNearestInt(), juce::Justification::centred);
            g.setFont(F(10.0f)); g.setColour(col::dim.withAlpha(0.65f));
            g.drawText(u.source == 0 ? "nothing is shown until real data arrives" : (outs.empty() ? "drone bed only: widen the radius" : "drone bed only until one flies in"), Rectangle<float>(c.x - 130, c.y + 40, 260, 12).toNearestInt(), juce::Justification::centred);
        }
        // trail of the selected aircraft
        for (auto& kv : acs)
        {
            const auto& s = kv.second; if (kv.first != sel || s.trail.size() < 2) continue;
            for (size_t i = 1; i < s.trail.size(); ++i)
            {
                g.setColour(catColour(s.a.category).withAlpha(0.35f * s.alpha * (float) i / (float) s.trail.size()));
                auto p0 = toPx(s.trail[i - 1].x, s.trail[i - 1].y), p1 = toPx(s.trail[i].x, s.trail[i].y); g.drawLine(p0.x, p0.y, p1.x, p1.y, 1.0f);
            }
        }
        // beating pairs (Advanced only): the strongest, plus the selected aircraft's, as arcs bending away from the place
        if (!proc.simpleUi)
        {
            struct Pr { uint32_t a, b; float score; double beat; };
            std::vector<Pr> prs;
            for (int i = 0; i < u.nVoices; ++i) for (int j = i + 1; j < u.nVoices; ++j)
            {
                const auto &va = u.v[i], &vb = u.v[j]; if (va.kind != 1 || vb.kind != 1) continue;
                uint32_t ia = va.id & ~kAcVoiceBit, ib = vb.id & ~kAcVoiceBit; if (!acs.count(ia) || !acs.count(ib)) continue;
                double bt = 1e9; for (int h1 = 1; h1 <= 3; ++h1) for (int h2 = 1; h2 <= 3; ++h2) bt = std::min(bt, (double) std::fabs(va.freq * h1 - vb.freq * h2));
                if (bt < 0.2 || bt > 12.0) continue; prs.push_back({ ia, ib, std::min(va.level, vb.level) / (float) (1.0 + bt), bt });
            }
            std::sort(prs.begin(), prs.end(), [](auto& x, auto& y) { return x.score > y.score; });
            std::vector<Pr> draw; if (!prs.empty()) draw.push_back(prs[0]);
            for (auto& p : prs) { if (draw.size() >= 3) break; if (sel && (p.a == sel || p.b == sel) && (draw.empty() || draw[0].a != p.a || draw[0].b != p.b)) draw.push_back(p); }
            for (auto& p : draw)
            {
                auto &A = acs[p.a], &B = acs[p.b]; auto pa = toPx(A.lat, A.lon), pb = toPx(B.lat, B.lon); auto d = pb - pa; float len = d.getDistanceFromOrigin(); if (len < 26.0f) continue;
                auto n = juce::Point<float>(-d.y, d.x) / len; auto mid = (pa + pb) * 0.5f; if ((mid + n - c).getDistanceFromOrigin() < (mid - n - c).getDistanceFromOrigin()) n = -n;
                auto cp = mid + n * (len * 0.18f); auto u2 = d / len; auto a0 = pa + u2 * 8.0f, b0 = pb - u2 * 8.0f;
                juce::Path arc; arc.startNewSubPath(a0); arc.quadraticTo(cp, b0);
                g.setColour(col::accent.withAlpha(0.4f * std::min(A.alpha, B.alpha))); g.strokePath(arc, juce::PathStrokeType(1.0f));
                auto m = a0 * 0.25f + cp * 0.5f + b0 * 0.25f; g.setColour(col::text.withAlpha(0.8f)); g.setFont(F(10.0f)); g.drawText(juce::String(p.beat, 1) + " Hz", (int) m.x - 24, (int) m.y - 14, 48, 12, juce::Justification::centred);
            }
        }
        // aircraft inside the circle
        std::vector<Rectangle<float>> used; std::vector<uint32_t> ids; for (auto& kv : acs) ids.push_back(kv.first);
        std::sort(ids.begin(), ids.end(), [&](uint32_t x, uint32_t y) { auto &A = acs[x].a, &B = acs[y].a; return (A.status == 1 || A.status == 2) < (B.status == 1 || B.status == 2); });
        for (uint32_t id : ids)
        {
            auto& s = acs[id]; const auto& a = s.a; if (a.status == 0) continue; auto p = toPx(s.lat, s.lon); const Colour cc = catColour(a.category);
            const bool snd = a.status == 1 || a.status == 2, isSel = id == sel; const float al = s.alpha * (snd ? 1.0f : (a.status == 3 ? 0.7f : 0.4f)) * (sel && !isSel ? 0.7f : 1.0f) * (focusClass >= 0 && a.category != focusClass ? 0.4f : 1.0f);
            const float rad = classRadius(a.category) * uiK * (0.6f + 0.4f * std::min(1.0f, s.alpha));
            if (snd)
            {
                const float lv = levelOf.count(id) ? levelOf[id] : 0.0f, pul = std::min(1.0f, std::sqrt(lv) * 1.6f), gr = (13.0f + 10.0f * pul) * uiK;
                g.setGradientFill(juce::ColourGradient(cc.withAlpha((0.14f + 0.34f * pul) * al), p.x, p.y, cc.withAlpha(0.0f), p.x + gr, p.y, true)); g.fillEllipse(p.x - gr, p.y - gr, gr * 2, gr * 2);
                if (a.category != kHeli) { const float t = a.track * 0.0174533f, tx = -std::sin(t), ty = std::cos(t); g.setGradientFill(juce::ColourGradient(cc.withAlpha(0.55f * al), p.x, p.y, cc.withAlpha(0.0f), p.x + tx * 16.0f, p.y + ty * 16.0f, false)); g.drawLine(p.x, p.y, p.x + tx * 16.0f, p.y + ty * 16.0f, 1.0f); }
                g.setColour(cc.withAlpha(al)); g.fillPath(mapGlyph(a.category, p.x, p.y, rad, a.track));
                if (a.status == 2) { g.setColour(col::accent2.withAlpha(al)); g.drawEllipse(p.x - 10, p.y - 10, 20, 20, 1.3f); }
            }
            else
            {
                g.setColour((a.status == 3 ? col::bad : cc).withAlpha(al)); g.strokePath(mapGlyph(a.category, p.x, p.y, rad, a.track), juce::PathStrokeType(1.2f));
                if (a.status == 3) { g.drawLine(p.x - 3, p.y - 3, p.x + 3, p.y + 3, 1.2f); g.drawLine(p.x - 3, p.y + 3, p.x + 3, p.y - 3, 1.2f); }
            }
            if (isSel || id == hoverId) { g.setColour(col::text.withAlpha(isSel ? 0.9f : 0.5f)); g.drawEllipse(p.x - 10, p.y - 10, 20, 20, 1.0f); }
            if (s.present) hits.push_back({ id, p, 9.0f });
            used.push_back(Rectangle<float>(p.x - rad - 2, p.y - rad - 2, rad * 2 + 4, rad * 2 + 4));
        }
        // labels for sounding aircraft: callsign and note
        int shown = 0;
        for (uint32_t id : ids)
        {
            auto& s = acs[id]; const auto& a = s.a; const bool snd = a.status == 1 || a.status == 2; if (!snd || shown >= 12) continue;
            auto p = toPx(s.lat, s.lon); juce::String t = juce::String(a.callsign).substring(0, 7) + (a.note > 0 ? "  " + noteLabel(a.note) : juce::String());
            g.setFont(F(10.0f)); const float w = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), t) + 4.0f; Rectangle<float> r;
            if (placeLabel(p, w, 12.0f, s.slotMap, used, PR, r, classRadius(a.category) * uiK + 6.0f)) { used.push_back(r); g.setColour(col::text.withAlpha(s.alpha)); g.drawText(t, r.toNearestInt(), juce::Justification::centred); ++shown; }
        }
        // hovering an aircraft beyond the circle: where it is going
        if (hoverOut && outs.count(hoverOut) && hoverId == 0)
        {
            const auto& s = outs[hoverOut]; const auto& a = s.a; const auto p = toPx(s.lat, s.lon);
            double dE, dN; rel(s.lat, s.lon, dE, dN);
            const double m = a.gs > 30.0f ? crossEtaMin(dE, dN, a.track, a.gs, Rn, false) : -1.0;
            const juce::String l1 = juce::String(a.callsign[0] ? a.callsign : "(no callsign)") + "  " + className(a.category);
            const juce::String l2 = juce::String(a.type) + "  " + juce::String(a.alt / 1000.0f, 1) + "k ft  " + juce::String((int) std::lround(a.distNm)) + " nm";
            const juce::String l3 = m > 0 ? "enters the circle in " + minutesText(m) : (a.gs > 30.0f ? juce::String("passing by: will not enter") : juce::String("not moving"));
            Rectangle<float> r(p.x + 12.0f, p.y + 10.0f, 182.0f, 52.0f);
            if (r.getRight() > PR.getRight()) r.setX(p.x - 12.0f - r.getWidth()); if (r.getBottom() > PR.getBottom()) r.setY(p.y - 10.0f - r.getHeight());
            g.setColour(Colour(0xf00e1012)); g.fillRoundedRectangle(r, 6.0f); g.setColour(catColour(a.category).withAlpha(0.5f)); g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
            auto t = r.reduced(9.0f, 5.0f); g.setColour(col::text); g.setFont(F(11.0f, juce::Font::bold)); g.drawText(l1, t.removeFromTop(15).toNearestInt(), juce::Justification::centredLeft, true);
            g.setFont(F(10.5f)); g.setColour(col::dim); g.drawText(l2, t.removeFromTop(14).toNearestInt(), juce::Justification::centredLeft, true);
            g.setColour(m > 0 ? col::accent : col::dim); g.drawText(l3, t.removeFromTop(14).toNearestInt(), juce::Justification::centredLeft, true);
        }
        // north and a scale bar
        {
            g.setColour(col::accent); g.setFont(F(10.0f, juce::Font::bold)); g.drawText("N", (int) PR.getRight() - 24, (int) PR.getY() + 6, 14, 11, juce::Justification::centred);
            juce::Path ar; const float ax = PR.getRight() - 17.0f, ay = PR.getY() + 20.0f; ar.addTriangle(ax, ay, ax - 4.0f, ay + 9.0f, ax + 4.0f, ay + 9.0f); g.fillPath(ar);
            static const double steps[] = { 0.5, 1, 2, 5, 10, 20, 25, 50, 100, 200 }; double st = steps[9]; for (double x : steps) if (x * S >= 50.0) { st = x; break; }
            const float bw = (float) st * S, bx = PR.getX() + 12.0f, by = PR.getY() + 22.0f;
            g.setColour(col::dim); g.fillRect(bx, by, bw, 1.0f); g.fillRect(bx, by - 3.0f, 1.0f, 4.0f); g.fillRect(bx + bw - 1.0f, by - 3.0f, 1.0f, 4.0f);
            g.setFont(F(9.5f)); g.drawText((st < 1 ? juce::String(st, 1) : juce::String((int) st)) + " nm", (int) bx, (int) by - 14, (int) bw, 11, juce::Justification::centred);
        }
    }
    // left flank: radius, map zoom, land toggle
    {
        g.setColour(col::text); g.setFont(F(12.0f, juce::Font::bold)); g.drawText("R " + (Rn < 10 ? juce::String(Rn, 1) : juce::String((int) std::lround(Rn))) + " nm", 6, (int) PR.getCentreY() - 56, 70, 18, juce::Justification::centredLeft);
        for (int i = 0; i < 2; ++i) { auto z = zoomRect(i); g.setColour(Colour(0xff202327)); g.fillEllipse(z); g.setColour(col::panelEdge); g.drawEllipse(z.reduced(0.5f), 1.0f); g.setColour(col::text); g.setFont(F(14.0f)); g.drawText(i ? juce::String::fromUTF8("\xe2\x88\x92") : juce::String("+"), z.toNearestInt(), juce::Justification::centred); }
        auto lr = landRect(); const bool on = (bool) proc.getProp("mapLand", true);
        g.setColour(on ? col::accent.withAlpha(0.2f) : Colour(0xff141618)); g.fillRoundedRectangle(lr, 5.0f); g.setColour(on ? col::accent : col::panelEdge); g.drawRoundedRectangle(lr.reduced(0.5f), 5.0f, 1.0f);
        g.setColour(on ? col::text : col::dim); g.setFont(F(10.5f)); g.drawText("land", lr.toNearestInt(), juce::Justification::centred);
    }
}

// --------------------------------------------------------------------------- earth ---------------------------------------------------------
// A tilted, rotatable view of the place: the ground (coast, grid, range rings) is drawn once into an image and only redrawn when the camera,
// radius or place changes; per frame only the aircraft (stem, shadow, ripples, trail, glow, glyph) are painted.
void SkyView::paintEarth(Graphics& g, const UiSnapshot& u, bool have, float dt, double now, uint32_t sel)
{
    const double tStart = juce::Time::getMillisecondCounterHiRes();
    const auto A = Rectangle<float>(0.0f, 60.0f, (float) getWidth(), (float) getHeight() - 60.0f);
    const double D = std::max(5.0, radiusShown), cosLat = std::cos(u.apLat * M_PI / 180.0);
    const float tiltDeg = (float) std::max(12.0, std::min(70.0, (double) proc.getProp("earthTilt", 28.0))), yawDeg = (float) (double) proc.getProp("earthYaw", 0.0);
    const float st = std::sin(tiltDeg * 0.0174533f), ct = std::cos(tiltDeg * 0.0174533f), sy = std::sin(yawDeg * 0.0174533f), cw = std::cos(yawDeg * 0.0174533f);
    const float k = std::min(1.45f, std::min((float) getWidth() / 1000.0f, A.getHeight() / 270.0f) * (A.getHeight() > 400.0f ? 1.4f : 1.0f));
    const float S = 7.2f * k * 40.0f / (float) D, zk = 20.0f * (float) D / 40.0f / 42000.0f, dk = 0.42f / (float) D;
    const float ox = A.getCentreX(), oy = A.getY() + A.getHeight() * 0.53f;      // 1.12: was 0.68 (sat too low)
    auto proj = [&](double X, double Y, double Z, float* pp = nullptr) {
        const double xr = X * cw - Y * sy, yr = X * sy + Y * cw, up = yr * st + Z * ct, dep = yr * ct - Z * st;
        const float p = (float) (1.0 / std::max(0.2, 1.0 + dep * dk)); if (pp) *pp = p;
        return juce::Point<float>(ox + (float) xr * S * p, oy - (float) up * S * p); };
    static const double steps[] = { 0.5, 1, 2, 5, 10, 20, 25, 50, 100 }; double step = steps[8]; for (double s2 : steps) if (D / s2 <= 4.01) { step = s2; break; }
    const bool landOn = (bool) proc.getProp("mapLand", true);

    // ---- ground cache ----
    const auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    const float sc = disp ? (float) std::max(1.0, std::min(2.0, disp->scale)) : 1.0f;
    const juce::String key = juce::String(getWidth()) + "x" + juce::String(getHeight()) + "|" + juce::String(D, 1) + "|" + juce::String(tiltDeg, 1) + "|" + juce::String(yawDeg, 1) + "|" + juce::String(u.apLat, 4) + juce::String(u.apLon, 4) + (landOn ? "L" : "l") + juce::String(sc, 1);
    if (key != earthKey || !earthCache.isValid())
    {
        const double c0 = juce::Time::getMillisecondCounterHiRes();
        juce::Image img(juce::Image::ARGB, (int) (A.getWidth() * sc), (int) (A.getHeight() * sc), true);
        {
            Graphics ig(img); ig.addTransform(juce::AffineTransform::translation(-A.getX(), -A.getY()).followedBy(juce::AffineTransform::scale(sc)));
            const float hy = proj(0, D * 2.4, 0).y;
            { juce::ColourGradient sk(col::panel, 0, A.getY(), Colour(0xff2a3541), 0, std::max(A.getY() + 40.0f, hy), false); sk.addColour(0.55, Colour(0xff0f1318)); ig.setGradientFill(sk); ig.fillRect(A); }
            juce::Path gp; for (int i = 0; i <= 120; ++i) { const double q = i / 120.0 * 6.2831853; auto p = proj(std::sin(q) * D * 2.4, std::cos(q) * D * 2.4, 0); if (i) gp.lineTo(p); else gp.startNewSubPath(p); }
            gp.closeSubPath();
            if (landOn)
            {   // 1.13: land as grey shapes on a dark sea, lakes cut out, a faint shoreline; clipped to the ground disc
                ig.setColour(Colour(0xff0e1114)); ig.fillPath(gp);
                juce::Graphics::ScopedSaveState ss(ig); ig.reduceClipRegion(gp);
                const double reach = D * 2.4, cl = std::max(0.05, cosLat); LandPaths lp;
                buildLand(u.apLat, u.apLon, u.apLat, u.apLon, reach / 60.0 + 0.1, std::min(180.0, reach / (60.0 * cl) + 0.1), reach > 160.0, [&](double dE, double dN) { return proj(dE, dN, 0); }, lp);
                ig.setColour(Colour(0xff21262b)); ig.fillPath(lp.land); ig.setColour(Colour(0xff0e1114)); ig.fillPath(lp.lakes);
                ig.setColour(Colour(0x14ffffff)); ig.strokePath(lp.shore, juce::PathStrokeType(0.8f));
            }
            else { ig.setGradientFill(juce::ColourGradient(Colour(0xff26303a), 0, hy, Colour(0xff101316), 0, hy + A.getHeight() * 0.7f, false)); ig.fillPath(gp); }
            { juce::ColourGradient fog(Colour(0xcc3a4856), 0, hy, Colour(0x003a4856), 0, hy + 90.0f, false); ig.setGradientFill(fog); ig.fillRect(0.0f, hy, A.getWidth(), 90.0f); }
            ig.setColour(Colour(0x40aab8c8)); ig.drawLine(0.0f, hy, A.getWidth(), hy, 1.0f);
            // grid
            ig.setColour(Colour(0x0bffffff)); const double ext = D * 2.2;
            for (double v = -std::floor(ext / step) * step; v <= ext; v += step) { auto a0 = proj(v, -ext, 0), a1 = proj(v, ext, 0), b0 = proj(-ext, v, 0), b1 = proj(ext, v, 0); ig.drawLine(a0.x, a0.y, a1.x, a1.y, 1.0f); ig.drawLine(b0.x, b0.y, b1.x, b1.y, 1.0f); }
            // range rings
            for (double r = step; r < D * 1.001; r += step)
            {
                const bool outer = r > D * 0.999 - 1e-6 || std::fabs(r - D) < step * 0.01;
                juce::Path rp; for (int i = 0; i <= 90; ++i) { const double q = i / 90.0 * 6.2831853; auto p = proj(std::sin(q) * r, std::cos(q) * r, 0); if (i) rp.lineTo(p); else rp.startNewSubPath(p); }
                ig.setColour(col::accent.withAlpha(outer ? 0.4f : 0.17f)); ig.strokePath(rp, juce::PathStrokeType(outer ? 1.4f : 1.0f));
                auto lp = proj(r * 0.7071, r * 0.7071, 0); ig.setColour(col::accent.withAlpha(0.6f)); ig.setFont(F(9.5f)); ig.drawText((r < 10 ? juce::String(r, r == std::floor(r) ? 0 : 1) : juce::String((int) std::lround(r))) + " nm", (int) lp.x + 3, (int) lp.y - 13, 44, 11, juce::Justification::left);
            }
            { auto pn = proj(0, D * 1.08, 0); ig.setColour(col::accent); ig.setFont(F(10.5f, juce::Font::bold)); ig.drawText("N", (int) pn.x - 8, (int) pn.y - 12, 16, 12, juce::Justification::centred); }
            // other airports and the listener
            { int na; const Airport* aps = airports(na);
              for (int i = 0; i < na; ++i) { double dN = (aps[i].lat - u.apLat) * 60.0, dE = (aps[i].lon - u.apLon) * 60.0 * cosLat, dd = std::hypot(dE, dN); if (dd > D * 0.98 || dd < 0.3) continue;
                  auto q = proj(dE, dN, 0); ig.setColour(col::dim.withAlpha(0.7f)); ig.drawEllipse(q.x - 2.5f, q.y - 2.5f, 5, 5, 1.0f); if (D <= 150.0) { ig.setFont(F(9.5f)); ig.setColour(col::dim.withAlpha(0.75f)); ig.drawText(iataOf(aps[i].icao), (int) q.x + 4, (int) q.y - 5, 30, 11, juce::Justification::left); } } }
            { auto b0 = proj(0, 0, 0), b1 = proj(0, 0, 42000.0 * zk); ig.setColour(Colour(0x30ffffff)); ig.drawLine(b0.x, b0.y, b1.x, b1.y, 1.0f);
              for (int ft = 10000; ft <= 40000; ft += 10000) { auto t = proj(0, 0, ft * (double) zk); ig.setColour(Colour(0x55ffffff)); ig.drawLine(t.x - 3, t.y, t.x + 3, t.y, 1.0f); ig.setColour(Colour(0x70ffffff)); ig.setFont(F(9.0f)); ig.drawText(juce::String(ft / 1000) + "k ft", (int) t.x + 6, (int) t.y - 5, 40, 10, juce::Justification::left); } }
            { auto c = proj(0, 0, 0); ig.setColour(col::text.withAlpha(0.16f)); ig.fillEllipse(c.x - 9, c.y - 4.5f, 18, 9); ig.setColour(col::text); ig.fillEllipse(c.x - 2.2f, c.y - 1.4f, 4.4f, 2.8f);
              ig.setFont(F(10.5f, juce::Font::bold)); ig.drawText(proc.isCustomPlace() ? proc.placeName().upToFirstOccurrenceOf(",", false, false).substring(0, 14) : juce::String(iataOf(proc.makePlace().code)), (int) c.x + 8, (int) c.y - 6, 90, 12, juce::Justification::left); }
        }
        earthCache = img; earthKey = key; earthCacheMs = juce::Time::getMillisecondCounterHiRes() - c0;
    }
    g.drawImage(earthCache, A.getX(), A.getY(), A.getWidth(), A.getHeight(), 0, 0, earthCache.getWidth(), earthCache.getHeight());

    // ---- aircraft, far to near ----
    struct E { uint32_t id; juce::Point<float> g0, p; float pa, gy; };
    std::vector<E> order;
    for (auto& kv : acs)
    {
        const auto& s = kv.second; if (s.a.status == 0) continue;
        const double X = (s.lon - u.apLon) * 60.0 * cosLat, Y = (s.lat - u.apLat) * 60.0, Z = (double) s.a.alt * zk;
        float pa = 1.0f; auto g0 = proj(X, Y, 0), p = proj(X, Y, Z, &pa); order.push_back({ kv.first, g0, p, pa, g0.y });
    }
    std::sort(order.begin(), order.end(), [](const E& a, const E& b) { return a.gy < b.gy; });
    std::vector<Rectangle<float>> used;
    for (auto& e : order)
    {
        auto& s = acs[e.id]; const auto& a = s.a; const Colour cc = catColour(a.category);
        const bool snd = a.status == 1 || a.status == 2, isSel = e.id == sel;
        const float al = s.alpha * (snd ? 1.0f : (a.status == 3 ? 0.7f : 0.4f)) * (sel && !isSel ? 0.7f : 1.0f) * (focusClass >= 0 && a.category != focusClass ? 0.4f : 1.0f);
        const float lv = levelOf.count(e.id) ? levelOf[e.id] : 0.0f, pul = std::min(1.0f, std::sqrt(lv) * 1.6f);
        const double X = (s.lon - u.apLon) * 60.0 * cosLat, Y = (s.lat - u.apLat) * 60.0, Z = (double) a.alt * zk;
        // ripples on the ground while it sounds
        if (snd) for (int r = 0; r < 2; ++r)
        {
            const double ph = std::fmod(now * 0.45 + (e.id % 97) * 0.11 + r * 0.5, 1.0), rr = 0.03 * D + ph * D * (0.07 + 0.16 * pul);
            juce::Path rp; for (int i = 0; i <= 28; ++i) { const double q = i / 28.0 * 6.2831853; auto p = proj(X + std::sin(q) * rr, Y + std::cos(q) * rr, 0); if (i) rp.lineTo(p); else rp.startNewSubPath(p); }
            g.setColour(cc.withAlpha((float) (1.0 - ph) * 0.34f * (0.35f + pul) * al)); g.strokePath(rp, juce::PathStrokeType(1.1f));
        }
        // trail: where it has been, at its altitude
        if (s.trail.size() > 2)
        {
            juce::Path tp; for (size_t i = 0; i < s.trail.size(); ++i) { auto q = proj((s.trail[i].y - u.apLon) * 60.0 * cosLat, (s.trail[i].x - u.apLat) * 60.0, Z); if (i) tp.lineTo(q); else tp.startNewSubPath(q); }
            tp.lineTo(e.p); g.setColour(cc.withAlpha(0.32f * al)); g.strokePath(tp, juce::PathStrokeType(1.5f));
        }
        // stem and shadow
        g.setGradientFill(juce::ColourGradient(cc.withAlpha(0.8f * al), e.p.x, e.p.y, cc.withAlpha(0.18f * al), e.g0.x, e.g0.y, false)); g.drawLine(e.p.x, e.p.y, e.g0.x, e.g0.y, 1.3f);
        const float shw = 7.0f * e.pa * uiK; g.setColour(Colour(0x66000000)); g.fillEllipse(e.g0.x - shw, e.g0.y - shw * st * 0.9f, shw * 2, shw * st * 1.8f);
        g.setColour(cc.withAlpha(0.55f * al)); g.drawEllipse(e.g0.x - shw, e.g0.y - shw * st * 0.9f, shw * 2, shw * st * 1.8f, 1.0f);
        const float rad = classRadius(a.category) * uiK * std::max(0.8f, e.pa) * (0.6f + 0.4f * std::min(1.0f, s.alpha));
        if (snd)
        {
            const float gr = (16.0f + 22.0f * pul) * uiK * std::max(0.8f, e.pa);
            g.setGradientFill(juce::ColourGradient(cc.withAlpha((0.16f + 0.36f * pul) * al), e.p.x, e.p.y, cc.withAlpha(0.0f), e.p.x + gr, e.p.y, true)); g.fillEllipse(e.p.x - gr, e.p.y - gr, gr * 2, gr * 2);
            g.setColour(cc.withAlpha(al)); g.fillPath(mapGlyph(a.category, e.p.x, e.p.y, rad, a.track - yawDeg));
            if (a.status == 2) { g.setColour(col::accent2.withAlpha(al)); g.drawEllipse(e.p.x - rad - 5, e.p.y - rad - 5, (rad + 5) * 2, (rad + 5) * 2, 1.3f); }
        }
        else { g.setColour((a.status == 3 ? col::bad : cc).withAlpha(al)); g.strokePath(mapGlyph(a.category, e.p.x, e.p.y, rad, a.track - yawDeg), juce::PathStrokeType(1.2f)); }
        if (isSel || e.id == hoverId) { g.setColour(col::text.withAlpha(isSel ? 0.9f : 0.5f)); g.drawEllipse(e.p.x - 11, e.p.y - 11, 22, 22, 1.0f); }
        if (s.present) hits.push_back({ e.id, e.p, 10.0f });
        used.push_back(Rectangle<float>(e.p.x - rad - 2, e.p.y - rad - 2, rad * 2 + 4, rad * 2 + 4));
    }
    int shown = 0;
    for (int i = (int) order.size() - 1; i >= 0; --i)
    {
        auto& s = acs[order[(size_t) i].id]; const auto& a = s.a; if (!(a.status == 1 || a.status == 2) || shown >= 12) continue;
        juce::String t = juce::String(a.callsign).substring(0, 7) + (a.note > 0 ? "  " + noteLabel(a.note) : juce::String());
        g.setFont(F(10.0f)); const float w = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), t) + 4.0f; Rectangle<float> r;
        if (placeLabel(order[(size_t) i].p, w, 12.0f, s.slotMap, used, A.reduced(4.0f), r, classRadius(a.category) * uiK + 6.0f)) { used.push_back(r); g.setColour(col::text.withAlpha(s.alpha)); g.drawText(t, r.toNearestInt(), juce::Justification::centred); ++shown; }
    }
    if (acs.empty()) { g.setColour(col::dim.withAlpha(0.9f)); g.setFont(F(11.5f)); g.drawText(!have ? "No data" : (u.source == 0 ? "Waiting for traffic data..." : "No aircraft within " + juce::String((int) std::lround(u.radiusNm)) + " nm"), Rectangle<float>(ox - 130, oy + 24, 260, 14).toNearestInt(), juce::Justification::centred); }
    g.setColour(col::dimmer); g.setFont(F(10.0f)); g.drawText("Drag to tilt and rotate  -  scroll to zoom  -  double-click to reset", A.getX() + 14, A.getY() + 4, 420, 12, juce::Justification::left);
    const double ms = juce::Time::getMillisecondCounterHiRes() - tStart; earthMs += (ms - earthMs) * 0.1;
    if ((bool) proc.getProp("debugPaint", false)) { g.setColour(col::warn); g.setFont(F(10.0f)); g.drawText("earth " + juce::String(earthMs, 2) + " ms  (ground rebuild " + juce::String(earthCacheMs, 1) + " ms)", getWidth() - 300, (int) A.getY() + 4, 286, 12, juce::Justification::right); }
}

void ScopeView::paint(Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(Colour(0xff111315)); g.fillRoundedRectangle(b, 8.0f);
    g.setColour(col::panelEdge); g.drawRoundedRectangle(b.reduced(0.5f), 8.0f, 1.0f);
    g.setColour(col::panelEdge.withAlpha(0.8f)); g.drawLine(b.getX() + 4, b.getCentreY(), b.getRight() - 4, b.getCentreY(), 1.0f);
    juce::Path p; const int n = (int) hist.size();
    for (int i = 0; i < n; ++i)
    {
        float x = b.getX() + 6.0f + (b.getWidth() - 12.0f) * (float) i / (float) (n - 1), y = b.getCentreY() - hist[(size_t) i] * (b.getHeight() * 0.5f - 6.0f);
        if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
    }
    g.setColour(col::accent2.withAlpha(0.25f)); g.strokePath(p, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(col::accent2); g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ================================ spectrum view ==========================================
void SpectrumView::paint(Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(Colour(0xff111315)); g.fillRoundedRectangle(b, 10.0f);
    g.setColour(col::panelEdge); g.drawRoundedRectangle(b.reduced(0.5f), 10.0f, 1.0f);
    float ratio[kPartials], amp[kPartials]; proc.getTimbrePreview(ratio, amp);
    // log-frequency axis from 80 Hz to 9 kHz, bars at the real partial frequencies of a G3 note
    const double f0 = 196.0; auto xOf = [&](double f) { return b.getX() + 8.0f + (float) ((std::log(f) - std::log(80.0)) / (std::log(9000.0) - std::log(80.0))) * (b.getWidth() - 16.0f); };
    float h = b.getHeight() - 24.0f;
    float peak = 1e-6f; for (int k = 0; k < kPartials; ++k) peak = std::max(peak, amp[k]);
    g.setColour(col::dim); g.setFont(F(10.5f, juce::Font::bold)); g.drawText("TIMBRE", (int) b.getX() + 10, (int) b.getY() + 4, 80, 12, juce::Justification::left);
    for (int k = 0; k < kPartials; ++k)
    {
        double f = f0 * ratio[k]; if (amp[k] < 1e-4f || f < 80 || f > 9000) continue;
        float x = xOf(f), a = amp[k] / peak, bh = std::sqrt(a) * h;
        auto c = col::accent.withAlpha(1.0f - 0.45f * (float) k / kPartials);
        g.setColour(c.withAlpha(0.25f)); g.fillRoundedRectangle(x - 3.0f, b.getBottom() - 8 - bh, 6.0f, bh, 2.0f);
        g.setColour(c); g.fillRoundedRectangle(x - 1.5f, b.getBottom() - 8 - bh, 3.0f, bh, 1.5f);
    }
    g.setColour(col::dim); g.setFont(F(8.5f));
    for (double f : { 100.0, 500.0, 2000.0, 8000.0 }) g.drawText(f >= 1000 ? juce::String((int) (f / 1000)) + "k" : juce::String((int) f), (int) xOf(f) - 12, (int) b.getBottom() - 12, 24, 10, juce::Justification::centred);
}

// ================================ midi drag handle =======================================
void MidiDragHandle::paint(Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(Colour(0xff202327)); g.fillRoundedRectangle(r, 6); g.setColour(dragging ? col::accent : col::panelEdge); g.drawRoundedRectangle(r, 6, 1.0f);
    g.setColour(col::text); g.setFont(F(12.0f)); g.drawText("Drag MIDI", r.withTrimmedLeft(22), juce::Justification::centredLeft);
    g.setColour(col::accent); for (int i = 0; i < 3; ++i) g.fillEllipse(8.0f, 8.0f + i * 5.0f, 3.0f, 3.0f);
}
void MidiDragHandle::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Aeola " + juce::Time::getCurrentTime().formatted("%Y-%m-%d %H%M") + ".mid");
    if (!proc.exportMidi(f)) { juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Nothing to export yet", "Aeola hasn't played any notes yet. Let it run for a bit, then drag again."); return; }
    dragging = true; repaint();
    juce::DragAndDropContainer::performExternalDragDropOfFiles({ f.getFullPathName() }, false, this, [this]() { dragging = false; repaint(); });
}

// ================================ tour ===================================================
TourOverlay::TourOverlay() { setInterceptsMouseClicks(true, false); }
static const int kTourSteps = 5;
void TourOverlay::paint(Graphics& g)
{
    struct S { const char* title; const char* body; };
    static const S t[kTourSteps] = {
        { "Welcome to Aeola", "The aircraft flying over a real place right now play this instrument. Every shape in the sky view is an aircraft, and every column is a note." },
        { "Higher flight, higher note. Closer, louder.", "Hover an aircraft to see what it is and why it sounds as it does. Where a note was moved, a dashed arrow shows it." },
        { "Choose where you listen", "Pick an airport or search for your own street. A cargo hub at night sounds nothing like a quiet regional field." },
        { "Consonance: how pleasant it sounds", "When two aircraft would play clashing notes, Consonance moves one of them to a nearby note. Turn it up to keep everything agreeing, down to let tension and clusters through." },
        { "Four macros are all you need", "Tension, Light, Motion and Space always stay musical. Open Advanced to shape every class of aircraft, the tuning and the synth." },
    };
    const int n = std::min(step, kTourSteps - 1);
    const auto all = getLocalBounds().toFloat();
    juce::Rectangle<float> tg = target ? target(n) : juce::Rectangle<float>();
    if (!tg.isEmpty()) tg = tg.expanded(6.0f);
    juce::Path dim; dim.addRectangle(all); if (!tg.isEmpty()) dim.addRoundedRectangle(tg, 9.0f); dim.setUsingNonZeroWinding(false);
    g.setColour(Colour(0xff0b0c0e).withAlpha(0.80f)); g.fillPath(dim);
    if (!tg.isEmpty()) { g.setColour(col::accent.withAlpha(0.16f)); g.drawRoundedRectangle(tg.expanded(3.0f), 11.0f, 5.0f); g.setColour(col::accent.withAlpha(0.9f)); g.drawRoundedRectangle(tg, 9.0f, 1.4f); }

    const float cw = 520.0f, ch = 196.0f; float cy = (all.getHeight() - ch) * 0.5f;
    if (!tg.isEmpty()) cy = tg.getCentreY() < all.getHeight() * 0.5f ? std::min(tg.getBottom() + 22.0f, all.getHeight() - ch - 16.0f) : std::max(16.0f, tg.getY() - 22.0f - ch);
    juce::Rectangle<float> card((all.getWidth() - cw) * 0.5f, cy, cw, ch);
    g.setColour(Colour(0xff000000).withAlpha(0.35f)); g.fillRoundedRectangle(card.translated(0, 6).expanded(2), 12.0f);
    g.setColour(col::panel); g.fillRoundedRectangle(card, 10.0f); g.setColour(col::panelEdge.brighter(0.15f)); g.drawRoundedRectangle(card.reduced(0.5f), 10.0f, 1.0f);
    auto in = card.reduced(26.0f, 22.0f);
    caps(g, "Step " + juce::String(n + 1) + " of " + juce::String(kTourSteps), in.removeFromTop(14.0f), col::accent);
    in.removeFromTop(6.0f);
    g.setColour(col::text); g.setFont(F(20.0f, juce::Font::bold)); g.drawText(t[n].title, in.removeFromTop(28.0f).toNearestInt(), juce::Justification::centredLeft);
    in.removeFromTop(6.0f);
    g.setColour(col::dim.brighter(0.25f)); g.setFont(F(13.5f)); g.drawFittedText(t[n].body, in.removeFromTop(66.0f).toNearestInt(), juce::Justification::topLeft, 4);
    auto foot = card.reduced(26.0f, 18.0f).removeFromBottom(30.0f);
    for (int i = 0; i < kTourSteps; ++i) { g.setColour(i == n ? col::accent : col::panelEdge.brighter(0.3f)); g.fillEllipse(foot.getX() + i * 16.0f, foot.getCentreY() - 3.5f, 7.0f, 7.0f); }
    const bool last = n == kTourSteps - 1;
    nextR = foot.removeFromRight(last ? 92.0f : 80.0f).withSizeKeepingCentre(last ? 92.0f : 80.0f, 30.0f);
    g.setColour(col::accent); g.fillRoundedRectangle(nextR, 6.0f); g.setColour(Colour(0xff0f1a14)); g.setFont(F(13.0f, juce::Font::bold)); g.drawText(last ? "Start" : "Next", nextR.toNearestInt(), juce::Justification::centred);
    skipR = foot.removeFromRight(70.0f).withSizeKeepingCentre(60.0f, 30.0f);
    if (!last) { g.setColour(col::dim); g.setFont(F(12.5f)); g.drawText("Skip", skipR.toNearestInt(), juce::Justification::centred); } else skipR = {};
}
void TourOverlay::mouseUp(const juce::MouseEvent& e)
{
    if (skipR.contains(e.position)) { if (onDone) onDone(); return; }
    if (++step >= kTourSteps) { if (onDone) onDone(); } else repaint();
}

// ================================ editor =================================================
Knob* AeolaDroneEditor::addKnob(const char* id, const char* label, bool adv)
{
    auto k = std::make_unique<Knob>(proc.apvts, id, label); k->advanced = adv;
    content.addAndMakeVisible(*k); k->slider.lookAndFeelChanged(); auto* p = k.get(); kmap[id] = p; p->proc = &proc;
    p->onShowMatrix = [this](int d) { showMatrixFor(d); };
    for (int d = 1; d < kNumDst; ++d) if (juce::String(ID::modDestParam(d)) == id) { p->modDst = d; break; }
    p->refreshMod(); knobs.push_back(std::move(k)); return p;
}
juce::ComboBox* AeolaDroneEditor::addCombo(const char* id)
{
    auto c = std::make_unique<juce::ComboBox>(); 
    bool isSrc = false, isDst = false;
    for (int i = 0; i < kModSlots; ++i) { if (juce::String(id) == ID::modSlotId(i, 0)) isSrc = true; if (juce::String(id) == ID::modSlotId(i, 1)) isDst = true; }
    if (isSrc || isDst)
    {   // grouped list: general entries flat, per-class ones in sub-menus. Item order must equal the parameter index (ComboBox counts sub-menu items in order).
        const char* const* nm = isSrc ? modSrcNames() : modDstNames();
        struct Seg { int start, count; const char* sub; };
        std::vector<Seg> segs;
        if (isSrc) segs = { { 0, SrcClassCount, nullptr }, { SrcClassCount, kNumClasses, "Count per class" }, { SrcClassAlt, kNumClasses, "Altitude per class" } };
        else segs = { { 0, DstClassLevel, nullptr }, { DstClassLevel, kNumClasses, "Gain per class" }, { DstClassOctave, kNumClasses, "Octave per class" },
                      { DstInharm, DstClassSwell - DstInharm, nullptr }, { DstClassSwell, kNumClasses, "Swell time per class" }, { DstClassChop, kNumClasses, "Chop per class" } };
        for (auto& sg : segs)
        {
            if (!sg.sub) { for (int i = sg.start; i < sg.start + sg.count; ++i) c->addItem(nm[i], i + 1); continue; }
            juce::PopupMenu sm; for (int i = sg.start; i < sg.start + sg.count; ++i) sm.addItem(i + 1, nm[i]);
            c->getRootMenu()->addSubMenu(sg.sub, sm);
        }
    }
    else if (auto* prm = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(id))) c->addItemList(prm->choices, 1);
    content.addAndMakeVisible(*c);
    comboAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, id, *c));
    c->setTooltip(tooltipFor(id)); auto* p = c.get(); cmap[id] = p; combos.push_back(std::move(c)); return p;
}
juce::ToggleButton* AeolaDroneEditor::addToggle(const char* id, const char* label)
{
    auto t = std::make_unique<juce::ToggleButton>(label); content.addAndMakeVisible(*t);
    toggleAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, id, *t));
    t->setTooltip(tooltipFor(id)); auto* p = t.get(); tmap[id] = p; toggles.push_back(std::move(t)); return p;
}

AeolaDroneEditor::AeolaDroneEditor(AeolaDroneProcessor& p) : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&laf);
    simple = (bool) proc.getProp("simple", true); proc.simpleUi = simple;
    uiScale = (float) (double) proc.getProp("uiScale", 1.0);
    addAndMakeVisible(content);
    content.addAndMakeVisible(outSlider); outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, ID::gain, outSlider);
    outSlider.setTooltip(tooltipFor(ID::gain)); outSlider.setDoubleClickReturnValue(true, 0.5);

    // ---- knobs ----
    addKnob(ID::brightness, "Colour", false); addKnob(ID::morph, "Morph", false); addKnob(ID::life, "Life", false); addKnob(ID::air, "Air", false);
    addKnob(ID::beat, "Beat", false); addKnob(ID::drift, "Drift", true); addKnob(ID::morphmod, "Morph motion", true);
    addKnob(ID::attack, "Attack", false); addKnob(ID::release, "Release", false); addKnob(ID::breath, "Breath", false);
    addKnob(ID::oddeven, "Odd/Even", true); addKnob(ID::inharm, "Inharmonic", true); addKnob(ID::driftrate, "Drift rate", true);
    addKnob(ID::voices, "Voices", true); addKnob(ID::panspread, "Pan spread", true); addKnob(ID::velsens, "Velocity", true);
    addKnob(ID::north, "NORTH", false); addKnob(ID::glide, "Glide", false);
    addKnob(ID::rvmix, "Reverb", false); addKnob(ID::rvsize, "Size", false); addKnob(ID::rvdamp, "Damp", true); addKnob(ID::width, "Width", true);
    addKnob(ID::rvpre, "Pre-delay", true);
    addKnob(ID::cutoff, "Cutoff", true); addKnob(ID::reso, "Resonance", true); addKnob(ID::lowcut, "Low cut", true); addKnob(ID::warmth, "Warmth", true);
    addKnob(ID::consonance, "Consonance", false); addKnob(ID::maxac, "Aircraft", false); addKnob(ID::range, "Range", true);
    addKnob(ID::evolve, "Evolve", false); addKnob(ID::ghostmin, "Quiet fill", true); addKnob(ID::stretch, "Loop stretch", true);
    addKnob(ID::drive, "Drive", true); addKnob(ID::fmix, "Mix", true); addKnob(ID::ftrack, "Key track", true);
    addKnob(ID::l1rate, "Rate", false); addKnob(ID::l2rate, "Rate", false);
    addKnob(ID::tension, "TENSION", false); addKnob(ID::light, "LIGHT", false); addKnob(ID::motion, "MOTION", false); addKnob(ID::space, "SPACE", false);
    for (auto* id : { ID::tension, ID::light, ID::motion, ID::space }) { kmap[id]->setSize(56, 90); }
    kmap[ID::north]->setSize(74, 86);
    for (auto* id : { ID::brightness, ID::morph, ID::life, ID::air, ID::beat, ID::attack, ID::release, ID::breath, ID::drift, ID::oddeven, ID::inharm, ID::morphmod, ID::driftrate, ID::voices, ID::panspread, ID::velsens }) kmap[id]->setSize(58, 78);

    for (auto* id : { ID::system, ID::rootmode, ID::fixedroot, ID::source, ID::airport, ID::key, ID::scale, ID::srca, ID::srcb, ID::ftype, ID::l1shape, ID::l1div, ID::l2shape, ID::l2div,
                      ID::m1src, ID::m1dst, ID::m2src, ID::m2dst, ID::m3src, ID::m3dst, ID::m4src, ID::m4dst,
                      ID::m5src, ID::m5dst, ID::m6src, ID::m6dst, ID::m7src, ID::m7dst, ID::m8src, ID::m8dst }) addCombo(id);
    addToggle(ID::skyon, "SKY"); addToggle(ID::anchor, "Anchor"); addToggle(ID::anchorfifth, "+ fifth"); addToggle(ID::hold, "HOLD"); addToggle(ID::rvsend, "Reverb > out 2"); addToggle(ID::simsync, "Sim = transport");
    addToggle(ID::l1sync, "Sync"); addToggle(ID::l2sync, "Sync");
    addToggle(ID::swell, "Overhead swell");
    for (int c = 0; c < kNumClasses; ++c) { addCombo(ID::classId(c, 0)); addToggle(ID::classId(c, 5), "S"); addToggle(ID::classId(c, 6), "M"); }
    addKnob(ID::radius, "Radius", false);
    for (int c = 0; c < kNumClasses; ++c) { addKnob(ID::classId(c, 1), "Gain", true); addKnob(ID::classId(c, 2), "Register", true); addKnob(ID::classId(c, 3), "Swell time", true); addKnob(ID::classId(c, 4), "Chop", true); }
    addKnob(ID::baseoct, "Lowest octave", true); addKnob(ID::evolveperiod, "Period", true);
    addKnob(ID::modamt, "Mod wheel", true); addKnob(ID::bendrange, "Bend range", true); addKnob(ID::ghostlevel, "Ghost level", true);      // 1.12: had no control
    for (int c = 0; c < kNumClasses; ++c) addKnob(ID::classMixId(c, 1), "Pan", true);
    auto bar = [&](juce::Slider& sl, const char* id, double dflt) {
        sl.setSliderStyle(juce::Slider::LinearBar); sl.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0); content.addAndMakeVisible(sl);
        classAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id, sl));
        sl.setDoubleClickReturnValue(true, dflt); sl.setTooltip(tooltipFor(id)); sl.setVelocityBasedMode(false); sl.lookAndFeelChanged();
    };
    bar(contrastBar, ID::contrast, 0.7); bar(stretchBar, ID::stretch, 1.0); bar(simBar, ID::simlevel, 1.0); bar(morphBar, ID::morph, 0.0);
    for (int c = 0; c < kNumClasses; ++c)
    {
        const double d[4] = { 0.0, 0.0, 1.0, 0.0 };
        for (int k = 0; k < 4; ++k) bar(classBar[c][k], ID::classId(c, k + 1), d[k]);
    }
    placeBtn.onClick = [this] { showPlaceMenu(); }; content.addAndMakeVisible(placeBtn);
    sky.onPlaceClick = [this] { showPlaceMenu(); };
    placeBtn.setTooltip("Where you are listening: pick an airport in the list, or search a city, or enter coordinates.");
    cmap[ID::airport]->onChange = [this] { int n; airports(n); if (cmap[ID::airport]->getSelectedItemIndex() >= n && !proc.apvts.state.hasProperty("placeLat")) showPlaceDialog(); };
    kmap[ID::radius]->setSize(58, 78);
    for (int i = 0; i < kModSlots; ++i)
    {
        auto& sl = modAmt[i]; sl.setSliderStyle(juce::Slider::LinearHorizontal); sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 20);
        content.addAndMakeVisible(sl); modAmtAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, ID::modSlotId(i, 2), sl));
        sl.setDoubleClickReturnValue(true, 0.0); sl.setTooltip(tooltipFor(ID::modSlotId(i, 2))); sl.lookAndFeelChanged();
        auto& del = modDel[i]; del.setButtonText(juce::String::fromUTF8("\xc3\x97")); del.setTooltip("Remove this connection"); content.addAndMakeVisible(del);
        del.onClick = [this, i] { for (int w = 0; w < 3; ++w) if (auto* q = proc.apvts.getParameter(ID::modSlotId(i, w))) q->setValueNotifyingHost(q->convertTo0to1(0.0f)); addSlot = -1; layoutAll(); };
    }
    {   // the Advanced drawer: one tab open at a time
        const char* names[] = { "Aircraft", "Sound", "Tune & Space", "Filter", "Modulation", "Sky", "Mix" };
        const char* tips[] = { "What each kind of aircraft sounds like.", "The base sound: the drone bed and every class on Global.", "Tuning system, North, glide, reverb and width.",
                               "Filter model, cutoff, drive and tone.", "LFOs and the Matrix: let anything move any control.", "Key, scale, Consonance, the drone bed, how the sky evolves, data.", "A fader, pan, solo, mute and meter per aircraft class, and separate outputs." };
        drawer = std::max(0, std::min(6, (int) proc.getProp("drawer", 0))); selClass = std::max(0, std::min(kNumClasses - 1, (int) proc.getProp("selClass", (int) kCargo)));
        for (int i = 0; i < 7; ++i) { dTab[i].setButtonText(names[i]); dTab[i].setComponentID("tab"); dTab[i].setClickingTogglesState(false); dTab[i].setTooltip(tips[i]); content.addAndMakeVisible(dTab[i]);
                                      dTab[i].onClick = [this, i] { setDrawer(i); }; }
        content.addAndMakeVisible(soloBtn); soloBtn.setClickingTogglesState(false);
        soloBtn.onClick = [this] { const int c = selClass; const bool on = !proc.classFlag(c, 5); proc.clearClassFlags(); proc.setClassFlag(c, 5, on); layoutAll(); };
        soloBtn.setTooltip("Hear only this class. If none is in the sky, a virtual one plays so you can tune it.");
        content.addAndMakeVisible(moreBtn); moreBtn.onClick = [this] { showMore = !showMore; layoutAll(); };
        content.addAndMakeVisible(addConnBtn); addConnBtn.setButtonText("+  Add a connection");
        for (int c = 0; c < kNumClasses; ++c)
        {   // 1.12 mixer
            auto& f = mixFader[c]; f.setSliderStyle(juce::Slider::LinearVertical); f.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 16);
            content.addAndMakeVisible(f); mixAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, ID::classMixId(c, 0), f));
            f.setDoubleClickReturnValue(true, 0.0); f.setTooltip(tooltipFor(ID::classMixId(c, 0))); f.lookAndFeelChanged();
            meters[c] = std::make_unique<MixMeter>(proc, c); content.addAndMakeVisible(*meters[c]);
        }
        content.addAndMakeVisible(classOutBtn); classOutBtn.setClickingTogglesState(false);
        classOutBtn.onClick = [this] { proc.setProp("classOuts", !(bool) proc.getProp("classOuts", false)); layoutAll(); };
        classOutBtn.setTooltip("Send every aircraft class to its own stereo output, so you can mix them in your DAW. Classes whose output the host has not switched on stay in the main output.");
        seedBar.setSliderStyle(juce::Slider::LinearBar); seedBar.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0); content.addAndMakeVisible(seedBar);
        seedAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, ID::simseed, seedBar);
        seedBar.textFromValueFunction = [](double v) { return "seed " + juce::String((int) v); }; seedBar.updateText(); seedBar.setTooltip(tooltipFor(ID::simseed)); seedBar.lookAndFeelChanged();
        addConnBtn.onClick = [this] { for (int i = 0; i < kModSlots; ++i) if (proc.P(ID::modSlotId(i, 0)) < 0.5f && proc.P(ID::modSlotId(i, 1)) < 0.5f) { addSlot = i; break; } layoutAll(); };
        const char* ft[] = { "Clean", "Ladder", "Ladder 12 dB" };
        for (int i = 0; i < 3; ++i) { ftBtn[i].setButtonText(ft[i]); content.addAndMakeVisible(ftBtn[i]); ftBtn[i].setClickingTogglesState(false);
                                      ftBtn[i].onClick = [this, i] { if (auto* p = proc.apvts.getParameter(ID::ftype)) p->setValueNotifyingHost(p->convertTo0to1((float) i)); layoutAll(); }; }
        auto relayout = [this] { juce::Component::SafePointer<AeolaDroneEditor> sp(this); juce::MessageManager::callAsync([sp] { if (sp) sp->layoutAll(); }); };
        for (int i = 0; i < kModSlots; ++i) { cmap[ID::modSlotId(i, 0)]->onChange = relayout; cmap[ID::modSlotId(i, 1)]->onChange = relayout; }
        tmap[ID::l1sync]->onClick = relayout; tmap[ID::l2sync]->onClick = relayout; cmap[ID::source]->onChange = relayout;
    }

    // ---- preset bar ----
    content.addAndMakeVisible(presetBox); rebuildPresetMenu();
    presetBox.onChange = [this] {
        int id = presetBox.getSelectedId();
        if (id >= 1 && id < 1000) proc.loadFactoryPreset(id - 1);
        else if (id >= 1000) proc.loadUserPreset(presetBox.getText());
    };
    prevBtn.onClick = [this] { int n = (int) factoryPresets().size(); proc.loadFactoryPreset((std::max(0, proc.currentPreset) + n - 1) % n); };
    nextBtn.onClick = [this] { int n = (int) factoryPresets().size(); proc.loadFactoryPreset((proc.currentPreset + 1) % n); };
    saveBtn.onClick = [this] {
        auto* w = new juce::AlertWindow("Save preset", "Name your sound:", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor("name", "My drone"); w->addButton("Save", 1); w->addButton("Cancel", 0);
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, w](int r) { if (r == 1) { proc.saveUserPreset(w->getTextEditorContents("name")); rebuildPresetMenu(); } delete w; }), false);
    };
    diceBtn.onClick = [this] { proc.randomise(); };
    setA.onClick = [this] { proc.storeScene(0); setA.setButtonText("A *"); morph.setValue(0.0, juce::dontSendNotification); };
    setB.onClick = [this] { proc.storeScene(1); setB.setButtonText("B *"); morph.setValue(1.0, juce::dontSendNotification); };
    morph.setRange(0.0, 1.0); morph.onValueChange = [this] { proc.morphScenes((float) morph.getValue()); };
    setA.setTooltip("Store the current sound in slot A. Store another in B, then use the slider to morph between them.");
    setB.setTooltip("Store the current sound in slot B.");
    morph.setTooltip("Morph between sound A and sound B.");
    diceBtn.setTooltip("Random sound within musical limits (the Sky always stays in key).");
    saveBtn.setTooltip("Save the current sound as a user preset."); presetBox.setTooltip("Choose a sound. Each factory preset says what it is for.");
    for (auto* c : std::initializer_list<juce::Component*>{ &presetBox, &prevBtn, &nextBtn, &saveBtn, &diceBtn, &setA, &setB, &morph, &midiHandle, &modeBtn, &gearBtn, &helpBtn, &fixBtn, &freezeBtn, &sky, &spectrum, &scope1, &scope2, &skyTab, &modTab, &aircraftTab, &status, &hint })
        content.addAndMakeVisible(*c);
    sky.toBack();                                   // the Sky panel owns the footer area: hint line and Why? / Freeze / Hold buttons sit above it
    midiHandle.setTooltip("Drag the last 30 minutes of notes Aeola played into your DAW as a MIDI clip, to use them with any other instrument.");
    modeBtn.setClickingTogglesState(false);
    modeBtn.onClick = [this] { simple = !simple; proc.simpleUi = simple; proc.setProp("simple", simple); layoutAll(); };
    modeBtn.setTooltip("Switch between the few essential controls and every control.");
    gearBtn.onClick = [this] { showSettings(); };
    helpBtn.onClick = [this] { if (!tour) { tour = std::make_unique<TourOverlay>(); tour->target = [this](int st) { return tourTarget(st); }; tour->onDone = [this] { proc.setProp("tourDone", true); tour.reset(); }; content.addAndMakeVisible(*tour); tour->setBounds(content.getLocalBounds()); } };
    fixBtn.onClick = [this] { auto m = proc.autoFix(); juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Why does this sound off?", m); };
    fixBtn.setTooltip("Checks the settings that most often make a sky sound unmusical, and fixes them.");
    freezeBtn.onClick = [this] {
        proc.freezeSkyForRender();
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Sky frozen",
            "The last " + juce::String(proc.capturedFrames()) + " traffic snapshots are saved in this project and now play back as Replay, so your render is identical every time.");
    };
    freezeBtn.setTooltip("Saves the recent real traffic inside your project and switches to Replay, so bounces are repeatable.");
    status.setFont(F(13.0f)); status.setJustificationType(juce::Justification::centred); status.setColour(juce::Label::textColourId, col::text);
    hint.setFont(F(12.5f)); hint.setJustificationType(juce::Justification::centredLeft); hint.setColour(juce::Label::textColourId, col::dim);
    status.setInterceptsMouseClicks(false, false); hint.setInterceptsMouseClicks(false, false);

    if (!(bool) proc.getProp("tourDone", false)) helpBtn.onClick();

    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio((double) W / H); getConstrainer()->setSizeLimits(W * 3 / 4, H * 3 / 4, W * 2, H * 2);
    layoutAll();
    setSize((int) (W * uiScale), (int) (H * uiScale));
    startTimerHz(20);
}

// ---- place: menu, dialog, geocoding --------------------------------------------------------
void AeolaDroneEditor::showPlaceMenu()
{
    juce::PopupMenu m; m.addItem(1, "Search a city or enter coordinates...");
    { int na; const Airport* aps = airports(na); m.addSeparator(); m.addSectionHeader("Airports");
      for (int i = 0; i < na; ++i) m.addItem(2000 + i, juce::String(iataOf(aps[i].icao)) + "  " + aps[i].name + "  -  " + airportNote(i)); }
    auto rec = proc.recentPlaces();
    if (!rec.empty()) { m.addSeparator(); m.addSectionHeader("Recent places"); for (size_t i = 0; i < rec.size(); ++i) m.addItem(100 + (int) i, rec[i].name); }
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sky), [this, rec](int r) {
        if (r == 1) showPlaceDialog();
        else if (r >= 2000) { if (auto* pr = proc.apvts.getParameter(ID::airport)) pr->setValueNotifyingHost(pr->convertTo0to1((float) (r - 2000))); }
        else if (r >= 100 && (size_t) (r - 100) < rec.size()) proc.setCustomPlace(rec[(size_t) (r - 100)].lat, rec[(size_t) (r - 100)].lon, rec[(size_t) (r - 100)].name);
    });
}
void AeolaDroneEditor::showPlaceDialog()
{
    auto* w = new juce::AlertWindow("Where are you?", "Type a city or address (needs internet), or coordinates like  59.33, 18.07", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor("q", "", "Place:"); w->addButton("Set place", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<AeolaDroneEditor> self(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([self, w](int r) { if (r == 1 && self != nullptr) self->applyPlaceQuery(w->getTextEditorContents("q")); delete w; }), false);
}
void AeolaDroneEditor::applyPlaceQuery(const juce::String& qIn)
{
    const juce::String q = qIn.trim(); if (q.isEmpty()) return;
    auto parts = juce::StringArray::fromTokens(q.replaceCharacter(';', ',').replaceCharacter('/', ','), ",", "");
    if (parts.size() == 2 && parts[0].containsAnyOf("0123456789") && parts[1].containsAnyOf("0123456789") && !parts[0].containsAnyOf("abcdfghijklmopqrstuvxyzABCDFGHIJKLMOPQRSTUVXYZ"))
    {
        double lat = parts[0].getDoubleValue(), lon = parts[1].getDoubleValue();
        if (std::fabs(lat) <= 90.0 && std::fabs(lon) <= 180.0) { proc.setCustomPlace(lat, lon, ""); return; }
    }
    hint.setText("Searching for " + q + " ...", juce::dontSendNotification);
    juce::Component::SafePointer<AeolaDroneEditor> self(this);
    juce::Thread::launch([self, q] {
        auto url = juce::URL("https://nominatim.openstreetmap.org/search").withParameter("q", q).withParameter("format", "json").withParameter("limit", "1");
        auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs(8000)
                        .withExtraHeaders("User-Agent: Aeola/" JucePlugin_VersionString " (+https://github.com/orgfwd/aeola-vst)\r\nAccept: application/json\r\n");
        juce::String body; if (auto in = url.createInputStream(opts)) body = in->readEntireStreamAsString();
        double lat = 0, lon = 0; juce::String name; bool ok = false;
        auto js = juce::JSON::parse(body);
        if (js.isArray() && js.size() > 0) if (auto* o = js[0].getDynamicObject()) { lat = (double) o->getProperty("lat"); lon = (double) o->getProperty("lon"); name = o->getProperty("display_name").toString(); ok = o->hasProperty("lat"); }
        auto nm = juce::StringArray::fromTokens(name, ",", ""); juce::String shortName = nm.size() > 1 ? nm[0].trim() + ", " + nm[nm.size() - 1].trim() : name.trim();
        juce::MessageManager::callAsync([self, ok, lat, lon, shortName, q] {
            if (self == nullptr) return;
            if (ok) { self->proc.setCustomPlace(lat, lon, shortName.isNotEmpty() ? shortName : q); self->hint.setText("", juce::dontSendNotification); }
            else juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Place not found", "Could not find \"" + q + "\". Check your internet connection, or enter coordinates like 59.33, 18.07.");
        });
    });
}

AeolaDroneEditor::~AeolaDroneEditor() { setLookAndFeel(nullptr); }

void AeolaDroneEditor::rebuildPresetMenu()
{
    int sel = presetBox.getSelectedId(); presetBox.clear(juce::dontSendNotification);
    presetBox.addSectionHeading("Factory");
    int i = 1; for (auto& p : factoryPresets()) presetBox.addItem(juce::String(p.name) + "   -   " + p.purpose, i++);
    auto users = proc.userPresetNames();
    if (users.size()) { presetBox.addSectionHeading("User"); int k = 1000; for (auto& u : users) presetBox.addItem(u, k++); }
    presetBox.setSelectedId(sel, juce::dontSendNotification);
}

void AeolaDroneEditor::showSettings()
{
    juce::PopupMenu m;
    if (const auto up = UpdateCheck::available(); up.valid) { m.addItem(20, "Version " + up.version + " is available: download"); m.addSeparator(); }
    m.addSectionHeader("Interface size");
    for (int pct : { 75, 100, 125, 150, 200 }) m.addItem(1000 + pct, juce::String(pct) + " %", true, std::abs(uiScale * 100 - pct) < 1);
    m.addSeparator(); m.addItem(1, "Show the intro tour again");
    m.addItem(2, "Fetch live traffic now");
    m.addItem(5, "Use adsb.fi as a backup when the daily limit is reached", true, (bool) proc.getProp("backupFeed", false));
    m.addItem(34, "Separate outputs: one stereo output per aircraft class", true, (bool) proc.getProp("classOuts", false));
    m.addSeparator(); m.addSectionHeader("MIDI out (VST3)");
    m.addItem(30, "On", true, proc.P(ID::midiout) > 0.5f);
    m.addItem(32, "Mute sound", true, proc.P(ID::midisilent) > 0.5f);
    m.addItem(31, "Channel per class (for hardware)", true, proc.P(ID::midimode) < 0.5f);
    { juce::PopupMenu cm; const int cur = (int) proc.P(ID::midiclass); cm.addItem(40, "All (source)", true, cur == 0);
      for (int c = 0; c < kNumClasses; ++c) cm.addItem(41 + c, className(c), true, cur == c + 1);
      m.addSubMenu("Class", cm); }
    if (proc.P(ID::midiout) > 0.5f && proc.P(ID::midiclass) > 0.5f)
        m.addItem(33, proc.isFollowing() ? "Copying the Aeola set to All" : "No Aeola set to All: own sky", false, false);
    m.addSeparator(); m.addSectionHeader("Help and support");
    m.addItem(10, "Send feedback...");
    m.addItem(11, "Details for a bug report...");
    m.addItem(12, "Support Aeola (Ko-fi)");
    m.addItem(13, "Website and download");
    m.addItem(14, "Look for a new version once a day", true, UpdateCheck::enabled());
    m.addSeparator(); m.addSectionHeader("About");
    m.addItem(3, "Aeola " JucePlugin_VersionString "  -  GPL-3.0, free software (source code)");
    m.addItem(4, "Live traffic: adsb.lol (ODbL 1.0)");
    m.addItem(6, "Optional backup: adsb.fi (personal, non-commercial use)");
    m.addItem(7, "Coastlines: Natural Earth (public domain)");
    m.addItem(8, "Built with JUCE");
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&gearBtn), [this](int r) {
        if (r >= 1075) applyScale((float) (r - 1000) / 100.0f);
        else if (r == 1) helpBtn.onClick();
        else if (r == 2) proc.getFeed().fetchNow();
        else if (r == 3) juce::URL(links::source).launchInDefaultBrowser();
        else if (r == 4) juce::URL("https://adsb.lol").launchInDefaultBrowser();
        else if (r == 6) juce::URL("https://adsb.fi").launchInDefaultBrowser();
        else if (r == 7) juce::URL("https://www.naturalearthdata.com").launchInDefaultBrowser();
        else if (r == 8) juce::URL("https://juce.com").launchInDefaultBrowser();
        else if (r == 10) feedbackUrl(collectDiagnostics().text()).launchInDefaultBrowser();
        else if (r == 11) showDiagnostics();
        else if (r == 12) juce::URL(links::donate).launchInDefaultBrowser();
        else if (r == 13) juce::URL(links::website).launchInDefaultBrowser();
        else if (r == 14) UpdateCheck::setEnabled(!UpdateCheck::enabled());
        else if (r == 20) juce::URL(links::download).launchInDefaultBrowser();
        else if (r == 34) { proc.setProp("classOuts", !(bool) proc.getProp("classOuts", false)); layoutAll(); }
        else if (r >= 40 && r <= 40 + kNumClasses)
        {
            if (auto* p = proc.apvts.getParameter(ID::midiclass)) p->setValueNotifyingHost(p->convertTo0to1((float) (r - 40)));
        }
        else if (r >= 30 && r <= 32)
        {
            auto* p = proc.apvts.getParameter(r == 30 ? ID::midiout : r == 31 ? ID::midimode : ID::midisilent);
            if (p) p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
        }
        else if (r == 5) { proc.setProp("backupFeed", !(bool) proc.getProp("backupFeed", false)); proc.getFeed().fetchNow(); }
    });
}
Diagnostics AeolaDroneEditor::collectDiagnostics()
{
    Diagnostics g; UiSnapshot u; const bool have = proc.getUi(u);
    static const char* names[] = { "Auto", "Live", "Cached", "Replay", "Simulated" }, *shownNames[] = { "waiting", "live", "saved", "replay", "simulated" };
    g.format = juce::AudioProcessor::getWrapperTypeDescription(proc.wrapperType); g.host = juce::PluginHostType().getHostDescription(); g.sampleRate = proc.getSampleRate();
    g.custom = proc.isCustomPlace(); g.place = juce::String(iataOf(proc.makePlace().code)) + " " + juce::String(proc.makePlace().name); g.radiusNm = (int) std::lround(proc.getRadius());
    g.mode = names[std::min(4, std::max(0, (int) proc.P(ID::source)))]; g.shown = shownNames[std::min(4, std::max(0, have ? u.source : 0))];
    g.aircraft = have ? u.nAc : 0; g.sounding = have ? u.accepted : 0; g.maxAircraft = (int) proc.P(ID::maxac); g.voices = (int) proc.P(ID::voices);
    g.feedName = have ? juce::String(u.feedName) : juce::String("adsb.lol"); g.feedErr = have ? juce::String(u.feedErr) : juce::String(); g.ageSec = have ? u.ageSec : 0.0;
    g.requestsToday = SkyFeed::requestsToday(); g.quotaLeft = proc.getFeed().quotaRemaining(); g.quotaResetEpoch = proc.getFeed().quotaResetEpoch(); g.backup = (bool) proc.getProp("backupFeed", false); g.uiScale = uiScale;
    return g;
}
namespace
{
    struct DetailsBox : public juce::Component
    {
        explicit DetailsBox(const juce::String& t)
        {
            ed.setMultiLine(true, false); ed.setReadOnly(true); ed.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.5f, 0))); ed.setText(t, false);
            ed.setColour(juce::TextEditor::backgroundColourId, col::bg0); ed.setColour(juce::TextEditor::textColourId, col::text); ed.setColour(juce::TextEditor::outlineColourId, col::panelEdge);
            addAndMakeVisible(ed); copy.setButtonText("Copy"); close.setButtonText("Close"); addAndMakeVisible(copy); addAndMakeVisible(close);
            copy.onClick = [this] { juce::SystemClipboard::copyTextToClipboard(ed.getText()); copy.setButtonText("Copied"); };
            close.onClick = [this] { if (auto* dw = findParentComponentOfClass<juce::DialogWindow>()) dw->exitModalState(0); };
            setSize(560, 330);
        }
        void paint(juce::Graphics& g) override { g.fillAll(col::panel); }
        void resized() override { auto r = getLocalBounds().reduced(14); auto b = r.removeFromBottom(28); close.setBounds(b.removeFromRight(80)); b.removeFromRight(8); copy.setBounds(b.removeFromRight(80)); r.removeFromBottom(10); ed.setBounds(r); }
        juce::TextEditor ed; juce::TextButton copy, close;
    };
}
void AeolaDroneEditor::showDiagnostics()
{
    juce::DialogWindow::LaunchOptions o; o.content.setOwned(new DetailsBox(collectDiagnostics().text())); o.dialogTitle = "Details for a bug report";
    o.dialogBackgroundColour = col::panel; o.escapeKeyTriggersCloseButton = true; o.useNativeTitleBar = true; o.resizable = false; o.componentToCentreAround = this; o.launchAsync();
}
void AeolaDroneEditor::applyScale(float s) { uiScale = s; proc.setProp("uiScale", (double) s); setSize((int) (W * s), (int) (H * s)); }

void AeolaDroneEditor::timerCallback()
{
    status.setText(proc.statusLine(), juce::dontSendNotification);
    hint.setText(proc.hintLine(), juce::dontSendNotification);
    if (++updateTick % 1200 == 1) { const bool up = UpdateCheck::available().valid; gearBtn.setButtonText(up ? juce::String::fromUTF8("Settings  \xe2\x97\x8f") : juce::String("Settings")); if (up) gearBtn.setTooltip("A new version is available (see Settings)."); }
    int cp = proc.currentPreset;
    if (cp != lastPresetShown) { lastPresetShown = cp; if (cp >= 0) presetBox.setSelectedId(cp + 1, juce::dontSendNotification); else presetBox.setText("Custom", juce::dontSendNotification); }
    content.repaint(Rectangle<int>(0, 0, W, 50));
    if (!simple && drawer == 3)
    {   // the filter response curve is painted on the background: redraw it when a setting that shapes it changes
        const std::array<float, 6> sig = { proc.P(ID::cutoff), proc.P(ID::reso), proc.P(ID::lowcut), proc.P(ID::drive), proc.P(ID::fmix), proc.P(ID::ftype) };
        if (sig != filterSig) { filterSig = sig; ++filterRepaints; content.repaint(Rectangle<int>(340, 440, 460, 132)); }
    }
    if (simple)
    {   // place name on the place button, and Hold shown only while it is on
        const auto pn = proc.placeName().substring(0, 40) + juce::String::fromUTF8("  \xe2\x96\xbe"); if (placeBtn.getButtonText() != pn) placeBtn.setButtonText(pn);
        const bool h = proc.P(ID::hold) > 0.5f; if (tmap[ID::hold]->isVisible() != h) layoutAll();
        content.repaint(20, 620, 1000, 122);
    }
    const double tNow = juce::Time::getMillisecondCounterHiRes() * 0.001; const bool refresh = (++modTick % 5) == 0;
    for (auto& k : knobs)
    {
        if (!k->isVisible()) continue;
        k->refreshValueAbove(); if (refresh && k->modDst) k->refreshMod();
        if (k->modDst || tNow < k->flashUntil + 0.2) k->repaint();
    }
    if (!simple && drawer == 4 && tNow < matrixFlashUntil + 0.2) content.repaint(430, 460, 380, 260);
    if (!simple && drawer == 6) { for (auto& m : meters) if (m) m->repaint(); if (++mixTick % 10 == 0) { content.repaint(260, 690, 540, 36); content.repaint(20, 440, 790, 32); } }
    if (!simple)
    {
        if (++drawerTick % 5 == 0) { if (drawer == 0) { content.repaint(20, 440, 790, 300); if (soloBtn.getToggleState() != proc.classFlag(selClass, 5)) layoutAll(); } content.repaint(818, 460, 202, 20); }
        const auto pn = proc.placeName().substring(0, 26) + juce::String::fromUTF8("  \xe2\x96\xbe"); if (placeBtn.getButtonText() != pn) placeBtn.setButtonText(pn);
    }
}

void AeolaDroneEditor::showMatrixFor(int dst)
{
    matrixFlashDst = dst; matrixFlashUntil = juce::Time::getMillisecondCounterHiRes() * 0.001 + 2.0;
    if (simple) setSimple(false);
    setDrawer(4);
}
void AeolaDroneEditor::goToTarget(int dst)
{
    if (dst <= 0 || dst >= kNumDst) return;
    const juce::String id = ID::modDestParam(dst); int dr = -1;
    auto in = [&](std::initializer_list<const char*> l) { for (auto* x : l) if (id == x) return true; return false; };
    if (in({ ID::brightness, ID::morph, ID::life, ID::air, ID::beat, ID::drift, ID::breath, ID::attack, ID::release })) dr = 1;
    else if (in({ ID::oddeven, ID::inharm, ID::driftrate, ID::morphmod, ID::panspread })) { dr = 1; showMore = true; }
    else if (in({ ID::north, ID::glide, ID::rvmix, ID::rvsize, ID::rvdamp, ID::width })) dr = 2;
    else if (in({ ID::cutoff, ID::reso, ID::drive, ID::fmix, ID::lowcut, ID::warmth, ID::ftrack })) dr = 3;
    else if (in({ ID::l1rate, ID::l2rate })) dr = 4;
    else if (in({ ID::consonance, ID::range, ID::ghostlevel, ID::evolve, ID::stretch })) dr = 5;
    for (int c = 0; c < kNumClasses; ++c) for (int w = 1; w <= 4; ++w) if (id == ID::classId(c, w)) { dr = 0; selClass = c; proc.setProp("selClass", c); }
    if (dr >= 0) setDrawer(dr); else layoutAll();
    auto it = kmap.find(id);
    if (it != kmap.end() && it->second) { it->second->flashUntil = juce::Time::getMillisecondCounterHiRes() * 0.001 + 1.8; it->second->repaint(); }
}

juce::Rectangle<float> AeolaDroneEditor::tourTarget(int st)
{
    if (st <= 1) return sky.getBounds().toFloat();
    if (st == 2) return placeBtn.isVisible() ? placeBtn.getBounds().toFloat() : juce::Rectangle<float>();
    if (st == 3) { auto* k = kmap[ID::consonance]; return k && k->isVisible() ? k->getBounds().toFloat() : juce::Rectangle<float>(); }
    juce::Rectangle<float> r; for (auto* id : { ID::tension, ID::light, ID::motion, ID::space }) { auto* k = kmap[id]; if (k && k->isVisible()) r = r.isEmpty() ? k->getBounds().toFloat() : r.getUnion(k->getBounds().toFloat()); }
    return r;
}
void AeolaDroneEditor::resized()
{
    float s = (float) getWidth() / (float) W;
    content.setBounds(0, 0, W, H);
    content.setTransform(juce::AffineTransform::scale(s));
    if (tour) tour->setBounds(content.getLocalBounds());
}

void AeolaDroneEditor::layoutAll()
{
    auto show = [&](juce::Component* c, bool v, int x, int y) { c->setVisible(v); if (v) c->setTopLeftPosition(x, y); };
    auto knob = [&](const char* id, bool v, int x, int y, int w = 60, int h = 62) { auto* k = kmap[id]; k->setSize(w, h); show(k, v, x, y); };
    auto bigKnob = [&](const char* id, bool v, int cx, int cy, bool val = false) { auto* k = kmap[id]; const bool above = val || (cy > 400 && cx < 810); k->setValueAbove(v && above); if (above) knob(id, v, cx - 38, cy - 50, 76, 99); else knob(id, v, cx - 38, cy - 33, 76, 82); };        // centred on the dial; drawer knobs show their value above
    auto combo = [&](const char* id, bool v, int x, int y, int w, int h = 24) { auto* c = cmap[id]; c->setSize(w, h); show(c, v, x, y); };
    auto toggle = [&](const char* id, bool v, int x, int y, int w, int h = 24) { auto* t = tmap[id]; t->setSize(w, h); show(t, v, x, y); };

    // ---- start from nothing: each mode / tab shows only what it needs ----
    status.setVisible(false); hint.setVisible(false);
    for (auto& k : knobs) { k->setVisible(false); k->setValueAbove(false); }
    for (auto& c : combos) c->setVisible(false);
    for (auto& t : toggles) t->setVisible(false);
    for (int c = 0; c < kNumClasses; ++c) for (int k = 0; k < 4; ++k) classBar[c][k].setVisible(false);
    for (int i = 0; i < kModSlots; ++i) { modAmt[i].setVisible(false); modDel[i].setVisible(false); }
    for (juce::Component* c : std::initializer_list<juce::Component*>{ &contrastBar, &stretchBar, &simBar, &morphBar, &scope1, &scope2, &spectrum, &aircraftTab, &skyTab, &modTab,
                                                                       &outSlider, &midiHandle, &saveBtn, &setA, &setB, &morph, &fixBtn, &freezeBtn, &soloBtn, &moreBtn, &addConnBtn })
        c->setVisible(false);
    for (auto& b : ftBtn) b.setVisible(false);
    for (auto& b : dTab) b.setVisible(false);
    for (auto& f : mixFader) f.setVisible(false); for (auto& m : meters) if (m) m->setVisible(false); classOutBtn.setVisible(false); seedBar.setVisible(false);

    if (simple)
    {   // SIMPLE: the sky, a place, a base sound and the macros. Nothing else.
        sky.focusClass = -1;
        prevBtn.setBounds(232, 10, 26, 26); presetBox.setBounds(262, 10, 260, 26); nextBtn.setBounds(526, 10, 26, 26); diceBtn.setBounds(560, 10, 86, 26);
        modeBtn.setButtonText("Advanced"); modeBtn.setBounds(820, 10, 90, 26); gearBtn.setBounds(914, 10, 64, 26); helpBtn.setBounds(982, 10, 26, 26);
        sky.setVisible(true); sky.setBounds(20, 56, 1000, 556);
        placeBtn.setButtonText(proc.placeName().substring(0, 40) + juce::String::fromUTF8("  \xe2\x96\xbe")); placeBtn.setBounds(36, 652, 236, 26); placeBtn.setVisible(true);
        knob(ID::consonance, true, 306, 650, 54, 66);
        combo(ID::srca, true, 456, 652, 150);
        toggle(ID::hold, proc.P(ID::hold) > 0.5f, 456, 688, 70);
        knob(ID::brightness, true, 610, 650, 54, 66); knob(ID::rvmix, true, 664, 650, 54, 66); knob(ID::cutoff, true, 718, 650, 54, 66);
        { int x = 800; for (auto* id : { ID::tension, ID::light, ID::motion, ID::space }) { knob(id, true, x, 650, 54, 66); x += 54; } }
        content.repaint(); return;
    }

    // ---- ADVANCED: the same sky, a little shorter; one drawer below with one tab open ----
    prevBtn.setBounds(200, 10, 26, 26); presetBox.setBounds(230, 10, 220, 26); nextBtn.setBounds(454, 10, 26, 26);
    saveBtn.setBounds(488, 10, 50, 26); diceBtn.setBounds(542, 10, 86, 26);
    setA.setBounds(644, 10, 28, 26); morph.setBounds(676, 10, 64, 26); setB.setBounds(744, 10, 28, 26);
    outSlider.setBounds(806, 13, 70, 20);
    for (juce::Component* c : std::initializer_list<juce::Component*>{ &saveBtn, &setA, &setB, &morph, &outSlider }) c->setVisible(true);
    modeBtn.setButtonText("Simple"); modeBtn.setBounds(884, 10, 64, 26); gearBtn.setBounds(952, 10, 50, 26); helpBtn.setBounds(1006, 10, 26, 26);
    sky.setVisible(true); sky.setBounds(20, 56, 1000, 330);

    // drawer tabs
    { int x = 34; for (int i = 0; i < 7; ++i) { const int w = (int) juce::GlyphArrangement::getStringWidth(juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), 10.0f, juce::Font::bold).withKerningFactor(0.14f)), dTab[i].getButtonText().toUpperCase()) + 4;
        dTab[i].setBounds(x, 404, w, 26); dTab[i].setVisible(true); dTab[i].setToggleState(i == drawer, juce::dontSendNotification); x += w + 28; } }
    const int sel = std::max(0, std::min(kNumClasses - 1, selClass));
    sky.focusClass = drawer == 0 ? sel : -1;

    // right column: place, radius, aircraft count, macros
    placeBtn.setButtonText(proc.placeName().substring(0, 26) + juce::String::fromUTF8("  \xe2\x96\xbe")); placeBtn.setBounds(832, 430, 174, 26); placeBtn.setVisible(true);
    bigKnob(ID::radius, true, 870, 512, true); bigKnob(ID::maxac, true, 956, 512, true);
    bigKnob(ID::tension, true, 870, 612); bigKnob(ID::light, true, 956, 612); bigKnob(ID::motion, true, 870, 690); bigKnob(ID::space, true, 956, 690);

    switch (drawer)
    {
        case 0:   // ---- Aircraft: class list + the selected class in detail ----
            for (int c = 0; c < kNumClasses; ++c) { const int y = 442 + c * 30; toggle(ID::classId(c, 5), true, 210, y + 4, 28, 21); toggle(ID::classId(c, 6), true, 242, y + 4, 28, 21); }
            combo(ID::classId(sel, 0), true, 318, 532, 170, 26);
            bigKnob(ID::classId(sel, 1), true, 560, 560); bigKnob(ID::classId(sel, 2), true, 650, 560); bigKnob(ID::classId(sel, 3), true, 740, 560); bigKnob(ID::classId(sel, 4), true, 560, 672);
            soloBtn.setBounds(318, 596, 170, 26); soloBtn.setVisible(true);
            soloBtn.setButtonText(proc.classFlag(sel, 5) ? "Stop solo" : juce::String::fromUTF8("\xe2\x96\xb6  Solo and listen")); soloBtn.setToggleState(proc.classFlag(sel, 5), juce::dontSendNotification);
            contrastBar.setVisible(true); contrastBar.setBounds(636, 662, 150, 20);
            break;
        case 1:   // ---- Sound: the base sound ----
            combo(ID::srca, true, 38, 466, 150, 26); morphBar.setBounds(234, 469, 90, 20); morphBar.setVisible(true); combo(ID::srcb, true, 334, 466, 150, 26);
            spectrum.setVisible(true); spectrum.setBounds(500, 446, 290, 74);
            { const char* g1[] = { ID::brightness, ID::life, ID::air }; const char* g2[] = { ID::beat, ID::drift, ID::breath }; const char* g3[] = { ID::attack, ID::release };
              for (int i = 0; i < 3; ++i) bigKnob(g1[i], true, 68 + i * 72, 600); for (int i = 0; i < 3; ++i) bigKnob(g2[i], true, 320 + i * 72, 600); for (int i = 0; i < 2; ++i) bigKnob(g3[i], true, 572 + i * 72, 600); }
            moreBtn.setBounds(showMore ? 700 : 38, showMore ? 610 : 660, showMore ? 90 : 120, 22); moreBtn.setVisible(true); moreBtn.setButtonText(juce::String::fromUTF8(showMore ? "\xe2\x96\xbe  Less" : "\xe2\x96\xb8  More controls"));
            if (showMore) { int x = 38; for (auto* id : { ID::oddeven, ID::inharm, ID::driftrate, ID::morphmod, ID::voices, ID::panspread, ID::velsens, ID::modamt, ID::bendrange }) { knob(id, true, x, 652, 76, 66); x += 82; } }
            break;
        case 2:   // ---- Tune & Space ----
            combo(ID::system, true, 38, 466, 200, 26);
            bigKnob(ID::north, true, 80, 560); bigKnob(ID::glide, true, 180, 560);
            combo(ID::rootmode, true, 38, 676, 130, 26); combo(ID::fixedroot, true, 176, 676, 62, 26);
            { int i = 0; for (auto* id : { ID::rvmix, ID::rvsize, ID::rvdamp, ID::rvpre, ID::width }) bigKnob(id, true, 330 + 96 * i++, 560); }
            toggle(ID::rvsend, true, 296, 640, 220);
            break;
        case 3:   // ---- Filter ----
            for (int i = 0; i < 3; ++i) { ftBtn[i].setBounds(38 + i * 98, 466, 92, 26); ftBtn[i].setVisible(true); ftBtn[i].setToggleState((int) proc.P(ID::ftype) == i, juce::dontSendNotification); }
            { int i = 0; for (auto* id : { ID::cutoff, ID::reso, ID::drive, ID::fmix }) bigKnob(id, true, 80 + 100 * i++, 640); }
            { int i = 0; for (auto* id : { ID::lowcut, ID::warmth, ID::ftrack }) bigKnob(id, true, 510 + 100 * i++, 640); }
            break;
        case 4:   // ---- Motion: two LFOs, then the connections that are in use ----
        {
            for (int l = 0; l < 2; ++l)
            {
                const int x = 38 + l * 190; auto sh = l ? ID::l2shape : ID::l1shape; auto sy2 = l ? ID::l2sync : ID::l1sync; auto dv = l ? ID::l2div : ID::l1div; auto rt = l ? ID::l2rate : ID::l1rate;
                const bool sync = proc.P(sy2) > 0.5f;
                combo(sh, true, x + 12, 476, 154, 26); (l ? scope2 : scope1).setVisible(true); (l ? scope2 : scope1).setBounds(x + 12, 510, 154, 46);
                toggle(sy2, true, x + 12, 582, 56); knob(rt, !sync, x + 90, 566, 76, 66); combo(dv, sync, x + 76, 582, 90, 24);
            }
            int row = 0;
            for (int i = 0; i < kModSlots; ++i)
            {
                const bool used = proc.P(ID::modSlotId(i, 0)) > 0.5f || proc.P(ID::modSlotId(i, 1)) > 0.5f || i == addSlot;
                if (!used) continue;
                const int y = 466 + row++ * 31;
                combo(ID::modSlotId(i, 0), true, 440, y, 120, 25); combo(ID::modSlotId(i, 1), true, 582, y, 110, 25);
                modAmt[i].setVisible(true); modAmt[i].setBounds(700, y, 82, 25);
                modDel[i].setVisible(true); modDel[i].setBounds(786, y + 3, 20, 19); rowSlot[row - 1] = i;
            }
            connRows = row;
            if (row < kModSlots) { addConnBtn.setBounds(440, 466 + row * 31, 350, 25); addConnBtn.setVisible(true); }
            break;
        }
        case 6:   // ---- Mix: a strip per class (1.12) ----
            for (int c = 0; c < kNumClasses; ++c)
            {
                const int x0 = 34 + c * 84;
                mixFader[c].setVisible(true); mixFader[c].setBounds(x0 + 6, 472, 58, 122);
                meters[c]->setVisible(true); meters[c]->setBounds(x0 + 66, 476, 8, 98);
                knob(ID::classMixId(c, 1), true, x0 + 10, 596, 60, 58);
                toggle(ID::classId(c, 5), true, x0 + 10, 658, 28, 21); toggle(ID::classId(c, 6), true, x0 + 42, 658, 28, 21);
            }
            {   const bool on = (bool) proc.getProp("classOuts", false);
                classOutBtn.setButtonText(on ? "Separate outputs: on" : "Separate outputs: off"); classOutBtn.setToggleState(on, juce::dontSendNotification);
                classOutBtn.setBounds(38, 694, 200, 24); classOutBtn.setVisible(true); }
            break;
        default:  // ---- Sky: key and scale, the drone bed, life of the sky, data ----
        {
            combo(ID::key, true, 38, 466, 60, 26); combo(ID::scale, true, 106, 466, 150, 26);
            bigKnob(ID::consonance, true, 70, 560); bigKnob(ID::range, true, 162, 560); bigKnob(ID::baseoct, true, 254, 560);
            toggle(ID::anchor, true, 322, 466, 74); toggle(ID::anchorfifth, true, 402, 466, 74);
            bigKnob(ID::ghostmin, true, 356, 560); toggle(ID::hold, true, 410, 548, 66); knob(ID::ghostlevel, true, 404, 578, 64, 62);
            toggle(ID::swell, true, 510, 466, 170);
            bigKnob(ID::evolve, true, 544, 560); bigKnob(ID::evolveperiod, true, 634, 560); bigKnob(ID::stretch, true, 724, 560);
            combo(ID::source, true, 322, 670, 110, 26);
            const bool sim = (int) proc.P(ID::source) == 4;
            toggle(ID::simsync, sim, 440, 671, 124); if (sim) { simBar.setVisible(true); simBar.setBounds(570, 674, 60, 20); seedBar.setVisible(true); seedBar.setBounds(440, 704, 124, 20); }
            fixBtn.setBounds(640, 670, 64, 26); freezeBtn.setBounds(710, 670, 76, 26); fixBtn.setVisible(true); freezeBtn.setVisible(true);
            midiHandle.setBounds(640, 704, 146, 24); midiHandle.setVisible(true);
            break;
        }
    }
    content.repaint();
}

static const char* classBlurb(int c)
{
    static const char* t[] = {
        "Small private planes, low and slow. A soft flute that comes and goes.",
        "Regional jets and small airliners. A reedy, light voice.",
        "The everyday airliners (A320, 737). They play the base sound: shape it on the Sound tab.",
        "Wide-body long-haul jets. A deep choir, an octave down, slow to swell.",
        "Helicopters: low, close, and chopped like a rotor.",
        "Parcel and cargo jets, mostly at night. A low, slow bowl that swells in and fades away.",
        "Private jets: fast and high. A bright glass tone, an octave up.",
        "Propeller airliners. An organ voice with a propeller flutter.",
        "Military aircraft. Gritty, forward and a little louder." };
    return t[std::max(0, std::min(kNumClasses - 1, c))];
}

void AeolaDroneEditor::Content::paint(Graphics& g)
{
    g.fillAll(col::bg0);
    // top bar
    g.setColour(col::panel); g.fillRect(0, 0, W, 46); g.setColour(col::panelEdge); g.fillRect(0, 46, W, 1);
    {   // wordmark: thin sans, wide tracking, one amber lamp
        auto wf = Sans(24.0f); const char* word = "AEOLA"; float x = 20.0f;
        for (int i = 0; i < 5; ++i)
        {
            juce::String ch = juce::String::charToString((juce::juce_wchar) word[i]); float adv = juce::GlyphArrangement::getStringWidth(wf, ch);
            g.setFont(wf); g.setColour(col::text); g.drawText(ch, (int) x, 8, 40, 30, juce::Justification::centredLeft); x += adv + 8.0f;
        }
        g.setColour(col::accent2); g.fillEllipse(x + 1.0f, 14.0f, 5.0f, 5.0f);
    }
    auto panel = [&](Rectangle<int> r, const char* title) {
        glassPanel(g, r.toFloat(), 8.0f, col::panelEdge);
        if (title && *title) caps(g, title, Rectangle<float>((float) r.getX() + 14.0f, (float) r.getY() + 6.0f, (float) r.getWidth() - 28.0f, 16.0f));
    };
    auto& e = ed; const bool s = e.simple;
    auto credit = [&] { g.setColour(col::dim.withAlpha(0.8f)); g.setFont(F(9.5f)); g.drawText("GPL-3.0. Traffic: adsb.lol (ODbL), optional backup adsb.fi (opendata.adsb.fi). Coastlines: Natural Earth.", 20, H - 10, 420, 10, juce::Justification::left); };
    if (s)
    {
        panel({ 20, 620, 1000, 122 }, nullptr);
        caps(g, "Place", Rectangle<float>(36.0f, 628.0f, 200.0f, 16.0f)); caps(g, "Sound", Rectangle<float>(456.0f, 628.0f, 140.0f, 16.0f)); caps(g, "Macros", Rectangle<float>(800.0f, 628.0f, 140.0f, 16.0f), col::accent);
        caps(g, "Harmony", Rectangle<float>(302.0f, 628.0f, 120.0f, 16.0f));
        g.setColour(col::panelEdge); g.fillRect(286, 632, 1, 96); g.fillRect(440, 632, 1, 96); g.fillRect(790, 632, 1, 96);
        g.setColour(col::dim); g.setFont(F(10.0f)); g.drawFittedText("Higher keeps notes agreeing, lower lets tension in", 364, 656, 68, 54, juce::Justification::topLeft, 5);
        g.setColour(col::dim); g.setFont(F(11.5f));
        g.drawFittedText(juce::String(airportNote(e.proc.isCustomPlace() ? 99 : (int) e.proc.P(ID::airport))), 36, 686, 236, 50, juce::Justification::topLeft, 4);
        if (e.proc.P(ID::morph) > 0.01f)
        {   // the preset blends a second sound in: say so, instead of pretending the base sound is all there is
            int nt = numTimbres(); const int b2 = std::min((int) e.proc.P(ID::srcb), nt - 1);
            g.setColour(col::dim); g.setFont(F(10.5f)); g.drawText("+ " + juce::String((int) std::lround(e.proc.P(ID::morph) * 100.0f)) + " % " + timbres()[(size_t) b2].name, 532, 690, 76, 16, juce::Justification::centredLeft, true);
        }
        credit(); return;
    }

    // ---- Advanced: drawer + right column ----
    panel({ 20, 396, 790, 350 }, nullptr);
    g.setColour(col::panelEdge); g.fillRect(20, 432, 790, 1);
    panel({ 818, 396, 202, 350 }, "Place");
    caps(g, "Macros", Rectangle<float>(832.0f, 563.0f, 160.0f, 16.0f), col::accent);
    g.setColour(col::dim); g.setFont(F(9.5f)); g.drawText("OUT", 776, 13, 28, 20, juce::Justification::centredRight);
    auto help = [&](const juce::String& t) { g.setColour(Colour(0xff6a7279)); g.setFont(F(10.5f)); g.drawText(t, 38, 722, 760, 16, juce::Justification::centredLeft, true); };
    auto vline = [&](int x, int y, int h) { g.setColour(col::panelEdge); g.fillRect(x, y, 1, h); };
    switch (e.drawer)
    {
        case 0:
        {
            UiSnapshot u; e.proc.getUi(u);
            int cnt[kNumClasses] = {}; juce::StringArray calls[kNumClasses];
            for (int i = 0; i < u.nAc; ++i) { const auto& a = u.ac[i]; if ((a.id & 0xFF000000u) == 0x03000000u) continue; const int c = std::max(0, std::min(kNumClasses - 1, a.category)); ++cnt[c]; if (calls[c].size() < 3 && a.callsign[0]) calls[c].add(a.callsign); }
            const int sel = std::max(0, std::min(kNumClasses - 1, e.selClass));
            for (int c = 0; c < kNumClasses; ++c)
            {
                const auto r = Rectangle<float>(30.0f, 442.0f + c * 30.0f, 250.0f, 28.0f);
                if (c == sel) { g.setColour(Colour(0xff25282c)); g.fillRoundedRectangle(r, 5.0f); g.setColour(col::panelEdge); g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 1.0f); }
                else if (c == hoverRow) { g.setColour(Colour(0xff202327)); g.fillRoundedRectangle(r, 5.0f); }
                g.setColour(catColour(c)); classMarker(g, c, 46.0f, r.getCentreY(), 4.5f);
                g.setColour(col::text); g.setFont(F(12.0f, c == sel ? juce::Font::bold : 0)); g.drawText(className(c), 60, (int) r.getY(), 120, 28, juce::Justification::centredLeft);
                g.setColour(cnt[c] ? col::dim : Colour(0xff5d646b)); g.setFont(F(10.5f)); g.drawText(cnt[c] ? juce::String(cnt[c]) + " now" : juce::String::fromUTF8("\xe2\x80\x93"), 150, (int) r.getY(), 52, 28, juce::Justification::centredRight);
            }
            vline(294, 444, 290);
            g.setColour(catColour(sel)); classMarker(g, sel, 318.0f, 462.0f, 7.0f);
            g.setColour(col::text); g.setFont(F(17.0f, juce::Font::bold)); g.drawText(className(sel), 334, 450, 300, 24, juce::Justification::centredLeft);
            g.setColour(col::dim); g.setFont(F(11.5f)); g.drawFittedText(classBlurb(sel), 318, 480, 470, 30, juce::Justification::topLeft, 2);
            caps(g, "Voice", Rectangle<float>(318.0f, 516.0f, 100.0f, 12.0f));
            g.setColour(col::dim); g.setFont(F(11.0f));
            g.drawText(cnt[sel] ? juce::String(cnt[sel]) + " in the sky now: " + calls[sel].joinIntoString(", ") : juce::String("None in the sky now. Solo plays a virtual one."), 318, 566, 300, 20, juce::Justification::centredLeft, true);
            const char* lbl[] = { "Level", "Register", "Swell time" }; (void) lbl;
            vline(612, 632, 70);
            caps(g, "Class contrast", Rectangle<float>(636.0f, 642.0f, 160.0f, 12.0f));
            g.setColour(Colour(0xff6a7279)); g.setFont(F(10.0f)); g.drawText("How different all classes sound", 636, 686, 160, 14, juce::Justification::centredLeft);
            help("Tip: a cargo hub at night is full of freighters.");
            break;
        }
        case 1:
            caps(g, "Source", Rectangle<float>(38.0f, 450.0f, 100.0f, 12.0f)); g.setColour(col::dim); g.setFont(F(10.5f)); g.drawText("blend", 194, 469, 36, 20, juce::Justification::centredLeft);
            caps(g, "Colour", Rectangle<float>(38.0f, 524.0f, 120.0f, 12.0f)); caps(g, "Movement", Rectangle<float>(290.0f, 524.0f, 120.0f, 12.0f)); caps(g, "Shape", Rectangle<float>(542.0f, 524.0f, 120.0f, 12.0f));
            vline(274, 540, 84); vline(526, 540, 84);
            if (!e.showMore) help("These shape the base sound: the drone bed and every class set to \"Global\". Classes with their own voice are set on the Aircraft tab.");
            break;
        case 2:
            caps(g, "Tuning", Rectangle<float>(38.0f, 450.0f, 100.0f, 12.0f)); caps(g, "Root", Rectangle<float>(38.0f, 660.0f, 100.0f, 12.0f)); caps(g, "Space", Rectangle<float>(296.0f, 450.0f, 100.0f, 12.0f));
            g.setColour(col::dim); g.setFont(F(11.0f)); g.drawFittedText("North: equal temperament (0 %) to pure ratios (100 %). Listen to the beating fade.", 38, 612, 220, 32, juce::Justification::topLeft, 2);
            vline(272, 444, 280);
            help("Tuning decides how the notes relate; Space puts them in a room.");
            break;
        case 3:
        {
            caps(g, "Model", Rectangle<float>(38.0f, 450.0f, 100.0f, 12.0f));
            auto pr = Rectangle<float>(350.0f, 446.0f, 440.0f, 120.0f); g.setColour(Colour(0xff111315)); g.fillRoundedRectangle(pr, 6.0f);
            const double fc = e.proc.P(ID::cutoff), q = e.proc.P(ID::reso), lc = e.proc.P(ID::lowcut), mix = e.proc.P(ID::fmix); const int ft = (int) e.proc.P(ID::ftype);
            auto xOf = [&](double f) { return pr.getX() + 8.0f + (float) ((std::log(f) - std::log(30.0)) / (std::log(20000.0) - std::log(30.0))) * (pr.getWidth() - 16.0f); };
            auto cpow = [](std::complex<double> z, int n) { std::complex<double> r(1.0, 0.0); for (int i = 0; i < n; ++i) r *= z; return r; };
            juce::Path curve; bool first = true;
            for (int i = 0; i <= 200; ++i)
            {
                const double f = 30.0 * std::pow(20000.0 / 30.0, i / 200.0), x = f / fc, hp = lc / f;
                double lp;
                if (ft == 1) lp = std::abs(1.0 / (cpow(std::complex<double>(1.0, x), 4) + 3.9 * q));          // ladder, 24 dB: resonance feeds back and the bass sags as it rises
                else if (ft == 2) lp = std::abs(1.0 / (cpow(std::complex<double>(1.0, x), 2) + 1.6 * q));     // ladder 12 dB
                else lp = 1.0 / std::sqrt(std::pow(1.0 - x * x, 2.0) + std::pow(x * (1.4 - 1.3 * q), 2.0));  // clean 12 dB
                const double h = 1.0 / std::sqrt(1.0 + hp * hp * hp * hp), mag = mix * lp + (1.0 - mix);
                const float db = (float) (20.0 * std::log10(std::max(1e-4, mag * h)));
                const float y = pr.getY() + 34.0f - db * 1.6f; const float yc = std::min(pr.getBottom() - 16.0f, std::max(pr.getY() + 6.0f, y));
                if (first) { curve.startNewSubPath(xOf(f), yc); first = false; } else curve.lineTo(xOf(f), yc);
            }
            { auto fillP = curve; fillP.lineTo(xOf(20000.0), pr.getBottom() - 16.0f); fillP.lineTo(xOf(30.0), pr.getBottom() - 16.0f); fillP.closeSubPath(); g.setColour(col::accent.withAlpha(0.08f)); g.fillPath(fillP); }
            g.setColour(col::dimmer.withAlpha(0.5f)); g.drawLine(xOf(fc), pr.getY() + 8.0f, xOf(fc), pr.getBottom() - 16.0f, 1.0f);
            g.setColour(col::accent); g.strokePath(curve, juce::PathStrokeType(1.6f));
            g.setColour(Colour(0xff5d646b)); g.setFont(F(9.5f)); for (double f : { 100.0, 1000.0, 10000.0 }) g.drawText(f >= 1000 ? juce::String((int) (f / 1000)) + "k" : juce::String((int) f), (int) xOf(f) - 12, (int) pr.getBottom() - 14, 24, 12, juce::Justification::centred);
            vline(456, 596, 90);
            help("Cutoff and Drive shape the whole instrument. Low cut keeps the sub out of your mix.");
            break;
        }
        case 4:
            for (int l = 0; l < 2; ++l)
            {
                Rectangle<int> r(38 + l * 190, 446, 178, 200); glassPanel(g, r.toFloat(), 6.0f, col::panelEdge, 0.0f);
                caps(g, l ? "LFO 2" : "LFO 1", Rectangle<float>((float) r.getX() + 12.0f, 454.0f, 80.0f, 14.0f), col::accent2);
                if (e.proc.P(l ? ID::l2sync : ID::l1sync) > 0.5f && !e.proc.hostHasTempo()) { g.setColour(col::warn); g.setFont(F(9.5f)); g.drawText("no tempo: 120", r.getX() + 90, 454, 78, 14, juce::Justification::right); }
            }
            vline(420, 444, 280);
            caps(g, "Matrix", Rectangle<float>(440.0f, 450.0f, 120.0f, 12.0f));
            for (int i = 0; i < e.connRows; ++i)
                if (e.matrixFlashDst > 0 && juce::Time::getMillisecondCounterHiRes() * 0.001 < e.matrixFlashUntil && (int) e.proc.P(ID::modSlotId(e.rowSlot[i], 1)) == e.matrixFlashDst)
                { g.setColour(col::accent2.withAlpha(0.16f)); g.fillRoundedRectangle(434.0f, 463.0f + (float) i * 31.0f, 376.0f, 31.0f, 5.0f); }
            g.setColour(col::dim); g.setFont(F(13.0f)); for (int i = 0; i < e.connRows; ++i) g.drawText(juce::String::fromUTF8("\xe2\x86\x92"), 562, 466 + i * 31, 18, 25, juce::Justification::centred);
            if (e.connRows == 0) { g.setColour(Colour(0xff6a7279)); g.setFont(F(10.5f)); g.drawText("Nothing is moving yet.", 440, 500, 340, 16, juce::Justification::centredLeft); }
            caps(g, "Sky as a source", Rectangle<float>(38.0f, 666.0f, 200.0f, 12.0f));
            g.setColour(col::dim); g.setFont(F(11.0f)); g.drawFittedText(juce::String::fromUTF8("Aircraft count \xc2\xb7 altitude \xc2\xb7 proximity \xc2\xb7 speed, for the whole sky or one class: let the traffic move any control."), 38, 682, 380, 30, juce::Justification::topLeft, 2, 1.0f);
            help(juce::String::fromUTF8("Click \xe2\x86\x92 to show the control. Right-click a knob to connect it; drag its orange ring to change the amount. Up to eight connections."));
            break;
        case 6:
        {
            for (int c = 0; c < kNumClasses; ++c)
            {
                const int x0 = 34 + c * 84; if (c) vline(x0 - 2, 444, 236);
                g.setColour(catColour(c)); classMarker(g, c, (float) x0 + 9.0f, 451.0f, 3.5f);
                g.setColour(col::text); g.setFont(F(10.5f, juce::Font::bold)); g.drawFittedText(className(c), x0 + 16, 444, 64, 14, juce::Justification::centredLeft, 1, 0.8f);
                const float gdb = e.proc.P(ID::classId(c, 1));
                g.setColour(col::dim); g.setFont(F(9.5f)); g.drawText("gain " + juce::String(gdb, std::fabs(gdb) < 10.0f ? 1 : 0) + " dB", x0 + 16, 457, 64, 12, juce::Justification::centredLeft);
            }
            const bool on = (bool) e.proc.getProp("classOuts", false); const int act = e.proc.classOutputsActive();
            g.setColour(col::dim); g.setFont(F(10.5f));
            g.drawFittedText(on ? (act > 0 ? juce::String(act) + " of 9 class outputs are switched on in the host: those classes play only there, dry (no Aeola reverb). The rest stay in the main output."
                                           : juce::String("No class output is switched on in the host yet. In Live: a new audio track, Audio From = this track, then pick a class (Light / GA, Heavy ...)."))
                                  : juce::String("All classes play in the main output. Separate outputs sends each class to its own stereo output, so you can mix them in your DAW."),
                             250, 690, 548, 32, juce::Justification::topLeft, 2, 1.0f);
            break;
        }
        default:
            caps(g, "Key and scale", Rectangle<float>(38.0f, 450.0f, 140.0f, 12.0f)); caps(g, "Drone bed", Rectangle<float>(322.0f, 450.0f, 140.0f, 12.0f));
            caps(g, "Life of the sky", Rectangle<float>(510.0f, 450.0f, 160.0f, 12.0f)); caps(g, "Data", Rectangle<float>(322.0f, 654.0f, 100.0f, 12.0f));
            g.setColour(col::dim); g.setFont(F(11.0f)); g.drawFittedText("Consonance: how strictly clashing notes are avoided. An aircraft whose note would clash is moved to a nearby note.", 38, 624, 250, 32, juce::Justification::topLeft, 2);
            vline(300, 444, 280);
            if ((int) e.proc.P(ID::source) == 4) { g.setColour(col::dim); g.setFont(F(9.5f)); g.drawText("level", 570, 660, 60, 12, juce::Justification::centredLeft); }
            help("Freeze saves the last minutes of real traffic in the project, so bounces are identical. Drag MIDI exports what played.");
            break;
    }
    credit();
}

const int AeolaDroneEditor::Content::kTry[6] = { 1, 2, 18, 16, 10, 8 };
juce::Rectangle<int> AeolaDroneEditor::Content::tryRect(int i) const { return { 30, 442 + i * 30, 176, 29 }; }      // Aircraft tab: class rows (left of S / M)
void AeolaDroneEditor::Content::mouseMove(const juce::MouseEvent& e)
{
    int h = -1; if (!ed.simple && ed.drawer == 0) for (int i = 0; i < kNumClasses; ++i) if (Rectangle<int>(30, 442 + i * 30, 176, 29).contains(e.getPosition())) h = i;
    if (h != hoverRow) { hoverRow = h; setMouseCursor(h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); repaint(20, 440, 280, 300); }
}
void AeolaDroneEditor::Content::mouseExit(const juce::MouseEvent&) { if (hoverRow != -1) { hoverRow = -1; repaint(20, 440, 280, 300); } }
void AeolaDroneEditor::Content::mouseDown(const juce::MouseEvent& e)
{
    if (!ed.simple && ed.drawer == 4)
    {   // Matrix: the arrow between source and target shows the control
        for (int i = 0; i < ed.connRows; ++i)
            if (Rectangle<int>(558, 466 + i * 31, 26, 25).contains(e.getPosition())) { ed.goToTarget((int) ed.proc.P(ID::modSlotId(ed.rowSlot[i], 1))); return; }
        return;
    }
    if (ed.simple || ed.drawer != 0) return;
    for (int i = 0; i < kNumClasses; ++i) if (Rectangle<int>(30, 442 + i * 30, 176, 29).contains(e.getPosition())) { ed.selectClass(i); return; }
}
