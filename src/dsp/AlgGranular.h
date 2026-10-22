// Tuman — granular group: Granular Shimmer (3) and Granular Reverb (4).
// Grains are 9361 samples (~200 ms) with triangular windows; four overlapping
// grains per set, alternating signs (+ - + -) for decorrelation. A new random
// start position is drawn when a grain's phase passes zero. Everything is read
// from (and fed back into) the shared 16-bit memory.
#pragma once
#include "Common.h"

namespace tuman {

namespace grain {
inline int wrapNeg(int p) { return p < 0 ? p + 9361 : p; }
inline int wrapIdx(int i) { return i >= 59997 ? i - 59997 : i; }
inline float tri(int p) { return (float)(p < 4681 ? p : 9360 - p) / 4680.0f; }
// distance from the write head on the circular memory, capped at 500
inline int headDist(int idx, int w) {
    int d = idx - w;
    int a = d < 0 ? -d : d;
    int b = d - 59997; b = b < 0 ? -b : b; if (b > a) b = a;
    int c = d + 59997; c = c < 0 ? -c : c; if (c > b) c = b;
    return c > 500 ? 500 : c;
}
inline void refresh(Ctx& c, int phase, int& slot) {
    if (phase == 0) slot = (int32_t)(c.noise.next() * 59996.0f);
}
}

struct AlgShimmer {
    int w = 0, p = 0, p2 = 0, h2 = 0;          // write head, normal phase, 2x phase, half-speed phase
    int slot[24]{};
    float env = 1.0f, lpUp[2]{}, lpDn[2]{};

    // normal-speed grain (sets 1 and 2)
    float g1(Ctx& c, int ph, int& sl) {
        ph = grain::wrapNeg(ph);
        grain::refresh(c, ph, sl);
        float v = (float)c.shared[grain::wrapIdx(ph + sl)];
        return v * grain::tri(ph);
    }
    // double-speed grain with fade near the write head (sets 3 and 4)
    float g2(Ctx& c, int ph, int& sl, float& fade) {
        ph = grain::wrapNeg(ph);
        grain::refresh(c, ph, sl);
        int idx = grain::wrapIdx(ph + sl);
        float v = (float)c.shared[idx] * grain::tri(ph);
        fade = (float)grain::headDist(idx, w) / 500.0f;
        return v;
    }
    // half-speed grain: odd samples average two neighbours (sets 5 and 6)
    float g3(Ctx& c, int ph, int& sl, int parity) {
        ph = grain::wrapNeg(ph);
        grain::refresh(c, ph, sl);
        int idx = grain::wrapIdx(ph + sl);
        float v;
        if (parity == 0) v = (float)c.shared[idx] * grain::tri(ph);
        else {
            int pr = idx - 1; if (pr < 0) pr = 59996;
            float s = (float)c.shared[idx] + (float)c.shared[pr];
            v = (s * 0.5f) * grain::tri(ph);
        }
        float fade = (float)(parity + 2 * grain::headDist(idx, w)) / 1000.0f;
        return fade * v;
    }

    void process(Ctx& c, float inL, float inR, float& outL, float& outR) {
        float wl = 0.0f, wr = 0.0f;
        if (c.state != 2) {
            if (c.state == 3) {
                w = p = p2 = h2 = 0;
                for (int& s : slot) s = 0;
                lpUp[0] = lpUp[1] = lpDn[0] = lpDn[1] = 0.0f; env = 1.0f;
                c.state = 4;
            }
            float sum = inL + inR;
            if (c.tick == 0) hrSkip(c, sum, wl, wr);
            else {
                sum = hrInput(c, sum);
                const float o1 = c.k[1], o2 = c.k[2], fbX = c.k[3], fb = c.k[4];
                // set 1 & 2: original pitch
                float s1 = g1(c, p, slot[0]);
                s1 = s1 - g1(c, p - 2340, slot[1]);
                s1 = s1 + g1(c, p - 4680, slot[2]);
                s1 = s1 - g1(c, p - 7020, slot[3]);
                float s2 = g1(c, p, slot[4]);
                s2 = s2 - g1(c, p - 2340, slot[5]);
                s2 = s2 + g1(c, p - 4680, slot[6]);
                s2 = s2 - g1(c, p - 7020, slot[7]);
                // set 3 & 4: octave up
                float f, v;
                v = g2(c, p2, slot[8], f); float s3 = f * v;
                v = g2(c, p2 - 2340, slot[9], f); s3 = s3 - f * v;
                v = g2(c, p2 - 4680, slot[10], f); s3 = s3 + f * v;
                v = g2(c, p2 - 7020, slot[11], f); s3 = s3 - f * v;
                v = g2(c, p2, slot[12], f); float s4 = f * v;
                v = g2(c, p2 - 2340, slot[13], f); s4 = s4 - f * v;
                v = g2(c, p2 - 4680, slot[14], f); s4 = s4 + f * v;
                v = g2(c, p2 - 7020, slot[15], f); s4 = s4 - f * v;
                float lc = 0.699999988f;
                if (fb >= 0.850000083f) {
                    float a = (o2 + o1) + -1.0f; if (a < 0.0f) a = 0.0f;
                    float b = (o2 + -0.5f) * (1.0f - a * 0.5f); if (b < 0.0f) b = 0.0f;
                    lc = lc + ((fb + -0.850000024f) * b) * -4.0f;
                }
                int h = h2 / 2, par = h2 - 2 * h;
                c.shared[59997] = c.shared[0];
                lpUp[0] = lpUp[0] + (s3 - lpUp[0]) * lc;
                lpUp[1] = lpUp[1] + (s4 - lpUp[1]) * lc;
                // set 5 & 6: octave down
                float s5 = g3(c, h, slot[16], par);
                s5 = s5 - g3(c, h - 2340, slot[17], par);
                s5 = s5 + g3(c, h - 4680, slot[18], par);
                s5 = s5 - g3(c, h - 7020, slot[19], par);
                float s6 = g3(c, h, slot[20], par);
                s6 = s6 - g3(c, h - 2340, slot[21], par);
                s6 = s6 + g3(c, h - 4680, slot[22], par);
                s6 = s6 - g3(c, h - 7020, slot[23], par);
                // mix: P1 octave down, P2 octave up, the rest original
                lpDn[0] = lpDn[0] + (lpDn[0] - s5) * -0.00499999989f;
                lpDn[1] = lpDn[1] + (lpDn[1] - s6) * -0.00499999989f;
                float orig = (1.0f - o2) - o1; if (orig < 0.0f) orig = 0.0f;
                float hiDn0 = s5 - lpDn[0], hiDn1 = s6 - lpDn[1];
                float over = (o2 + o1) + -1.0f; if (over < 0.0f) over = 0.0f;
                float norm = 1.0f - over * 0.5f;
                float shL = lpUp[0] * o2 + hiDn0 * o1;
                float L = s1 * orig + shL * norm;
                float shR = lpUp[1] * o2 + hiDn1 * o1;
                float R = s2 * orig + shR * norm;
                wl = L / 25000.0f; wr = R / 25000.0f;
                // feedback into memory
                float old = (float)c.shared[w] / 25000.0f;
                float back = (1.0f - fbX) * old - ((wl + wr) * 0.620000005f) * fbX;
                back = back * fb;
                float shape = 0.5f - std::fabs(fbX + -0.5f);
                float x = ((sum * 0.5f) * c.gain) + (back * (shape * 0.75f + 1.0f)) * 1.20000005f;
                float lvl = std::fabs(x * 1.10000002f + (sum * 0.25f) * c.gain);
                if (env < lvl) env = env + (env - lvl) * -0.100000001f;
                else env = env + (env + -1.0f) * -0.000300000014f;
                int32_t q = (int32_t)((x / env) * 25000.0f);
                q = q < -32767 ? -32767 : (q > 32767 ? 32767 : q);
                c.shared[w] = (int16_t)q;
                w = w > 0 ? w - 1 : 59996;
                p = p > 0 ? p - 1 : 9360;
                p2 = p2 > 0 ? p2 - 2 : 9360;
                h2 = h2 > 0 ? h2 - 1 : 18721;
                hrCommit(c, wl, wr);
            }
        }
        float m = c.k[0], dry = 1.0f - m;
        float bump = (dry * m) * 1.70000005f + 1.0f;
        outL = c.lim((((wl * c.gain) * m) + dry * inL) * bump);
        outR = c.lim((((wr * c.gain) * m) + dry * inR) * bump);
    }
};

struct AlgGrainVerb { void process(Ctx& c, float l, float r, float& ol, float& orr) { ol = c.lim(l); orr = c.lim(r); } };

} // namespace tuman
