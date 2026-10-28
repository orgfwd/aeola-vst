#include <array>
// Aeola — editor. Copyright (C) 2026 Alex Skepp. GPL-3.0-or-later.
#pragma once
#include <set>
#include <deque>
#include <juce_gui_basics/juce_gui_basics.h>
#include <deque>
#include <map>
#include "PluginProcessor.h"
#include "AppInfo.h"

namespace aeola
{
namespace col
{
    static const juce::Colour bg0 { 0xff141618 }, bg1 { 0xff1b1d20 }, panel { 0xff1b1d20 }, panelEdge { 0xff2b2e33 }, dimmer { 0xff6a7279 };
    static const juce::Colour text { 0xffdde1e4 }, dim { 0xff8a9299 }, accent { 0xff6ed6a0 }, accent2 { 0xffe0a854 }, warm { 0xfff0be6e };
    static const juce::Colour good { 0xff6ed6a0 }, warn { 0xffe0a854 }, bad { 0xffe4745e };
}

class AeolaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AeolaLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool hl, bool down) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool hl, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool hl, bool down) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
};

class Knob : public juce::Component
{
public:
    Knob(juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& label, float dialSize = 50.0f);
    void resized() override;
    juce::Slider slider; juce::Label name, vlab; juce::String paramId; bool advanced = false;
    void setValueAbove(bool on);                    // the value as text above the dial, owned by the knob so it can never go stale
    void refreshValueAbove();
    int modDst = 0; AeolaDroneProcessor* proc = nullptr;
    // 1.13 modulation on the knob: rings per connection, a badge, the menu, dragging the ring sets the amount
    int nConn = 0, activeSlot = -1; double flashUntil = 0; juce::String baseTip; std::function<void(int)> onShowMatrix;
    void refreshMod(); juce::Point<float> dialCentre() const; float dialRadius() const; std::unique_ptr<juce::Component> ring;
    void showValue(); void showName();
    void mouseEnter(const juce::MouseEvent&) override; void mouseExit(const juce::MouseEvent&) override; void mouseUp(const juce::MouseEvent&) override; void mouseDown(const juce::MouseEvent&) override;          // set when a modulation target acts on this knob (draws the ring)
    void paintOverChildren(juce::Graphics&) override;
    void typeValue();
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att; float dial; juce::String labelText; bool over = false;
public: bool above = false;
private:
};

// 1.12 mixer meter: one aircraft class, peak with a short fall
float landAreaNear(double lat, double lon, double reachDeg, bool coarse);      // tests: land near a place (0 = open sea)

class MixMeter : public juce::Component
{
public:
    MixMeter(AeolaDroneProcessor& p, int c) : proc(p), cls(c) { setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics&) override;
private:
    AeolaDroneProcessor& proc; int cls; float shown = 0.0f;
};

class SkyView : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit SkyView(AeolaDroneProcessor&);
    std::function<void()> onPlaceClick;
    int focusClass = -1;                           // Advanced, Aircraft tab: the selected class stands out, the others dim
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    double earthMs = 0, earthCacheMs = 0;          // paint cost of the Earth view (smoothed) and of the last ground-cache rebuild
private:
    juce::Image earthCache; juce::String earthKey; juce::Point<float> lastDrag;
    static constexpr uint32_t kGroundId = 0x80000000u;
    struct Ac { UiAircraft a; double lat = 0, lon = 0; float alpha = 0; bool present = false; int slotMap = 0, slotProf = 0; std::deque<juce::Point<double>> trail; double lastTrail = 0; };
    struct Hit { uint32_t id; juce::Point<float> p; float r; };
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> segRect(int i) const, chipRect(int c) const, plotRect() const, zoomRect(int i) const, landRect() const;
    void paintSound(juce::Graphics&, const UiSnapshot&, bool have, float dt, double now, uint32_t sel);
    void paintMap(juce::Graphics&, const UiSnapshot&, bool have, float dt, double now, uint32_t sel);
    void paintEarth(juce::Graphics&, const UiSnapshot&, bool have, float dt, double now, uint32_t sel);
    void paintCard(juce::Graphics&, const UiSnapshot&, uint32_t sel);
    AeolaDroneProcessor& proc;
    int tab = 0;                                   // 0 Sound, 1 Sky map, 2 Earth (stored in the project as "skyTab")
    std::map<uint32_t, Ac> acs; std::vector<Hit> hits; float occ[128] = {};
    double radiusShown = 40.0, lastT = 0; uint32_t hoverId = 0, pinnedId = 0; int hoverChip = -1; bool hoverPlace = false; juce::Rectangle<float> placeHit;
    std::map<uint32_t, float> levelOf; float uiK = 1.0f; juce::Path landCache, landFill, lakeFill; juce::String landCacheKey;
    // 1.12 map: traffic beyond the radius, crossings of the circle, zoom / pan independent of the radius
    std::map<uint32_t, Ac> outs; std::vector<Hit> outHits; uint32_t hoverOut = 0;
    struct Cross { double lat, lon, t; bool in; }; std::deque<Cross> crosses; std::set<uint32_t> lastIn, lastOut;
    float mapS = 1.0f; double mapZ = 54.0; juce::Point<float> mapC; int dragMode = 0; bool nearEdge = false;
};

class SpectrumView : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumView(AeolaDroneProcessor& p) : proc(p) { startTimerHz(12); }
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    AeolaDroneProcessor& proc;
};

// Live trace of one modulation source (LFO, random walk ...), last ~4 seconds.
class ScopeView : public juce::Component, private juce::Timer
{
public:
    ScopeView(AeolaDroneProcessor& p, int srcIndex) : proc(p), src(srcIndex) { startTimerHz(30); hist.assign(120, 0.0f); }
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override { hist.erase(hist.begin()); hist.push_back(proc.modSrcShown(src)); repaint(); }
    AeolaDroneProcessor& proc; int src; std::vector<float> hist;
};

class MidiDragHandle : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MidiDragHandle(AeolaDroneProcessor& p) : proc(p) {}
    void paint(juce::Graphics&) override;
    void mouseDrag(const juce::MouseEvent&) override;
private:
    AeolaDroneProcessor& proc; bool dragging = false;
};

class TourOverlay : public juce::Component
{
public:
    std::function<void()> onDone; std::function<juce::Rectangle<float>(int)> target;
    juce::Rectangle<float> nextR, skipR;
    TourOverlay();
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    int step = 0;
};

class AeolaDroneEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    Diagnostics collectDiagnostics(); void showDiagnostics();
    juce::Rectangle<float> tourTarget(int step); juce::String valueAbove(const char* id) { auto* k = kmap[id]; return k && k->isVisible() && k->above ? k->vlab.getText() : juce::String("<none>"); } void tourStep(int s) { if (tour) tour->step = s; }
    explicit AeolaDroneEditor(AeolaDroneProcessor&);
    ~AeolaDroneEditor() override;
    void resized() override;
    void paint(juce::Graphics&) override {}
    void showModPage(bool on) { setDrawer(on ? 4 : 5); }
    void showAircraftPage(bool on) { setDrawer(on ? 0 : 1); }
    void showMixPage() { setDrawer(6); }
    void setDrawer(int d) { drawer = std::max(0, std::min(6, d)); proc.setProp("drawer", drawer); addSlot = -1; layoutAll(); }
    void selectClass(int c) { selClass = c; proc.setProp("selClass", c); layoutAll(); }
    void setSimple(bool on) { simple = on; proc.simpleUi = on; layoutAll(); }

    static constexpr int W = 1040, H = 760;
private:
    struct Content : juce::Component { AeolaDroneEditor& ed; int hoverTry = -1, hoverRow = -1; explicit Content(AeolaDroneEditor& e) : ed(e) {} void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override; void mouseMove(const juce::MouseEvent&) override; void mouseExit(const juce::MouseEvent&) override; juce::Rectangle<int> tryRect(int i) const; static const int kTry[6]; };
    void timerCallback() override;
    Knob* addKnob(const char* id, const char* label, bool advanced);
    juce::ComboBox* addCombo(const char* id);
    juce::ToggleButton* addToggle(const char* id, const char* label);
    void layoutAll();
    void rebuildPresetMenu();
    void showSettings();
    void applyScale(float s);

    AeolaDroneProcessor& proc;
    AeolaLookAndFeel laf;
    Content content { *this };
    juce::TooltipWindow tooltip { this, 500 };
    SkyView sky { proc };
    SpectrumView spectrum { proc };
    ScopeView scope1 { proc, SrcLfo1 }, scope2 { proc, SrcLfo2 };
    juce::TextButton skyTab { "Sky" }, modTab { "Modulation" }, aircraftTab { "Classes" }, placeBtn { "Place..." };
    int tuneH = 140, spaceY = 204, spaceH = 88, filterY = 300, filterH = 186;
    int updateTick = 0, mixTick = 0, modTick = 0;
    std::array<float, 6> filterSig = { -1, -1, -1, -1, -1, -1 };
public: int filterRepaints = 0;
private:
    int drawer = 0, selClass = 5, addSlot = -1, connRows = 0, drawerTick = 0; bool showMore = false;
    juce::TextButton dTab[7], soloBtn, moreBtn, addConnBtn, ftBtn[3], classOutBtn; juce::Slider morphBar, seedBar;
    juce::Slider mixFader[9]; std::unique_ptr<MixMeter> meters[9]; std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> mixAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> seedAtt;
    juce::Slider classBar[9][4]; juce::Slider contrastBar, stretchBar, simBar;       // Aircraft page: per-class level / octave / attack / chop, and the global ones
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> classAtt;
    void showPlaceDialog(); void showPlaceMenu(); void applyPlaceQuery(const juce::String& q);
    juce::Slider modAmt[kModSlots]; juce::TextButton modDel[kModSlots]; int rowSlot[kModSlots] = {};
    int matrixFlashDst = 0; double matrixFlashUntil = 0;
public:
    void showMatrixFor(int dst);                  // a knob's "Show in the Matrix": open the Modulation tab and light up its rows
    void goToTarget(int dst);                     // a Matrix row's arrow: open the tab with that control and flash it
private:
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> modAmtAtt;
    MidiDragHandle midiHandle { proc };
    std::unique_ptr<TourOverlay> tour;

    std::vector<std::unique_ptr<Knob>> knobs; std::map<juce::String, Knob*> kmap;
    std::vector<std::unique_ptr<juce::ComboBox>> combos; std::map<juce::String, juce::ComboBox*> cmap;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles; std::map<juce::String, juce::ToggleButton*> tmap;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggleAtt;

    juce::ComboBox presetBox; juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "Save" }, diceBtn { "Randomise" }, setA { "A" }, setB { "B" },
        modeBtn { "Advanced" }, gearBtn { "Settings" }, fixBtn { "Why?" }, freezeBtn { "Freeze" }, helpBtn { "?" };
    juce::Slider morph { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox }, outSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outAtt;
    juce::Label status, hint;
    bool simple = true; float uiScale = 1.0f;
    int lastPresetShown = -2;
    friend struct Content;
};
}

using AeolaDroneEditor = aeola::AeolaDroneEditor;
