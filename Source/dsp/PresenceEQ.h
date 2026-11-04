#pragma once
#include "Biquad.h"
#include <algorithm>

namespace vc {

// "Presence": an SSL E-series style high-mid boost, fixed at 1.5 kHz.
//
// The E-series parametric mid bands are a logarithmically symmetric bell whose
// +/-3 dB points keep the same musical interval from the centre regardless of
// gain (SSL's own description), i.e. constant Q, unlike the proportional-Q
// Pultec-style bells elsewhere in the chain. Width fixed at the Q pot's centre
// detent (~1.0 on the 0.5-2.5 range). Boost only, 0 dB by default.
class PresenceEQ
{
public:
    void prepare (double sampleRate) { fs = sampleRate; setBoostDb (boostDb); }
    void reset() { bell.reset(); }

    void setBoostDb (float db)
    {
        boostDb = std::clamp (db, 0.0f, kMaxDb);
        bell.setPeakAnalogMatched (fs, kFreq, boostDb, kQ);
    }

    inline float process (float x) { return bell.process (x); }

    static constexpr double kFreq  = 1500.0;
    static constexpr double kQ     = 1.0;     // E-series Q detent
    static constexpr float  kMaxDb = 12.0f;

private:
    double fs = 44100;
    float boostDb = 0.0f;
    Biquad bell;
};

} // namespace vc
