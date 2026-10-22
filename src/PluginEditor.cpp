#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

namespace tuman_ui {

namespace {
constexpr float kPi = juce::MathConstants<float>::pi;

const char* const kShort[tuman::kNumAlgs] = {
    "LUSH", "INFIN", "GENER", "SHIMR", "GRAIN", "LO-FI", "LOOPS", "L+VERB",
    "FLANG", "NYQST", "TAPE", "PUMP", "MULTI", "FOLD", "FILTR"
};

float posDeg(int i) { return -140.0f + 20.0f * (float)i; }

juce::Point<float> polar(juce::Point<float> c, float deg, float r) {
    float a = deg * kPi / 180.0f;
    return { c.x + r * std::sin(a), c.y - r * std::cos(a) };
}

juce::Typeface::Ptr face(const char* data, int size) {
    return juce::Typeface::createSystemTypefaceFor(data, (size_t)size);
}

bool longAllowed(int alg) { return alg <= 7 && alg != 5; }
bool gateAllowed(int alg) { return alg == 11 || alg == 13; }

void setAlgParam(juce::AudioProcessorValueTreeState& s, int i) {
    if (auto* p = s.getParameter("alg")) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1((float)i));
        p->endChangeGesture();
    }
}

void drawScrew(juce::Graphics& g, float cx, float cy) {
    g.setColour(juce::Colour(0xffb9b5ac)); g.fillEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
    g.setColour(juce::Colour(0xffa29e95)); g.drawEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f, 1.0f);
}
} // namespace

// ---------------------------------------------------------------- type
juce::Font Type::mono(float pt, bool bold, float tracking) {
    static auto reg = face(BinaryData::SpaceMonoRegular_ttf, BinaryData::SpaceMonoRegular_ttfSize);
    static auto bld = face(BinaryData::SpaceMonoBold_ttf, BinaryData::SpaceMonoBold_ttfSize);
    return juce::Font(juce::FontOptions().withTypeface(bold ? bld : reg).withPointHeight(pt).withKerningFactor(tracking));
}

juce::Font Type::grotesk(float pt, float tracking) {
    static auto b = face(BinaryData::SpaceGroteskBold_ttf, BinaryData::SpaceGroteskBold_ttfSize);
    return juce::Font(juce::FontOptions().withTypeface(b).withPointHeight(pt).withKerningFactor(tracking));
}

// ---------------------------------------------------------------- look and feel
PanelLookAndFeel::PanelLookAndFeel() {
    setColour(juce::PopupMenu::backgroundColourId, Ink::panel());
    setColour(juce::PopupMenu::textColourId, Ink::ink());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Ink::ink());
    setColour(juce::PopupMenu::highlightedTextColourId, Ink::paper());
    setColour(juce::PopupMenu::headerTextColourId, Ink::label());
}

void PanelLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) {
    const bool big = (bool)s.getProperties()["big"];
    const float sc = (float)juce::jmin(w, h) / 100.0f;
    const juce::Point<float> c((float)x + (float)w * 0.5f, (float)y + (float)h * 0.5f);
    auto at = [&](float ang, float r) { return juce::Point<float>(c.x + r * sc * std::sin(ang), c.y - r * sc * std::cos(ang)); };

    // printed scale
    g.setColour(Ink::ink());
    const float t0 = big ? 40.0f : 39.0f, t1 = 46.0f;
    for (int i = 0; i <= 10; ++i) {
        float ang = a0 + (a1 - a0) * (float)i / 10.0f;
        g.drawLine(juce::Line<float>(at(ang, t0), at(ang, t1)), (big ? 1.6f : 2.0f) * sc);
    }
    // cap
    const float rOuter = big ? 34.0f : 32.0f;
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillEllipse(juce::Rectangle<float>(rOuter * 2 * sc, rOuter * 2 * sc).withCentre(c.translated(0, 1.5f * sc)));
    g.setColour(Ink::cap());
    g.fillEllipse(juce::Rectangle<float>(rOuter * 2 * sc, rOuter * 2 * sc).withCentre(c));
    if (big) {
        g.setColour(Ink::capIn());
        g.fillEllipse(juce::Rectangle<float>(56 * sc, 56 * sc).withCentre(c));
    }
    // indicator
    float ang = a0 + pos * (a1 - a0);
    juce::Path line;
    line.startNewSubPath(at(ang, big ? 12.0f : 10.0f));
    line.lineTo(at(ang, 26.0f));
    g.setColour(Ink::paper());
    g.strokePath(line, juce::PathStrokeType((big ? 3.5f : 4.0f) * sc, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (s.isMouseOverOrDragging()) {
        g.setColour(Ink::ink().withAlpha(0.08f));
        g.fillEllipse(juce::Rectangle<float>(96 * sc, 96 * sc).withCentre(c));
    }
}

// ---------------------------------------------------------------- selector
Selector::Selector(juce::AudioProcessorValueTreeState& s) : state(s) {}

juce::Rectangle<float> Selector::labelBox(int i) const {
    return juce::Rectangle<float>(72.0f, 24.0f).withCentre(polar(centre, posDeg(i), 176.0f));
}

int Selector::labelAt(juce::Point<float> p) const {
    for (int i = 0; i < tuman::kNumAlgs; ++i)
        if (labelBox(i).expanded(2.0f).contains(p)) return i;
    return -1;
}

void Selector::choose(int i) {
    i = juce::jlimit(0, tuman::kNumAlgs - 1, i);
    if (i != alg) setAlgParam(state, i);
}

void Selector::paint(juce::Graphics& g) {
    const auto& info = algInfo(alg);
    g.setColour(Ink::ink());
    g.setFont(Type::mono(11.0f, false, 0.2f));
    g.drawText("ALGORITHM", juce::Rectangle<float>(40, 30, 200, 16), juce::Justification::centredLeft);

    for (int i = 0; i < tuman::kNumAlgs; ++i)
        g.drawLine(juce::Line<float>(polar(centre, posDeg(i), 126.0f), polar(centre, posDeg(i), 138.0f)), 2.0f);

    auto disc = [&](float r) { return juce::Rectangle<float>(r * 2, r * 2).withCentre(centre); };
    g.setColour(juce::Colour(0xffc7c3ba)); g.fillEllipse(disc(118.0f));
    g.setColour(juce::Colours::black.withAlpha(0.22f)); g.fillEllipse(disc(104.0f).translated(0, 3.0f));
    g.setColour(Ink::cap()); g.fillEllipse(disc(104.0f));
    g.setColour(Ink::capIn()); g.fillEllipse(disc(92.0f));
    g.setColour(juce::Colour(0xff2e2f32)); g.drawEllipse(disc(92.0f), 2.0f);

    float d = posDeg(alg);
    juce::Path pointer;
    pointer.startNewSubPath(polar(centre, d, 30.0f));
    pointer.lineTo(polar(centre, d, 84.0f));
    g.setColour(Ink::paper());
    g.strokePath(pointer, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(Ink::group(info.group));
    g.fillEllipse(juce::Rectangle<float>(12, 12).withCentre(polar(centre, d, 111.0f)));

    g.setFont(Type::mono(11.0f, true, 0.04f));
    for (int i = 0; i < tuman::kNumAlgs; ++i) {
        auto b = labelBox(i);
        if (i == alg) {
            g.setColour(Ink::ink()); g.fillRoundedRectangle(b, 4.0f);
            g.setColour(Ink::paper());
        } else {
            if (i == hover) { g.setColour(Ink::ink().withAlpha(0.08f)); g.fillRoundedRectangle(b, 4.0f); }
            g.setColour(Ink::ink());
        }
        g.drawText(kShort[i], b, juce::Justification::centred);
    }

    // group legend
    const char* names[4] = { "REVERB", "DELAY", "SPECIAL", "DYNAMICS" };
    const char* ids[4] = { "reverb", "delay", "special", "master" };
    float x = 40.0f, y = 540.0f - 34.0f - 14.0f;
    g.setFont(Type::mono(10.0f, false, 0.12f));
    for (int i = 0; i < 4; ++i) {
        g.setColour(Ink::group(ids[i]));
        g.fillEllipse(x, y + 2.0f, 10.0f, 10.0f);
        g.setColour(Ink::ink());
        float tw = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), names[i]);
        g.drawText(names[i], juce::Rectangle<float>(x + 16.0f, y, tw + 4.0f, 14.0f), juce::Justification::centredLeft);
        x += 16.0f + tw + 16.0f;
    }
}

void Selector::mouseDown(const juce::MouseEvent& e) {
    auto p = e.position;
    int l = labelAt(p);
    if (l >= 0) { choose(l); return; }
    dragging = p.getDistanceFrom(centre) < 120.0f;
    if (dragging) mouseDrag(e);
}

void Selector::mouseDrag(const juce::MouseEvent& e) {
    if (!dragging) return;
    auto v = e.position - centre;
    if (v.getDistanceFromOrigin() < 12.0f) return;
    float deg = std::atan2(v.x, -v.y) * 180.0f / kPi;
    choose((int)std::lround((juce::jlimit(-140.0f, 140.0f, deg) + 140.0f) / 20.0f));
}

void Selector::mouseUp(const juce::MouseEvent&) { dragging = false; }

void Selector::mouseMove(const juce::MouseEvent& e) {
    int l = labelAt(e.position);
    bool onKnob = e.position.getDistanceFrom(centre) < 120.0f;
    setMouseCursor(l >= 0 || onKnob ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (l != hover) { hover = l; repaint(); }
}

void Selector::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
    wheelAcc += w.deltaY;
    if (std::abs(wheelAcc) >= 0.12f) { choose(alg + (wheelAcc > 0 ? 1 : -1)); wheelAcc = 0.0f; }
}

// ---------------------------------------------------------------- small controls
void ToggleSwitch::paintButton(juce::Graphics& g, bool over, bool) {
    const bool on = getToggleState();
    const juce::Colour txt = isEnabled() ? Ink::ink() : Ink::muted();
    auto pill = juce::Rectangle<float>(0, 2, 22, 40);
    g.setColour(Ink::cap()); g.fillRoundedRectangle(pill, 11.0f);
    auto knob = juce::Rectangle<float>(16, 16).withCentre({ 11.0f, on ? 13.0f : 31.0f });
    g.setColour(isEnabled() ? juce::Colour(0xffe9e6df) : juce::Colour(0xff7d7a74));
    g.fillEllipse(knob);
    g.setColour(txt.withAlpha(over && isEnabled() ? 0.75f : 1.0f));
    g.setFont(Type::mono(12.0f, true, 0.1f));
    g.drawText(getButtonText(), juce::Rectangle<float>(32, 0, (float)getWidth() - 32, (float)getHeight()), juce::Justification::centredLeft);
}

void DbReadout::paint(juce::Graphics& g) {
    double v = getValue();
    juce::String s = caption + " " + (v < -0.05 ? juce::String::fromUTF8("\xe2\x88\x92") : juce::String()) + juce::String(std::abs(v), std::abs(v) < 9.95 ? 1 : 0) + " dB";
    g.setColour(Ink::ink().withAlpha(isMouseOverOrDragging() ? 0.7f : 1.0f));
    g.setFont(Type::mono(10.0f, false, 0.12f));
    g.drawText(s, getLocalBounds(), juce::Justification::centredLeft);
}

void PresetButton::paint(juce::Graphics& g) {
    int i = proc.getCurrentProgram();
    g.setColour(Ink::ink().withAlpha(isMouseOver() ? 0.7f : 1.0f));
    g.setFont(Type::mono(10.0f, false, 0.12f));
    g.drawText(juce::String("PRESET  ") + proc.getProgramName(i).toUpperCase() + juce::String::fromUTF8("  \xe2\x96\xbe"),
               getLocalBounds(), juce::Justification::centredRight);
}

void PresetButton::mouseDown(const juce::MouseEvent&) {
    juce::PopupMenu m;
    m.setLookAndFeel(&getLookAndFeel());
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        m.addItem(i + 1, proc.getProgramName(i), true, i == proc.getCurrentProgram());
    juce::Component::SafePointer<PresetButton> self(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [self](int r) {
        if (r > 0 && self != nullptr) { self->proc.setCurrentProgram(r - 1); self->repaint(); }
    });
}

// ---------------------------------------------------------------- brushed surface
juce::Image Panel::makeBrushedSurface(int scale) {
    const int w = W * scale, h = H * scale;
    juce::Image img(juce::Image::RGB, w, h, false);
    juce::Random rng(20261022);
    const juce::Colour base = Ink::panel();
    const float br = base.getFloatRed(), bg = base.getFloatGreen(), bb = base.getFloatBlue();

    std::vector<float> row((size_t)w);
    float rowSlow = 0.0f;
    // direction of the soft sheen (CSS 100deg)
    const float dx = 0.9848f, dy = 0.1736f;
    const float tMin = 0.0f, tMax = (float)w * dx + (float)h * dy;
    auto sheen = [](float t) {
        struct S { float p, a; };
        static const S s[] = { { 0.0f, 0 }, { 0.18f, 0 }, { 0.40f, 0.30f }, { 0.58f, 0 }, { 0.78f, 0.13f }, { 0.92f, 0 }, { 1.0f, 0 } };
        for (int i = 0; i < 6; ++i)
            if (t <= s[i + 1].p) { float u = (t - s[i].p) / (s[i + 1].p - s[i].p); u = u * u * (3 - 2 * u); return s[i].a + (s[i + 1].a - s[i].a) * u; }
        return 0.0f;
    };

    juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y) {
        // every row of the panel gets its own long, slightly varying scratch profile
        rowSlow = 0.82f * rowSlow + 0.18f * (rng.nextFloat() * 2.0f - 1.0f);
        const float rowAmp = 0.5f + rng.nextFloat();
        int x = 0; float v0 = rng.nextFloat() * 2 - 1;
        while (x < w) {
            int len = 40 * scale + rng.nextInt(360 * scale);
            float v1 = rng.nextFloat() * 2 - 1;
            for (int i = 0; i < len && x < w; ++i, ++x) {
                float u = (float)i / (float)len; u = u * u * (3 - 2 * u);
                row[(size_t)x] = v0 + (v1 - v0) * u;
            }
            v0 = v1;
        }
        for (int xx = 0; xx < w; ++xx) {
            float fine = rng.nextFloat() * 2 - 1;
            float n = 0.030f * rowSlow + 0.022f * rowAmp * row[(size_t)xx] + 0.010f * fine;
            float t = ((float)xx * dx + (float)y * dy - tMin) / (tMax - tMin);
            float s = sheen(t);
            float k = 1.0f - n;
            float r = juce::jlimit(0.0f, 1.0f, (br * k) * (1 - s) + s);
            float gg = juce::jlimit(0.0f, 1.0f, (bg * k) * (1 - s) + s);
            float b = juce::jlimit(0.0f, 1.0f, (bb * k) * (1 - s) + s);
            bd.setPixelColour(xx, y, juce::Colour::fromFloatRGBA(r, gg, b, 1.0f));
        }
    }
    return img;
}

// ---------------------------------------------------------------- panel
void Panel::setupKnob(juce::Slider& s, bool big) {
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters(1.25f * kPi, 2.75f * kPi, true);
    s.setMouseDragSensitivity(240);
    s.setVelocityBasedMode(false);
    s.getProperties().set("big", big);
    s.setPopupDisplayEnabled(true, true, this);
    s.setLookAndFeel(&lnf);
    addAndMakeVisible(s);
}

Panel::Panel(TumanProcessor& p_) : proc(p_), selector(p_.apvts), preset(p_) {
    setSize(W, H);
    setLookAndFeel(&lnf);
    surface = makeBrushedSurface(2);
    addAndMakeVisible(selector);
    selector.setBounds(0, 0, 460, H);

    auto& s = proc.apvts;
    setupKnob(mix, true);
    mix.setDoubleClickReturnValue(true, 0.5);
    mixAtt = std::make_unique<SA>(s, "mix", mix);
    for (int i = 0; i < 4; ++i) {
        setupKnob(p[i], false);
        p[i].setDoubleClickReturnValue(true, 0.5);
        pAtt[i] = std::make_unique<SA>(s, "p" + juce::String(i + 1), p[i]);
    }
    for (auto* d : { &inDb, &outDb }) { d->setDoubleClickReturnValue(true, 0.0); addAndMakeVisible(*d); }
    inAtt = std::make_unique<SA>(s, "in", inDb);
    outAtt = std::make_unique<SA>(s, "out", outDb);

    for (auto* t : { &longSw, &gateSw }) addAndMakeVisible(*t);
    longAtt = std::make_unique<BA>(s, "long", longSw);
    gateAtt = std::make_unique<BA>(s, "gate", gateSw);
    longSw.setTooltip("Hidden device mode: half rate - twice as long and darker");
    gateSw.setTooltip("Alternate behaviour of Pump and Wavefolder");
    addAndMakeVisible(preset);

    // layout (panel coordinates)
    const int x0 = 501, colW = 419;
    preset.setBounds(x0 + colW - 260, 62, 260, 18);
    mix.setBounds(x0, 93, 132, 132);
    const int mx = x0 + 132 + 28;
    longSw.setBounds(mx, 148, 84, 44);
    gateSw.setBounds(mx + 106, 148, 84, 44);
    inDb.setBounds(mx + 70, 203, 76, 20);
    outDb.setBounds(mx + 150, 203, 86, 20);
    const float cw = (float)(colW - 24) / 4.0f;
    for (int i = 0; i < 4; ++i)
        p[i].setBounds(juce::Rectangle<int>(92, 92).withCentre({ x0 + (int)(cw * (float)i + cw * 0.5f) + 8 * i, 287 + 46 }));
}

Panel::~Panel() {
    mix.setLookAndFeel(nullptr);
    for (auto& k : p) k.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void Panel::refresh(int alg, bool clip) {
    if (alg != shown) {
        shown = alg;
        selector.alg = alg;
        const auto& a = algInfo(alg);
        for (int i = 0; i < 4; ++i) knobLabel[i] = juce::String(a.p[i]).toUpperCase();
        longSw.setEnabled(longAllowed(alg));
        gateSw.setEnabled(gateAllowed(alg));
        selector.repaint();
        preset.repaint();
        repaint();
    }
    if (clip != led) { led = clip; repaint(661, 200, 20, 20); }
}

void Panel::paint(juce::Graphics& g) {
    g.drawImage(surface, juce::Rectangle<float>(0, 0, (float)W, (float)H));
    drawScrew(g, 19, 19); drawScrew(g, W - 19, 19); drawScrew(g, 19, H - 19); drawScrew(g, W - 19, H - 19);

    g.setColour(Ink::rule());
    g.fillRect(460.0f, 40.0f, 1.0f, (float)H - 80.0f);

    const int x0 = 501, colW = 419;
    const auto& a = algInfo(juce::jmax(0, shown));
    g.setColour(Ink::ink());
    g.setFont(Type::grotesk(34.0f, 0.18f));
    g.drawText("TUMAN", juce::Rectangle<int>(x0, 26, 260, 44), juce::Justification::centredLeft);

    // current algorithm, top right
    g.setFont(Type::mono(11.0f));
    juce::String name = juce::String(a.name).toUpperCase();
    float tw = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), name);
    float nx = (float)(x0 + colW) - tw;
    g.drawText(name, juce::Rectangle<float>(nx, 38, tw + 2, 16), juce::Justification::centredLeft);
    auto dot = juce::Rectangle<float>(nx - 18, 41, 10, 10);
    g.setColour(Ink::group(a.group)); g.fillEllipse(dot);
    g.setColour(Ink::ink()); g.drawEllipse(dot, 1.0f);

    // mix caption
    g.setFont(Type::mono(12.0f, true, 0.14f));
    g.drawText("MIX", juce::Rectangle<int>(x0, 231, 132, 16), juce::Justification::centred);

    // mode block
    const int mx = x0 + 132 + 28;
    g.setColour(Ink::label());
    g.setFont(Type::mono(10.0f, false, 0.14f));
    g.drawText("MODE", juce::Rectangle<int>(mx, 120, 100, 14), juce::Justification::centredLeft);
    auto ledR = juce::Rectangle<float>((float)mx, 207.0f, 12.0f, 12.0f);
    if (led) { g.setColour(juce::Colour(0x66ff3b2e)); g.fillEllipse(ledR.expanded(4.0f)); }
    g.setColour(led ? juce::Colour(0xffff3b2e) : juce::Colour(0xff7a1f1a)); g.fillEllipse(ledR);
    g.setColour(Ink::ink()); g.drawEllipse(ledR, 1.0f);
    g.setFont(Type::mono(10.0f, false, 0.12f));
    g.drawText("CLIP", juce::Rectangle<int>(mx + 20, 203, 44, 20), juce::Justification::centredLeft);

    g.setColour(Ink::rule());
    g.fillRect((float)x0, 267.0f, (float)colW, 1.0f);

    g.setColour(Ink::ink());
    g.setFont(Type::mono(11.0f, true, 0.08f));
    for (int i = 0; i < 4; ++i) {
        auto b = p[i].getBounds();
        g.drawText(knobLabel[i], juce::Rectangle<int>(b.getCentreX() - 60, b.getBottom() + 6, 120, 16), juce::Justification::centred);
    }
}

} // namespace tuman_ui

// ---------------------------------------------------------------- editor
TumanEditor::TumanEditor(TumanProcessor& owner) : AudioProcessorEditor(&owner), proc(owner), panel(owner) {
    addAndMakeVisible(panel);
    setResizable(true, true);
    setResizeLimits(720, 405, 1920, 1080);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio((double)tuman_ui::Panel::W / (double)tuman_ui::Panel::H);
    setSize(tuman_ui::Panel::W, tuman_ui::Panel::H);
    timerCallback();
    startTimerHz(30);
}

TumanEditor::~TumanEditor() = default;

void TumanEditor::paint(juce::Graphics& g) { g.fillAll(tuman_ui::Ink::panel()); }

void TumanEditor::resized() {
    float s = (float)getWidth() / (float)tuman_ui::Panel::W;
    panel.setTransform(juce::AffineTransform::scale(s));
}

void TumanEditor::timerCallback() {
    int alg = (int)proc.apvts.getRawParameterValue("alg")->load();
    panel.refresh(alg, proc.clipLed.load());
}
