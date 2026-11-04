#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/VocalChain.h"
#include <atomic>

namespace ID
{
    inline const juce::String fetDrive   = "fetDrive";
    inline const juce::String fetOn      = "fetOn";
    inline const juce::String optoPR     = "optoPeakReduction";
    inline const juce::String optoOn     = "optoOn";
    inline const juce::String boom       = "controlBoom";
    inline const juce::String harsh      = "controlHarsh";
    inline const juce::String controlOn  = "controlOn";
    inline const juce::String low        = "eqLow";
    inline const juce::String high       = "eqHigh";
    inline const juce::String eqOn       = "eqOn";
    inline const juce::String presence   = "presence";
    inline const juce::String air        = "air";
    inline const juce::String airOn      = "airOn";
    inline const juce::String output     = "output";
    inline const juce::String gainMatch  = "gainMatch";
    inline const juce::String bypass     = "bypass";
}

// Meter values the editor polls (lock-free).
struct MeterState
{
    std::atomic<float> fetGr { 0 }, optoGr { 0 }, harshGr { 0 }, honkGr { 0 }, matchDb { 0 };
    std::atomic<int>   fetState { 0 }, optoState { 0 }, harshState { 0 }, honkState { 0 };
};

// Mono vocal chain. The two compressors run 2x oversampled; everything else at
// the host rate. Continuous parameters are smoothed over ~20 ms and applied in
// 32-sample sub-blocks, recomputing filter coefficients only when a value moved.
class VocalChainProcessor : public juce::AudioProcessor
{
public:
    VocalChainProcessor();
    ~VocalChainProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    MeterState meters;

    static constexpr int kOversampleOrder = 1;   // 2x
    static constexpr int kSubBlock = 32;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pullSmoothedParams (vc::ChainParams& out, int numSamplesToSkip);

    vc::VocalChain chain;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> dryBuf, stageBuf;

    using Smooth = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    Smooth sFetDrive, sOptoPR, sBoom, sHarsh, sLow, sHigh, sPresence, sAir, sOutput;
    float bypassRamp = 0.0f, bypassCoef = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalChainProcessor)
};
