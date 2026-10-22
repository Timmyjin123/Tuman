#include "PluginEditor.h"

using namespace tuman_ui;

// ---------------------------------------------------------------- look & feel
LookAndFeel::LookAndFeel() {
    setColour(juce::ComboBox::backgroundColourId, Palette::panel());
    setColour(juce::ComboBox::textColourId, Palette::text());
    setColour(juce::ComboBox::outlineColourId, Palette::line());
    setColour(juce::PopupMenu::backgroundColourId, Palette::panel());
    setColour(juce::PopupMenu::textColourId, Palette::text());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Palette::line());
    setColour(juce::ToggleButton::textColourId, Palette::text());
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) {
    auto b = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h).reduced(4.0f);
    float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
    auto c = b.getCentre();
    float ang = a0 + pos * (a1 - a0);
    float thick = juce::jmax(2.5f, r * 0.12f);
    juce::Path track; track.addCentredArc(c.x, c.y, r - thick, r - thick, 0, a0, a1, true);
    g.setColour(Palette::line());
    g.strokePath(track, juce::PathStrokeType(thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val; val.addCentredArc(c.x, c.y, r - thick, r - thick, 0, a0, ang, true);
    g.setColour(accent);
    g.strokePath(val, juce::PathStrokeType(thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    float ir = r - thick * 2.6f;
    g.setColour(Palette::panel().brighter(0.08f));
    g.fillEllipse(c.x - ir, c.y - ir, ir * 2, ir * 2);
    juce::Point<float> tip(c.x + std::sin(ang) * ir * 0.8f, c.y - std::cos(ang) * ir * 0.8f);
    g.setColour(Palette::text());
    g.drawLine(c.x + std::sin(ang) * ir * 0.25f, c.y - std::cos(ang) * ir * 0.25f, tip.x, tip.y, juce::jmax(2.0f, r * 0.06f));
}

void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool hl, bool) {
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    bool on = b.getToggleState();
    g.setColour(on ? accent.withAlpha(b.isEnabled() ? 0.9f : 0.3f) : Palette::panel().brighter(hl ? 0.1f : 0.0f));
    g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
    g.setColour(Palette::line());
    g.drawRoundedRectangle(r, r.getHeight() * 0.5f, 1.0f);
    g.setColour(on ? Palette::bg() : Palette::text().withAlpha(b.isEnabled() ? 0.9f : 0.35f));
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText(b.getButtonText(), r, juce::Justification::centred);
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) {
    auto r = juce::Rectangle<float>(0, 0, (float)w, (float)h).reduced(0.5f);
    g.setColour(Palette::panel()); g.fillRoundedRectangle(r, 6.0f);
    g.setColour(Palette::line()); g.drawRoundedRectangle(r, 6.0f, 1.0f);
    juce::Path arrow; float cx = (float)w - 14.0f, cy = (float)h * 0.5f;
    arrow.addTriangle(cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
    g.setColour(Palette::dim()); g.fillPath(arrow);
}
juce::Font LookAndFeel::getComboBoxFont(juce::ComboBox&) { return juce::Font(juce::FontOptions(14.0f)); }

// ---------------------------------------------------------------- knob
Knob::Knob(juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& l, bool b) : label(l), big(b) {
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setDoubleClickReturnValue(true, 0.5);
    addAndMakeVisible(slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(s, id, slider);
}
void Knob::resized() {
    auto r = getLocalBounds();
    r.removeFromBottom(big ? 22 : 18);
    slider.setBounds(r);
}
void Knob::paint(juce::Graphics& g) {
    auto r = getLocalBounds();
    auto t = r.removeFromBottom(big ? 22 : 18);
    g.setColour(Palette::text().withAlpha(0.85f));
    g.setFont(juce::Font(juce::FontOptions(big ? 15.0f : 12.5f, juce::Font::bold)));
    g.drawText(label.toUpperCase(), t, juce::Justification::centred);
}

// ---------------------------------------------------------------- algorithm list
int AlgList::rowAt(float y) const { return juce::jlimit(0, tuman::kNumAlgs - 1, (int)(y / rowH())); }

void AlgList::paint(juce::Graphics& g) {
    for (int i = 0; i < tuman::kNumAlgs; ++i) {
        const auto& a = algInfo(i);
        auto r = juce::Rectangle<float>(0, rowH() * i, (float)getWidth(), rowH()).reduced(2.0f, 1.5f);
        auto col = Palette::group(a.group);
        bool sel = (i == selected);
        if (sel) { g.setColour(col.withAlpha(0.18f)); g.fillRoundedRectangle(r, 5.0f); }
        else if (i == hover) { g.setColour(Palette::line().withAlpha(0.6f)); g.fillRoundedRectangle(r, 5.0f); }
        // LED
        float d = 7.0f;
        auto led = juce::Rectangle<float>(r.getX() + 8.0f, r.getCentreY() - d * 0.5f, d, d);
        g.setColour(sel ? col : col.withAlpha(0.25f));
        g.fillEllipse(led);
        if (i == active && sel) { g.setColour(col.withAlpha(0.35f)); g.fillEllipse(led.expanded(3.0f)); }
        g.setColour(sel ? Palette::text() : Palette::dim());
        g.setFont(juce::Font(juce::FontOptions(13.5f, sel ? juce::Font::bold : juce::Font::plain)));
        g.drawText(a.name, r.withTrimmedLeft(24.0f), juce::Justification::centredLeft);
    }
}
void AlgList::mouseDown(const juce::MouseEvent& e) {
    int row = rowAt((float)e.y);
    if (auto* p = state.getParameter("alg")) p->setValueNotifyingHost(p->convertTo0to1((float)row));
}
void AlgList::mouseMove(const juce::MouseEvent& e) { int h = rowAt((float)e.y); if (h != hover) { hover = h; repaint(); } }

// ---------------------------------------------------------------- editor
TumanEditor::TumanEditor(TumanProcessor& owner) : AudioProcessorEditor(&owner), proc(owner), algList(owner.apvts) {
    setLookAndFeel(&lnf);
    addAndMakeVisible(algList);
    mix = std::make_unique<Knob>(owner.apvts, "mix", "Mix", true);
    addAndMakeVisible(*mix);
    for (int i = 0; i < 4; ++i) {
        p[i] = std::make_unique<Knob>(owner.apvts, "p" + juce::String(i + 1), "P" + juce::String(i + 1), false);
        addAndMakeVisible(*p[i]);
    }
    inK = std::make_unique<Knob>(owner.apvts, "in", "In", false);
    outK = std::make_unique<Knob>(owner.apvts, "out", "Out", false);
    inK->slider.setDoubleClickReturnValue(true, 0.0); outK->slider.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(*inK); addAndMakeVisible(*outK);
    addAndMakeVisible(longBtn); addAndMakeVisible(gateBtn);
    longAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.apvts, "long", longBtn);
    gateAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.apvts, "gate", gateBtn);
    longBtn.setTooltip("Hidden device mode: the algorithm runs at half rate - twice as long, darker, with aliasing");
    gateBtn.setTooltip("Noise gate (Pump and Wavefolder)");

    presets.setTextWhenNothingSelected("Presets");
    for (int i = 0; i < owner.getNumPrograms(); ++i) presets.addItem(owner.getProgramName(i), i + 1);
    presets.onChange = [this] { int i = presets.getSelectedId() - 1; if (i >= 0) proc.setCurrentProgram(i); };
    addAndMakeVisible(presets);

    setSize(780, 440);
    setResizable(true, true);
    setResizeLimits(640, 380, 1400, 800);
    startTimerHz(30);
}

TumanEditor::~TumanEditor() { setLookAndFeel(nullptr); }

void TumanEditor::refreshForAlgorithm(int alg) {
    const auto& a = algInfo(alg);
    lnf.accent = Palette::group(a.group);
    for (int i = 0; i < 4; ++i) p[i]->setLabel(a.p[i]);
    bool longOk = alg <= 7 && alg != 5;
    bool gateOk = alg == 11 || alg == 13;
    longBtn.setEnabled(longOk); gateBtn.setEnabled(gateOk);
    algList.selected = alg;
    repaint();
}

void TumanEditor::timerCallback() {
    int alg = (int)proc.apvts.getRawParameterValue("alg")->load();
    if (alg != shownAlg) { shownAlg = alg; refreshForAlgorithm(alg); }
    int act = proc.activeAlg.load();
    if (act != algList.active) { algList.active = act; algList.repaint(); }
    inMeter = proc.inLevel.load(); outMeter = proc.outLevel.load(); led = proc.clipLed.load();
    repaint(getLocalBounds().removeFromTop(56));
}

void TumanEditor::paint(juce::Graphics& g) {
    g.fillAll(Palette::bg());
    auto r = getLocalBounds();
    auto top = r.removeFromTop(56).reduced(18, 0);
    g.setColour(Palette::text());
    g.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
    g.drawText("TUMAN", top.removeFromLeft(130), juce::Justification::centredLeft);
    g.setColour(Palette::dim());
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(juce::String(algInfo(shownAlg).name).toUpperCase(), top.removeFromLeft(220), juce::Justification::centredLeft);

    // meters + clip LED, right of the header
    auto meters = juce::Rectangle<float>((float)getWidth() - 140.0f, 18.0f, 70.0f, 20.0f);
    auto bar = [&](juce::Rectangle<float> b, float v, const char* t) {
        g.setColour(Palette::panel()); g.fillRoundedRectangle(b, 3.0f);
        float db = juce::Decibels::gainToDecibels(v, -60.0f);
        float f = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setColour(lnf.accent.withAlpha(0.85f)); g.fillRoundedRectangle(b.withWidth(b.getWidth() * f), 3.0f);
        g.setColour(Palette::dim()); g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.drawText(t, b.translated(-18.0f, 0).withWidth(16.0f), juce::Justification::centredRight);
    };
    bar(meters.withHeight(8.0f), inMeter, "IN");
    bar(meters.withY(30.0f).withHeight(8.0f), outMeter, "OUT");
    auto ledR = juce::Rectangle<float>((float)getWidth() - 56.0f, 22.0f, 12.0f, 12.0f);
    g.setColour(led ? juce::Colour(0xffff4a3d) : juce::Colour(0xff3a1f1d));
    g.fillEllipse(ledR);

    // panels
    g.setColour(Palette::panel());
    g.fillRoundedRectangle(algList.getBounds().toFloat().expanded(6.0f), 10.0f);
    auto knobArea = juce::Rectangle<int>(algList.getRight() + 18, 66, getWidth() - algList.getRight() - 36, getHeight() - 84);
    g.fillRoundedRectangle(knobArea.toFloat(), 10.0f);
    g.setColour(lnf.accent.withAlpha(0.6f));
    g.fillRoundedRectangle(knobArea.toFloat().removeFromTop(3.0f).reduced(20.0f, 0), 1.5f);
}

void TumanEditor::resized() {
    auto r = getLocalBounds();
    auto top = r.removeFromTop(56);
    presets.setBounds(top.getRight() - 360, 14, 190, 28);
    r.removeFromTop(10);
    r.reduce(18, 18);
    algList.setBounds(r.removeFromLeft(210).reduced(4));
    r.removeFromLeft(18);
    auto area = r.reduced(16);
    auto bottom = area.removeFromBottom(90);
    auto upper = area;
    int mw = juce::jmin(upper.getHeight(), 190);
    mix->setBounds(upper.removeFromLeft(mw).withSizeKeepingCentre(mw, mw));
    upper.removeFromLeft(10);
    int kw = upper.getWidth() / 2, kh = upper.getHeight() / 2;
    for (int i = 0; i < 4; ++i) {
        auto cell = juce::Rectangle<int>(upper.getX() + (i % 2) * kw, upper.getY() + (i / 2) * kh, kw, kh);
        int s = juce::jmin(cell.getWidth(), cell.getHeight()) - 6;
        p[i]->setBounds(cell.withSizeKeepingCentre(s, s));
    }
    auto b = bottom.reduced(0, 6);
    inK->setBounds(b.removeFromLeft(80));
    outK->setBounds(b.removeFromLeft(80));
    b.removeFromLeft(20);
    auto btns = b.withSizeKeepingCentre(b.getWidth(), 30);
    longBtn.setBounds(btns.removeFromLeft(90)); btns.removeFromLeft(10);
    gateBtn.setBounds(btns.removeFromLeft(90));
}
