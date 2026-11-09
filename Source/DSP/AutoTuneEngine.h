#pragma once
#include <atomic>
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
//  * One shared YIN pitch track on the mono sum  -> both channels get the *same* shift (stable stereo image).
//  * Pitch-synchronous two-tap granular shifter  (window = 2k pitch periods, taps half a window apart).
//  * Unvoiced material (breaths, "s", "t") fades to the delay-matched dry signal.
class AutoTuneEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        line[0].prepare (sr);
        line[1].prepare (sr);
        detector.prepare (sr);
        hopSeconds = (double) PitchDetector::hop * detector.getDecimation() / sr;
        ratioCoef  = 1.0 - std::exp (-1.0 / (0.003 * sr));
        wCoef      = 1.0 - std::exp (-1.0 / (0.120 * sr));
        voiceCoef  = 1.0 - std::exp (-1.0 / (0.010 * sr));
        smoothCoef = 1.0 - std::exp (-1.0 / (0.015 * sr));
        reset();
    }

    void reset()
    {
        line[0].clear(); line[1].clear();
        detector.prepare (sr);
        phase = 0.0; ratioSm = targetRatio = 1.0;
        wSm = targetW = 0.020 * sr;
        voiceSm = targetVoice = 0.0;
        corrSm = 0.0; active = false; haveTarget = false; unvoicedHops = 1000;
        smoothInit = false;
        uiInMidi = -1.0f; uiOutMidi = -1.0f;
    }

    int getLatencySamples() const { return (int) std::lround (0.010 * sr); }

    // right may be nullptr for mono.
    void process (float* left, float* right, int numSamples, const TuneSettings& s)
    {
        cfg = s;
        cfg.noteMask &= 0xFFF;
        if (cfg.noteMask == 0) cfg.noteMask = 0xFFF;
        detector.setRange (cfg.rangeLoHz, cfg.rangeHiHz);
        corrAlpha = cfg.speedMs < 0.5f ? 1.0 : 1.0 - std::exp (-hopSeconds / ((double) cfg.speedMs * 0.001));

        if (! smoothInit) { mixSm = cfg.mix; gainSm = cfg.outputGain; smoothInit = true; }

        const double twoPi = 6.283185307179586;
        const int numCh = right != nullptr ? 2 : 1;

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = left[i];
            const float inR = numCh > 1 ? right[i] : inL;

            if (detector.pushSample (0.5f * (inL + inR))) onAnalysis();

            ratioSm += ratioCoef * (targetRatio - ratioSm);
            wSm     += wCoef     * (targetW     - wSm);
            voiceSm += voiceCoef * (targetVoice - voiceSm);
            mixSm   += smoothCoef * ((double) cfg.mix        - mixSm);
            gainSm  += smoothCoef * ((double) cfg.outputGain - gainSm);

            phase += (1.0 - ratioSm) / wSm;
            phase -= std::floor (phase);
            double ph2 = phase + 0.5;
            ph2 -= std::floor (ph2);

            const float d1 = (float) (phase * wSm + 2.0);
            const float d2 = (float) (ph2   * wSm + 2.0);
            const float dd = (float) (0.5   * wSm + 2.0);
            const float g1 = (float) (0.5 - 0.5 * std::cos (twoPi * phase));
            const float g2 = (float) (0.5 - 0.5 * std::cos (twoPi * ph2));
            const float v = (float) voiceSm, m = (float) mixSm, g = (float) gainSm;

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto& dl = line[ch];
                dl.push (ch == 0 ? inL : inR);
                const float wet  = g1 * dl.read (d1) + g2 * dl.read (d2);
                const float dry  = dl.read (dd);
                const float proc = v * wet + (1.0f - v) * dry;
                (ch == 0 ? left : right)[i] = (dry * (1.0f - m) + proc * m) * g;
            }
        }
    }

    // Read by the editor (message thread). MIDI note numbers relative to refHz = A4 (69); < 0 means "no pitch".
    std::atomic<float> uiInMidi { -1.0f }, uiOutMidi { -1.0f };

private:
    static int pitchClass (int n) { return ((n % 12) + 12) % 12; }

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

    void onAnalysis()
    {
        const auto& r = detector.getResult();

        if (r.voiced)
        {
            unvoicedHops = 0;
            if (! active) { active = true; corrSm = 0.0; haveTarget = false; }

            const double midiIn = 69.0 + 12.0 * std::log2 ((double) r.frequency / (double) cfg.refHz);
            const int target = chooseTarget (midiIn);
            double corr = ((double) target + (double) cfg.detuneSemitones - midiIn) * (double) cfg.amount;
            corr = std::max (-12.0, std::min (12.0, corr));

            corrSm += corrAlpha * (corr - corrSm);
            targetRatio = std::pow (2.0, corrSm / 12.0);

            const double T = 1.0 / (double) r.frequency;
            const int k = std::max (1, (int) std::lround (0.018 / (2.0 * T)));
            targetW = std::max (0.010 * sr, std::min (0.045 * sr, 2.0 * k * T * sr));
            targetVoice = 1.0;

            uiInMidi  = (float) midiIn;
            uiOutMidi = (float) (midiIn + corrSm);
        }
        else if (++unvoicedHops >= 3)          // ~17 ms hangover so brief detector drop-outs don't glitch
        {
            active = false; corrSm = 0.0; targetRatio = 1.0; targetVoice = 0.0;
            uiInMidi = -1.0f; uiOutMidi = -1.0f;
        }
    }

    DelayLine line[2];
    PitchDetector detector;
    TuneSettings cfg;

    double sr = 44100.0, hopSeconds = 0.006, corrAlpha = 1.0;
    double ratioCoef = 0.01, wCoef = 0.001, voiceCoef = 0.01, smoothCoef = 0.01;
    double phase = 0.0, ratioSm = 1.0, targetRatio = 1.0, wSm = 882.0, targetW = 882.0;
    double voiceSm = 0.0, targetVoice = 0.0, corrSm = 0.0, mixSm = 1.0, gainSm = 1.0;
    int    curTarget = 0, unvoicedHops = 1000;
    bool   active = false, haveTarget = false, smoothInit = false;
};
