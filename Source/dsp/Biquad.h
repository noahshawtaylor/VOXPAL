#pragma once
#include <cmath>

namespace vc {

// Transposed direct-form II biquad. All filters in the chain are built from
// this plus a couple of one-pole sections.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    inline float process (float x)
    {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

    // RBJ cookbook designs ----------------------------------------------------
    void setHighpass (double fs, double fc, double Q)
    {
        double w0 = 2.0 * M_PI * fc / fs, c = std::cos (w0), s = std::sin (w0);
        double alpha = s / (2.0 * Q);
        double a0 = 1 + alpha;
        b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = (1 + c) / 2 / a0;
        a1 = -2 * c / a0; a2 = (1 - alpha) / a0;
    }

    void setLowShelf (double fs, double fc, double gainDb, double S = 1.0)
    {
        double A = std::pow (10.0, gainDb / 40.0);
        double w0 = 2.0 * M_PI * fc / fs, c = std::cos (w0), s = std::sin (w0);
        double alpha = s / 2.0 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2);
        double sq = 2 * std::sqrt (A) * alpha;
        double a0 = (A + 1) + (A - 1) * c + sq;
        b0 = A * ((A + 1) - (A - 1) * c + sq) / a0;
        b1 = 2 * A * ((A - 1) - (A + 1) * c) / a0;
        b2 = A * ((A + 1) - (A - 1) * c - sq) / a0;
        a1 = -2 * ((A - 1) + (A + 1) * c) / a0;
        a2 = ((A + 1) + (A - 1) * c - sq) / a0;
    }

    void setHighShelf (double fs, double fc, double gainDb, double S = 1.0)
    {
        double A = std::pow (10.0, gainDb / 40.0);
        double w0 = 2.0 * M_PI * fc / fs, c = std::cos (w0), s = std::sin (w0);
        double alpha = s / 2.0 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2);
        double sq = 2 * std::sqrt (A) * alpha;
        double a0 = (A + 1) - (A - 1) * c + sq;
        b0 = A * ((A + 1) + (A - 1) * c + sq) / a0;
        b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
        b2 = A * ((A + 1) + (A - 1) * c - sq) / a0;
        a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
        a2 = ((A + 1) - (A - 1) * c - sq) / a0;
    }

    // First-order shelf from its two corner frequencies (bilinear, prewarped).
    // H(s) = (s + wz) / (s + wp):  DC gain = wz/wp, HF gain = 1.
    // Passive RC shelving networks (the Pultec low section) are exactly this.
    void setFirstOrderShelf (double fs, double zeroHz, double poleHz)
    {
        auto warp = [fs] (double f) { return 2.0 * fs * std::tan (M_PI * f / fs); };
        double wz = warp (zeroHz), wp = warp (poleHz), K = 2.0 * fs;
        double a0 = K + wp;
        b0 = (K + wz) / a0; b1 = (wz - K) / a0; b2 = 0;
        a1 = (wp - K) / a0; a2 = 0;
    }

    // Analog-matched peaking filter (Vicanek, "Matched Second Order Digital
    // Filters", 2016). The textbook bilinear peak gets squashed against Nyquist,
    // so a 16 kHz bell at 48 kHz looks nothing like the same bell at 192 kHz.
    // Here the poles come from the matched-Z transform of the analog prototype
    // and the numerator is fitted so the magnitude equals the analog curve at
    // DC, at the centre and at Nyquist. Works even when the bell's skirt runs
    // past Nyquist, which is the 16 kHz-at-48 kHz case.
    void setPeakAnalogMatched (double fs, double fc, double gainDb, double Q)
    {
        if (std::fabs (gainDb) < 1.0e-4) { b0 = 1; b1 = b2 = a1 = a2 = 0; return; }
        const double G  = std::pow (10.0, gainDb / 20.0);
        const double w0 = 2.0 * M_PI * fc / fs;
        const double q  = 1.0 / (2.0 * Q);
        // poles: matched Z of s^2 + s*w0/Q + w0^2
        if (q <= 1.0) a1 = -2.0 * std::exp (-q * w0) * std::cos (std::sqrt (1.0 - q * q) * w0);
        else          a1 = -2.0 * std::exp (-q * w0) * std::cosh (std::sqrt (q * q - 1.0) * w0);
        a2 = std::exp (-2.0 * q * w0);
        // |D|^2 = A0 + A1*phi + A2*phi^2,  phi = sin^2(w/2)
        const double A0 = std::pow (1 + a1 + a2, 2);
        const double A1 = -4.0 * (a1 + 4 * a2 + a1 * a2);
        const double A2 = 16.0 * a2;
        // analog |H|^2 at the three match frequencies (no warping: f maps to f)
        auto target = [&] (double w) {
            double n = std::pow (w0 * w0 - w * w, 2) + std::pow (G * w0 * w / Q, 2);
            double d = std::pow (w0 * w0 - w * w, 2) + std::pow (w0 * w / Q, 2);
            return n / d; };
        const double p0 = std::pow (std::sin (w0 / 2), 2);
        const double T0 = target (0.0) * A0;                                  // phi = 0
        const double Tc = target (w0) * (A0 + A1 * p0 + A2 * p0 * p0);        // phi = p0
        const double Tn = target (M_PI) * (A0 + A1 + A2);                     // phi = 1
        // solve B0 + B1*phi + B2*phi^2 through (0,T0), (p0,Tc), (1,Tn)
        const double B0 = T0;
        const double B2 = ((Tc - B0) - p0 * (Tn - B0)) / (p0 * p0 - p0);
        const double B1 = Tn - B0 - B2;
        // back to b coefficients: B0 = (b0+b1+b2)^2, B0+B1+B2 = (b0-b1+b2)^2, B2 = 16 b0 b2
        const double S = std::sqrt (std::max (B0, 0.0));
        const double P = std::sqrt (std::max (B0 + B1 + B2, 0.0));
        const double M = 0.5 * (S + P);
        b1 = 0.5 * (S - P);
        const double disc = std::max (M * M - B2 / 4.0, 0.0);
        b0 = 0.5 * (M + std::sqrt (disc));
        b2 = M - b0;
    }

    void setPeak (double fs, double fc, double gainDb, double Q)
    {
        double A = std::pow (10.0, gainDb / 40.0);
        double w0 = 2.0 * M_PI * fc / fs, c = std::cos (w0), s = std::sin (w0);
        double alpha = s / (2.0 * Q);
        double a0 = 1 + alpha / A;
        b0 = (1 + alpha * A) / a0; b1 = -2 * c / a0; b2 = (1 - alpha * A) / a0;
        a1 = -2 * c / a0; a2 = (1 - alpha / A) / a0;
    }
};

struct OnePoleHP
{
    double a = 0, z = 0, prevX = 0;
    void set (double fs, double fc) { a = std::exp (-2.0 * M_PI * fc / fs); }
    void reset() { z = prevX = 0; }
    inline float process (float x) { double y = a * (z + x - prevX); prevX = x; z = y; return (float) y; }
};

struct OnePoleLP
{
    double a = 0, z = 0;
    void set (double fs, double fc) { a = std::exp (-2.0 * M_PI * fc / fs); }
    void reset() { z = 0; }
    inline float process (float x) { z = (1 - a) * x + a * z; return (float) z; }
};

inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

} // namespace vc
