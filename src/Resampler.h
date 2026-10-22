// Tuman — streaming windowed-sinc sample-rate converter (stereo, lock-step channels).
// Used to run the engine at its native 46875 Hz inside any host rate.
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace tuman {

class SincResampler {
public:
    // inRate -> outRate. zeros = half-width of the kernel in units of the lower rate.
    void prepare(double inRate, double outRate, int zeros = 16) {
        step = inRate / outRate;
        const double ratio = outRate / inRate;
        fc = 0.94 * std::min(1.0, ratio);          // cutoff relative to the input Nyquist
        span = zeros / fc;                          // kernel half-width in input samples
        nz = zeros;
        buildTable();
        for (auto& b : buf) b.assign((size_t)std::ceil(span) * 2 + 8, 0.0f);   // silence history
        base = 0; t = std::ceil(span);              // first output centred after the history
    }
    void push(float l, float r) { buf[0].push_back(l); buf[1].push_back(r); }

    // Produces outputs while enough look-ahead is available.
    template <typename Fn>
    void pull(Fn&& emit) {
        const double avail = (double)(base + (long long)buf[0].size());
        while (t + span + 1.0 < avail) {
            emit(sampleAt(0, t), sampleAt(1, t));
            t += step;
        }
        // drop history we no longer need
        long long keepFrom = (long long)std::floor(t - span) - 2;
        if (keepFrom - base > 4096) {
            size_t drop = (size_t)(keepFrom - base);
            for (auto& b : buf) b.erase(b.begin(), b.begin() + (long)drop);
            base += (long long)drop;
        }
    }

    double latencyInputSamples() const { return std::ceil(span) + span + 1.0; }

private:
    static constexpr int kRes = 512;    // table points per zero crossing
    std::vector<float> table;
    std::vector<float> buf[2];
    long long base = 0;
    double t = 0, step = 1, fc = 1, span = 16;
    int nz = 16;

    static double bessel0(double x) {
        double s = 1, d = 1;
        for (int k = 1; k < 40; ++k) { d *= (x / (2 * k)); d *= (x / (2 * k)); s += d; if (d < 1e-12 * s) break; }
        return s;
    }
    void buildTable() {
        const double beta = 9.0, i0b = bessel0(beta);
        table.assign((size_t)nz * kRes + 2, 0.0f);
        for (size_t i = 0; i < table.size(); ++i) {
            double u = (double)i / kRes;           // in zero crossings
            double sinc = u < 1e-9 ? 1.0 : std::sin(M_PI * u) / (M_PI * u);
            double r = u / nz;
            double win = r >= 1.0 ? 0.0 : bessel0(beta * std::sqrt(1.0 - r * r)) / i0b;
            table[i] = (float)(sinc * win);
        }
    }
    float kernel(double x) const {               // x in input samples
        double u = std::fabs(x) * fc * kRes;
        size_t i = (size_t)u;
        if (i + 1 >= table.size()) return 0.0f;
        float f = (float)(u - (double)i);
        return table[i] + (table[i + 1] - table[i]) * f;
    }
    float sampleAt(int ch, double pos) const {
        const std::vector<float>& bf = buf[ch];
        long long lo = (long long)std::floor(pos - span) + 1;
        long long hi = (long long)std::floor(pos + span);
        double acc = 0.0;
        for (long long n = lo; n <= hi; ++n) {
            long long k = n - base;
            if (k < 0 || k >= (long long)bf.size()) continue;
            acc += (double)bf[(size_t)k] * kernel(pos - (double)n);
        }
        return (float)(acc * fc);
    }
};

// Stands in for the AC-coupled analog output of the hardware: removes the DC
// some algorithms (Old Tape's hysteresis, Wavefolder) leave on the output. ~3 Hz.
struct DcBlock {
    float x1[2] = { 0, 0 }, y1[2] = { 0, 0 };
    float r = 0.9996f;
    void prepare(double rate) { r = (float)(1.0 - 2.0 * 3.14159265358979 * 3.0 / rate); x1[0] = x1[1] = y1[0] = y1[1] = 0; }
    void process(float& l, float& rr) {
        float yl = l - x1[0] + r * y1[0]; x1[0] = l; y1[0] = yl; l = yl;
        float yr = rr - x1[1] + r * y1[1]; x1[1] = rr; y1[1] = yr; rr = yr;
    }
};

} // namespace tuman
