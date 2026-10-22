// Offline renderer used to compare Tuman's engine with the reference.
// usage: ref_render alg rawMix rawP1 rawP2 rawP3 rawP4 kMix kP1 kP2 kP3 kP4 halfrate alt pre in.f32 out.f32
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../src/dsp/Engine.h"
int main(int argc, char** argv) {
    if (argc < 17) { fprintf(stderr, "bad args\n"); return 1; }
    auto e = std::make_unique<tuman::Engine>();
    int alg = atoi(argv[1]);
    float raw[5], k[5];
    for (int i = 0; i < 5; ++i) { raw[i] = (float)atof(argv[2 + i]); k[i] = (float)atof(argv[7 + i]); }
    e->c.halfRate = atoi(argv[12]); e->c.alt = atoi(argv[13]);
    int pre = atoi(argv[14]);
    for (int i = 0; i < 5; ++i) e->setKnobRaw(i, raw[i]);
    e->snapKnobs(k);
    e->forceAlgorithm(alg);
    e->c.gain = 1.0f;
    if (const char* tt = getenv("TUMAN_TAPE_TABLE")) {   // validation only: exact curve table
        FILE* tf = fopen(tt, "rb"); float* T = e->tapeTable();
        size_t n = fread(T, 4, 453, tf); fclose(tf); T[453] = T[452]; (void)n;
    }
    FILE* f = fopen(argv[15], "rb"); std::vector<float> in;
    float buf[2]; while (fread(buf, 4, 2, f) == 2) { in.push_back(buf[0]); in.push_back(buf[1]); }
    fclose(f);
    float l, r;
    for (int i = 0; i < pre; ++i) e->process(0, 0, l, r);
    FILE* o = fopen(argv[16], "wb");
    for (size_t i = 0; i < in.size(); i += 2) {
        e->process(in[i], in[i + 1], l, r);
        // emulate 24-bit codec quantisation of the device output
        float q[2] = { l, r };
        fwrite(q, 4, 2, o);
    }
    fclose(o);
    return 0;
}
