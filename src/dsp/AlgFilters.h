// Tuman — "master" group: Wavefolder (13) and HPF/LPF (14).
#pragma once
#include "Common.h"

namespace tuman {

struct AlgWavefolder {
    FilterLP lp;
    FoldCurve curve[3];
    int quiet = 0;          // samples of silence (gate timer)
    float gate = 0.0f;      // gate gain applied to the drive
    AlgWavefolder() { for (int i = 0; i < 3; ++i) curve[i].build(kFoldSpecs[i]); }

    void process(Ctx& c, float inL, float inR, float& outL, float& outR) {
        const float mix = c.k[0], p1 = c.k[1], res = c.k[2] * 0.61f, p3 = c.k[3];
        float xl = inL * c.gain, xr = inR * c.gain;
        float wl = xl, wr = xr;
        if (c.state != 2) {
            if (c.state == 3) { c.state = 4; gate = 0.0f; }
            if (std::fabs(inL + inR) < 0.015f) {
                if (quiet < 0xecfa) ++quiet;
                if (quiet >= 0xea61 && c.alt) gate = gate + gate * -0.0001f;
                else gate = gate + (gate - 1.001f) * -0.01f;
            } else {
                quiet = 0;
                gate = gate + (gate - 1.001f) * -0.01f;
            }
            float drive = c.k[4] * gate;
            float w0 = 0.0f, w1, w2 = 0.0f;
            if (p3 < 0.5f) { w1 = p3 * 2.0f; w0 = 1.0f - w1; }
            else { w2 = (p3 - 0.5f) * 2.0f; w1 = 1.0f - w2; }
            float g = drive * 20.0f + 1.0f;   // 1 + 20 * drive
            float gl = g * xl, gr = g * xr;
            float yl = curve[0](gl) * w0; yl = yl + curve[1](gl) * w1; yl = yl + curve[2](gl) * w2;
            float yr0 = curve[0](gr) * w0, yr1 = curve[1](gr) * w1, yr2 = curve[2](gr) * w2;
            float cut = p1 * p1 * 0.5f;
            wl = lp(0, yl, cut, res) * 0.6f;
            wr = lp(1, (yr0 + yr1) + yr2, cut, res) * 0.6f;
        }
        float dry = 1.0f - mix;
        outL = c.lim(wl * c.gain * mix + dry * inL);
        outR = c.lim(wr * c.gain * mix + dry * inR);
    }
};

struct AlgFilter {
    FilterHP hp;
    FilterLP lp;
    void process(Ctx& c, float inL, float inR, float& outL, float& outR) {
        const float mix = c.k[0];
        float hpCut = c.k[1] * 0.5f; hpCut = hpCut * hpCut;
        float hpRes = c.k[2] * 0.45f;
        float lpCut = (c.k[3] * 0.4f) * c.k[3];
        float lpRes = c.k[4] * 0.5f;
        float xl = inL * c.gain, xr = inR * c.gain;
        float wl = xl, wr = xr;
        if (c.state != 2) {
            if (c.state == 3) c.state = 4;
            float hl = hp(1, hp(0, xl, hpCut, hpRes), hpCut, hpRes);
            float hr = hp(3, hp(2, xr, hpCut, hpRes), hpCut, hpRes);
            // HP above LP -> parallel, otherwise series
            float par = unitClamp((hpCut - lpCut) * 70.0f);
            if ((hpCut - lpCut) * 70.0f < 0.0f) par = 0.0f;
            float ser = 1.0f - par;
            float ll = lp(1, lp(0, ser * hl + xl * par, lpCut, lpRes), lpCut, lpRes);
            float lr = lp(3, lp(2, ser * hr + xr * par, lpCut, lpRes), lpCut, lpRes);
            wl = ll + hl * par;
            wr = lr + hr * par;
        }
        float dry = 1.0f - mix;
        outL = c.lim(wl * c.gain * mix + dry * inL);
        outR = c.lim(wr * c.gain * mix + dry * inR);
    }
};

} // namespace tuman
