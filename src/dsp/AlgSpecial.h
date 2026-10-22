// Tuman — special group: Flanger (8), Nyquist (9), Old Tape (10).
#pragma once
#include "Common.h"

namespace tuman {

// Envelope-driven flanger: up to 10 cascaded short comb stages per channel,
// the delay of every stage follows an envelope follower on the input.
struct AlgFlanger {
    std::vector<float> buf = std::vector<float>(16384, 0.0f);
    uint32_t w = 0;
    float envL = 0.0f, envR = 0.0f;

    float chain(float x, uint32_t base, float delay, float sign, float gain, float stages) {
        uint32_t ip = base;
        for (int s = 0; s < 10; ++s) {
            float pos = (float)ip + delay;
            buf[ip & 0x3fff] = x;
            int32_t i = (int32_t)pos;
            float fr = pos - (float)(uint32_t)i;
            float rd = (1.0f - fr) * buf[(uint32_t)i & 0x3fff];
            rd = rd + buf[(uint32_t)(i + 1) & 0x3fff] * fr;
            float y = (x + rd * sign) * gain;
            float wgt = stages - (float)s;
            if (wgt < 0.0f) wgt = 0.0f; else if (wgt >= 1.00000012f) wgt = 1.0f;
            x = (1.0f - wgt) * x + y * wgt;
            ip += 300;
        }
        return x;
    }

    void process(Ctx& c, float inL, float inR, float& outL, float& outR) {
        const float mix = c.k[0];
        float d = (c.k[1] + -0.5f) * 2.0f;
        float depth = d * d; if (d <= 0.0f) depth = -depth;
        const float sens = c.k[2] * c.k[2];
        float xl = inL * c.gain, xr = inR * c.gain;
        if (c.state != 2) {
            if (c.state == 3) { w = 0; envL = envR = 0.0f; c.state = 4; }
            float base = (1.0f - c.k[3]) * 297.0f + 2.0f;
            float room = 100.0f - base; if (room < 0.0f) room = 0.0f;
            float st = c.k[4] * 20.0f, gain, sign;
            if (st >= 10.000001f) {
                st = st + -10.0f;
                gain = 0.58f - ((st * 0.005f) * room) * 0.003f;
                sign = 1.0f;
            } else {
                st = 10.0f - st; gain = 0.6f; sign = -1.0f;
            }
            float att = 0.1f / (sens * 100.0f + 1.0f);
            float rel = 0.01f / (sens * 1000.0f + 1.0f);
            float a = std::fabs(xl);
            envL = envL + (a - envL) * (envL < a ? att : rel);
            a = std::fabs(xr);
            envR = envR + (a - envR) * (envR < a ? att : rel);
            float dl = base + ((envL * depth) * 300.0f) * -3.0f;
            dl = dl < 2.0f ? 2.0f : (dl >= 299.000031f ? 299.0f : dl);
            float dr = base + ((envR * depth) * 300.0f) * -3.0f;
            dr = dr < 2.0f ? 2.0f : (dr >= 299.000031f ? 299.0f : dr);
            xl = chain(xl, w, dl, sign, gain, st);
            xr = chain(xr, w + 0xce4, dr, sign, gain, st);
            w = w ? w - 1 : 0x3fff;
        }
        float dry = 1.0f - mix, bump = mixBump(mix, 1.7f);
        outL = c.lim((xl * c.gain * mix + dry * inL) * bump);
        outR = c.lim((xr * c.gain * mix + dry * inR) * bump);
    }
};

// YIN pitch detector working on a 3x-decimated (15625 Hz) copy of the signal.
struct Yin {
    std::vector<float> ring = std::vector<float>(4096, 0.0f);
    std::vector<float> win = std::vector<float>(2000, 0.0f);
    std::vector<float> d = std::vector<float>(2000, 0.0f);   // not cleared between runs, like the device
    uint32_t idx = 0, n = 2000, half = 1000;
    float thr = 0.05f;
    void reset() { std::fill(ring.begin(), ring.end(), 0.0f); std::fill(win.begin(), win.end(), 0.0f);
                   std::fill(d.begin(), d.end(), 0.0f); idx = 0; n = 2000; half = 1000; thr = 0.05f; }
    void push(float x) { ring[idx] = x; idx = (idx + 1) & 0xfff; }
    float detect(uint32_t N, float threshold) {
        n = N; half = N >> 1; thr = threshold;
        uint32_t start = idx - N - 1;
        for (uint32_t i = 0; i < N; ++i) win[i] = ring[(start + i) & 0xfff];
        for (uint32_t t = 0; t < half; ++t)
            for (uint32_t j = 0; j < half; ++j) { float e = win[j] - win[j + t]; d[t] = d[t] + e * e; }
        d[0] = 1.0f;
        if (d[1] == 0.0f) d[1] = d[1] + 0.001f;
        float run = 0.0f;
        for (uint32_t t = 1; t < half; ++t) { run = run + d[t]; d[t] = ((float)(int)t / run) * d[t]; }
        int32_t tau = -1;
        uint32_t t = 2;
        for (; t < half; ++t) {
            if (d[t] < thr) {
                while (t + 1 < half && d[t + 1] < d[t]) ++t;
                break;
            }
        }
        if (t != half && d[t] < thr) tau = (int32_t)t;
        if (tau < 0) return -1.0f;
        int32_t x0 = tau < 1 ? tau : tau - 1;
        int32_t x2 = (uint32_t)(tau + 1) >= half ? tau : tau + 1;
        float better;
        if (x0 == tau) better = (d[x2] < d[tau]) ? (float)x2 : (float)tau;
        else if (x2 == tau) better = (d[x0] >= d[tau]) ? (float)tau : (float)x0;
        else {
            float s0 = d[x0], s1 = d[tau], s2 = d[x2];
            float den = (((s1 * 2.0f) - s2) - s0) * 2.0f;
            better = (float)tau + (s2 - s0) / den;
        }
        return 15625.0f / better;
    }
};

// Nyquist: 8th-order low-pass -> sample & hold (rate fixed, envelope-swept, or
// a harmonic of the tracked input pitch) -> bit-depth quantiser.
struct AlgNyquist {
    float bq[16]{};        // four DF-I biquads: x1,x2... stored per stage as {x0,x1,y1,y2}
    float hold[2]{};
    float f0 = 440.0f;     // tracked pitch
    float env = 0.0f, prevAbs = 0.0f, clock = 0.0f;
    int decim = 0;
    Yin yin;

    static float biquad(float x, float* s) {   // s: {xprev, xprev2, y1, y2}
        float y = x * 0.1546493f;
        y = y + s[0] * 0.3092986f;
        y = y + s[1] * 0.1546493f;
        y = y - s[2] * -0.622196376f;
        y = y - s[3] * 0.2407936f;
        s[1] = s[0]; s[0] = x; s[3] = s[2]; s[2] = y;
        return y;
    }

    void background(Ctx& c) {
        float a = 1.0f - c.k[2];
        int N = (int)((a * 520.0f) / 1.0f + 380.0f) & ~1;
        float thr = ((a * a) * 0.65f) / 1.0f + 0.05f;
        float f = yin.detect((uint32_t)N, thr);
        if (f > 0.0f && f < 6000.0f) f0 = f;
    }

    void process(Ctx& c, float inL, float inR, float& outL, float& outR) {
        if (c.state == 3) {
            f0 = 440.0f; yin.reset();
            for (float& v : bq) v = 0.0f;
            hold[0] = hold[1] = 0.0f; env = 0.0f; clock = 0.0f;
            c.state = 4;
        }
        if (c.state != 2) {
            float y = (inL + inR) * 0.5f;
            for (int st = 0; st < 4; ++st) y = biquad(y, bq + st * 4);
            if (decim == 0) { yin.push(y); decim = 2; } else --decim;
            float a = unitClamp(std::fabs(y));
            env = env + (a - env) * (prevAbs < a ? 0.1f : 0.0001f);
            prevAbs = a;
            float sweep = c.k[2] * 2.0f + -1.0f;
            float mult = (float)(int32_t)(c.k[4] * 16.0f);
            float t = clock + 2.13333333e-05f;
            float period;
            if (mult >= 1.0f) {
                float fine = c.k[3] + -0.5f; fine = (fine * fine) * fine;
                float ratio = ((fine * 4.0f + 0.5f) * 2.0f + -1.0f) + mult;
                period = 1.0f / (ratio * f0);
            } else {
                float r = c.k[3] * c.k[3] + ((sweep * sweep) * sweep) * env;
                float hz;
                if (r >= 1.00000012f) hz = 20020.0f;
                else if (r < 0.0f) hz = 20.0f;
                else hz = r * 20000.0f + 20.0f;
                period = 1.0f / hz;
            }
            float bits = (c.k[1] * c.k[1]) * 510.0f + 2.0f;
            float raw;
            if (c.k[1] >= 1.00000012f) raw = 0.99999994f;
            else if (c.k[1] < 0.8f) raw = 0.0f;
            else raw = (c.k[1] + -0.8f) * 5.0f;
            clock = t;
            if (period < t) {
                float xin[2] = { inL * c.gain, inR * c.gain };
                for (int ch = 0; ch < 2; ++ch) {
                    float x = xin[ch];
                    float q = std::floor(std::fabs(x) * bits) / bits;
                    if (std::signbit(x)) q = -q;
                    hold[ch] = x * raw + (1.0f - raw) * q;
                }
                clock = clock - period;
                while (period < clock) clock = clock - period;
            }
        }
        hold[0] = hold[0] * c.gain; hold[1] = hold[1] * c.gain;
        float m = c.k[0];
        outL = c.lim(hold[0] * m + (1.0f - m) * inL);
        outR = c.lim(hold[1] * m + (1.0f - m) * inR);
    }
};
struct AlgOldTape { void background(Ctx&) {} void process(Ctx& c, float l, float r, float& ol, float& orr) { ol = c.lim(l); orr = c.lim(r); } };

} // namespace tuman
