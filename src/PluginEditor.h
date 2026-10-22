#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// "Brushed aluminium" hardware panel. Everything is laid out on a fixed 960x540
// panel that the editor scales as a whole when the window is resized.
namespace tuman_ui {

struct Ink {
    static juce::Colour panel()  { return juce::Colour(0xffd8d5cd); }
    static juce::Colour ink()    { return juce::Colour(0xff17181a); }
    static juce::Colour rule()   { return juce::Colour(0xffb9b5ac); }
    static juce::Colour cap()    { return juce::Colour(0xff1b1c1e); }
    static juce::Colour capIn()  { return juce::Colour(0xff232426); }
    static juce::Colour paper()  { return juce::Colour(0xfff2efe8); }
    static juce::Colour muted()  { return juce::Colour(0xff8e8a82); }
    static juce::Colour label()  { return juce::Colour(0xff4a4b4e); }
    static juce::Colour group(const juce::String& g) {
        if (g == "reverb")  return juce::Colour(0xff2f6db5);
        if (g == "delay")   return juce::Colour(0xff2e8a4e);
        if (g == "special") return juce::Colour(0xffc9a227);
        return juce::Colour(0xffc2412f);
    }
};

struct Type {
    static juce::Font mono(float pt, bool bold = false, float tracking = 0.0f);
    static juce::Font grotesk(float pt, float tracking = 0.0f);
};

class PanelLookAndFeel : public juce::LookAndFeel_V4 {
public:
    PanelLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    juce::Font getPopupMenuFont() override { return Type::mono(13.0f); }
};

// 15-position rotary selector with printed labels around it
class Selector : public juce::Component {
public:
    explicit Selector(juce::AudioProcessorValueTreeState& s);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    int alg = 0;
private:
    juce::AudioProcessorValueTreeState& state;
    juce::Point<float> centre { 230.0f, 285.0f };
    int hover = -1; bool dragging = false; float wheelAcc = 0.0f;
    int labelAt(juce::Point<float>) const;
    juce::Rectangle<float> labelBox(int i) const;
    void choose(int i);
};

class ToggleSwitch : public juce::ToggleButton {
public:
    using juce::ToggleButton::ToggleButton;
    void paintButton(juce::Graphics&, bool over, bool down) override;
};

// "IN -3 dB" printed value; drag vertically to change, double-click resets
class DbReadout : public juce::Slider {
public:
    explicit DbReadout(juce::String c) : caption(std::move(c)) {
        setSliderStyle(juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setMouseDragSensitivity(220);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    }
    void paint(juce::Graphics&) override;
private:
    juce::String caption;
};

class PresetButton : public juce::Component {
public:
    explicit PresetButton(TumanProcessor& p) : proc(p) { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
private:
    TumanProcessor& proc;
};

class Panel : public juce::Component {
public:
    explicit Panel(TumanProcessor&);
    ~Panel() override;
    void paint(juce::Graphics&) override;
    void refresh(int alg, bool clip);
    static constexpr int W = 960, H = 540;
private:
    TumanProcessor& proc;
    PanelLookAndFeel lnf;
    juce::Image surface;
    Selector selector;
    juce::Slider mix, p[4];
    DbReadout inDb { "IN" }, outDb { "OUT" };
    ToggleSwitch longSw { "LONG" }, gateSw { "GATE" };
    PresetButton preset;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> mixAtt, pAtt[4], inAtt, outAtt;
    std::unique_ptr<BA> longAtt, gateAtt;
    int shown = -1; bool led = false;
    juce::String knobLabel[4];
    static juce::Image makeBrushedSurface(int scale);
    void setupKnob(juce::Slider&, bool big);
};

} // namespace tuman_ui

class TumanEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit TumanEditor(TumanProcessor&);
    ~TumanEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    TumanProcessor& proc;
    tuman_ui::Panel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TumanEditor)
};
