// Tuman — dynamics group: Pump (11) and Multiband Drive (12).
#pragma once
#include "Common.h"

namespace tuman {

// Quadratic soft clip: 2x - x|x| on [-1,1].
inline float quadSat(float x) {
    if (x >= 1.0f) x = 1.0f;
    else if (x < -1.0f) x = -1.0f;
    if (x > 0.0f) return (x + x * x * -0.5f) * 2.0f;
    return (x + x * x * 0.5f) * 2.0f;
}

struct AlgMultiband {
    // per band: input band-pass (a,b), output band-pass (c,d), saturated feedback fb; [L,R]
    struct Band { float a[2]{}, b[2]{}, c[2]{}, d[2]{}, fb[2]{}; } lo, mid, hi;

    static float drive(float p, float slope) {
        float g = 1.0f;
        if (p >= 0.333000034f) g = (p + -0.333000004f) * slope + 1.0f;
        return g;
    }
    static float blend(float p) { float b = p * 3.0f; return b >= 1.00000012f ? 1.0f : b; }
    static float shape(float x, float g, float b) {
        float d = x * g;
        float y = (1.0f - b) * d;
        return y + quadSat(d) * b;
    }

    void process(Ctx& c, float inL, float inR, float& outL, float& outR) {
        const float mix = c.k[0], fat = c.k[4] * 1.1f;
        const float xl = inL * c.gain, xr = inR * c.gain;
        const float cLow = fat * 0.005f + 0.045f;
        float oLo[2] = { 0, 0 }, oMid[2] = { 0, 0 }, oHi[2] = { 0, 0 };
        if (c.state != 2) {
            if (c.state == 3) c.state = 4;
            const float x[2] = { xl, xr };
            // low band
            {
                float b = blend(c.k[1]), g = drive(c.k[1], 20.0f);
                float fbk = (fat * 1.005f) / (g * 1.86f + b * 2.0f);
                for (int ch = 0; ch < 2; ++ch) {
                    float in = x[ch] + lo.fb[ch] * fbk;
                    lo.a[ch] = lo.a[ch] + (in - lo.a[ch]) * 0.0004f;
                    lo.b[ch] = lo.b[ch] - ((lo.a[ch] - in) + lo.b[ch]) * cLow;
                    float y = shape(lo.b[ch], g, b);
                    lo.c[ch] = lo.c[ch] + (y - lo.c[ch]) * 0.0004f;
                    lo.d[ch] = lo.d[ch] - ((lo.c[ch] - y) + lo.d[ch]) * cLow;
                    oLo[ch] = lo.d[ch];
                    lo.fb[ch] = cubicSat(lo.d[ch]);
                }
            }
            // mid band
            {
                float b = blend(c.k[2]), g = drive(c.k[2], 30.0f);
                float fbk = (fat * 1.5f) / (g * 2.7f + b * 2.0f);
                for (int ch = 0; ch < 2; ++ch) {
                    float in = x[ch] + mid.fb[ch] * fbk;
                    mid.a[ch] = mid.a[ch] + (in - mid.a[ch]) * cLow;
                    mid.b[ch] = mid.b[ch] - ((mid.a[ch] - in) + mid.b[ch]) * 0.3111f;
                    float y = shape(mid.b[ch], g, b);
                    mid.c[ch] = mid.c[ch] + (y - mid.c[ch]) * cLow;
                    mid.d[ch] = mid.d[ch] - ((mid.c[ch] - y) + mid.d[ch]) * 0.3111f;
                    oMid[ch] = mid.d[ch];
                    mid.fb[ch] = cubicSat(mid.d[ch]);
                }
            }
            // high band
            {
                float b = blend(c.k[3]), g = drive(c.k[3], 30.0f);
                float fbk = (fat * 2.6f) / (g * 2.7f + b * 2.0f);
                for (int ch = 0; ch < 2; ++ch) {
                    float in = x[ch] + hi.fb[ch] * fbk;
                    hi.a[ch] = hi.a[ch] + (in - hi.a[ch]) * 0.3111f;
                    hi.b[ch] = hi.b[ch] - ((hi.a[ch] - in) + hi.b[ch]) * 0.5f;
                    float y = shape(hi.b[ch], g, b);
                    hi.c[ch] = hi.c[ch] + (y - hi.c[ch]) * 0.3111f;
                    hi.d[ch] = hi.d[ch] - ((hi.c[ch] - y) + hi.d[ch]) * 0.5f;
                    oHi[ch] = hi.d[ch];
                    hi.fb[ch] = cubicSat(hi.d[ch]);
                }
            }
        }
        float dry = 1.0f - mix;
        float wl = ((oLo[0] + oMid[0]) + oHi[0]) * 0.5f;
        float wr = ((oLo[1] + oMid[1]) + oHi[1]) * 0.5f;
        outL = c.lim(wl * mix * c.gain + dry * inL);
        outR = c.lim(wr * mix * c.gain + dry * inR);
    }
};

// Pump: peak compressor with a resonant bass booster ahead of the detector and a gate.
struct AlgPump {
    FilterLP lp;
    float env = 0.0f;     // gain-computer state (target gain, up to 55x)
    float gate = 0.0f;    // gain ceiling that sinks toward 2.5 after long loud-gain periods
    int count = 0;
    void process(Ctx& c, float inL, float inR, float& outL, float& outR) {
        const float mix = c.k[0], bass = c.k[1], att = c.k[2], rel = 1.0f - c.k[3], amt = c.k[4];
        float l = inL, r = inR;
        if (c.state != 2) {
            if (c.state == 3) { env = 0.0f; count = 0; gate = 0.0f; c.state = 4; }
            float cut = bass * 0.0001f + 0.001835f;
            float bl = lp(0, l, cut, 0.395f);
            float br = lp(1, r, cut, 0.395f);
            l = l + (bl * bass) * 3.0f;
            r = r + (br * bass) * 3.0f;
            float pk = std::fabs(l); float ar = std::fabs(r); if (pk < ar) pk = ar;
            float g = 0.56f / (pk + 1e-6f);
            if (g >= 55.0000038f) g = 55.0f;
            if (g < env) env = env + (g - env) * (0.1f / ((att * att) * 100.0f + 1.0f));
            else env = (0.02f / (((rel * rel) * rel) * 300.0f + 1.0f)) + env;
            int n = count;
            if (env >= 50.0000038f && c.alt) {
                if (count < 0xecfa) n = count + 1;
                if (n > 0xea60) { gate = gate + (gate + -2.5f) * -0.0003f; n = 0xea60; }
            } else {
                gate = gate + (gate + -56.0f) * -0.01f;
                n = 0;
            }
            count = n;
            float gg = env; if (gate < gg) gg = gate;
            float cap = amt * 100.0f + 10.0f; if (cap < gg) gg = cap;
            float gain = (1.0f - amt) + (gg * amt) * 1.2f;
            l = l * gain; r = r * gain;
        }
        float dry = 1.0f - mix;
        outL = c.lim(l * c.gain * mix + dry * inL);
        outR = c.lim(r * c.gain * mix + dry * inR);
    }
};

} // namespace tuman
