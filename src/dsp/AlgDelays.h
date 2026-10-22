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

// Three loop heads playing fixed regions of the shared memory while a write head
// sweeps backwards through all of it (overdub with feedback = P4), so the loops
// slowly renew themselves. Loop lengths follow P1..P3 with a very slow glide.
struct LoopBank {
    float sm[3]{};                  // slowly smoothed length knobs
    float gain[3] = { 0, 0, 0 };    // per-loop duck when its knob is turned down quickly
    int timer[3]{};
    int w = 0, pos[3]{};
    float env = 1.0f;

    static int wrap(int i) { if (i >= 59997) i -= 59997; return i; }
    static int wrap2(int i) { if (i >= 59997) i -= 59997; if (i < 0) i += 59997; return i; }

    void smooth(const Ctx& c, float duckFall) {
        for (int i = 0; i < 3; ++i) {
            float k = c.k[1 + i];
            sm[i] = sm[i] + (sm[i] - k) * -0.000300000014f;
            float d = sm[i] - k;
            if (d >= 0.0700000077f) {
                gain[i] = gain[i] + (gain[i] + duckFall) * -0.00100000005f;
                timer[i] = 35000;
            } else if (timer[i] > 0) {
                --timer[i];
            } else {
                gain[i] = gain[i] + (gain[i] + -1.00010002f) * -0.00100000005f;
            }
        }
    }
    int len(int i) const { return (int32_t)((sm[i] * 59996.0f) * 0.930000007f) + 3600; }

    float head(const Ctx& c, int i, int base) const {
        const int r = pos[i], L = len(i);
        auto rd = [&](int idx) { return (float)c.shared[idx]; };
        if (r < 1800) {
            float f = (float)r / 1800.0f;
            float a = (i == 0) ? rd(r) : rd(wrap(base + r));
            float b = rd(wrap(L + r + base + 1));
            float h = f * 0.5f;
            return b * (0.5f - h) + (h + 0.5f) * a;
        }
        if (L - 1800 < r) {
            float f = (float)(r - L + 1800) / 1800.0f;
            float cc = rd(wrap2(r - L + 1800 - 1801 + base));
            float a = rd(wrap(base + r));
            float h = f * 0.5f;
            return a * (1.0f - h) + h * cc;
        }
        return rd(wrap(base + r));
    }

    void heads(const Ctx& c, float& l1, float& l2, float& l3) const {
        l1 = head(c, 0, 0); l2 = head(c, 1, 19997); l3 = head(c, 2, 39994);
    }
    void advance(Ctx& c) {
        if (w == 0) { c.shared[59997] = c.shared[0]; w = 59996; } else --w;
        for (int i = 0; i < 3; ++i) pos[i] = pos[i] > 0 ? pos[i] - 1 : len(i);
    }
    static int16_t q16(float v) {
        int32_t q = (int32_t)v;
        return (int16_t)(q < -32767 ? -32767 : (q > 32767 ? 32767 : q));
    }
    // returns wet L/R (already /25000); sum = L+R input (gain applied inside)
    void run(Ctx& c, float sum, float& wl, float& wr) {
        float l1, l2, l3; heads(c, l1, l2, l3);
        float mid = (l2 * gain[1]) * 0.75f;
        float L = mid + l1 * gain[0];
        float R = mid + l3 * gain[2];
        wl = L / 25000.0f; wr = R / 25000.0f;
        float old = (float)c.shared[w];
        float x = (sum * 0.5f) * c.gain + (old / 25000.0f) * c.k[4];
        float lvl = std::fabs(x * 1.10000002f + (sum * 0.25f) * c.gain);
        if (env < lvl) env = env + (env - lvl) * -0.100000001f;
        else env = env + (env + -1.0f) * -0.000300000014f;
        c.shared[w] = q16((x / env) * 25000.0f);
        advance(c);
    }
    void reset() { w = 0; pos[0] = pos[1] = pos[2] = 0; env = 1.0f; }
};

struct AlgMicroLoops {
    LoopBank bank;
    void process(Ctx& c, float inL, float inR, float& outL, float& outR) {
        bank.smooth(c, -0.100000001f);
        float wl = 0.0f, wr = 0.0f;
        if (c.state != 2) {
            if (c.state == 3) { bank.reset(); c.state = 4; }
            float sum = inL + inR;
            if (c.tick == 0) hrSkip(c, sum, wl, wr);
            else {
                bank.run(c, hrInput(c, sum), wl, wr);
                hrCommit(c, wl, wr);
            }
        }
        float dry = 1.0f - c.k[0];
        float bump = (dry * c.k[0]) * 1.70000005f + 1.0f;
        outL = c.lim(((wl * c.gain) * c.k[0] + dry * inL) * bump);
        outR = c.lim(((wr * c.gain) * c.k[0] + dry * inR) * bump);
    }
};
// Micro Loops Reverb: the same three loops, but the write head also feeds back a
// chorus-modulated read of the freshly written material (negative feedback = diffusion)
// plus the loop mix itself; the loop output gets its own AGC.
struct AlgMicroLoopsVerb {
    LoopBank bank;
    float inEnv = 1.0f, outEnv = 1.0f, dc = 0.0f;
    float tri = 0.0f, triStep = 4.4e-05f;
    void process(Ctx& c, float inL, float inR, float& outL, float& outR) {
        bank.smooth(c, -0.0399999991f);
        float wl = 0.0f, wr = 0.0f;
        if (c.state != 2) {
            if (c.state == 3) { bank.reset(); outEnv = 1.0f; inEnv = 1.0f; dc = 0.0f; c.state = 4; }
            float sum = inL + inR;
            if (c.tick == 0) hrSkip(c, sum, wl, wr);
            else {
                sum = hrInput(c, sum);
                float l1, l2, l3; bank.heads(c, l1, l2, l3);
                float a = l1 * bank.gain[0], b = l2 * bank.gain[1];
                float mid = b * 0.75f;
                float L = (mid + a) / 25000.0f;
                float cc = l3 * bank.gain[2];
                float R = (mid + cc) / 25000.0f;
                // modulated read just behind the write head
                float pos = (float)bank.w + tri * -100.0f;
                if (pos < 0.0f) pos = (pos + 59996.0f) + 1.0f;
                int32_t i = (int32_t)pos;
                float fr = pos - (float)i;
                float v = (float)c.shared[i] * (1.0f - fr);
                v = v + (float)c.shared[i + 1] * fr;
                float x = (sum * 0.5f) * c.gain;
                float fb = c.k[4];
                float y = x - (v / 25000.0f) * fb;
                float side = ((a - b) + cc) / 25000.0f;
                y = y + (side * fb) * 0.5f;
                dc = dc + (dc - y) * -0.000500000024f;
                y = y - dc;
                float lvl = std::fabs(x + y * 1.10000002f);
                if (inEnv < lvl) inEnv = inEnv + (inEnv - lvl) * -0.100000001f;
                else inEnv = inEnv + (inEnv + -1.0f) * -0.000300000014f;
                c.shared[bank.w] = LoopBank::q16((y / inEnv) * 25000.0f);
                bank.advance(c);
                float t = tri + triStep;
                if (t >= 1.00000012f) triStep = -triStep;
                if (t < 0.0f) { t = 0.0f; triStep = -triStep; }
                tri = t;
                float pk = std::fabs(L), pr = std::fabs(R);
                pk = (pr < pk) ? pk * 1.20000005f : pr * 1.20000005f;
                if (outEnv < pk) outEnv = outEnv + (outEnv - pk) * -0.100000001f;
                else outEnv = outEnv + (outEnv + -1.0f) * -0.000300000014f;
                wl = L / outEnv; wr = R / outEnv;
                hrCommit(c, wl, wr);
            }
        }
        float m = c.k[0], dry = 1.0f - m;
        float bump = (dry * m) * 1.70000005f + 1.0f;
        outL = c.lim(((wl * c.gain) * m + dry * inL) * bump);
        outR = c.lim(((wr * c.gain) * m + dry * inR) * bump);
    }
};

} // namespace tuman
