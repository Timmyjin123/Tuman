#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace tuman_ui {
struct Palette {
    static juce::Colour bg()    { return juce::Colour(0xff0f1114); }
    static juce::Colour panel() { return juce::Colour(0xff171a1f); }
    static juce::Colour line()  { return juce::Colour(0xff2a2e36); }
    static juce::Colour text()  { return juce::Colour(0xffe6e2da); }
    static juce::Colour dim()   { return juce::Colour(0xff7b818c); }
    static juce::Colour group(const juce::String& g) {
        if (g == "reverb")  return juce::Colour(0xff6fa8dc);
        if (g == "delay")   return juce::Colour(0xff7cc489);
        if (g == "special") return juce::Colour(0xffe2c65a);
        return juce::Colour(0xffe0675a);
    }
};

class LookAndFeel : public juce::LookAndFeel_V4 {
public:
    LookAndFeel();
    juce::Colour accent { Palette::group("reverb") };
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
};

class Knob : public juce::Component {
public:
    Knob(juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& label, bool big);
    void setLabel(const juce::String& l) { label = l; repaint(); }
    void resized() override;
    void paint(juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String label; bool big;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class AlgList : public juce::Component {
public:
    explicit AlgList(juce::AudioProcessorValueTreeState& s) : state(s) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    int selected = 0, active = 0, hover = -1;
private:
    juce::AudioProcessorValueTreeState& state;
    int rowAt(float y) const;
    float rowH() const { return (float)getHeight() / (float)tuman::kNumAlgs; }
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
    void refreshForAlgorithm(int alg);
    TumanProcessor& proc;
    tuman_ui::LookAndFeel lnf;
    tuman_ui::AlgList algList;
    std::unique_ptr<tuman_ui::Knob> mix, p[4], inK, outK;
    juce::ToggleButton longBtn { "Long" }, gateBtn { "Gate" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> longAtt, gateAtt;
    juce::ComboBox presets;
    int shownAlg = -1;
    float inMeter = 0, outMeter = 0; bool led = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TumanEditor)
};
