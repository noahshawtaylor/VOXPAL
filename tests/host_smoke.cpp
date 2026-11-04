// Headless host: instantiate the processor, run audio, check meters and output.
#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <cmath>

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int fails = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what); if (! ok) ++fails; };

    for (double fs : { 48000.0, 192000.0 })
    {
        VocalChainProcessor p;
        p.setPlayConfigDetails (1, 1, fs, 512);
        p.prepareToPlay (fs, 512);
        check (p.getParameters().size() == 16, "16 parameters exposed");
        std::printf ("latency reported: %d samples\n", p.getLatencySamples());
        check (p.getLatencySamples() > 0 && p.getLatencySamples() < 16, "oversampling latency reported (small)");

        juce::AudioBuffer<float> buf (1, 512);
        juce::MidiBuffer midi;
        double t = 0; float peakOut = 0; bool finite = true;
        for (int block = 0; block < (int) (fs / 512) * 3; ++block)
        {
            for (int i = 0; i < 512; ++i, t += 1.0 / fs)
            {
                double env = 0.3 + 0.2 * std::sin (2 * M_PI * 1.5 * t);
                float x = (float) (env * (std::sin (2 * M_PI * 220 * t) + 0.5 * std::sin (2 * M_PI * 440 * t) + 0.25 * std::sin (2 * M_PI * 3300 * t)));
                buf.setSample (0, i, x);
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < 512; ++i) { float y = buf.getSample (0, i); if (! std::isfinite (y)) finite = false; peakOut = std::max (peakOut, std::fabs (y)); }
        }
        std::printf ("fs %.0f: peak out %.3f, TOM GR %.2f dB (state %d), SPIKE GR %.2f dB (state %d), harsh %.2f, boom %.2f\n",
                     fs, peakOut, p.meters.fetGr.load(), p.meters.fetState.load(), p.meters.optoGr.load(), p.meters.optoState.load(),
                     p.meters.harshGr.load(), p.meters.honkGr.load());
        check (finite, "output is finite");
        check (peakOut > 0.05f && peakOut < 2.0f, "output level sane");
        check (p.meters.fetGr.load() > 0.5f, "TOM meter moves");
        check (p.meters.optoGr.load() > 0.2f, "SPIKE meter moves");

        // bypass: output should equal input
        if (auto* byp = p.apvts.getParameter (ID::bypass)) byp->setValueNotifyingHost (1.0f);
        const int bypassBlocks = (int) (0.5 * fs / 512);
        for (int block = 0; block < bypassBlocks; ++block)
        {
            for (int i = 0; i < 512; ++i) { float x = 0.3f * (float) std::sin (2 * M_PI * 440 * (block * 512 + i) / fs); buf.setSample (0, i, x); }
            p.processBlock (buf, midi);
        }
        float lastIn = 0.3f * (float) std::sin (2 * M_PI * 440 * ((bypassBlocks - 1) * 512 + 511) / fs);
        check (std::fabs (buf.getSample (0, 511) - lastIn) < 1.0e-3f, "bypass passes input through");

        // parameter smoothing: jump Output by 12 dB mid-stream and look for a discontinuity
        {
            if (auto* byp = p.apvts.getParameter (ID::bypass)) byp->setValueNotifyingHost (0.0f);
            for (int block = 0; block < 40; ++block)   // let bypass ramp settle
            { for (int i = 0; i < 512; ++i) buf.setSample (0, i, 0.2f * (float) std::sin (2 * M_PI * 440 * (block * 512 + i) / fs)); p.processBlock (buf, midi); }
            float maxStep = 0, prev = 0; bool first = true;
            for (int block = 0; block < 12; ++block)
            {
                if (block == 2) if (auto* o = p.apvts.getParameter (ID::output)) o->setValueNotifyingHost (1.0f); // +12 dB
                for (int i = 0; i < 512; ++i) buf.setSample (0, i, 0.2f * (float) std::sin (2 * M_PI * 440 * ((40 + block) * 512 + i) / fs));
                p.processBlock (buf, midi);
                for (int i = 0; i < 512; ++i) { float y = buf.getSample (0, i); if (! first) maxStep = std::max (maxStep, std::fabs (y - prev)); prev = y; first = false; }
            }
            // a 440 Hz sine at this level never moves more than ~0.06/sample at 48k; a hard +12 dB step would add a jump
            float sineMax = 0.2f * 4.0f * 2 * (float) M_PI * 440 / (float) fs * 1.2f;
            std::printf ("max sample-to-sample step during +12 dB output jump: %.4f (sine slope limit %.4f)\n", maxStep, sineMax);
            check (maxStep < sineMax, "output change is smoothed (no zipper step)");
            if (auto* o = p.apvts.getParameter (ID::output)) o->setValueNotifyingHost (0.5f);
        }

        // state save / restore round trip
        juce::MemoryBlock state; p.getStateInformation (state);
        VocalChainProcessor q; q.setStateInformation (state.getData(), (int) state.getSize());
        check (std::fabs (q.apvts.getRawParameterValue (ID::air)->load() - 4.0f) < 0.01f, "state round-trips (Air default 4 dB)");
    }
    std::printf ("\n%d failure(s)\n", fails);
    return fails;
}
