#pragma once
#include "Biquad.h"
#include <algorithm>

namespace vc {

// FET limiter, modelled on the 1176 circuit rather than on a dB-domain compressor.
//
// Circuit it follows (UA 1176LN manual / schematic):
//   input attenuator -> series R with a JFET shunt to ground (the gain cell)
//   -> class-A preamp -> line amp -> output.  The sidechain taps the LINE AMP
//   OUTPUT (feedback), amplifies it, full-wave rectifies it, and charges the
//   detector capacitor through the Attack pot; the Release pot discharges it.
//   The detector voltage pulls the FET gate up from near pinch-off, which lowers
//   its drain-source resistance and shunts signal to ground.
//
// What that buys us:
//   * The knee is the FET's own conductance curve, so it is soft and gets
//     softer at low ratios, exactly like the hardware.
//   * The ratio emerges from the loop. With a conductance law g ~ Vdet^p inside
//     a feedback loop the asymptotic slope is 1/(1+p); the 4:1 button is the
//     sidechain bias/gain combination that puts the cell in a p = 3 region.
//   * Distortion is the JFET's Vds-dependence of Rds (2nd harmonic that rises
//     with gain reduction), largely cancelled by the half-drain-to-gate network
//     of the LN circuit; we keep the residual.
//
// Fixed to: 4:1, Attack "3" (~0.5 ms), Release "7" (50 ms).
// User control: Drive (dB) into the cell, compensated at the output.
class FetCompressor
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        attCoef = (float) std::exp (-1.0 / (kAttackMs * 0.001 * fs));
        relCoef = (float) std::exp (-1.0 / (kReleaseMs * 0.001 * fs));
        reset();
    }

    void reset() { vdet = 0; lastOut = 0; gain = 1; }

    void setDriveDb (float db) { driveLin = dbToGain (db); makeupLin = dbToGain (-db); }

    inline float process (float x)
    {
        const float in = x * driveLin;

        // --- sidechain: feedback from the previous output, full-wave rectified,
        //     attack/release RC on the detector capacitor
        const float rect = std::fabs (lastOut) * kSidechainGain;
        if (rect > vdet) vdet = attCoef * vdet + (1 - attCoef) * rect;
        else             vdet = relCoef * vdet + (1 - relCoef) * rect;

        // --- FET gate: the Q-bias trim parks the gate just below pinch-off, so a
        //     small detector voltage must be exceeded before the channel starts
        //     to conduct (this is the "threshold").
        const float vg = std::max (0.0f, vdet - kBiasOffset);

        // --- JFET ohmic region: channel conductance rises with gate voltage.
        //     g = (vg)^p in normalised units; the shunt divider gives the gain.
        const float g = vg * vg * vg;                       // p = 3  ->  4:1
        gain = 1.0f / (1.0f + kDividerRatio * g);

        // --- gain cell with Rds(Vds) nonlinearity: 2nd harmonic proportional to
        //     signal swing across the FET, scaled by how hard it conducts, after
        //     the LN half-signal cancellation.
        float cell = in * gain;
        const float conduct = 1.0f - gain;                  // 0 = open, 1 = full shunt
        cell += kFetAsym * conduct * cell * std::fabs (cell);

        // --- class-A amp/transformer colour: small symmetric soft clip
        float out = cell / (1.0f + kAmpSoft * cell * cell);

        lastOut = out;
        return out * makeupLin;
    }

    float getGainReductionDb() const { return -gainToDb (gain); }

    // Hardware time constants (1176LN manual: attack 20 us .. 800 us, release 50 ms .. 1.1 s)
    static constexpr float kAttackMs  = 0.5f;    // "3"
    static constexpr float kReleaseMs = 50.0f;   // "7"
    // Loop constants, calibrated so that at 0 dB Drive a -18 dBFS peak gives ~3-4 dB GR
    static constexpr float kSidechainGain = 5.0f;
    static constexpr float kBiasOffset    = 0.20f;
    static constexpr float kDividerRatio  = 40.0f;   // Rs / Rds_on  (max GR ~ -32 dB)
    static constexpr float kFetAsym       = 0.06f;
    static constexpr float kAmpSoft       = 0.03f;

private:
    double fs = 44100;
    float attCoef = 0, relCoef = 0;
    float vdet = 0, lastOut = 0, gain = 1;
    float driveLin = 1, makeupLin = 1;
};

} // namespace vc
