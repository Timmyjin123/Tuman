// Tuman — engine: knob smoothing, algorithm switching with fades, dispatch.
// Runs one stereo sample per call at kEngineRate (46875 Hz).
#pragma once
#include <memory>
#include "Common.h"
#include "AlgFilters.h"
#include "AlgDynamics.h"
#include "AlgSpecial.h"
#include "AlgReverbs.h"
#include "AlgGranular.h"
#include "AlgDelays.h"

namespace tuman {

enum AlgId {
    LushReverb, InfinityReverb, GenerativeReverb, GranularShimmer, GranularReverb,
    LoFiDelay, MicroLoops, MicroLoopsReverb,
    Flanger, Nyquist, OldTape,
    Pump, MultibandDrive, Wavefolder, HpfLpf,
    kNumAlgs
};

inline const char* algName(int i) {
    static const char* n[] = { "Lush Reverb", "Infinity Reverb", "Generative Reverb", "Granular Shimmer",
        "Granular Reverb", "Lo-Fi Delay", "Micro Loops", "Micro Loops Reverb", "Flanger", "Nyquist",
        "Old Tape", "Pump", "Multiband Drive", "Wavefolder", "HPF / LPF" };
    return (i >= 0 && i < kNumAlgs) ? n[i] : "?";
}

class Engine {
public:
    Ctx c;
    int alg = 0;
    int pendingAlg = 0;
    float raw[5] = { 0, 0, 0, 0, 0 };   // knob targets in ADC units (0..4095)
    float preMix = 0.0f;

    Engine() { algs = std::make_unique<Algs>(); c.state = 3; }

    // v in [0,1]
    void setKnob(int i, float v) { raw[i] = clampf(v, 0.0f, 1.0f) * 4095.0f; }
    void setKnobRaw(int i, float r) { raw[i] = r; }
    // Jump smoothed values straight to the targets (no glide), e.g. on load.
    void snapKnobs(const float* v) {
        for (int i = 0; i < 5; ++i) c.k[i] = v[i];
        preMix = v[0];
    }
    void requestAlgorithm(int a) {
        if (a == pendingAlg || a < 0 || a >= kNumAlgs) return;
        pendingAlg = a;
        if (c.state == 0 || c.state == 4) c.state = 1;   // fade out first
    }
    void forceAlgorithm(int a) { alg = pendingAlg = a; c.state = 3; }

    void process(float inL, float inR, float& outL, float& outR) {
        smoothKnobs();
        if (c.state == 1) {
            c.gain = c.gain + -0.001f;
            if (c.gain < 0.0f) { c.gain = 0.0f; c.state = 2; }
        } else if (c.state == 4) {
            c.gain = c.gain + 0.001f;
            if (c.gain >= 1.0f) { c.gain = 1.0f; c.state = 0; }
        }
        if (c.state == 2 && pendingAlg != alg) {
            alg = pendingAlg;
            std::fill(c.shared.begin(), c.shared.end(), 0);
            c.state = 3;
        }
        c.tick = c.halfRate ? 1 - c.tick : 1;
        dispatch(inL, inR, outL, outR);
        // the device runs some slow analysis in its main loop; emulate it at a fixed cadence
        if (++bgCount >= kBackgroundEvery) { bgCount = 0; if (c.state != 2) background(); }
    }

    static constexpr int kBackgroundEvery = 128;
    int bgCount = 0;

    void background() {
        Algs& a = *algs;
        switch (alg) {
            case LushReverb: a.lush.background(c); break;
            case Nyquist:    a.nyquist.background(c); break;
            case OldTape:    a.tape.background(c); break;
            default: break;
        }
    }

private:
    struct Algs {
        AlgLush lush; AlgInfinity infinity; AlgGenerative generative;
        AlgShimmer shimmer; AlgGrainVerb grainVerb; AlgLoFiDelay lofi;
        AlgMicroLoops loops; AlgMicroLoopsVerb loopsVerb;
        AlgFlanger flanger; AlgNyquist nyquist; AlgOldTape tape;
        AlgPump pump; AlgMultiband multiband; AlgWavefolder folder; AlgFilter filter;
    };
    std::unique_ptr<Algs> algs;
public:
    float algsNyq() const { return algs->nyquist.f0; }
    const AlgNyquist& nyq() const { return algs->nyquist; }
private:

    void smoothKnobs() {
        // mix: two cascaded one-pole smoothers, as on the device
        float s5 = preMix + raw[0] * -0.000254452927f;
        float s6 = s5 + 0.021f;
        preMix = preMix + s6 * -0.001f;
        float m = c.k[0] + (c.k[0] - preMix) * -0.001f;
        c.k[0] = unitClamp(m);
        if (m < 0.0f) c.k[0] = 0.0f;
        for (int i = 1; i < 5; ++i) {
            float t = c.k[i] + raw[i] * -0.000244200259f;
            c.k[i] = c.k[i] + t * -0.001f;
        }
    }

    void dispatch(float l, float r, float& ol, float& orr) {
        Algs& a = *algs;
        switch (alg) {
            case LushReverb:       a.lush.process(c, l, r, ol, orr); break;
            case InfinityReverb:   a.infinity.process(c, l, r, ol, orr); break;
            case GenerativeReverb: a.generative.process(c, l, r, ol, orr); break;
            case GranularShimmer:  a.shimmer.process(c, l, r, ol, orr); break;
            case GranularReverb:   a.grainVerb.process(c, l, r, ol, orr); break;
            case LoFiDelay:        a.lofi.process(c, l, r, ol, orr); break;
            case MicroLoops:       a.loops.process(c, l, r, ol, orr); break;
            case MicroLoopsReverb: a.loopsVerb.process(c, l, r, ol, orr); break;
            case Flanger:          a.flanger.process(c, l, r, ol, orr); break;
            case Nyquist:          a.nyquist.process(c, l, r, ol, orr); break;
            case OldTape:          a.tape.process(c, l, r, ol, orr); break;
            case Pump:             a.pump.process(c, l, r, ol, orr); break;
            case MultibandDrive:   a.multiband.process(c, l, r, ol, orr); break;
            case Wavefolder:       a.folder.process(c, l, r, ol, orr); break;
            default:               a.filter.process(c, l, r, ol, orr); break;
        }
    }
};

} // namespace tuman
