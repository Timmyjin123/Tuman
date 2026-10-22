// Tuman — shared DSP building blocks.
// All processing runs at the engine's fixed internal rate (46875 Hz) in float32,
// one sample at a time. Compile with -ffp-contract=off for bit-stable results.
#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>
#include "FoldCurves.h"

namespace tuman {

constexpr float kPi = 3.14159274f;
constexpr double kEngineRate = 46875.0;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x >= hi ? hi : x); }
// [0,1] clamp with the device's comparison constant (1 + 1ulp)
inline float unitClamp(float x) { return x >= 1.00000012f ? 1.0f : (x < 0.0f ? 0.0f : x); }

// Global white-noise source (xorshift32), shared by every algorithm.
struct Noise {
    uint32_t s = 1234;
    float next() {                       // uniform [0,1)
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float)((double)s * (1.0 / 4294967296.0));
    }
};

// Output soft limiter: linear to 0.9, parabolic knee up to a 1.0 ceiling.
struct Limiter {
    int led = 0;
    float operator()(float x) {
        if (x >= 0.900000036f) {
            if (x >= 1.09840012f) { x = 1.0984f; led = 500; }
            float d = x - 0.9f; x = x + d * d * -2.52f;
        }
        if (x < -0.9f) {
            if (x < -1.0984f) { x = -1.0984f; led = 500; }
            float d = x + 0.9f; x = x + d * d * 2.52f;
        }
        return x;
    }
};

// Cubic soft saturator acting only on the excess beyond +-1 (input limited to +-2).
inline float excessSat(float d) {
    float e = 0.0f;
    if (d >= 2.00000024f) { d = 2.0f; e = 1.0f; }
    else if (d < -2.0f) { d = -2.0f; e = -1.0f; }
    else {
        if (d >= 1.00000012f) e = d - 1.0f;
        if (d < -1.0f) e = d + 1.0f;
    }
    return d + e * e * e * -0.333333343f;
}

inline float cubicSat(float d) {   // limits at +-1.732
    if (d >= 1.73200011f) d = 1.73199999f;
    else if (d < -1.73199999f) d = -1.73199999f;
    return d + d * d * d * -0.333333343f;
}

// Nonlinear 2-pole filter cells with saturated resonance feedback.
// Each "bank" owns up to 12 independent instances, like the original static arrays.
struct FilterHP {          // returns x - lp (high-pass), resonance comp 1+3f^3
    float a[12]{}, b[12]{}, c[12]{};
    float operator()(int i, float x, float f, float res) {
        float k = res * (1.0f + f * f * f * 3.0f);
        float w = unitClamp(f * kPi);
        float t = (a[i] - x) + (c[i] * k) * -4.0f;
        a[i] = a[i] - t * w;
        b[i] = b[i] + (a[i] - b[i]) * w;
        c[i] = excessSat(a[i] - b[i]);
        return x - a[i];
    }
};

struct FilterLP {          // returns lp, resonance comp 1+2w^3, feedback x3, cubic sat at +-1.732
    float a[12]{}, b[12]{}, c[12]{};
    float operator()(int i, float x, float f, float res) {
        float w = unitClamp(f * kPi);
        float k = res * (1.0f + w * w * w * 2.0f);
        float t = (a[i] - x) + (k * c[i]) * -3.0f;
        a[i] = a[i] - t * w;
        b[i] = b[i] + ((b[i] - a[i]) * w) * -0.9f;
        c[i] = cubicSat(a[i] - b[i]);
        return a[i];
    }
};

struct FilterBP {          // returns a - b (band), resonance comp 1+2f^3
    float a[12]{}, b[12]{}, c[12]{};
    float operator()(int i, float x, float f, float res) {
        float k = res * (1.0f + f * f * f * 2.0f);
        float w = unitClamp(f * kPi);
        float t = (a[i] - x) + (c[i] * k) * -4.0f;
        a[i] = a[i] - t * w;
        b[i] = b[i] + (a[i] - b[i]) * w;
        float d = a[i] - b[i];
        c[i] = excessSat(d);
        return d;
    }
};

// Periodic fold curve (period 2), 1024-point table rebuilt from a Fourier spec.
struct FoldCurve {
    float t[1025];
    void build(const FoldSpec& s) {
        for (int n = 0; n < 1024; ++n) {
            double v = s.dc;
            for (int k = 0; k < 24; ++k)
                v += s.h[k][0] * std::cos(2.0 * M_PI * (k + 1) * n / 1024.0 + s.h[k][1]);
            t[n] = (float)std::max(-1.0, std::min(1.0, v));
        }
        t[1024] = t[0];
    }
    float operator()(float x) const {
        if (x > 0.0f) {
            uint32_t n = ((uint32_t)x + 1u) & 0xfffeu;
            x = x - (float)n;
        } else {
            uint32_t n = ((uint32_t)(-x) + 1u) & 0xfffeu;
            x = (float)n + x;
        }
        float p = (x + 1.0f) * 512.0f;
        uint32_t i = (uint32_t)p; if (i > 1023) i = 1023;
        float fr = p - (float)i;
        return (1.0f - fr) * t[i] + t[i + 1] * fr;
    }
};

// Shared context every algorithm sees (mirrors the device's global state).
struct Ctx {
    float k[5]{};          // smoothed knobs: mix, p1..p4
    float gain = 1.0f;     // fade gain used during algorithm changes
    uint8_t state = 0;     // 1 fade-out, 2 muted, 3 init request, 4 fade-in, 0 run
    int tick = 1;          // half-rate toggle (1 = process this sample)
    bool halfRate = false; // hidden "long" mode
    bool alt = true;       // hidden alternate mode (gates in Pump / Wavefolder)
    // half-rate ("long" mode) bookkeeping shared by algorithms 0-7
    float hrSum = 0.0f;            // input L+R stored on the skipped sample
    float hrNew[2]{}, hrOld[2]{};  // last two wet outputs
    Noise noise;
    Limiter lim;
    std::vector<int16_t> shared;   // 60000-sample int16 recording memory
    Ctx() : shared(60008, 0) {}
};

// Half-rate helpers. On the skipped sample the input is remembered and the output
// interpolates the last two wet values; on the processed sample the input is the
// average of both and the output is delayed by one processed step.
inline float hrInput(Ctx& c, float sum) { return c.halfRate ? (c.hrSum + sum) * 0.5f : sum; }
inline void hrSkip(Ctx& c, float sum, float& wl, float& wr) {
    c.hrSum = sum;
    wl = (c.hrOld[0] + c.hrNew[0]) * 0.5f;
    wr = (c.hrOld[1] + c.hrNew[1]) * 0.5f;
}
inline void hrCommit(Ctx& c, float& wl, float& wr) {
    float pl = c.hrNew[0], pr = c.hrNew[1];
    c.hrNew[0] = wl; c.hrNew[1] = wr;
    if (c.halfRate) { wl = pl; wr = pr; }
    c.hrOld[0] = pl; c.hrOld[1] = pr;
}

// Mix law used by most algorithms: dry/wet with a mid-position bump.
inline float mixBump(float m, float c) { return (1.0f - m) * m * c + 1.0f; }

} // namespace tuman
