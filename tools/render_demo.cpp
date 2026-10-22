// Offline demo renderer: plays a short synthesized phrase through every algorithm
// (with the factory-preset settings) and writes 48 kHz / 24-bit WAV files.
// usage: tuman_render <outdir> [input.wav]   (input: 16/24-bit PCM or 32-bit float WAV)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <memory>
#include "../src/dsp/Engine.h"
#include "../src/Resampler.h"

struct Setting { const char* file; int alg; float mix, p1, p2, p3, p4; bool longMode; };
static const Setting kSettings[] = {
    { "00_lush_hall",          0, 0.45f, 0.55f, 0.30f, 0.70f, 0.72f, false },
    { "01_infinity_endless",   1, 0.60f, 0.20f, 0.45f, 0.20f, 1.00f, false },
    { "02_generative_singing", 2, 0.55f, 0.20f, 0.50f, 0.75f, 0.80f, false },
    { "03_shimmer_up",         3, 0.55f, 0.00f, 0.80f, 0.50f, 0.70f, false },
    { "03_shimmer_down",       3, 0.55f, 0.80f, 0.00f, 0.50f, 0.60f, false },
    { "04_granular_reverse",   4, 0.60f, 0.50f, 0.80f, 0.50f, 0.70f, false },
    { "05_lofi_delay",         5, 0.45f, 0.45f, 0.10f, 0.45f, 0.55f, false },
    { "05_lofi_backwards",     5, 0.55f, 0.50f, 0.95f, 0.60f, 0.50f, false },
    { "06_micro_loops",        6, 0.65f, 0.30f, 0.55f, 0.80f, 0.70f, false },
    { "07_micro_loops_reverb", 7, 0.65f, 0.40f, 0.70f, 0.90f, 0.60f, false },
    { "08_flanger",            8, 0.80f, 0.80f, 0.30f, 0.50f, 0.70f, false },
    { "09_nyquist",            9, 0.70f, 0.60f, 0.70f, 0.40f, 0.00f, false },
    { "10_old_tape",          10, 1.00f, 0.30f, 0.70f, 0.40f, 0.50f, false },
    { "11_pump",              11, 1.00f, 0.50f, 0.30f, 0.60f, 0.70f, false },
    { "12_multiband_drive",   12, 1.00f, 0.50f, 0.50f, 0.40f, 0.50f, false },
    { "13_wavefolder",        13, 1.00f, 0.60f, 0.30f, 0.40f, 0.45f, false },
    { "14_hpf_lpf",           14, 1.00f, 0.25f, 0.50f, 0.70f, 0.40f, false },
    { "01_infinity_long_mode", 1, 0.70f, 0.30f, 0.35f, 0.10f, 0.97f, true  },
};

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
    FILE* f = fopen(path.c_str(), "wb"); if (!f) return;
    uint32_t n = (uint32_t)L.size(), data = n * 6;
    auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f); w32(36 + data); fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(1); w16(2); w32(sr); w32(sr * 6); w16(6); w16(24);
    fwrite("data", 1, 4, f); w32(data);
    for (uint32_t i = 0; i < n; ++i)
        for (float v : { L[i], R[i] }) {
            int32_t q = (int32_t)std::lround(std::max(-1.0f, std::min(1.0f, v)) * 8388607.0f);
            uint8_t b[3] = { (uint8_t)(q & 0xff), (uint8_t)((q >> 8) & 0xff), (uint8_t)((q >> 16) & 0xff) };
            fwrite(b, 1, 3, f);
        }
    fclose(f);
}

static bool readWav(const char* path, std::vector<float>& L, std::vector<float>& R, int& sr) {
    FILE* f = fopen(path, "rb"); if (!f) return false;
    std::vector<uint8_t> d; uint8_t buf[65536]; size_t k;
    while ((k = fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + k);
    fclose(f);
    if (d.size() < 44 || memcmp(d.data(), "RIFF", 4) || memcmp(d.data() + 8, "WAVE", 4)) return false;
    int ch = 0, bits = 0, fmt = 0; size_t pos = 12;
    while (pos + 8 <= d.size()) {
        uint32_t len; memcpy(&len, d.data() + pos + 4, 4);
        const uint8_t* c = d.data() + pos + 8;
        if (!memcmp(d.data() + pos, "fmt ", 4)) {
            uint16_t a, b, bb; uint32_t s;
            memcpy(&a, c, 2); memcpy(&b, c + 2, 2); memcpy(&s, c + 4, 4); memcpy(&bb, c + 14, 2);
            fmt = a; ch = b; sr = (int)s; bits = bb;
        } else if (!memcmp(d.data() + pos, "data", 4)) {
            int bps = bits / 8; size_t frames = len / (size_t)(bps * ch);
            for (size_t i = 0; i < frames; ++i) {
                float v[2] = { 0, 0 };
                for (int cc = 0; cc < ch && cc < 2; ++cc) {
                    const uint8_t* p = c + (i * ch + cc) * bps;
                    if (fmt == 3 && bits == 32) { float x; memcpy(&x, p, 4); v[cc] = x; }
                    else if (bits == 16) { int16_t x; memcpy(&x, p, 2); v[cc] = x / 32768.0f; }
                    else if (bits == 24) { int32_t x = (p[0] | (p[1] << 8) | (p[2] << 16)); if (x & 0x800000) x -= 0x1000000; v[cc] = x / 8388608.0f; }
                }
                L.push_back(v[0]); R.push_back(ch > 1 ? v[1] : v[0]);
            }
            return true;
        }
        pos += 8 + len + (len & 1);
    }
    return false;
}

// a little phrase: plucked A-minor arpeggio, a bass note and a few soft clicks, then silence
static void makePhrase(std::vector<float>& L, std::vector<float>& R, int sr) {
    const double secs = 10.0; size_t n = (size_t)(secs * sr);
    L.assign(n, 0.0f); R.assign(n, 0.0f);
    const double notes[] = { 220.0, 261.63, 329.63, 440.0, 392.0, 329.63, 261.63, 196.0 };
    uint32_t seed = 7;
    for (int k = 0; k < 8; ++k) {
        double f = notes[k]; size_t start = (size_t)(k * 0.42 * sr);
        int per = (int)(sr / f); std::vector<float> line(per);
        for (auto& v : line) { seed = seed * 1664525u + 1013904223u; v = ((seed >> 9) / 8388608.0f - 1.0f) * 0.5f; }
        float pan = 0.3f + 0.4f * (k % 2);
        for (size_t i = 0; i < (size_t)(2.5 * sr) && start + i < n; ++i) {
            size_t j = i % per; size_t j2 = (i + 1) % per;
            float v = line[j];
            line[j] = 0.996f * 0.5f * (line[j] + line[j2]);
            L[start + i] += v * (1.0f - pan) * 0.6f;
            R[start + i] += v * pan * 0.6f;
        }
    }
    for (size_t i = 0; i < (size_t)(3.4 * sr); ++i) {
        double t = (double)i / sr;
        float v = (float)(0.25 * std::sin(2 * M_PI * 55.0 * t) * std::exp(-t * 0.8));
        L[i] += v; R[i] += v;
    }
    for (int c = 0; c < 4; ++c) {
        size_t s = (size_t)((0.21 + c * 0.84) * sr);
        for (int i = 0; i < 300; ++i) { float v = 0.5f * std::exp(-i / 40.0f) * ((i % 7) < 3 ? 1.0f : -1.0f); L[s + i] += v; R[s + i] += v; }
    }
}

int main(int argc, char** argv) {
    std::string out = argc > 1 ? argv[1] : ".";
    std::vector<float> L, R; int sr = 48000;
    if (argc > 2) { if (!readWav(argv[2], L, R, sr)) { fprintf(stderr, "cannot read %s\n", argv[2]); return 1; } }
    else makePhrase(L, R, sr);
    { std::vector<float> l2 = L, r2 = R; writeWav(out + "/input.wav", l2, r2, sr); }
    for (const auto& s : kSettings) {
        auto e = std::make_unique<tuman::Engine>();
        float k[5] = { s.mix, s.p1, s.p2, s.p3, s.p4 };
        for (int i = 0; i < 5; ++i) e->setKnob(i, k[i]);
        e->snapKnobs(k);
        e->forceAlgorithm(s.alg);
        e->c.halfRate = s.longMode;
        tuman::SincResampler up, dn; tuman::DcBlock dc; dc.prepare(tuman::kEngineRate);
        up.prepare(sr, tuman::kEngineRate); dn.prepare(tuman::kEngineRate, sr);
        std::vector<float> oL, oR;
        for (size_t i = 0; i < L.size(); ++i) {
            up.push(std::max(-1.0f, std::min(1.0f, L[i])), std::max(-1.0f, std::min(1.0f, R[i])));
            up.pull([&](float a, float b) {
                float x, y; e->process(a, b, x, y); dc.process(x, y); dn.push(x, y);
            });
            dn.pull([&](float a, float b) { oL.push_back(a); oR.push_back(b); });
        }
        writeWav(out + "/" + s.file + ".wav", oL, oR, sr);
        printf("%s\n", s.file);
    }
    return 0;
}
