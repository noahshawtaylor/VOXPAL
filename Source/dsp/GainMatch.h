#pragma once
#include "Biquad.h"
#include <algorithm>

namespace vc {

// Loudness-based gain match. Tracks RMS of the (post-highpass) input and the
// processed output with a long window, and applies the dB difference as output
// makeup with a slow ramp. RMS rather than peak so that two compressors in
// series don't make the "matched" output feel quieter than the dry signal.
class GainMatch
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        winCoef  = (float) std::exp (-1.0 / (kWindowMs * 0.001 * fs));
        rampCoef = (float) std::exp (-1.0 / (kRampMs * 0.001 * fs));
        reset();
    }
    void reset() { inMs = outMs = 1.0e-8f; makeupDb = 0; makeupLin = 1; }

    void setEnabled (bool e) { enabled = e; }
    bool isEnabled() const { return enabled; }

    // Call once per sample with the reference (dry) sample and the processed sample.
    inline void analyse (float ref, float out)
    {
        inMs  = winCoef * inMs  + (1 - winCoef) * ref * ref;
        outMs = winCoef * outMs + (1 - winCoef) * out * out;
        // Below the noise floor (a gap between phrases) there is nothing to
        // measure, so HOLD the last good makeup instead of drifting toward 0.
        // Otherwise the first word after a gap would get a wrong, moving gain.
        float target;
        if (! enabled)                        target = 0.0f;
        else if (inMs > kFloor && outMs > kFloor) target = std::clamp (10.0f * std::log10 (inMs / outMs), -kMaxDb, kMaxDb);
        else                                  target = makeupDb;
        makeupDb  = rampCoef * makeupDb + (1 - rampCoef) * target;
        makeupLin = dbToGain (makeupDb);
    }

    inline float apply (float out) const { return out * makeupLin; }
    float currentMakeupDb() const { return makeupDb; }

    static constexpr float kWindowMs = 1500.0f;
    static constexpr float kRampMs   = 300.0f;
    static constexpr float kMaxDb    = 24.0f;
    static constexpr float kFloor    = 1.0e-6f; // ~ -60 dBFS RMS: below this, hold

private:
    double fs = 44100;
    float winCoef = 0, rampCoef = 0;
    float inMs = 0, outMs = 0, makeupDb = 0, makeupLin = 1;
    bool enabled = false;
};

// Sweet-spot indicator: rolling average of GR over a short window and a
// three-state verdict. 0 = under, 1 = in the zone, 2 = over.
class SweetSpot
{
public:
    void prepare (double sampleRate, float lowDb, float highDb)
    {
        lo = lowDb; hi = highDb;
        coef = (float) std::exp (-1.0 / (kWindowMs * 0.001 * sampleRate));
        avg = 0;
    }
    inline void push (float grDb) { avg = coef * avg + (1 - coef) * grDb; }
    float averageDb() const { return avg; }
    int state() const { return avg < lo ? 0 : (avg > hi ? 2 : 1); }

    static constexpr float kWindowMs = 400.0f;
private:
    float lo = 2, hi = 5, coef = 0, avg = 0;
};

} // namespace vc
