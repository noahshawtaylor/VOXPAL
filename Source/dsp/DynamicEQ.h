#pragma once
#include "Biquad.h"
#include <algorithm>

namespace vc {

// "Control": a dynamic-EQ band that cuts only when the band gets loud.
// Functionally what a multiband compressor does on one band with the stock
// Pro-MB style ballistics, but done as a dynamic bell so there are no
// crossover phase artefacts and the cut is shaped like the problem.
//
//   sidechain: bandpass around the centre -> peak envelope -> gain computer
//   audio:     bell cut whose depth follows the computed gain reduction
//
// One user control per band: Threshold (dB). Lower = more control.
class DynamicBand
{
public:
    struct Design
    {
        double centreHz; double q; float maxCutDb;
        float attackMs; float releaseMs; float ratio; float kneeDb;
    };

    void prepare (double sampleRate, const Design& d)
    {
        fs = sampleRate; design = d;
        // sidechain bandpass (constant skirt gain) built from a peak filter
        // with a big boost, then normalised: cheap and phase-tolerant
        scFilter.setPeak (fs, d.centreHz, 24.0, d.q * 1.2);
        scNorm = 1.0f / dbToGain (24.0f);
        attCoef = coef (d.attackMs);
        relCoef = coef (d.releaseMs);
        cutSmooth = coef (4.0f);
        reset();
        setThresholdDb (thresholdDb);
    }

    void reset() { env = 0; grDb = 0; appliedCutDb = -1; updateCounter = kUpdateEvery; scFilter.reset(); band.reset(); }
    static constexpr int kUpdateEvery = 16;

    void setThresholdDb (float db) { thresholdDb = db; }

    inline float process (float x)
    {
        // --- sidechain: isolate the band, detect
        float sc = std::fabs (scFilter.process (x) * scNorm);
        if (sc > env) env = attCoef * env + (1 - attCoef) * sc;
        else          env = relCoef * env + (1 - relCoef) * sc;

        float lvl = gainToDb (env);
        float over = lvl - thresholdDb;
        float target = 0.0f;
        if (over > -design.kneeDb * 0.5f)
        {
            if (over < design.kneeDb * 0.5f)
            {
                float t = over + design.kneeDb * 0.5f;
                target = (1.0f / design.ratio - 1.0f) * t * t / (2.0f * design.kneeDb);
            }
            else
                target = (1.0f / design.ratio - 1.0f) * over;
        }
        target = std::max (target, -design.maxCutDb);
        grDb = cutSmooth * grDb + (1 - cutSmooth) * target;

        // --- recompute the bell at a fixed rate (every kUpdateEvery samples) and
        //     only when the cut has moved; the matched design is a few trig calls,
        //     and doing it per sample during an attack would spike the CPU.
        if (++updateCounter >= kUpdateEvery)
        {
            updateCounter = 0;
            if (std::fabs (grDb - appliedCutDb) > 0.02f)
            {
                appliedCutDb = grDb;
                // constant-width band: with the analog peak prototype the half-depth
                // bandwidth is sqrt(G)*w0/Q, so scaling Q by sqrt(G) keeps the band
                // edges fixed at every depth (what a multiband band does)
                const double qEff = design.q * std::pow (10.0, appliedCutDb / 40.0);
                band.setPeakAnalogMatched (fs, design.centreHz, appliedCutDb, qEff);
            }
        }
        return band.process (x);
    }

    float getGainReductionDb() const { return -grDb; }

private:
    inline float coef (float ms) const { return (float) std::exp (-1.0 / (ms * 0.001 * fs)); }

    double fs = 44100;
    Design design { 3300, 0.8, 9, 5, 80, 3, 6 };
    Biquad scFilter, band;
    float scNorm = 1, attCoef = 0, relCoef = 0, cutSmooth = 0;
    float env = 0, grDb = 0, appliedCutDb = -1, thresholdDb = -20;
    int updateCounter = 0;
};

class ControlSection
{
public:
    void prepare (double fs)
    {
        // Band edges are the half-depth points of the bell.
        // Harsh: 2.5–5.5 kHz  -> centre = sqrt(2500*5500) = 3708 Hz, Q = 3708/3000 = 1.24
        // Boom : 200–350 Hz   -> centre = sqrt(200*350)   =  265 Hz, Q =  265/150  = 1.76
        harsh.prepare (fs, { 3708.0, 1.24, 10.0f, 4.0f, 90.0f, 3.0f, 6.0f });
        honk.prepare  (fs, {  265.0, 1.76,  8.0f, 8.0f, 120.0f, 3.0f, 6.0f });
    }
    void reset() { harsh.reset(); honk.reset(); }

    void setHarshThresholdDb (float db) { harsh.setThresholdDb (db); }
    void setHonkThresholdDb  (float db) { honk.setThresholdDb (db); }

    inline float process (float x) { return honk.process (harsh.process (x)); }

    float harshGrDb() const { return harsh.getGainReductionDb(); }
    float honkGrDb()  const { return honk.getGainReductionDb(); }

private:
    DynamicBand harsh, honk;
};

} // namespace vc
