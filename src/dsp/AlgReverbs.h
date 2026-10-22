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

struct AlgLush { void background(Ctx&) {} void process(Ctx& c, float l, float r, float& ol, float& orr) { ol = c.lim(l); orr = c.lim(r); } };
struct AlgGenerative { void process(Ctx& c, float l, float r, float& ol, float& orr) { ol = c.lim(l); orr = c.lim(r); } };

} // namespace tuman
