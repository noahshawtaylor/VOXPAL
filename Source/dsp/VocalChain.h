#pragma once
#include "Highpass.h"
#include "FetCompressor.h"
#include "OptoCompressor.h"
#include "DynamicEQ.h"
#include "PassiveEQ.h"
#include "PresenceEQ.h"
#include "GainMatch.h"

namespace vc {

// The complete chain, mono:
//   HPF -> [ TOM -> SPIKE ]  (this bracket runs oversampled)  -> Control -> EQ -> Presence -> Air
//       -> gain match -> output
//
// The chain is split into three stages so the host wrapper can oversample the
// two nonlinear compressors and run everything else at the base rate.
// Defaults are the house settings.
struct ChainParams
{
    float fetDriveDb      = 0.0f;   // -12..+12
    float optoPeakRed     = 50.0f;  // 0..100
    float boomKnob        = 5.0f;   // 0..10  Control Boom (200–350 Hz), 0 = off
    float harshKnob       = 5.0f;   // 0..10  Control Harsh (2.5–4.5k), 0 = off
    float lowKnob         = 3.0f;   // 0..10  EQ Low: 100 Hz boost = knob, atten follows at 2/3
    float highKnob        = 3.0f;   // 0..10  EQ High: 10k boost
    float presenceDb      = 0.0f;   // 0..12  SSL-style 1.5k boost, off by default
    float airDb           = 4.0f;   // 0..8
    float outputDb        = 0.0f;   // -12..+12
    bool  gainMatch       = false;
    bool  fetOn = true, optoOn = true, controlOn = true, eqOn = true, presenceOn = true, airOn = true;

    bool operator== (const ChainParams& o) const
    {
        return fetDriveDb == o.fetDriveDb && optoPeakRed == o.optoPeakRed && boomKnob == o.boomKnob && harshKnob == o.harshKnob
            && lowKnob == o.lowKnob && highKnob == o.highKnob && presenceDb == o.presenceDb && airDb == o.airDb && outputDb == o.outputDb && gainMatch == o.gainMatch
            && fetOn == o.fetOn && optoOn == o.optoOn && controlOn == o.controlOn && eqOn == o.eqOn && presenceOn == o.presenceOn && airOn == o.airOn;
    }
    bool operator!= (const ChainParams& o) const { return ! (*this == o); }
};

class VocalChain
{
public:
    // baseRate: the host rate. compRate: the rate the compressor stage runs at
    // (baseRate * oversampling factor).
    void prepare (double baseRate, double compRate)
    {
        hpf.prepare (baseRate);
        fet.prepare (compRate); opto.prepare (compRate);
        control.prepare (baseRate); eq.prepare (baseRate); presence.prepare (baseRate); air.prepare (baseRate); match.prepare (baseRate);
        fetSpot.prepare   (baseRate, 2.0f, 5.0f);
        optoSpot.prepare  (baseRate, 3.0f, 5.0f);
        harshSpot.prepare (baseRate, 3.0f, 4.0f);
        honkSpot.prepare  (baseRate, 2.0f, 4.0f);
        applied = params; apply (params, true);
    }

    void reset() { hpf.reset(); fet.reset(); opto.reset(); control.reset(); eq.reset(); presence.reset(); air.reset(); match.reset(); }

    // Recomputes only what changed. Cheap to call often.
    void apply (const ChainParams& p, bool force = false)
    {
        if (force || p.fetDriveDb != applied.fetDriveDb)   fet.setDriveDb (p.fetDriveDb);
        if (force || p.optoPeakRed != applied.optoPeakRed) opto.setPeakReduction (p.optoPeakRed);
        if (force || p.harshKnob != applied.harshKnob)     control.setHarshThresholdDb (-4.8f * p.harshKnob);
        if (force || p.boomKnob != applied.boomKnob)       control.setHonkThresholdDb (-4.8f * p.boomKnob);
        if (force || p.lowKnob != applied.lowKnob)         eq.setLow (p.lowKnob, p.lowKnob * (2.0f / 3.0f));
        if (force || p.highKnob != applied.highKnob)       eq.setHigh (p.highKnob);
        if (force || p.presenceDb != applied.presenceDb)   presence.setBoostDb (p.presenceDb);
        if (force || p.airDb != applied.airDb)             air.setAirDb (p.airDb);
        if (force || p.gainMatch != applied.gainMatch)     match.setEnabled (p.gainMatch);
        if (force || p.outputDb != applied.outputDb)       outLin = dbToGain (p.outputDb);
        params = p; applied = p;
    }

    // Stage 1 (base rate): always-on highpass. Returns the post-HPF "dry" sample
    // which is also the gain-match reference.
    inline float stage1 (float x) { return hpf.process (x); }

    // Stage 2 (oversampled rate): the two compressors.
    inline float stage2 (float x)
    {
        if (params.fetOn)  x = fet.process (x);
        if (params.optoOn) x = opto.process (x);
        return x;
    }

    // Stage 3 (base rate): control, EQ, air, gain match, output, meters.
    inline float stage3 (float dry, float y)
    {
        if (params.controlOn) y = control.process (y);
        if (params.eqOn)       y = eq.process (y);
        if (params.presenceOn) y = presence.process (y);
        if (params.airOn)      y = air.process (y);

        match.analyse (dry, y);
        y = match.apply (y);

        fetSpot.push   (fet.getGainReductionDb());
        optoSpot.push  (opto.getGainReductionDb());
        harshSpot.push (control.harshGrDb());
        honkSpot.push  (control.honkGrDb());
        return y * outLin;
    }

    // Convenience for tests: whole chain at one rate, no oversampling.
    inline float process (float x) { float d = stage1 (x); return stage3 (d, stage2 (d)); }

    float fetGrDb()   const { return fet.getGainReductionDb(); }
    float optoGrDb()  const { return opto.getGainReductionDb(); }
    float harshGrDb() const { return control.harshGrDb(); }
    float honkGrDb()  const { return control.honkGrDb(); }
    int   fetState()   const { return fetSpot.state(); }
    int   optoState()  const { return optoSpot.state(); }
    int   harshState() const { return harshSpot.state(); }
    int   honkState()  const { return honkSpot.state(); }
    float matchDb()    const { return match.currentMakeupDb(); }

    ChainParams params, applied;
    Highpass hpf; FetCompressor fet; OptoCompressor opto; ControlSection control; PassiveEQ eq; PresenceEQ presence; AirEQ air;
    GainMatch match; SweetSpot fetSpot, optoSpot, harshSpot, honkSpot;
    float outLin = 1.0f;
};

} // namespace vc
