// Tuman — special group: Flanger (8), Nyquist (9), Old Tape (10).
#pragma once
#include "Common.h"
#include "TapeData.h"

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
// Old Tape: head response (29-tap FIR) -> drive into a cubic saturator -> two-branch
// magnetisation hysteresis -> 10000-sample tape delay read with wow (slow sines with a
// random walk), flutter (faster sines), scrape noise and random drop-outs that freeze
// the treble; P1 flutter, P2 wow / hysteresis width, P3 instability + drop-outs, P4 drive/tone.
struct AlgOldTape {
    struct Osc { float s = 1, c = 0, f = 0; void tick() {
        float t = s - (c * kPi) * f; c = c + (t * kPi) * f;
        float o = t * 1.00000095f; if (o >= 1.00000012f) o = 1.0f; else if (o < -1.0f) o = -1.0f; s = o; } };
    struct Line { std::vector<float> b = std::vector<float>(10000, 0.0f); uint32_t idx = 0, len = 10000;
        void write(float x) { b[idx] = x; idx = (idx + 1) % len; }
        float read(float frac) const {
            float L = (float)len, d = frac * L, m = L + -2.0f;
            if (!(m < d)) d = (d < 2.0f) ? 2.0f : d; else d = m;
            float pos = (float)idx - d;
            if (pos < 0.0f) pos = pos + L;
            if (pos >= L) pos = 0.0f;
            uint32_t i = (uint32_t)pos; float fr = pos - (float)i;
            uint32_t n = (i + 1) % len;
            float v = (1.0f - fr) * b[i];
            return v + b[n] * fr;
        } };

    RandSmooth r0, r1;
    uint32_t xs[4]{};
    Line dl0, dl1;
    float firL[30]{}, firR[30]{};
    Osc o0, o1, o2, o3, o4;
    uint32_t hystN = 1; int32_t cnt = 0; float lastCnt = 0, walk = 0, walkTgt = 0; int32_t walkCnt = 0;
    int32_t dropPol = 0; float lpR = 0, lpL2 = 0, drop = 1.0f, dropRef = 0; int32_t dropOn = 0;
    float envL = 0, envR = 0, prevL = 0, prevR = 0; int32_t hcL = 0, hcR = 0;
    float slow = 0, slowTgt = 0; int32_t slowCnt = 0;
    float hfL = 0, hfR = 0, gain = 1.0f, toneR = 0, toneL = 0, nz = 0, nzTgt = 0; int32_t nzCnt = 0;
    float T[454];

    AlgOldTape() {
        auto cheb = [](const double* c, double x) {
            double b0 = 0, b1 = 0, b2 = 0;
            for (int k = 24; k >= 0; --k) { b2 = b1; b1 = b0; b0 = 2 * x * b1 - b2 + c[k]; }
            return (b0 - b2) * 0.5 + c[0] * 0.5;
        };
        for (int i = 0; i < 227; ++i) T[i] = (float)cheb(kTapeCurveA, -1.0 + 2.0 * i / 226.0);
        for (int i = 0; i < 227; ++i) T[226 + i] = (float)cheb(kTapeCurveB, -1.0 + 2.0 * i / 226.0);
        T[453] = T[452];
    }

    uint32_t xo() {
        uint32_t r = xs[3] + xs[0];
        uint32_t t = xs[1] << 9;
        xs[3] ^= xs[1]; xs[2] ^= xs[0]; xs[1] ^= xs[2]; xs[2] ^= t; xs[0] ^= xs[3];
        xs[3] = (xs[3] >> 21) | (xs[3] << 11);
        return r;
    }
    static float u01(uint32_t v) { return (float)v * 2.32830644e-10f; }

    static float fir(float* st, float x) {
        float y = st[0] + x * kTapeHeadFir[0];
        for (int k = 1; k < 29; ++k) st[k - 1] = st[k] + x * kTapeHeadFir[k];
        return y;
    }

    float hyst(float& prev, int32_t& count, float x, float p2) {
        int32_t n = count;
        if (x < prev) { if ((uint32_t)n < hystN) n = n + 1; }
        else if (n >= 1) n = n - 1;
        prev = x;
        float p = (x + 1.0f) * 113.0f;
        uint32_t i = (uint32_t)p; float f = p - (float)i, g = 1.0f - f;
        float A = T[i + 1] * f; A = A + g * T[i];
        float Bv = T[226 + i + 1] * f; Bv = Bv + g * T[226 + i];
        float Cv = T[452 - i] * f; Cv = Cv + g * T[std::min<uint32_t>(453 - i, 453)];
        float h = (float)n / (float)hystN;
        float mw;
        if (p2 >= 1.00000012f) mw = 0.428571463f; else if (p2 < 0.699999988f) mw = 0.0f; else mw = (p2 + -0.699999988f) / 0.699999988f;
        count = n;
        float s1 = (1.0f - h) * A;
        float s0 = s1 + Bv * h;
        s0 = s0 * mw;
        s1 = s1 + Cv * h;
        return s0 + (1.0f - mw) * s1;
    }

    void init(Ctx& c) {
        for (int i = 0; i < 30; ++i) firL[i] = firR[i] = 0.0f;
        xs[0] = c.rng.next() + c.adcExtra[0];
        xs[1] = c.rng.next() + c.adcExtra[1];
        xs[2] = c.rng.next() + c.adcExtra[2];
        xs[3] = c.rng.next() + c.adcExtra[3];
        Osc* os[5] = { &o0, &o1, &o2, &o3, &o4 };
        for (Osc* o : os) { o->s = 1; o->c = 0; o->f = 0; }
        o0.f = 8.53333313e-06f; o1.f = 1.70666663e-05f; o2.f = 1.70666658e-06f; o3.f = 8.53333331e-05f; o4.f = 0.000353066658f;
        r1.seed(); r1.period = 1000; r1.coef = 9.99999975e-05f;
        r0.seed(); r0.period = 1000; r0.coef = 9.99999975e-05f;
    }

    void background(Ctx& c) {
        if ((float)cnt != lastCnt) {
            float u = u01(xo());
            int32_t per = (int32_t)(u01(xo()) * 25000.0f + 2500.0f);
            int32_t n = walkCnt + 1;
            if (!(per >= walkCnt)) { walkTgt = u01(xo()) * 2.0f + -1.0f; n = 0; }
            walkCnt = n;
            walk = walk + (walkTgt - walk) * 0.00100000005f;
            o3.f = (((c.k[2] * walk + 1.0f) * 54.7999992f) / 46875.0f);
            float q = c.k[2];
            if (q >= 1.00000012f) q = 1.0f; else if (q < 0.300000012f) q = 0.0f; else q = (q + -0.300000012f) / 0.699999988f;
            q = q * q;
            r1.period = (uint32_t)(int32_t)((q * -940.0f + 1000.0f) + u * 30.0f);
            r1.coef = q * 0.00600000005f + 0.00100000005f;
            lastCnt = (float)cnt;
        }
        if (cnt >= 49) {
            cnt = 0;
            float b = (c.k[1] * c.k[1]) * 3.63000011f + 0.270000011f;
            o0.f = ((b + (b * o2.s) * 0.100000001f) * 2.0f) / 46875.0f;
            float b2 = b * 2.0f;
            o1.f = ((b2 + (b2 * o2.s) * 0.100000001f) * 2.0f) / 46875.0f;
            float u = (float)r0.a * 2.32830644e-10f;
            float per;
            if (u < 0.0599999987f) per = u * 14000.0f;
            else if (u < 0.220000014f) per = u * 90000.0f;
            else per = (((float)r0.a * 9.31322588e-11f + 0.600000024f) * 0.720000029f + r0.smooth * 0.200000003f) * 90000.0f;
            r0.period = (uint32_t)(int32_t)per;
            r0.coef = 0.000124000013f;
        }
    }

    void process(Ctx& c, float inL, float inR, float& outL, float& outR) {
        if (c.state == 3) {
            envL = envR = prevL = prevR = 0; hcL = hcR = 0; slow = slowTgt = 0; slowCnt = 0;
            hfL = hfR = 0; toneR = toneL = 0; nz = nzTgt = 0; nzCnt = 0; gain = 1.0f;
            init(c);
            c.state = 4;
        }
        float mod = 0.0f;
        if (c.state != 2) {
            ++cnt;
            float xl = fir(firL, inL * c.gain);
            float xr = fir(firR, inR * c.gain);
            float p4 = c.k[4];
            float drv = (p4 * 20.0f) * p4 + 1.0f;
            float a = -(drv * xl), bb = -(drv * xr);
            float al = std::fabs(a), ar = std::fabs(bb);
            al = al >= 1.00000012f ? 1.0f : (al < 0.0f ? 0.0f : al);
            ar = ar >= 1.00000012f ? 1.0f : (ar < 0.0f ? 0.0f : ar);
            envL = envL + (al - envL) * (envL < al ? 0.300000012f : 0.0299999993f);
            envR = envR + (ar - envR) * (envL < ar ? 0.300000012f : 0.0299999993f);
            if (a >= 1.00000012f) a = 1.0f; else if (a < -1.0f) a = -1.0f;
            if (bb >= 1.00000012f) bb = 1.0f; else if (bb < -1.0f) bb = -1.0f;
            a = ((a + (a * a * a) * -0.333333343f) * 2.0f) / 3.0f;
            bb = ((bb + (bb * bb * bb) * -0.333333343f) * 2.0f) / 3.0f;
            float cl = 1.0f, cr = 1.0f;
            if (!(envL < 0.159999996f)) {
                float t = (envL + -0.159999996f) / (c.k[4] * 0.200000003f + 1.0f);
                if (t >= 1.00000012f) t = -1.0f; else if (t < 0.0f) t = 0.0f; else t = (1.0f - t) + -1.0f;
                cl = cl + t * 0.100000001f;
            }
            if (!(envR < 0.159999996f)) {
                float t = (envR + -0.159999996f) / (c.k[4] * 0.200000003f + 1.0f);
                if (t >= 1.00000012f) t = -1.0f; else if (t < 0.0f) t = 0.0f; else t = (1.0f - t) + -1.0f;
                cr = cr + t * 0.100000001f;
            }
            float yl = hyst(prevL, hcL, a, c.k[2]) * cl;
            float yr = hyst(prevR, hcR, bb, c.k[2]) * cr;
            hystN = (uint32_t)(int32_t)(c.k[2] * -1900.0f + 2000.0f);
            dl0.write(yl); dl1.write(yr);
            uint32_t rr = xo();
            int32_t nc = nzCnt + 1;
            if (nzCnt >= 0x9c5) { nzTgt = u01(xo()); nc = 0; }
            nzCnt = nc;
            nz = nz + (nzTgt - nz) * 0.00100000005f;
            float p2 = c.k[2];
            float w2 = p2 >= 0.50000006f ? 1.0f : (p2 < 0.0f ? 0.0f : p2 * 2.0f);
            w2 = w2 * w2;
            float wowAmt = w2 * 0.200000003f + 0.0270000007f;
            mod = ((o3.s + 1.0f) * wowAmt) * nz;
            float flAmt = p2 >= 1.00000012f ? 1.0f : (p2 < 0.300000012f ? 0.0f : (p2 + -0.300000012f) / 0.699999988f);
            float rs = r1.tick();
            float scr = p2 >= 0.100000009f ? 0.112000003f : (p2 < 0.00999999978f ? 0.0111999996f : p2 * 1.12f);
            o4.tick(); float o4s = o4.s;
            o0.tick(); o1.tick(); o2.tick(); o3.tick();
            float p1 = c.k[1];
            float lw = p1 >= 0.100000009f ? 1.0f : (p1 < 0.0f ? 0.0f : p1 * 10.0f);
            float wsum = (1.0f / (p1 + 0.100000001f)) * lw;
            wsum = wsum + (1.0f - lw) * 0.0f;
            float hw = p1;
            if (hw >= 1.00000012f) hw = 0.99999994f; else if (hw < 0.600000024f) hw = 0.0f; else hw = (hw + -0.600000024f) / 0.400000006f;
            float depth = hw * 0.600000024f + (1.0f - hw) * wsum;
            if (p1 >= 0.50000006f) depth = depth + 0.5f; else if (!(p1 < 0.0f)) depth = p1 + depth;
            float fl = (o0.s + 1.0f) + (p1 * 0.5f) * (o1.s + 1.0f);
            mod = mod + flAmt * rs;
            mod = mod + scr * o4s;
            mod = mod + fl * depth;
            float p3 = c.k[3];
            float sc = p3 * 0.000400000019f + 9.99999975e-05f;
            int32_t per = (int32_t)(p3 * -50000.0f + 60000.0f);
            int32_t n2 = slowCnt + 1;
            if (per < slowCnt) { slowTgt = u01(xo()); n2 = (int32_t)rr % per; }
            slowCnt = n2;
            slow = slow + (slowTgt - slow) * sc;
            float rsm = r0.tick();
            float d = r0.raw() - rsm;
            if (d >= 0.300000012f) {
                float rv = r0.raw();
                if (dropRef != rv && drop >= 0.999000072f) {
                    dropOn = 1; dropPol = (int32_t)(rr & 1u); dropRef = rv;
                }
            }
            mod = mod + (rsm * 31.25f + slow * 10.0f) * p3;
        }
        // tape read
        float ms0 = (float)dl0.len / 46875.0f * 1000.0f;
        float rl = dl0.read(mod / ms0) * c.gain;
        float ms1 = (float)dl1.len / 46875.0f * 1000.0f;
        float rrv = dl1.read(mod / ms1) * c.gain;
        float out0 = rl, out1 = rrv;
        hfR = hfR + (rrv * gain - hfR) * 0.0799999982f;
        hfL = hfL + (rl * gain - hfL) * 0.0799999982f;
        float p3 = c.k[3];
        float tgt, rate;
        if (dropOn == 0) { rate = 7.20000025e-05f; tgt = 1.0f; }
        else {
            rate = 0.000119999997f;
            if (p3 >= 0.300000042f) tgt = 0.0f; else if (p3 < 0.0f) tgt = 1.0f; else tgt = 1.0f - p3 / 0.300000012f;
        }
        drop = drop + (tgt - drop) * rate;
        if (drop < (0.0399999991f - p3 * 0.0199999996f)) dropOn = 0;
        float p4 = c.k[4];
        float tc = p4 >= 0.75000006f ? -0.207499996f : (p4 < 0.5f ? -0.0f : (p4 + -0.5f) * -0.829999983f);
        float lc = tc * 2.0f + 1.0f;
        float tw = p4 >= 0.420000017f ? 0.419999987f : (p4 < 0.0f ? 0.0f : p4);
        float sl = hfL * tw + (1.0f - tw) * out0;
        toneL = toneL + (sl - toneL) * lc;
        float sr = hfR * tw + (1.0f - tw) * out1;
        toneR = toneR + (sr - toneR) * lc;
        float d9 = drop * 9.0f; d9 = d9 >= 1.00000012f ? 1.0f : (d9 < 0.0f ? 0.0f : d9);
        lpL2 = lpL2 + (toneL - lpL2) * (dropPol ? drop : d9);
        lpR = lpR + (toneR - lpR) * (dropPol ? d9 : drop);
        float m = c.k[0], dry = 1.0f - m;
        outL = c.lim(lpL2 * m + dry * inL);
        outR = c.lim(lpR * m + dry * inR);
    }
};

} // namespace tuman
