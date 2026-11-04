#pragma once
#include "Biquad.h"
#include <algorithm>

namespace vc {

// Pultec EQP-1A program equaliser, modelled on the passive network.
//
// Low section (EQP-1A schematic; turnover measurements from the GroupDIY
// analysis of the hardware):
//   * BOOST is a first-order RC shelf. At the "100" setting its upper turnover
//     is ~700 Hz and it reaches ~13 dB at full boost. Because the network is a
//     series element against a divider, lowering the boost pot keeps the upper
//     turnover and raises the lower one (lower knee = upper knee / gain).
//   * ATTEN is a separate first-order shelf whose upper turnover sits much
//     higher: ~1.5 kHz at the "100" setting, ~17 dB at full cut.
//   The two overlap imperfectly; that is the whole "boost and cut together"
//   trick: net lift below the boost's lower knee, net dip across the band where
//   the cut is still working and the boost has already fallen away.
//
// High boost: a series LC resonance across the signal with the BANDWIDTH pot
//   as series resistance. More resistance = lower Q = broader bell. The boost
//   pot also loads the resonator, so the bell tightens as boost increases
//   (proportional Q). Fixed at 10 kHz, bandwidth at the panel default.
//
// The passive network is followed by the tube make-up stage (restores the
// ~16 dB insertion loss; we keep gain at unity and only model its colour).
class PultecHighBoost
{
public:
    void prepare (double sampleRate) { fs = sampleRate; update(); }
    void reset() { bell.reset(); }

    void set (double freqHz, float bandwidthKnob, float boostKnob) { freq = freqHz; bw = bandwidthKnob; boost = boostKnob; useDb = false; update(); }
    void setBoostDb (double freqHz, float bandwidthKnob, float db) { freq = freqHz; bw = bandwidthKnob; boostDbOverride = db; useDb = true; update(); }

    inline float process (float x) { return bell.process (x); }
    float currentGainDb() const { return gainDb; }
    float currentQ() const { return q; }

    // Panel 0..10 -> dB. Hardware reaches ~18 dB at 10; pot is audio-taper-ish.
    static float knobToDb (float knob) { knob = std::clamp (knob, 0.0f, 10.0f); return 18.0f * std::pow (knob / 10.0f, 1.25f); }

private:
    void update()
    {
        gainDb = useDb ? boostDbOverride : knobToDb (boost);
        // Series R from the bandwidth pot: 0 = sharp (Q ~1.6), 10 = broad (Q ~0.35)
        float qBase = 0.35f + (1.6f - 0.35f) * std::pow (1.0f - bw / 10.0f, 1.5f);
        // Boost pot loads the resonator: Q rises with boost
        q = qBase * (1.0f + gainDb / 12.0f);
        if (freq >= 12000.0) q *= 1.15f;   // top settings use a smaller L; narrower
        if (gainDb < 0.01f) bell.setPeak (fs, freq, 0.0, 1.0);
        else                bell.setPeakAnalogMatched (fs, freq, gainDb, q);
    }

    double fs = 44100, freq = 10000;
    float bw = 5, boost = 0, boostDbOverride = 0, gainDb = 0, q = 0.7f;
    bool useDb = false;
    Biquad bell;
};

class PassiveEQ
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        high.prepare (fs);
        updateLow();
        high.set (kHighFreq, kBandwidthDefault, highKnob);
        reset();
    }
    void reset() { boostShelf.reset(); attenShelf.reset(); high.reset(); }

    void setLow (float boostKnob, float attenKnob) { lowBoost = boostKnob; lowAtten = attenKnob; updateLow(); }
    void setHigh (float boostKnob) { highKnob = boostKnob; high.set (kHighFreq, kBandwidthDefault, highKnob); }

    inline float process (float x)
    {
        float y = boostShelf.process (x);
        y = attenShelf.process (y);
        y = high.process (y);
        return y + kTubeH2 * y * std::fabs (y);   // make-up stage: a whisper of even harmonic
    }

    // Panel tapers. Hardware maxima from the turnover measurements: ~13 dB boost, ~17 dB cut.
    static float lowBoostKnobToDb (float k) { return 13.0f * std::pow (std::clamp (k, 0.0f, 10.0f) / 10.0f, 1.0f); }
    static float lowAttenKnobToDb (float k) { return 17.0f * std::pow (std::clamp (k, 0.0f, 10.0f) / 10.0f, 1.1f); }

    // "100 Hz" position of the LOW FREQUENCY switch (measured upper turnovers)
    static constexpr double kBoostUpperKneeHz = 700.0;
    static constexpr double kAttenUpperKneeHz = 1500.0;
    static constexpr double kHighFreq         = 10000.0;
    static constexpr float  kBandwidthDefault = 5.0f;
    static constexpr float  kTubeH2           = 0.008f;

private:
    void updateLow()
    {
        float bDb = lowBoostKnobToDb (lowBoost), aDb = lowAttenKnobToDb (lowAtten);
        double gB = std::pow (10.0, bDb / 20.0), gA = std::pow (10.0, aDb / 20.0);
        // boost: DC gain gB, zero at the upper knee, pole at upper/gB
        boostShelf.setFirstOrderShelf (fs, kBoostUpperKneeHz, kBoostUpperKneeHz / std::max (gB, 1.0001));
        // atten: DC gain 1/gA, pole at the upper knee, zero at upper/gA
        attenShelf.setFirstOrderShelf (fs, kAttenUpperKneeHz / std::max (gA, 1.0001), kAttenUpperKneeHz);
    }

    double fs = 44100;
    float lowBoost = 3, lowAtten = 2, highKnob = 3;
    Biquad boostShelf, attenShelf;
    PultecHighBoost high;
};

// "Air": Ozone Vintage EQ high boost at 16 kHz with its Q dial set to the same
// number as the boost. Same resonant-bell model, Q from the dial value.
class AirEQ
{
public:
    void prepare (double sampleRate) { fs = sampleRate; setAirDb (airDb); }
    void reset() { bell.reset(); }

    void setAirDb (float db)
    {
        airDb = std::clamp (db, 0.0f, 10.0f);
        float dial = airDb;                                  // Q dial == boost value
        float sharp = std::pow (dial / 10.0f, 1.5f);
        q = (0.35f + 1.25f * sharp) * (1.0f + airDb / 12.0f);
        if (airDb < 0.01f) bell.setPeak (fs, kFreq, 0.0, 1.0);
        else               bell.setPeakAnalogMatched (fs, kFreq, airDb, q);
    }

    inline float process (float x) { return bell.process (x); }
    float currentQ() const { return q; }

    static constexpr double kFreq = 16000.0;

private:
    double fs = 44100;
    float airDb = 4.0f, q = 0.9f;
    Biquad bell;
};

} // namespace vc
