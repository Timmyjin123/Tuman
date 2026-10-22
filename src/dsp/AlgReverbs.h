// Tuman — reverb group: Lush (0), Infinity (1), Generative (2).
#pragma once
#include "Common.h"

namespace tuman {

// Infinity: a single 16k-sample ring holding a loop of four allpass+delay stages
// (one write pointer, fixed offsets, four slowly swept read taps). The loop output
// feeds back into the input; P4 sets the loop gain up to ~1 (endless), P2 damping,
// P1 the input high-pass, P3 drive + a little extra feedback. A level follower
// normalises the loop gain so self-oscillation stays bounded.
struct AlgInfinity {
    std::vector<float> m = std::vector<float>(16384, 0.0f);
    uint32_t ptr = 0;
    float mod[4] = { 0, 0, 0, 0 }, step[4] = { 0.1f, 0.1f, -0.1f, -0.1f };
    float lp = 0, hp = 0, loop = 0, env = 1;

    float& at(uint32_t i) { return m[i & 0x3fff]; }
    float tap(uint32_t base, float off) {
        int32_t io = (int32_t)off;
        uint32_t r = (uint32_t)(io + (int32_t)base) & 0xffff;
        float fr = off - (float)(uint32_t)(uint16_t)io;
        float v = (1.0f - fr) * at(r);
        return v + at((r + 1) & 0xffff) * fr;
    }

    void process(Ctx& c, float inL, float inR, float& outL, float& outR) {
        const float mix = c.k[0], drive = c.k[3];
        const float hpc = c.k[1] * 0.0700000003f + 0.000500000024f;
        const float lpc = c.k[2] * 0.980000019f + 0.0199999996f;
        float fb = c.k[4] * 0.5f + 0.5f;
        float wl = 0.0f, wr = 0.0f;
        if (c.state != 2) {
            float in = (((inL + inR) * 0.5f) * c.gain) * 0.25f;
            if (c.state == 3) { c.state = 4; lp = hp = loop = 0.0f; env = 1.0f; ptr = 0; }
            if (c.tick == 0) hrSkip(c, in, wl, wr);
            else {
                in = hrInput(c, in);
                float x = loop + in;
                fb = fb + drive * 0.0149999997f;
                float t = x - hp; hp = hp + t * hpc;
                x = x + hp * -0.800000012f;
                float sq = x * x;
                fb = fb / env;
                float d = sq * drive;
                if (x > 0.0f) x = x + d * -0.5f; else x = x + d * 0.5f;
                t = x - lp; lp = lp + t * lpc;
                uint32_t p = ptr;
                // stage 1
                float v = tap(p, mod[0]);
                float a = x * 0.150000006f; a = a + lp * 0.850000024f; a = a + v * 0.5f;
                uint32_t r = p + 0x7db;
                at(r) = a;
                float o = v - a * 0.5f;
                r = (r + 1) & 0xffff;
                float rd = at(r);
                r = r + 0x4a9; at(r) = o; r = (r + 1) & 0xffff;
                // stage 2
                v = tap(r, mod[2]);
                rd = rd * fb;
                r = r + 0x6f7;
                a = rd + in; a = a + v * 0.5f;
                o = v - a * 0.5f;
                at(r) = a; r = (r + 1) & 0xffff;
                rd = at(r); r = r + 0xef7; at(r) = o; r = (r + 1) & 0xffff;
                // stage 3
                v = tap(r, mod[1]);
                rd = rd * fb;
                r = r + 0x35f;
                a = rd + in; a = a + v * 0.5f;
                at(r) = a; r = (r + 1) & 0xffff;
                float rd2 = at(r);
                r = r + 0x8eb;
                o = v - a * 0.5f;
                rd2 = rd2 * fb;
                at(r) = o; r = (r + 1) & 0xffff;
                // stage 4
                v = tap(r, mod[3]);
                r = r + 0x79b;
                a = rd2 + in; a = a + v * 0.5f;
                o = v - a * 0.5f;
                at(r) = a; r = (r + 1) & 0xffff;
                loop = at(r);
                at(r + 0x55d) = o;
                ptr = (ptr + 1) & 0x3fff;
                // output taps
                uint32_t q = p + 0x8a5;
                wl = at(q) * 0.600000024f;
                q = q + 0xc33; float B = at(q);
                q = (q + 0x1511) & 0xffff; float C = at(q);
                q = q + 0x12cc; float D = at(q) * 0.400000006f;
                wl = wl + B * 0.5f;
                wl = wl + C * 0.579999983f;
                wl = wl + D * fb;
                q = p + 0xa95; float E = at(q);
                q = q + 0xeef; float F = at(q);
                q = (q + 0x143f) & 0xffff; float G = at(q);
                q = q + 0x1394; float H = at(q);
                wr = E * 0.600000024f;
                wr = wr + F * 0.550000012f;
                wr = wr + G * 0.400000006f;
                loop = -(loop * fb);
                wr = wr + (H * 0.5f) * fb;
                // sweep the four read offsets (triangles)
                static const float hi[4] = { 53.0000038f, 71.0000076f, 68.0000076f, 85.0000076f };
                static const float dn[4] = { -0.00700999983f, -0.0040426203f, -0.00750999991f, -0.00527000008f };
                static const float up[4] = { 0.00704000005f, 0.00401797798f, 0.00724000018f, 0.00503200013f };
                for (int i = 0; i < 4; ++i) {
                    mod[i] = mod[i] + step[i] * 0.0599999987f;
                    if (mod[i] >= hi[i]) step[i] = dn[i];
                    if (mod[i] < 0.0f) { mod[i] = 0.0f; step[i] = up[i]; }
                }
                float lvl = in * 0.5f;
                lvl = std::fabs((wr < wl) ? lvl + wl * 1.60000002f : lvl + wr * 1.60000002f);
                if (env < lvl) env = env + (env - lvl) * -0.100000001f;
                else env = env + (env + -1.0f) * -0.000300000014f;
                hrCommit(c, wl, wr);
            }
        }
        float dry = 1.0f - mix;
        float bump = (dry * mix) * 1.70000005f + 1.0f;
        float yl = (((wl * c.gain) * mix) * 1.5f) / env;
        outL = c.lim((yl + dry * inL) * bump);
        float yr = (((wr * c.gain) * mix) * 1.5f) / env;
        outR = c.lim((yr + dry * inR) * bump);
    }
};

// Delay line addressed in milliseconds (scale = 46.875 / length), linear interpolation.
template <typename T>
struct MsLine {
    std::vector<T> buf; uint32_t idx = 0, len = 0; float scale = 0;
    void init(uint32_t n, float sc) { if (buf.size() != n) buf.assign(n, T(0)); len = n; scale = sc; }
    float val(uint32_t i) const;
    float read(float ms) const {
        float L = (float)len;
        float x = (ms * L) * scale;
        float d = L + -2.0f;
        if (d >= x) { d = 2.0f; if (x >= 2.0f) d = x; }
        float pos = (float)idx - d;
        if (pos < 0.0f) pos = pos + L;
        if (pos >= L) pos = 0.0f;
        uint32_t i = (uint32_t)pos;
        float fr = pos - (float)i;
        uint32_t n = (i + 1) % len;
        return valMix(i, n, fr);
    }
    float valMix(uint32_t i, uint32_t n, float fr) const;
    void write(T v) { buf[idx] = v; idx = (idx + 1) % len; }
};
template <> inline float MsLine<float>::valMix(uint32_t i, uint32_t n, float fr) const {
    float v = (1.0f - fr) * buf[i];
    return v + buf[n] * fr;
}
template <> inline float MsLine<int16_t>::valMix(uint32_t i, uint32_t n, float fr) const {
    float a = (float)buf[i] * 6.10370189e-05f;
    float v = a * (1.0f - fr);
    float b = (float)buf[n] * 6.10370189e-05f;
    return v + b * fr;
}

// Quadrature ("magic circle") sine oscillator.
struct CircleOsc {
    float s = 1.0f, c = 0.0f, f = 0.0f;
    void tick() {
        float t = s - (c * kPi) * f;
        c = c + (t * kPi) * f;
        float o = t * 1.00000095f;
        if (o >= 1.00000012f) o = 1.0f; else if (o < -1.0f) o = -1.0f;
        s = o;
    }
};

// Lush: input diffusion (multi-tap line -> Hadamard with a cubic-saturated branch),
// four short diffusion lines feeding early reflections, then four long lines
// (stored as 16-bit, base times 15/40/75/100 ms -> 91/131/201/291 ms with P3)
// with a Householder feedback matrix whose mixing coefficients are swept by sine
// LFOs. Delay times wander with a smoothed random walk; P2 sets chorus depth and
// LFO speed, P1 a tilt tone, P4 decay.
struct AlgLush {
    MsLine<float> lin, e[4];
    MsLine<int16_t> late[4];
    CircleOsc osc[4];
    uint32_t lfsrA = 0, lfsrB = 0, rnd = 0, count = 0, period = 1000;
    float rndSmooth = 0.0f, rndCoef = 0.0001f;
    float lpA[4]{}, hpB[4]{};
    float eHpL = 0, eHpR = 0, eLpL = 0, eLpR = 0, p3s = 0, outEnv = 1.0f;
    float tri = 0.0f, triDir = 1.0f;
    const float lateTone[2] = { 0.89f, 0.06f }, earlyTone[2] = { 0.89f, 0.06f };
    bool ready = false;

    void initLines() {
        lin.init(0x7e0, 0.0232514888f);
        e[0].init(0x2c0, 0.0665838048f); e[1].init(0x5ae, 0.0322386511f);
        e[2].init(0x7e0, 0.0232514888f); e[3].init(0xb5b, 0.0161248706f);
        late[0].init(0x110a, 0.0107462173f); late[1].init(0x185c, 0.00751683768f);
        late[2].init(0x252e, 0.00492487894f); late[3].init(0x35a8, 0.00341256545f);
        static const float fr[4] = { 8.53333313e-06f, 8.53333331e-05f, 3.84000032e-06f, 7.47520016e-06f };
        for (int i = 0; i < 4; ++i) { osc[i].s = 1.0f; osc[i].c = 0.0f; osc[i].f = fr[i]; }
        lfsrA = 0xabcde; lfsrB = 0x23456789; period = 1000; rndCoef = 0.0001f;
        ready = true;
    }

    void background(Ctx& c) {
        if (!ready) return;
        float p = c.k[2] * c.k[2];
        osc[0].f = ((((p * 0.150000006f) / 1.0f) + 0.0500000007f) * 2.0f) / 46875.0f;
        osc[1].f = ((((p * 2.0f) / 1.0f) + 1.0f) * 2.0f) / 46875.0f;
        float u = (float)lfsrA;
        float r = u * 2.32830644e-10f;
        if (r < 0.160000011f) period = (uint32_t)(r * 90000.0f);
        else {
            float t = u * 9.31322588e-11f + 0.600000024f;
            period = (uint32_t)((t * 0.720000029f + rndSmooth * 0.200000003f) * 90000.0f);
        }
    }

    static float sat1(float x) {
        if (x >= 1.00000012f) x = 1.0f; else if (x < -1.0f) x = -1.0f;
        return x + (x * x * x) * -0.333333343f;
    }

    void process(Ctx& c, float inL, float inR, float& outL, float& outR) {
        if (!ready) initLines();
        float wl = 0.0f, wr = 0.0f;
        if (c.state == 3) {
            for (int i = 0; i < 4; ++i) lpA[i] = hpB[i] = 0.0f;
            eLpL = 0.0f; initLines();
            eHpR = eLpR = eHpL = p3s = 0.0f; outEnv = 1.0f;
            c.state = 4;
        }
        if (c.state != 2) {
            float sum = inR + inL;
            if (c.tick == 0) hrSkip(c, sum, wl, wr);
            else {
                sum = hrInput(c, sum);
                for (int i = 0; i < 4; ++i) osc[i].tick();
                float t = tri + triDir * 1.70666663e-05f;
                if (t >= 1.00000012f) { triDir = -triDir; t = 2.0f - t; }
                else if (t < 0.0f) { triDir = -triDir; t = -t; }
                tri = t;
                // input diffusion
                float t0 = lin.read(osc[1].s + 9.0f);
                float t1 = lin.read(osc[0].s + 21.0f);
                float t2 = lin.read(osc[1].s + 27.0f);
                float t3 = lin.read(osc[0].s + 41.0f);
                float sa = sat1(-t1);
                float A = t2 + -t3, B = t2 - -t3, C = sa + -t0, D = sa - -t0;
                e[0].write((A + C) * 0.5f);
                e[1].write((B + D) * 0.5f);
                e[2].write((A - C) * 0.5f);
                e[3].write((B - D) * 0.5f);
                // early reflections
                float r7 = e[0].read(7.0f), r13 = e[1].read(13.0f), r25 = e[2].read(25.0f), r45 = e[0].read(45.0f);
                float E = r25 + r45;
                float eR0 = r7 + E, eL0 = r13 + E;
                float depth = c.k[2];
                if (depth >= 0.50000006f) depth = 0.5f; else if (depth < 0.0f) depth = 0.0f;
                float tw = tri * 0.400000006f;
                float q0 = e[0].read(14.0f - tw * depth);
                float q1 = e[1].read(30.0f - tw * depth);
                float q2 = e[2].read(42.0f - tw * depth);
                float q3 = e[3].read(61.0f - tw * depth);
                float s3 = sat1(q3);
                float P = -q2 + q0, Q = -q2 - q0, R = s3 + q1, S = s3 - q1;
                float z[4] = { (P + R) * 0.5f, (Q + S) * 0.5f, (P - R) * 0.5f, (Q - S) * 0.5f };
                float ge = 1.0f - c.k[0];
                if (ge >= 0.50000006f) ge = 1.0f; else if (ge < 0.0f) ge = 0.5f; else ge = ge + 0.5f;
                float eL = ((eL0 + z[0]) + z[3]) * ge;
                float eR = ((eR0 + z[1]) + z[2]) * ge;
                // tilt tone
                float p1 = c.k[1], lo, hi;
                if (p1 >= 0.550000072f) lo = 0.0f; else if (p1 < 0.0f) lo = 1.0f; else lo = 1.0f - p1 * 1.81818175f;
                hi = p1 + -0.449999988f;
                if (hi >= 0.550000072f) hi = 1.0f; else if (hi < 0.0f) hi = 0.0f; else hi = hi * 1.81818175f;
                float hs = hi * hi;
                float cR = hs * earlyTone[1];
                eLpR = eLpR + (eR - eLpR) * cR;
                eHpR = eHpR + ((eLpR - eR) - eHpR) * (1.0f - lo * earlyTone[0]);
                eLpL = eLpL + (eL - eLpL) * (hs * earlyTone[1]);
                eHpL = eHpL + ((eLpL - eL) - eHpL) * (1.0f - lo * earlyTone[0]);
                // late lines
                float rw = c.k[2];
                if (rw >= 1.00000012f) rw = 1.0f; else if (rw < 0.5f) rw = 0.5f;
                float mod = osc[0].c * rw;
                static const float baseA[4] = { 15.0f, 40.0f, 75.0f, 100.0f }, baseB[4] = { 91.0f, 131.0f, 201.0f, 291.0f };
                float l[4];
                uint32_t a = lfsrA, b = lfsrB, cnt = count;
                for (int i = 0; i < 4; ++i) {
                    float ms = baseA[i] + (baseB[i] - baseA[i]) * c.k[3];
                    if (i <= 1) {
                        uint32_t nxt = cnt + 1;
                        if (period < cnt) {
                            uint32_t a1 = (a >> 1) ^ ((a & 1) ? 0xb4bcd35cu : 0u);
                            a = (a1 >> 1) ^ ((a1 & 1) ? 0xb4bcd35cu : 0u);
                            b = (b >> 1) ^ ((b & 1) ? 0x7a5bc2e3u : 0u);
                            rnd = b ^ a;
                            nxt = 0;
                        }
                        float u = (float)rnd * 2.32830644e-10f;
                        rndSmooth = rndSmooth + (u - rndSmooth) * rndCoef;
                        ms = ms - (rndSmooth * 5.0f) * rw;
                        cnt = nxt;
                    }
                    l[i] = late[i].read(mod + ms);
                }
                count = cnt; lfsrA = a; lfsrB = b;
                float m0 = (osc[2].s * 0.400000006f) * osc[2].s + 0.600000024f;
                float m1 = (osc[3].c * 0.699999988f) * osc[3].c + 0.300000012f;
                float mx = c.k[0], extra;
                if (mx >= 1.00000012f) extra = 0.125f; else if (mx < 0.5f) extra = 0.0f; else extra = (mx + -0.5f) * 0.25f;
                float boost = extra / 0.5f + 1.0f;
                float x0 = (l[0] + m0 * l[2]) + m1 * l[3];
                float x1 = (l[1] + (1.0f - m0) * l[2]) + (1.0f - m1) * l[3];
                wl = eHpR + x0 * boost;
                float h = (((l[0] + l[1]) + l[2]) + l[3]) * -0.5f;
                float lh[4] = { l[0] + h, l[1] + h, l[2] + h, l[3] + h };
                p3s = p3s + (c.k[3] - p3s) * 0.00039999999f;
                float gd = (c.k[4] * 0.5f + 0.5f) - std::fabs(c.k[3] - p3s);
                if (gd >= 1.00000012f) gd = 1.0f; else if (gd < 0.0f) gd = 0.0f;
                wr = eHpL + x1 * boost;
                for (int i = 0; i < 4; ++i) {
                    float x = z[i] + lh[i] * gd;
                    lpA[i] = lpA[i] + (x - lpA[i]) * (hs * lateTone[1] + 0.00100000005f);
                    hpB[i] = hpB[i] + ((lpA[i] - x) - hpB[i]) * (1.0f - lo * lateTone[0]);
                    float ab = std::fabs(hpB[i]);
                    float env = 1.0f;
                    if (env < ab) env = env + (env - ab) * -0.100000001f;
                    else env = env + (env + -1.0f) * -0.000300000014f;
                    int32_t q = (int32_t)((hpB[i] / env) * 16383.5f);
                    if (q >= 0x8000) q = 0x7fff; else if (q < -32767) q = -32767;
                    late[i].write((int16_t)q);
                }
                lin.write((sum * 0.5f) * c.gain);
                hrCommit(c, wl, wr);
            }
        }
        float m = c.k[0], dry = 1.0f - m;
        float bump = (dry * m) * 1.66600001f + 1.0f;
        float gw = c.gain * 0.550000012f;
        float ol = bump * ((gw * wl) * m + dry * inL);
        float orr = bump * ((gw * wr) * m + dry * inR);
        float al = std::fabs(ol), ar = std::fabs(orr);
        float pk = (ar < al) ? al * 1.20000005f : ar * 1.20000005f;
        if (outEnv < pk) outEnv = outEnv + (outEnv - pk) * -0.100000001f;
        else outEnv = outEnv + (outEnv + -1.0f) * -0.000300000014f;
        outL = c.lim(ol / outEnv);
        outR = c.lim(orr / outEnv);
    }
};
// Generative: four delay lines (two pairs swept in opposite directions by slow
// triangles) mixed through a lossless-ish 4x4 matrix; every line passes a DC
// blocker (P1), two resonant band-pass "singing" filters (P3) and a low-pass (P2).
// P4 is the decay; with high P3 the filters self-excite and the reverb generates
// its own tones, kept in check by two level followers.
struct AlgGenerative {
    std::vector<float> m = std::vector<float>(16384, 0.0f);
    uint32_t ptr = 0;
    float lpA[4]{}, hpA[4]{}, dc[2]{}, lpo[2]{};
    float sweep[2] = { 0, 0 }, step[2] = { 4.4e-06f, 5.5e-06f };
    float env1 = 1.0f, env2 = 1.0f;
    FilterBP bp;

    float& at(uint32_t i) { return m[i & 0x3fff]; }
    float tap(uint32_t base, float off) {
        int32_t io = (int32_t)off;
        float fr = off - (float)(uint32_t)io;
        uint32_t r = (uint32_t)io + base;
        float v = (1.0f - fr) * at(r);
        return v + at(r + 1) * fr;
    }

    void process(Ctx& c, float inL, float inR, float& outL, float& outR) {
        const float mix = c.k[0];
        const float hpc = (c.k[1] * c.k[1]) * 0.100000001f + 0.000500000024f;
        const float lpc = (c.k[2] * c.k[2]) * 0.99000001f + 0.00999999978f;
        const float res = c.k[3] * 0.600000024f;
        const float inv = 1.0f - c.k[4];
        float in = ((inL + inR) * 0.5f) * c.gain;
        float wl = 0.0f, wr = 0.0f;
        if (c.state != 2) {
            if (c.state == 3) {
                for (int i = 0; i < 4; ++i) { lpA[i] = hpA[i] = 0.0f; }
                dc[0] = dc[1] = lpo[0] = lpo[1] = 0.0f;
                ptr = 0; env1 = env2 = 1.0f; c.state = 4;
            }
            if (c.tick == 0) hrSkip(c, in, wl, wr);
            else {
                in = hrInput(c, in);
                float v1 = tap(ptr, sweep[0] * 1100.0f);
                float v2 = tap(ptr + 0x7d0, sweep[1] * 880.0f);
                float v3 = tap(ptr + 0x143c, (1.0f - sweep[0]) * 1100.0f);
                float v4 = tap(ptr + 0x2508, (1.0f - sweep[1]) * 880.0f);
                float k = (1.0f - inv * inv) / env1;
                float a = v4 + v1 * 0.300000012f;
                float b = v2 + v3 * 0.300000012f;
                float cc = v3 + v4 * 0.300000012f;
                float d = v1 + v2 * 0.300000012f;
                float x = in * 0.5f;
                float y[4];
                y[0] = x + ((cc * -0.300000012f) - d) * k;
                y[1] = ((a * 0.300000012f) - b) * k - x;
                y[2] = x + ((d * 0.300000012f) - cc) * k;
                y[3] = ((b * -0.300000012f) - a) * k - x;
                const float f1 = hpc / 3.14159274f, f2 = lpc / 3.14159274f;
                for (int i = 0; i < 4; ++i) {
                    hpA[i] = hpA[i] + (y[i] - hpA[i]) * hpc;
                    float s = y[i] - hpA[i];
                    s = s + bp(i, s, f1, 0.075000003f) * res;
                    lpA[i] = lpA[i] + (s - lpA[i]) * lpc;
                    y[i] = lpA[i] + (bp(i + 4, s, f2, 0.0549999997f) * res) * 2.0f;
                }
                at(ptr + 0x7d0) = -(y[0] * c.gain);
                at(ptr + 0x143c) = y[1] * c.gain;
                at(ptr + 0x2508) = -(y[2] * c.gain);
                at(ptr + 0x3f0c) = y[3] * c.gain;
                for (int i = 0; i < 2; ++i) {
                    sweep[i] = sweep[i] + step[i];
                    if (sweep[i] >= 1.00000012f) step[i] = -step[i];
                    if (sweep[i] < 0.0f) { step[i] = -step[i]; sweep[i] = 0.0f; }
                }
                float ol = v1 * 0.899999976f; ol = ol + v3 * 0.800000012f;
                float orr = v2 + v4 * 0.800000012f;
                dc[0] = dc[0] + (ol - dc[0]) * hpc;
                ol = ol - dc[0];
                ol = ol + (bp(8, ol * 0.25f, f1, 0.075000003f) * res) * 4.0f;
                lpo[0] = lpo[0] + (ol - lpo[0]) * lpc;
                ol = lpo[0] + (bp(9, ol * 0.25f, f2, 0.0549999997f) * res) * 8.0f;
                dc[1] = dc[1] + (orr - dc[1]) * hpc;
                orr = orr - dc[1];
                orr = orr + (bp(10, orr * 0.25f, f1, 0.075000003f) * res) * 4.0f;
                lpo[1] = lpo[1] + (orr - lpo[1]) * lpc;
                orr = lpo[1] + (bp(11, orr * 0.25f, f2, 0.0549999997f) * res) * 8.0f;
                ptr = (ptr + 1) & 0x3fff;
                float lvl = std::fabs(ol + orr);
                if (env1 < lvl) env1 = env1 + (env1 - lvl) * -0.0500000007f;
                else env1 = env1 + (env1 + -1.0f) * -0.000300000014f;
                float al = std::fabs(ol), ar = std::fabs(orr);
                float pk = (ar < al) ? al * 1.20000005f : ar * 1.20000005f;
                if (env2 < pk) env2 = env2 + (env2 - pk) * -0.00999999978f;
                else env2 = env2 + (env2 + -1.0f) * -0.000300000014f;
                wl = ol / env2; wr = orr / env2;
                hrCommit(c, wl, wr);
            }
        }
        float dry = 1.0f - mix;
        float bump = (dry * mix) * 1.70000005f + 1.0f;
        outL = c.lim(((((wl * c.gain) * mix) * 0.800000012f) + dry * inL) * bump);
        outR = c.lim(((((wr * c.gain) * mix) * 0.800000012f) + dry * inR) * bump);
    }
};

} // namespace tuman
