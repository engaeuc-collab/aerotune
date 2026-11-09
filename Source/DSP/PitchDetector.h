#pragma once
#include <array>
#include <cmath>
#include <algorithm>

// YIN fundamental-frequency estimator running on a decimated (~11 kHz) copy of
// the input so it is cheap enough to run every ~6 ms in the audio thread.
// No allocations, no locks.
class PitchDetector
{
public:
    static constexpr int frameSize = 512;   // decimated samples analysed per hop
    static constexpr int hop       = 64;    // decimated samples between analyses

    struct Result
    {
        float frequency  = 0.0f;
        float confidence = 0.0f;
        bool  voiced     = false;
    };

    void prepare (double sampleRate)
    {
        decimation = std::max (1, (int) std::lround (sampleRate / 11025.0));
        dsr = (float) (sampleRate / decimation);
        ring.fill (0.0f);
        writePos = 0; sinceAnalysis = 0; accum = 0.0f; accCount = 0;
        result = {};
    }

    void setRange (float lo, float hi) { minHz = lo; maxHz = hi; }
    int  getDecimation() const         { return decimation; }
    const Result& getResult() const    { return result; }

    // Feed one host-rate sample. Returns true whenever a new result is ready.
    bool pushSample (float x)
    {
        accum += x;
        if (++accCount < decimation) return false;

        ring[(size_t) writePos] = accum / (float) decimation;
        accum = 0.0f; accCount = 0;
        writePos = (writePos + 1) % frameSize;

        if (++sinceAnalysis < hop) return false;
        sinceAnalysis = 0;
        analyse();
        return true;
    }

private:
    void analyse()
    {
        result = {};

        float mean = 0.0f;
        for (int i = 0; i < frameSize; ++i)
        {
            frame[(size_t) i] = ring[(size_t) ((writePos + i) % frameSize)];
            mean += frame[(size_t) i];
        }
        mean /= (float) frameSize;

        float energy = 0.0f;
        for (auto& s : frame) { s -= mean; energy += s * s; }
        if (std::sqrt (energy / (float) frameSize) < 0.0015f) return;      // gate (~ -56 dBFS)

        const int tauMin = std::max (2, (int) std::floor (dsr / maxHz));
        const int tauMax = std::min ((int) std::ceil (dsr / minHz) + 1, frameSize / 2 - 2);
        const int W      = frameSize - tauMax - 1;

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float sum = 0.0f;
            const float* a = frame.data();
            const float* b = frame.data() + tau;
            for (int j = 0; j < W; ++j) { const float df = a[j] - b[j]; sum += df * df; }
            d[(size_t) tau] = sum;
        }

        // cumulative mean normalised difference
        float run = 0.0f;
        d[0] = 1.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            run += d[(size_t) tau];
            d[(size_t) tau] = run > 0.0f ? d[(size_t) tau] * (float) tau / run : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (d[(size_t) tau] < 0.15f)
            {
                while (tau + 1 <= tauMax && d[(size_t) (tau + 1)] < d[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }

        if (best < 0)
        {
            float m = 1e9f;
            for (int tau = tauMin; tau <= tauMax; ++tau)
                if (d[(size_t) tau] < m) { m = d[(size_t) tau]; best = tau; }
            if (best < 0 || m > 0.25f) return;
        }

        float refined = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float s0 = d[(size_t) (best - 1)], s1 = d[(size_t) best], s2 = d[(size_t) (best + 1)];
            const float denom = s0 + s2 - 2.0f * s1;
            if (denom > 1e-9f) refined += std::max (-1.0f, std::min (1.0f, 0.5f * (s0 - s2) / denom));
        }

        result.frequency  = dsr / refined;
        result.confidence = 1.0f - d[(size_t) best];
        result.voiced     = true;
    }

    std::array<float, frameSize> ring{}, frame{}, d{};
    Result result;
    float  dsr = 11025.0f, minHz = 70.0f, maxHz = 1100.0f, accum = 0.0f;
    int    decimation = 4, writePos = 0, sinceAnalysis = 0, accCount = 0;
};
