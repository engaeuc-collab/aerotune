// Offline sanity test for the DSP core (no JUCE needed):  g++ -std=c++17 -O2 -I../Source/DSP engine_test.cpp -o engine_test
#include "AutoTuneEngine.h"
#include <cstdio>
#include <vector>
#include <random>
#include <numeric>

static float measure (const std::vector<float>& x, double sr, int from)
{
    PitchDetector pd; pd.prepare (sr);
    std::vector<float> f;
    for (size_t i = 0; i < x.size(); ++i)
        if (pd.pushSample (x[i]) && (int) i > from && pd.getResult().voiced) f.push_back (pd.getResult().frequency);
    if (f.empty()) return 0.0f;
    std::sort (f.begin(), f.end());
    return f[f.size() / 2];
}

static std::vector<float> voice (double f0, double sr, double secs, double vibCents = 0.0, double vibHz = 5.5)
{
    std::vector<float> v ((size_t) (sr * secs));
    double ph = 0;
    for (size_t i = 0; i < v.size(); ++i)
    {
        double f = f0 * std::pow (2.0, vibCents / 1200.0 * std::sin (6.2831853 * vibHz * (double) i / sr));
        ph += 6.2831853 * f / sr;
        double s = 0; for (int k = 1; k <= 8; ++k) s += std::sin (k * ph) / k;
        v[i] = (float) (0.25 * s);
    }
    return v;
}

static void run (const char* name, double sr, double f0, TuneSettings s, double vib = 0.0)
{
    AutoTuneEngine e; e.prepare (sr);
    auto in = voice (f0, sr, 3.0, vib);
    auto L = in, R = in;
    for (size_t i = 0; i < L.size(); i += 256)
        e.process (L.data() + i, R.data() + i, (int) std::min<size_t> (256, L.size() - i), s);
    int from = (int) (sr * 1.2);
    double midi = 69 + 12 * std::log2 (f0 / s.refHz);
    std::printf ("%-34s in %.2f Hz (midi %.2f) -> out L %.2f Hz  R %.2f Hz\n", name, f0, midi, measure (L, sr, from), measure (R, sr, from));
}

int main()
{
    TuneSettings s; s.speedMs = 0;
    run ("chromatic, hard, 44.1k", 44100, 236.0, s);          // expect ~233.08 (A#3)
    run ("chromatic, hard, 48k",   48000, 236.0, s);
    run ("chromatic, hard, 96k",   96000, 236.0, s);
    run ("chromatic, hard, high note", 48000, 523.0, s);       // C5 = 523.25
    run ("chromatic, hard, low note",  48000, 110.0 * 1.02, s);// A2 = 110
    s.noteMask = 0xAB5;   // C major
    run ("C major, 250 Hz (B3 -> expect 246.9)", 48000, 250.0, s);
    run ("C major, 280 Hz (C#4 -> C4 261.6 or D4 293.7)", 48000, 280.0, s);
    s.noteMask = 0xFFF; s.speedMs = 0; s.amount = 0.5f;
    run ("50% amount, 236 -> halfway", 48000, 236.0, s);
    s.amount = 1.0f; s.speedMs = 0;
    run ("vibrato +-40 cents, hard", 48000, 220.0, s, 40.0);
    s.speedMs = 300;
    run ("vibrato +-40 cents, slow (kept)", 48000, 220.0, s, 40.0);

    // noise + silence robustness
    AutoTuneEngine e; e.prepare (48000); TuneSettings t;
    std::mt19937 rng (1); std::uniform_real_distribution<float> u (-0.1f, 0.1f);
    std::vector<float> n (48000), n2; for (auto& x : n) x = u (rng); n2 = n;
    for (size_t i = 0; i < n.size(); i += 128) e.process (n.data() + i, n2.data() + i, 128, t);
    double rIn = 0, rOut = 0; bool bad = false;
    for (size_t i = 24000; i < n.size(); ++i) { rOut += n[i] * n[i]; if (! std::isfinite (n[i])) bad = true; }
    std::printf ("noise: out rms %.4f (in ~0.0577) finite=%d\n", std::sqrt (rOut / 24000), ! bad);
    std::vector<float> z (48000, 0.0f), z2 = z;
    for (size_t i = 0; i < z.size(); i += 128) e.process (z.data() + i, z2.data() + i, 128, t);
    float mx = 0; for (auto x : z) mx = std::max (mx, std::abs (x));
    std::printf ("silence: max |out| %.9f\n", mx);
    return 0;
}
