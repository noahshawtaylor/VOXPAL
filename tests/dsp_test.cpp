// Standalone DSP checks (no JUCE needed):  g++ -O2 -std=c++17 tests/dsp_test.cpp -o dsp_test
#include "../Source/dsp/VocalChain.h"
#include <cstdio>
#include <vector>
#include <cmath>

using namespace vc;
static double FS = 48000.0;

template <class F>
static double magDb (F& filter, double freq, int n = 48000)
{
    // steady-state magnitude via sine probe
    double acc = 0; int count = 0;
    for (int i = 0; i < n; ++i)
    {
        float x = (float) std::sin (2 * M_PI * freq * i / FS) * 0.1f;
        float y = filter.process (x);
        if (i > n / 2) { acc += (double) y * y; ++count; }
    }
    double rmsOut = std::sqrt (acc / count), rmsIn = 0.1 / std::sqrt (2.0);
    return 20 * std::log10 (rmsOut / rmsIn);
}

#include <cstdlib>
int main (int argc, char** argv)
{
    if (argc > 1) FS = std::atof (argv[1]);
    std::printf ("=== sample rate %.0f ===\n", FS);
    int fails = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what); if (! ok) ++fails; };

    // ---------------- Highpass: 100 Hz, 18 dB/oct
    {
        Highpass h; h.prepare (FS);
        double d100 = magDb (h, 100), d50 = magDb (h, 50), d25 = magDb (h, 25), d1k = magDb (h, 1000);
        std::printf ("HPF   100Hz %.2f dB | 50Hz %.2f | 25Hz %.2f | 1k %.2f\n", d100, d50, d25, d1k);
        check (std::fabs (d100 + 3.0) < 0.5, "HPF -3 dB at 100 Hz");
        check (std::fabs ((d25 - d50) + 18.0) < 1.5, "HPF slope ~18 dB/oct");
        check (std::fabs (d1k) < 0.1, "HPF flat at 1 kHz");
    }

    // ---------------- Passive EQ: low trick shape and 10k bell at knob 3
    {
        PassiveEQ eq; eq.prepare (FS); eq.setLow (3, 2); eq.setHigh (3);
        double a60 = magDb (eq, 60), a100 = magDb (eq, 100), a250 = magDb (eq, 250), a500 = magDb (eq, 500),
               a2k = magDb (eq, 2000), a10k = magDb (eq, 10000), a16k = magDb (eq, 16000);
        std::printf ("EQ    60:%.2f 100:%.2f 250:%.2f 500:%.2f 2k:%.2f 10k:%.2f 16k:%.2f\n", a60, a100, a250, a500, a2k, a10k, a16k);
        double a1k = magDb (eq, 1000);
        std::printf ("EQ    1k:%.2f\n", a1k);
        check (a100 > 0.5 && a100 > a500, "EQ net lift at 100 Hz");
        check (a1k < 0.0 && a1k < a100 - 1.0, "EQ net dip in the low mids (the Pultec trick)");
        check (a10k > 3.0 && a10k < 6.0, "EQ 10k boost ~4 dB at knob 3");
        check (std::fabs (a2k) < 1.0, "EQ roughly flat at 2k");
    }

    // ---------------- Air: +4 dB at 16k and proportional Q
    {
        AirEQ a; a.prepare (FS); a.setAirDb (4);
        double q4 = a.currentQ();
        double g16 = magDb (a, 16000), g8 = magDb (a, 8000);
        a.setAirDb (8); double q8 = a.currentQ();
        a.setAirDb (1); double q1 = a.currentQ();
        std::printf ("AIR   16k:%.2f dB  8k:%.2f dB   Q@1dB=%.2f Q@4dB=%.2f Q@8dB=%.2f\n", g16, g8, q1, q4, q8);
        check (std::fabs (g16 - 4.0) < 0.6, "Air +4 dB at 16 kHz");
        check (q1 < q4 && q4 < q8, "Air Q narrows as boost increases");
    }

    // ---------------- FET: fast peak catching, ~2-5 dB on a hot burst
    {
        FetCompressor c; c.prepare (FS); c.setDriveDb (0);
        // static curve: steady tones at several levels -> GR; slope above the knee should approach 4:1
        auto grAt = [&] (float peak) { c.reset(); float g = 0; for (int i = 0; i < 24000; ++i) { c.process (peak * (float) std::sin (2 * M_PI * 1000 * i / FS)); g = c.getGainReductionDb(); } return g; };
        float g18 = grAt (dbToGain (-18)), g12 = grAt (dbToGain (-12)), g6 = grAt (dbToGain (-6)), g0 = grAt (1.0f);
        float slopeHi = 1.0f - (g0 - g6) / 6.0f;           // output change per input dB at the top
        std::printf ("FET   GR @ -18:%.2f  -12:%.2f  -6:%.2f  0:%.2f dBFS   top slope 1:%.1f\n", g18, g12, g6, g0, 1.0f / std::max (slopeHi, 0.01f));
        check (g18 > 2.0f && g18 < 6.0f, "FET: 2-5 dB GR on a -18 dBFS vocal peak at 0 dB drive");
        check (1.0f / slopeHi > 3.0f && 1.0f / slopeHi < 5.5f, "FET: slope near 4:1 above the knee");
        float peakGr = 0; int settleSample = -1; int n = (int) FS; c.reset();
        for (int i = 0; i < n; ++i)
        {
            float x = i < n / 2 ? 0.5f * (float) std::sin (2 * M_PI * 1000 * i / FS) : 0.0f;
            c.process (x);
            float gr = c.getGainReductionDb();
            if (i < n / 2) peakGr = std::max (peakGr, gr);
            if (i > n / 2 && settleSample < 0 && gr < 0.5f) settleSample = i - n / 2;
        }
        std::printf ("FET   release to <0.5 dB in %.1f ms\n", settleSample * 1000.0 / FS);
        check (settleSample > 0 && settleSample < 400 * FS / 1000, "FET releases fast (50 ms constant)");
    }

    // ---------------- Opto: two-stage release
    {
        OptoCompressor o; o.prepare (FS); o.setPeakReduction (50);
        auto grAt = [&] (float peak) { o.reset(); float g = 0; for (int i = 0; i < 96000; ++i) { o.process (peak * (float) std::sin (2 * M_PI * 1000 * i / FS)); g = o.getGainReductionDb(); } return g; };
        float o18 = grAt (dbToGain (-18)), o12 = grAt (dbToGain (-12)), o6 = grAt (dbToGain (-6));
        float oslope = 1.0f - (o6 - o12) / 6.0f;
        std::printf ("OPTO  GR @ -18:%.2f  -12:%.2f  -6:%.2f dBFS   slope 1:%.1f\n", o18, o12, o6, 1.0f / std::max (oslope, 0.01f));
        check (o18 > 2.0f && o18 < 6.0f, "Opto: 3-5 dB GR on a -18 dBFS vocal at PR 50");
        check (1.0f / oslope > 2.0f && 1.0f / oslope < 4.5f, "Opto: slope near 3:1");
        o.reset();
        int n = (int) (4 * FS);
        float grEnd = 0, gr100ms = 0, gr1s = 0;
        int offAt = (int) (2 * FS);
        for (int i = 0; i < n; ++i)
        {
            float x = i < offAt ? 0.5f * (float) std::sin (2 * M_PI * 1000 * i / FS) : 0.0f;
            o.process (x);
            if (i == offAt - 1) grEnd = o.getGainReductionDb();
            if (i == offAt + (int) (0.1 * FS)) gr100ms = o.getGainReductionDb();
            if (i == offAt + (int) (1.0 * FS)) gr1s = o.getGainReductionDb();
        }
        std::printf ("OPTO  GR %.2f dB during 2 s tone, %.2f dB 100 ms after, %.2f dB 1 s after\n", grEnd, gr100ms, gr1s);
        check (grEnd > 1.0f, "Opto compresses");
        check (gr100ms < grEnd * 0.75f && gr100ms > grEnd * 0.2f, "Opto fast stage recovers part way in 100 ms");
        check (gr1s < gr100ms && gr1s > 0.05f, "Opto slow tail still releasing at 1 s");
    }

    // ---------------- Presence: constant-Q 1.5k bell, off at 0
    {
        PresenceEQ pr; pr.prepare (FS); pr.setBoostDb (0);
        double z = magDb (pr, 1500);
        pr.setBoostDb (6); pr.reset();
        double g15 = magDb (pr, 1500), g750 = magDb (pr, 750), g3k = magDb (pr, 3000), g200 = magDb (pr, 200), g10k = magDb (pr, 10000);
        std::printf ("PRES  0dB@1.5k:%.2f | +6: 1.5k:%.2f 750:%.2f 3k:%.2f 200:%.2f 10k:%.2f\n", z, g15, g750, g3k, g200, g10k);
        check (std::fabs (z) < 0.05, "Presence: flat at 0");
        check (std::fabs (g15 - 6.0) < 0.3, "Presence: +6 dB lands at 1.5 kHz");
        check (std::fabs (g750 - g3k) < 0.3, "Presence: log-symmetric (same gain an octave either side)");
        check (std::fabs (g200) < 0.3 && std::fabs (g10k) < 0.3, "Presence: back to flat far from the band");
    }

    // ---------------- Control: dynamic bands only act on their own band
    {
        ControlSection c; c.prepare (FS);
        c.setHarshThresholdDb (-30); c.setHonkThresholdDb (-30);
        // loud 3.3k tone -> harsh band works, honk band doesn't
        for (int i = 0; i < (int) FS; ++i) c.process (0.3f * (float) std::sin (2 * M_PI * 3700 * i / FS));
        float h1 = c.harshGrDb(), k1 = c.honkGrDb();
        c.reset();
        // loud 250 Hz tone -> honk band works, harsh band doesn't
        for (int i = 0; i < (int) FS; ++i) c.process (0.3f * (float) std::sin (2 * M_PI * 265 * i / FS));
        float h2 = c.harshGrDb(), k2 = c.honkGrDb();
        c.reset();
        // 1 kHz tone -> neither band moves much
        for (int i = 0; i < (int) FS; ++i) c.process (0.3f * (float) std::sin (2 * M_PI * 1000 * i / FS));
        float h3 = c.harshGrDb(), k3 = c.honkGrDb();
        std::printf ("CTRL  3.7k tone: harsh %.2f honk %.2f | 250 tone: harsh %.2f honk %.2f | 1k tone: harsh %.2f honk %.2f\n", h1, k1, h2, k2, h3, k3);
        check (h1 > 2.0f && k1 < 0.5f, "Control: harsh band reacts to 3.7k only");
        check (k2 > 2.0f && h2 < 0.5f, "Control: boom band reacts to 265 only");
        check (h3 < 1.0f && k3 < 1.0f, "Control: neither band reacts much to 1k");
        // band edges: measure the static cut depth of each bell at its edges vs centre
        {
            DynamicBand b; b.prepare (FS, { 3708.0, 1.24, 10.0f, 4.0f, 90.0f, 3.0f, 6.0f }); b.setThresholdDb (-60); // fully cutting
            for (int i = 0; i < (int) FS; ++i) b.process (0.5f * (float) std::sin (2 * M_PI * 3708 * i / FS));
            auto depth = [&] (double f) { double acc = 0; int n = 0; for (int i = 0; i < (int) FS; ++i) { float y = b.process (0.1f * (float) std::sin (2 * M_PI * f * i / FS)); if (i > FS / 2) { acc += y * y; ++n; } } return 20 * std::log10 (std::sqrt (acc / n) / (0.1 / std::sqrt (2.0))); };
            double dc = depth (3708), dlo = depth (2500), dhi = depth (5500), dfar = depth (1000);
            std::printf ("CTRL  harsh bell depth: centre %.1f | 2.5k %.1f | 5.5k %.1f | 1k %.1f dB\n", dc, dlo, dhi, dfar);
            check (dlo < dc * 0.4 && dhi < dc * 0.4, "Control: harsh edges at 2.5k/5.5k carry about half the depth");
            check (dfar > dc * 0.25, "Control: harsh mostly gone by 1k");
        }
    }

    // ---------------- Gain match: output RMS converges to input RMS
    {
        VocalChain ch; ch.prepare (FS, FS);
        ChainParams p; p.gainMatch = true; p.fetDriveDb = 6; ch.apply (p);
        double inAcc = 0, outAcc = 0; int cnt = 0;
        int n = (int) (8 * FS);
        for (int i = 0; i < n; ++i)
        {
            // crude "vocal": 220 Hz + harmonics with a slow amplitude wobble
            double t = i / FS;
            double env = 0.3 + 0.2 * std::sin (2 * M_PI * 1.5 * t);
            float x = (float) (env * (std::sin (2 * M_PI * 220 * t) + 0.5 * std::sin (2 * M_PI * 440 * t) + 0.25 * std::sin (2 * M_PI * 3300 * t)));
            float y = ch.process (x);
            if (t > 6.0) { inAcc += (double) x * x; outAcc += (double) y * y; ++cnt; }
        }
        double diff = 10 * std::log10 (outAcc / inAcc);
        std::printf ("MATCH output vs input RMS: %+.2f dB (makeup %+.2f dB), FET avg GR %.2f, Opto avg GR %.2f\n",
                     diff, ch.matchDb(), ch.fetSpot.averageDb(), ch.optoSpot.averageDb());
        check (std::fabs (diff) < 1.0, "Gain match holds RMS within 1 dB");
    }

    // ---------------- Sweet spot states
    {
        SweetSpot s; s.prepare (FS, 2, 5);
        for (int i = 0; i < (int) FS; ++i) s.push (1.0f);
        check (s.state() == 0, "Sweet spot: under");
        for (int i = 0; i < (int) FS; ++i) s.push (3.5f);
        check (s.state() == 1, "Sweet spot: in zone");
        for (int i = 0; i < (int) FS; ++i) s.push (8.0f);
        check (s.state() == 2, "Sweet spot: over");
    }

    std::printf ("\n%d failure(s)\n", fails);
    return fails;
}
