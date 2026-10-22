// Tuman — delay group: Lo-Fi Delay (5), Micro Loops (6), Micro Loops Reverb (7).
// All three record into the shared 60000-sample int16 memory (~1.28 s).
#pragma once
#include "Common.h"

namespace tuman {

constexpr int kMem = 59998;   // last usable index of the shared recording memory

// Linear-interpolated read from the shared memory (int16 / 32767), wraps at the end.
inline float memRead(const Ctx& c, float pos) {
    float fr, a, b;
    if (pos >= 0.0f) {
        int32_t i = (int32_t)pos;
        fr = pos - (float)i;
        a = (float)c.shared[i] / 32767.0f;
        b = (float)c.shared[i + 1] / 32767.0f;
    } else {
        fr = pos + 1.0f;
        a = (float)c.shared[59998] / 32767.0f;
        b = (float)c.shared[0] / 32767.0f;
    }
    return (1.0f - fr) * a + b * fr;
}

// Lo-Fi Delay: half-rate tape-ish delay with a forward and a reverse head pair.
struct AlgLoFiDelay {
    float hp1L = 0, lp1L = 0, hp1R = 0, lp1R = 0;   // forward-tap tone filters
    float hp2A = 0, lp2A = 0, hp2B = 0, lp2B = 0;   // reverse-tap tone filters
    float time = 0, env = 1, prevIn = 0;
    float fwdFull = 0, fwdHalf = 0, revA = 0, revB = 0;
    int w = 0, phase = 0;

    void process(Ctx& c, float inL, float inR, float& outL, float& outR) {
        const float p1 = c.k[1], p2 = c.k[2], mix = c.k[0];
        float target = c.k[3] * 0.980000019f + 0.0199999996f;
        float sHalf = fwdHalf, sFull = fwdFull, rB = revB, rA = revA;
        if (c.state != 2) {
            c.shared[59999] = c.shared[0];
            bool readTaps = true;
            if (c.state == 3) {
                c.state = 4; prevIn = 0;
                lp1L = hp1L = lp1R = hp1R = hp2A = lp2A = hp2B = lp2B = 0;
                env = 1.0f; phase = 1; rA = rB = 0; w = kMem;
            } else if (phase == 0) {
                readTaps = false;
            } else {
                if (w <= 0) w = kMem; else --w;
            }
            float tapFull = fwdFull, tapHalf = fwdHalf;   // stale values when not re-read
            if (readTaps) {
                time = time + (time - target) * -0.000500000024f;
                if (time >= 1.00000012f) time = 1.0f;
                float d = time * 59998.0f;
                float wf = (float)w;
                float pos = wf + d;
                if (pos >= 59998.0039f) pos = (pos + -59998.0f) + -1.0f;
                tapFull = memRead(c, pos);
                wf = wf + d * 0.5f;
                if (wf >= 59998.0039f) wf = (wf + -59998.0f) + -1.0f;
                tapHalf = memRead(c, wf);
            }
            // bipolar tone: above centre a high-pass opens, below it a low-pass closes
            float hpc = 0.0f, lpc = 1.0f;
            if (p1 >= 0.550000072f) hpc = (p1 + -0.550000012f) * 0.111110002f;
            else { hp1L *= 0.999000013f; hp1R *= 0.999000013f; hp2A *= 0.999000013f; hp2B *= 0.999000013f; }
            if (p1 < 0.449999988f) lpc = (p1 + -0.449999988f) * 1.89999998f + 1.0f;
            hp1L = hp1L + (tapFull - hp1L) * hpc;
            lp1L = lp1L - ((hp1L - tapFull) + lp1L) * lpc;
            hp1R = hp1R + (tapHalf - hp1R) * hpc;
            lp1R = lp1R - ((hp1R - tapHalf) + lp1R) * lpc;
            float sum = inL + inR;
            float x = (sum * 0.5f) * c.gain;
            x = x + lp1L * c.k[4];
            float lvl = std::fabs(x * 1.10000002f + (sum * 0.25f) * c.gain);
            if (env < lvl) env = env + (env - lvl) * -0.100000001f;
            else env = env + (env + -1.0f) * -0.000300000014f;
            sHalf = lp1R; sFull = lp1L;
            x = x / env;
            float inA = revB, inB = revA;   // stale reverse inputs on the skipped phase
            if (phase == 0) {
                prevIn = x; phase = 1;
            } else {
                float v = ((x + prevIn) * 0.5f) * 32767.0f;
                int32_t q = (int32_t)v; q = q > 32767 ? 32767 : (q < -32768 ? -32768 : q);
                c.shared[w] = (int16_t)q;
                float wf = (float)w;
                float r = 59998.0f - wf;
                float y = memRead(c, r);
                float dd = r - wf;
                float a = std::fabs(dd);
                if (a < 500.0f) y = (a / 500.0f) * y;
                a = std::fabs(dd + 59998.0f);
                if (a < 500.0f) y = (a / 500.0f) * y;
                a = std::fabs(dd + -59998.0f);
                if (a < 500.0f) y = (a / 500.0f) * y;
                inB = y;
                r = r + time * 29999.0f;
                if (r >= 59998.0039f) r = (r + -59998.0f) + -1.0f;
                y = memRead(c, r);
                a = std::fabs(r - wf);
                if (a < 500.0f) y = (a / 500.0f) * y;
                inA = y;
                phase = 0;
            }
            hp2A = hp2A + (inA - hp2A) * hpc;
            lp2A = lp2A - ((hp2A - inA) + lp2A) * lpc;
            hp2B = hp2B + (inB - hp2B) * hpc;
            lp2B = lp2B - ((hp2B - inB) + lp2B) * lpc;
            rB = lp2A; rA = lp2B;
            fwdHalf = sHalf; fwdFull = sFull; revB = rB; revA = rA;
        }
        float comp = 1.75f - std::fabs(0.5f - p2) * 1.5f;
        float dry = 1.0f - mix;
        float bump = (dry * mix) * 1.70000005f + 1.0f;
        float rev = 1.0f - p2;
        float yl = (((rev * sHalf + rB * p2) * comp) * mix) * 0.75f;
        outL = c.lim((yl * c.gain + dry * inL) * bump);
        float yr = (((rev * sFull + rA * p2) * comp) * mix) * 0.75f;
        outR = c.lim((yr * c.gain + dry * inR) * bump);
    }
};

struct AlgMicroLoops { void process(Ctx& c, float l, float r, float& ol, float& orr) { ol = c.lim(l); orr = c.lim(r); } };
struct AlgMicroLoopsVerb { void process(Ctx& c, float l, float r, float& ol, float& orr) { ol = c.lim(l); orr = c.lim(r); } };

} // namespace tuman
