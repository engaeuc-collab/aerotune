#pragma once
#include <atomic>
#include <array>
#include <cmath>
#include <algorithm>
#include "PitchDetector.h"
#include "DelayLine.h"

struct TuneSettings
{
    float speedMs         = 20.0f;     // retune speed: 0 = instant ("hard tune")
    float amount          = 1.0f;      // 0..1 correction strength
    float detuneSemitones = 0.0f;
    float refHz           = 440.0f;    // pitch of A4
    float mix             = 1.0f;      // 0..1 dry/wet
    float outputGain      = 1.0f;      // linear
    float rangeLoHz       = 70.0f;
    float rangeHiHz       = 1100.0f;
    int   noteMask        = 0xFFF;     // bit n = pitch class n (C = 0) may be a target
};

// Real-time stereo pitch corrector.
//
//  Pitch tracking : YIN on the mono sum -> 5-point median -> light smoothing, so the correction
//                   amount is steady instead of following frame-to-frame estimation jitter.
//  Shifting       : ONE read head travels through a short delay line at `ratio` x real time.
//                   When it has drifted half a pitch period away from its centre delay it jumps by
//                   exactly one period (repeat or skip), with a short cross-fade between the two
//                   positions. The jump lag is refined by cross-correlation so the splice lines up
//                   sample-accurately. There is never a second overlapping copy of the voice, so no
//                   comb filtering / "doubling"; with ratio == 1 the head never moves and the signal
//                   passes through bit-exact (just delayed).
//  Stereo         : both channels share the pitch track, the head position and the jump decisions.
class AutoTuneEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& l : line) l.prepare (sr);
        detector.prepare (sr);
        hopSeconds = (double) PitchDetector::hop * detector.getDecimation() / sr;
        centreDelay = 0.016 * sr;
        maxDelay    = (double) line[0].capacity() - 16.0;
        ratioCoef   = 1.0 - std::exp (-1.0 / (0.004 * sr));
        smoothCoef  = 1.0 - std::exp (-1.0 / (0.015 * sr));
        reset();
    }

    void reset()
    {
        for (auto& l : line) l.clear();
        detector.prepare (sr);
        dA = dB = centreDelay;
        fading = false; fadePos = fadeLen = 0; cooldown = 0;
        ratioSm = targetRatio = 1.0;
        period = sr / 200.0;
        corrSm = 0.0; midiSm = trkX = 60.0; trkV = 0.0; active = false; haveTarget = false; unvoicedHops = 1000;
        hist.fill (60.0); histPos = 0;
        smoothInit = false;
        uiInMidi = -1.0f; uiOutMidi = -1.0f;
    }

    int getLatencySamples() const { return (int) std::lround (centreDelay); }

    // right may be nullptr for mono.
    void process (float* left, float* right, int numSamples, const TuneSettings& s)
    {
        cfg = s;
        cfg.noteMask &= 0xFFF;
        if (cfg.noteMask == 0) cfg.noteMask = 0xFFF;
        detector.setRange (cfg.rangeLoHz, cfg.rangeHiHz);
        corrAlpha = cfg.speedMs < 0.5f ? 1.0 : 1.0 - std::exp (-hopSeconds / ((double) cfg.speedMs * 0.001));
        if (! smoothInit) { mixSm = cfg.mix; gainSm = cfg.outputGain; smoothInit = true; }

        const int numCh = right != nullptr ? 2 : 1;
        const double minD = 0.0045 * sr + 4.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = left[i];
            const float inR = numCh > 1 ? right[i] : inL;
            const float mono = 0.5f * (inL + inR);
            line[2].push (mono);

            if (detector.pushSample (mono)) onAnalysis();

            ratioSm += ratioCoef  * (targetRatio - ratioSm);
            mixSm   += smoothCoef * ((double) cfg.mix        - mixSm);
            gainSm  += smoothCoef * ((double) cfg.outputGain - gainSm);

            // move the read head; while nothing is being tuned, drift gently back to the centre delay
            double step = 1.0 - ratioSm;
            if (! active) step += std::max (-0.002, std::min (0.002, (centreDelay - dA) * 0.001));
            dA += step;
            if (fading) dB += step;

            if (cooldown > 0) --cooldown;
            if (! fading && cooldown == 0)
            {
                const double hw = 0.5 * period;
                if      (dA < centreDelay - hw) doJump (true);
                else if (dA > centreDelay + hw) doJump (false);
            }
            dA = std::max (minD, std::min (maxDelay, dA));
            dB = std::max (2.0,  std::min (maxDelay, dB));

            float gA = 1.0f, gB = 0.0f;
            if (fading)
            {
                const float t = (float) fadePos / (float) fadeLen;
                gA = t * t * (3.0f - 2.0f * t);
                gB = 1.0f - gA;
                if (++fadePos >= fadeLen) fading = false;
            }

            const float m = (float) mixSm, g = (float) gainSm;
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto& dl = line[ch];
                dl.push (ch == 0 ? inL : inR);
                float wet = gA * dl.read ((float) dA);
                if (gB > 0.0f) wet += gB * dl.read ((float) dB);
                const float dry = dl.read ((float) centreDelay);
                (ch == 0 ? left : right)[i] = (dry * (1.0f - m) + wet * m) * g;
            }
        }
    }

    // Read by the editor (message thread). MIDI note numbers relative to refHz = A4 (69); < 0 means "no pitch".
    std::atomic<float> uiInMidi { -1.0f }, uiOutMidi { -1.0f };

private:
    static int pitchClass (int n) { return ((n % 12) + 12) % 12; }

    // ---- splice: jump the read head one pitch period, aligned by cross-correlation -------------------
    double corrAt (int dr, int lag, bool up, int F, double ea) const
    {
        double num = 0.0, eb = 0.0;
        const int base = up ? dr + lag : dr - lag;
        for (int k = 0; k < F; ++k)
        {
            const double b = line[2].at (base - k);
            num += (double) a[(size_t) k] * b;
            eb  += b * b;
        }
        return num / std::sqrt (ea * eb + 1e-12);
    }

    void doJump (bool up)
    {
        const double T = std::max (period, 8.0);
        const int F = (int) std::max (8.0, std::min ({ 0.5 * T, 0.004 * sr, (double) kMaxFade }));
        const int dr = (int) std::lround (dA);

        int lo = (int) std::floor (0.88 * T), hi = (int) std::ceil (1.12 * T);
        hi = up ? std::min (hi, (int) maxDelay - dr - 4) : std::min (hi, dr - F - 2);

        double tau = T;
        if (hi >= lo && dr - F >= 1)
        {
            double ea = 0.0;
            for (int k = 0; k < F; ++k) { a[(size_t) k] = line[2].at (dr - k); ea += (double) a[(size_t) k] * a[(size_t) k]; }

            int best = lo; double bestC = -2.0;
            for (int lag = lo; lag <= hi; ++lag)
            {
                const double c = corrAt (dr, lag, up, F, ea);
                if (c > bestC) { bestC = c; best = lag; }
            }
            tau = best;
            if (best > lo && best < hi)   // parabolic refinement -> fractional lag
            {
                const double cm = corrAt (dr, best - 1, up, F, ea), cp = corrAt (dr, best + 1, up, F, ea);
                const double den = cm - 2.0 * bestC + cp;
                if (den < -1e-12) tau += std::max (-0.5, std::min (0.5, 0.5 * (cm - cp) / den));
            }
        }

        const double dNew = up ? dA + tau : dA - tau;
        if (dNew - F < 2.0 || dNew > maxDelay) { cooldown = F; return; }

        dB = dA; dA = dNew;
        fadeLen = F; fadePos = 0; fading = true;
    }

    // ---- note targeting -----------------------------------------------------------------------------
    int chooseTarget (double midiIn)
    {
        const int centre = (int) std::lround (midiIn);
        int best = centre; double bestD = 1e9;
        for (int k = -6; k <= 6; ++k)
        {
            const int n = centre + k;
            if (! ((cfg.noteMask >> pitchClass (n)) & 1)) continue;
            const double dist = std::abs ((double) n - midiIn);
            if (dist < bestD) { bestD = dist; best = n; }
        }
        // hysteresis: don't flip to a neighbouring note until clearly past the boundary
        if (haveTarget && ((cfg.noteMask >> pitchClass (curTarget)) & 1)
            && std::abs ((double) curTarget - midiIn) <= bestD + 0.2)
            best = curTarget;

        curTarget = best; haveTarget = true;
        return best;
    }

    static double median5 (const std::array<double, 5>& h)
    {
        auto t = h;
        std::sort (t.begin(), t.end());
        return t[2];
    }

    void onAnalysis()
    {
        const auto& r = detector.getResult();

        if (r.voiced)
        {
            unvoicedHops = 0;
            const double midiRaw = 69.0 + 12.0 * std::log2 ((double) r.frequency / (double) cfg.refHz);

            if (! active)
            {
                active = true; haveTarget = false; corrSm = 0.0;
                hist.fill (midiRaw); histPos = 0; midiSm = trkX = midiRaw; trkV = 0.0;
            }
            else
            {
                hist[(size_t) histPos] = midiRaw;
                histPos = (histPos + 1) % 5;
            }

            // Median gate rejects single-frame octave/estimation glitches. An alpha-beta tracker then removes
            // jitter AND estimates pitch velocity, so we can look ahead by the pipeline's latency; without that
            // lead, the correction arrives ~30 ms late and cannot cancel fast vibrato.
            const double med = median5 (hist);
            const double z   = std::abs (midiRaw - med) < 1.0 ? midiRaw : med;
            const double dt  = hopSeconds;
            if (std::abs (z - (trkX + trkV * dt)) > 1.5)          // genuine leap: follow immediately
            {
                trkX = med; trkV = 0.0;
            }
            else
            {
                const double pred = trkX + trkV * dt, res = z - pred;
                trkX = pred + 0.45 * res;
                trkV += 0.12 * res / dt;
                trkV = std::max (-20.0, std::min (20.0, trkV));    // semitones / second
            }
            midiSm = trkX + trkV * kLeadSeconds;
            const int target = chooseTarget (midiSm);
            double corr = ((double) target + (double) cfg.detuneSemitones - midiSm) * (double) cfg.amount;
            corr = std::max (-12.0, std::min (12.0, corr));
            corrSm += corrAlpha * (corr - corrSm);
            targetRatio = std::pow (2.0, corrSm / 12.0);

            const double f = (double) cfg.refHz * std::pow (2.0, (midiSm - 69.0) / 12.0);
            period = std::max (sr / 1100.0, std::min (0.0143 * sr, sr / f));

            uiInMidi  = (float) trkX;
            uiOutMidi = (float) (trkX + corrSm);
        }
        else if (++unvoicedHops >= 3)          // ~17 ms hangover so brief detector drop-outs don't glitch
        {
            active = false; corrSm = 0.0; targetRatio = 1.0;
            uiInMidi = -1.0f; uiOutMidi = -1.0f;
        }
    }

    static constexpr int    kMaxFade = 1024;
    static constexpr double kLeadSeconds = 0.010;

    DelayLine line[3];                       // L, R, mono (for analysis / splice alignment)
    PitchDetector detector;
    TuneSettings cfg;
    std::array<float, kMaxFade> a {};
    std::array<double, 5> hist {};
    int histPos = 0;

    double sr = 44100.0, hopSeconds = 0.006, corrAlpha = 1.0;
    double centreDelay = 700.0, maxDelay = 4000.0;
    double ratioCoef = 0.01, smoothCoef = 0.01;
    double dA = 700.0, dB = 700.0, period = 220.0;
    double trkX = 60.0, trkV = 0.0;
    double ratioSm = 1.0, targetRatio = 1.0, corrSm = 0.0, midiSm = 60.0, mixSm = 1.0, gainSm = 1.0;
    int    fadePos = 0, fadeLen = 0, cooldown = 0, curTarget = 0, unvoicedHops = 1000;
    bool   fading = false, active = false, haveTarget = false, smoothInit = false;
};
