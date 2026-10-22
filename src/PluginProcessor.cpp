#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
const AlgInfo kAlgs[tuman::kNumAlgs] = {
    { "Lush Reverb",        "reverb",  { "Tone", "Motion", "Size", "Decay" } },
    { "Infinity Reverb",    "reverb",  { "Low Cut", "Damping", "Drive", "Time" } },
    { "Generative Reverb",  "reverb",  { "Low Cut", "High Cut", "Resonance", "Decay" } },
    { "Granular Shimmer",   "reverb",  { "Oct Down", "Oct Up", "Smear", "Feedback" } },
    { "Granular Reverb",    "reverb",  { "Tone", "Reverse", "Smear", "Feedback" } },
    { "Lo-Fi Delay",        "delay",   { "Tone", "Reverse", "Time", "Feedback" } },
    { "Micro Loops",        "delay",   { "Loop 1", "Loop 2", "Loop 3", "Overdub" } },
    { "Micro Loops Reverb", "delay",   { "Loop 1", "Loop 2", "Loop 3", "Diffuse" } },
    { "Flanger",            "special", { "Env", "Release", "Time", "Stages" } },
    { "Nyquist",            "special", { "Bits", "Sweep", "Rate", "Multiply" } },
    { "Old Tape",           "special", { "Flutter", "Wow", "Flicker", "Tone" } },
    { "Pump",               "master",  { "Bass", "Punch", "Density", "Amount" } },
    { "Multiband Drive",    "master",  { "Low", "Mid", "High", "Fat" } },
    { "Wavefolder",         "master",  { "Tone", "Resonance", "Shape", "Fold" } },
    { "HPF / LPF",          "master",  { "HP Freq", "HP Res", "LP Freq", "LP Res" } },
};

struct Preset { const char* name; int alg; float mix, p1, p2, p3, p4; bool longMode; };
const Preset kPresets[] = {
    { "Lush Hall",          0, 0.45f, 0.55f, 0.30f, 0.70f, 0.72f, false },
    { "Chorus Room",        0, 0.40f, 0.50f, 0.50f, 0.05f, 0.20f, false },
    { "Endless",            1, 0.60f, 0.20f, 0.45f, 0.20f, 1.00f, false },
    { "Frozen Pad (Long)",  1, 0.70f, 0.30f, 0.35f, 0.10f, 0.97f, true  },
    { "Singing Walls",      2, 0.55f, 0.20f, 0.50f, 0.75f, 0.80f, false },
    { "Shimmer Up",         3, 0.55f, 0.00f, 0.80f, 0.50f, 0.70f, false },
    { "Shimmer Down",       3, 0.55f, 0.80f, 0.00f, 0.50f, 0.60f, false },
    { "Reverse Cloud",      4, 0.60f, 0.50f, 0.80f, 0.50f, 0.70f, false },
    { "Lo-Fi Echo",         5, 0.45f, 0.45f, 0.10f, 0.45f, 0.55f, false },
    { "Backwards Tape",     5, 0.55f, 0.50f, 0.95f, 0.60f, 0.50f, false },
    { "Memory Loops",       6, 0.65f, 0.30f, 0.55f, 0.80f, 0.70f, false },
    { "Loops in Fog",       7, 0.65f, 0.40f, 0.70f, 0.90f, 0.60f, false },
    { "Warbled Cassette",  10, 1.00f, 0.30f, 0.70f, 0.40f, 0.50f, false },
    { "Breathing Flanger",  8, 0.80f, 0.80f, 0.30f, 0.50f, 0.70f, false },
    { "Aliased Ghost",      9, 0.70f, 0.60f, 0.70f, 0.40f, 0.00f, false },
    { "Pumped Drums",      11, 1.00f, 0.50f, 0.30f, 0.60f, 0.70f, false },
    { "Fat Drive",         12, 1.00f, 0.50f, 0.50f, 0.40f, 0.50f, false },
    { "Soft Fold",         13, 1.00f, 0.60f, 0.30f, 0.40f, 0.45f, false },
    { "Band Sweep",        14, 1.00f, 0.25f, 0.50f, 0.70f, 0.40f, false },
};
constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));
constexpr double kEngineRate = tuman::kEngineRate;
constexpr size_t kFifoPrefill = 96;
}

// Measures the pipeline delay (prefill + both resampling stages) with an impulse.
static int measureLatency(double sr) {
    tuman::SincResampler u, d;
    u.prepare(sr, kEngineRate); d.prepare(kEngineRate, sr);
    std::vector<float> fifo(kFifoPrefill, 0.0f);
    size_t rd = 0; int best = 0; float bestV = 0.0f;
    for (int i = 0; i < 4000; ++i) {
        u.push(i == 0 ? 1.0f : 0.0f, 0.0f);
        u.pull([&](float a, float) { d.push(a, 0.0f); });
        d.pull([&](float a, float) { fifo.push_back(a); });
        float y = rd < fifo.size() ? fifo[rd++] : 0.0f;
        if (std::abs(y) > bestV) { bestV = std::abs(y); best = i; }
    }
    return best;
}

const AlgInfo& algInfo(int i) { return kAlgs[juce::jlimit(0, tuman::kNumAlgs - 1, i)]; }

juce::AudioProcessorValueTreeState::ParameterLayout TumanProcessor::createLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    StringArray names;
    for (int i = 0; i < tuman::kNumAlgs; ++i) names.add(kAlgs[i].name);
    l.add(std::make_unique<AudioParameterChoice>(ParameterID { "alg", 1 }, "Algorithm", names, 0));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID { "mix", 1 }, "Mix", NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    for (int i = 0; i < 4; ++i)
        l.add(std::make_unique<AudioParameterFloat>(ParameterID { "p" + String(i + 1), 1 }, "P" + String(i + 1),
                                                    NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID { "in", 1 }, "Input", NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
                                                AudioParameterFloatAttributes().withLabel("dB")));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID { "out", 1 }, "Output", NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f,
                                                AudioParameterFloatAttributes().withLabel("dB")));
    l.add(std::make_unique<AudioParameterBool>(ParameterID { "long", 1 }, "Long (half rate)", false));
    l.add(std::make_unique<AudioParameterBool>(ParameterID { "gate", 1 }, "Gate", true));
    return l;
}

TumanProcessor::TumanProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "TUMAN", createLayout()) {
    pAlg = apvts.getRawParameterValue("alg");
    pMix = apvts.getRawParameterValue("mix");
    for (int i = 0; i < 4; ++i) pP[i] = apvts.getRawParameterValue("p" + juce::String(i + 1));
    pIn = apvts.getRawParameterValue("in");
    pOut = apvts.getRawParameterValue("out");
    pLong = apvts.getRawParameterValue("long");
    pGate = apvts.getRawParameterValue("gate");
    engine = std::make_unique<tuman::Engine>();
}

bool TumanProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void TumanProcessor::prepareToPlay(double sampleRate, int) {
    hostRate = sampleRate;
    engine = std::make_unique<tuman::Engine>();
    float k[5] = { pMix->load(), pP[0]->load(), pP[1]->load(), pP[2]->load(), pP[3]->load() };
    for (int i = 0; i < 5; ++i) engine->setKnob(i, k[i]);
    engine->snapKnobs(k);
    engine->forceAlgorithm((int)pAlg->load());
    engine->c.gain = 0.0f;   // fade in like the device does after an algorithm change
    juce::Random rnd;
    for (auto& a : engine->c.adcExtra) a = (uint32_t)rnd.nextInt(4096);

    bypassResample = std::abs(sampleRate - kEngineRate) < 0.5;
    up.prepare(sampleRate, kEngineRate);
    down.prepare(kEngineRate, sampleRate);
    dc.prepare(kEngineRate);
    fifoL.reserve(65536); fifoR.reserve(65536);
    fifoL.assign(kFifoPrefill, 0.0f); fifoR.assign(kFifoPrefill, 0.0f); fifoRead = 0;

    setLatencySamples(bypassResample ? 0 : measureLatency(sampleRate));
    inGain.reset(sampleRate, 0.05); outGain.reset(sampleRate, 0.05);
    inGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(pIn->load()));
    outGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(pOut->load()));
}

void TumanProcessor::runEngine(float l, float r, float& ol, float& orr) {
    // the device's converters clip at full scale
    l = juce::jlimit(-1.0f, 1.0f, l); r = juce::jlimit(-1.0f, 1.0f, r);
    engine->process(l, r, ol, orr);
    dc.process(ol, orr);
    if (engine->c.lim.led > 0) { ledHold = 4000; engine->c.lim.led = 0; }
    else if (ledHold > 0) --ledHold;
}

void TumanProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    float* L = buffer.getWritePointer(0);
    float* R = nch > 1 ? buffer.getWritePointer(1) : nullptr;

    // controls -> engine (the engine applies the device's own knob glide)
    engine->setKnob(0, pMix->load());
    for (int i = 0; i < 4; ++i) engine->setKnob(i + 1, pP[i]->load());
    engine->requestAlgorithm((int)pAlg->load());
    engine->c.halfRate = pLong->load() > 0.5f;
    engine->c.alt = pGate->load() > 0.5f;
    inGain.setTargetValue(juce::Decibels::decibelsToGain(pIn->load()));
    outGain.setTargetValue(juce::Decibels::decibelsToGain(pOut->load()));

    float inPk = 0.0f, outPk = 0.0f;
    for (int i = 0; i < n; ++i) {
        float g = inGain.getNextValue();
        float xl = L[i] * g, xr = (R ? R[i] : L[i]) * g;
        inPk = std::max(inPk, std::max(std::abs(xl), std::abs(xr)));
        float yl, yr;
        if (bypassResample) {
            runEngine(xl, xr, yl, yr);
        } else {
            up.push(xl, xr);
            up.pull([&](float el, float er) {
                float ol, orr; runEngine(el, er, ol, orr);
                down.push(ol, orr);
            });
            down.pull([&](float a, float b) { fifoL.push_back(a); fifoR.push_back(b); });
            if (fifoRead < fifoL.size() && fifoRead < fifoR.size()) { yl = fifoL[fifoRead]; yr = fifoR[fifoRead]; ++fifoRead; }
            else { yl = yr = 0.0f; }
        }
        float og = outGain.getNextValue();
        yl *= og; yr *= og;
        outPk = std::max(outPk, std::max(std::abs(yl), std::abs(yr)));
        L[i] = yl; if (R) R[i] = yr;
    }
    if (!bypassResample && fifoRead > 4096) {
        fifoL.erase(fifoL.begin(), fifoL.begin() + (long)fifoRead);
        fifoR.erase(fifoR.begin(), fifoR.begin() + (long)fifoRead);
        fifoRead = 0;
    }
    for (int ch = 2; ch < nch; ++ch) buffer.clear(ch, 0, n);
    inLevel.store(std::max(inPk, inLevel.load() * 0.9f));
    outLevel.store(std::max(outPk, outLevel.load() * 0.9f));
    clipLed.store(ledHold > 0);
    activeAlg.store(engine->alg);
}

int TumanProcessor::getNumPrograms() { return kNumPresets; }
const juce::String TumanProcessor::getProgramName(int i) { return (i >= 0 && i < kNumPresets) ? kPresets[i].name : ""; }
void TumanProcessor::setCurrentProgram(int i) {
    if (i < 0 || i >= kNumPresets) return;
    currentPreset = i;
    const Preset& p = kPresets[i];
    auto set = [&](const char* id, float v) {
        if (auto* prm = apvts.getParameter(id)) prm->setValueNotifyingHost(prm->convertTo0to1(v));
    };
    set("alg", (float)p.alg); set("mix", p.mix);
    set("p1", p.p1); set("p2", p.p2); set("p3", p.p3); set("p4", p.p4);
    set("long", p.longMode ? 1.0f : 0.0f);
}

void TumanProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void TumanProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* TumanProcessor::createEditor() { return new TumanEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TumanProcessor(); }
