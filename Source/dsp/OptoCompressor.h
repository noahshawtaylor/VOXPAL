#pragma once
#include "Biquad.h"
#include <algorithm>

namespace vc {

// Opto leveller modelled on the LA-2A circuit and the T4 cell physics.
//
// Circuit (Teletronix LA-2A manual / schematic):
//   input transformer -> T4 optical attenuator (a series resistor with the
//   cell's CdS photoresistor shunting to ground) -> tube gain stage -> output.
//   Sidechain taps the OUTPUT (feedback), passes the R37 high-frequency
//   emphasis network, is amplified by a tube stage whose drive is set by the
//   Peak Reduction pot, and feeds the cell's electroluminescent panel.
//   The Limit/Compress switch changes the sidechain stage's feedback and so its
//   gain law; we are in Compress.
//
// T4 cell physics used here:
//   * EL panel luminance rises roughly with the square of drive voltage above a
//     small turn-on threshold.
//   * CdS photoresistor conductance rises roughly linearly with luminance, so
//     conductance ~ V^2 ->  in the feedback loop the asymptotic slope is 1/(1+2)
//     = 3:1, which is where the LA-2A's "about 3:1" comes from. The knee is the
//     EL turn-on curve, so it is very soft.
//   * The photoresistor has two populations of carriers: a fast one (attack
//     ~10 ms, release ~60 ms) and a slow one that builds with exposure and
//     releases over 0.5–5 s depending on how long and how hard the panel has
//     been lit. That is the manual's "two-stage release" and the memory effect.
//
// User control: Peak Reduction 0..100.
class OptoCompressor
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        fastAtt = coef (kFastAttackMs);
        fastRel = coef (kFastReleaseMs);
        slowAtt = coef (kSlowAttackMs);
        exposureAtt = coef (300.0f);
        exposureRel = coef (6000.0f);
        emphasis.setHighShelf (fs, kEmphasisHz, kEmphasisDb, 0.7);
        reset();
    }

    void reset() { gFast = gSlow = exposure = 0; lastOut = 0; gain = 1; emphasis.reset(); }

    // 0..100 front panel. Sets sidechain drive into the EL panel.
    void setPeakReduction (float pr)
    {
        pr = std::clamp (pr, 0.0f, 100.0f);
        // pot taper: the LA-2A does very little below ~30 and most of its work 50-80
        drive = kDriveMax * std::pow (pr / 100.0f, 2.2f);
    }

    inline float process (float x)
    {
        // --- sidechain: feedback, HF-emphasised, rectified
        const float v = std::fabs (emphasis.process (lastOut)) * drive;

        // --- EL panel: square law above a turn-on threshold
        const float ve = std::max (0.0f, v - kElThreshold);
        const float lum = ve * ve;

        // --- photoresistor conductance: fast and slow carrier populations
        // fast state follows luminance quickly both ways
        if (lum > gFast) gFast = fastAtt * gFast + (1 - fastAtt) * lum;
        else             gFast = fastRel * gFast + (1 - fastRel) * lum;

        // exposure integrates how lit the cell has been; it sets the slow release time
        if (lum > exposure) exposure = exposureAtt * exposure + (1 - exposureAtt) * lum;
        else                exposure = exposureRel * exposure + (1 - exposureRel) * lum;
        const float slowMs = kSlowReleaseMinMs + (kSlowReleaseMaxMs - kSlowReleaseMinMs) * std::min (1.0f, exposure / kExposureFull);
        const float slowRel = coef (slowMs);

        // slow state charges more slowly and holds a fraction of the conductance
        const float slowTarget = lum * kSlowFraction;
        if (slowTarget > gSlow) gSlow = slowAtt * gSlow + (1 - slowAtt) * slowTarget;
        else                    gSlow = slowRel * gSlow + (1 - slowRel) * slowTarget;

        const float conductance = gFast * (1.0f - kSlowFraction) + gSlow;

        // --- shunt divider
        gain = 1.0f / (1.0f + kDividerRatio * conductance);

        float cell = x * gain;

        // --- tube gain stage: 2nd then 3rd harmonic, gentle
        float out = cell + kH2 * cell * cell - kH3 * cell * cell * cell;

        lastOut = out;
        return out * kMakeupLin;
    }

    float getGainReductionDb() const { return -gainToDb (gain); }

    // Hardware time behaviour (LA-2A manual): attack 10 ms; release 60 ms to 50%,
    // then 0.5–5 s for the remainder depending on signal history.
    static constexpr float kFastAttackMs     = 10.0f;
    static constexpr float kFastReleaseMs    = 60.0f;
    static constexpr float kSlowAttackMs     = 120.0f;
    static constexpr float kSlowReleaseMinMs = 500.0f;
    static constexpr float kSlowReleaseMaxMs = 5000.0f;
    static constexpr float kSlowFraction     = 0.5f;    // "to 50 %" in the manual
    static constexpr float kExposureFull     = 2.0f;
    // Sidechain
    static constexpr float kEmphasisHz = 3000.0f;
    static constexpr float kEmphasisDb = 2.0f;         // R37 at its stock position
    static constexpr float kElThreshold = 0.12f;
    static constexpr float kDriveMax = 14.0f;
    static constexpr float kDividerRatio = 40.0f;      // max GR ~ -32 dB
    // Output stage
    static constexpr float kH2 = 0.015f;
    static constexpr float kH3 = 0.004f;
    static inline const float kMakeupLin = dbToGain (4.0f);  // the Gray sits ~+4 dB over unity

private:
    inline float coef (float ms) const { return (float) std::exp (-1.0 / (ms * 0.001 * fs)); }

    double fs = 44100;
    float fastAtt = 0, fastRel = 0, slowAtt = 0, exposureAtt = 0, exposureRel = 0;
    float gFast = 0, gSlow = 0, exposure = 0, lastOut = 0, gain = 1;
    float drive = 1;
    Biquad emphasis;
};

} // namespace vc
