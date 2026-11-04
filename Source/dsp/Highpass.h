#pragma once
#include "Biquad.h"

namespace vc {

// Always-on 100 Hz, 18 dB/oct (3rd-order Butterworth): one biquad (Q = 1.0)
// cascaded with a 1st-order section gives the Butterworth pole placement.
class Highpass
{
public:
    void prepare (double fs)
    {
        bq.setHighpass (fs, kFreq, 1.0);
        op.set (fs, kFreq);
        reset();
    }
    void reset() { bq.reset(); op.reset(); }
    inline float process (float x) { return op.process (bq.process (x)); }

    static constexpr double kFreq = 100.0;
private:
    Biquad bq;
    OnePoleHP op;
};

} // namespace vc
