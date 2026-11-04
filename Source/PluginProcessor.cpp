#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

VocalChainProcessor::VocalChainProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::mono(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::mono(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

// Defaults ARE the house chain. "Reset" lands on it.
APVTS::ParameterLayout VocalChainProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    using R = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<P> (juce::ParameterID { ID::fetDrive, 1 }, "TOM",   R { -12.f, 12.f, 0.1f }, 0.f,  juce::AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<B> (juce::ParameterID { ID::fetOn, 1 },    "TOM On",  true));
    p.push_back (std::make_unique<P> (juce::ParameterID { ID::optoPR, 1 },   "SPIKE", R { 0.f, 100.f, 0.5f }, 50.f));
    p.push_back (std::make_unique<B> (juce::ParameterID { ID::optoOn, 1 },   "SPIKE On", true));
    p.push_back (std::make_unique<P> (juce::ParameterID { ID::boom, 1 },     "Control Boom",  R { 0.f, 10.f, 0.1f }, 5.f));
    p.push_back (std::make_unique<P> (juce::ParameterID { ID::harsh, 1 },    "Control Harsh", R { 0.f, 10.f, 0.1f }, 5.f));
    p.push_back (std::make_unique<B> (juce::ParameterID { ID::controlOn, 1 },"Control On", true));
    p.push_back (std::make_unique<P> (juce::ParameterID { ID::low, 1 },      "EQ Low",   R { 0.f, 10.f, 0.1f }, 3.f));
    p.push_back (std::make_unique<P> (juce::ParameterID { ID::high, 1 },     "EQ High",  R { 0.f, 10.f, 0.1f }, 3.f));
    p.push_back (std::make_unique<B> (juce::ParameterID { ID::eqOn, 1 },     "EQ On",    true));
    p.push_back (std::make_unique<P> (juce::ParameterID { ID::presence, 1 }, "Presence", R { 0.f, 12.f, 0.1f }, 0.f, juce::AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<P> (juce::ParameterID { ID::air, 1 },      "Air",      R { 0.f, 8.f, 0.1f }, 4.f, juce::AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<B> (juce::ParameterID { ID::airOn, 1 },    "Air On",   true));
    p.push_back (std::make_unique<P> (juce::ParameterID { ID::output, 1 },   "Output",   R { -12.f, 12.f, 0.1f }, 0.f, juce::AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<B> (juce::ParameterID { ID::gainMatch, 1 },"Gain Match", false));
    p.push_back (std::make_unique<B> (juce::ParameterID { ID::bypass, 1 },   "Bypass",   false));
    return { p.begin(), p.end() };
}

void VocalChainProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        1, kOversampleOrder, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampler->initProcessing ((size_t) samplesPerBlock);
    oversampler->reset();
    setLatencySamples ((int) std::ceil (oversampler->getLatencyInSamples()));

    chain.prepare (sampleRate, sampleRate * (1 << kOversampleOrder));
    chain.reset();

    dryBuf.setSize (1, samplesPerBlock);
    stageBuf.setSize (1, samplesPerBlock);

    const double rampSec = 0.02;
    for (auto* s : { &sFetDrive, &sOptoPR, &sBoom, &sHarsh, &sLow, &sHigh, &sPresence, &sAir, &sOutput }) s->reset (sampleRate, rampSec);
    auto v = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    sFetDrive.setCurrentAndTargetValue (v (ID::fetDrive)); sOptoPR.setCurrentAndTargetValue (v (ID::optoPR));
    sBoom.setCurrentAndTargetValue (v (ID::boom));         sHarsh.setCurrentAndTargetValue (v (ID::harsh));
    sLow.setCurrentAndTargetValue (v (ID::low));           sHigh.setCurrentAndTargetValue (v (ID::high));
    sPresence.setCurrentAndTargetValue (v (ID::presence));
    sAir.setCurrentAndTargetValue (v (ID::air));           sOutput.setCurrentAndTargetValue (v (ID::output));

    bypassCoef = (float) std::exp (-1.0 / (0.02 * sampleRate)); // 20 ms bypass crossfade
    bypassRamp = v (ID::bypass) > 0.5f ? 1.0f : 0.0f;

    vc::ChainParams p; pullSmoothedParams (p, 0); chain.apply (p, true);
}

bool VocalChainProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    // Mono vocal plugin: mono in, mono out.
    return l.getMainInputChannelSet() == juce::AudioChannelSet::mono()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

// Advances the smoothers by n samples and reads the current values plus the switches.
void VocalChainProcessor::pullSmoothedParams (vc::ChainParams& c, int n)
{
    auto v = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    sFetDrive.setTargetValue (v (ID::fetDrive)); sOptoPR.setTargetValue (v (ID::optoPR));
    sBoom.setTargetValue (v (ID::boom));         sHarsh.setTargetValue (v (ID::harsh));
    sLow.setTargetValue (v (ID::low));           sHigh.setTargetValue (v (ID::high));
    sPresence.setTargetValue (v (ID::presence));
    sAir.setTargetValue (v (ID::air));           sOutput.setTargetValue (v (ID::output));
    if (n > 0) for (auto* s : { &sFetDrive, &sOptoPR, &sBoom, &sHarsh, &sLow, &sHigh, &sPresence, &sAir, &sOutput }) s->skip (n);

    c.fetDriveDb  = sFetDrive.getCurrentValue(); c.optoPeakRed = sOptoPR.getCurrentValue();
    c.boomKnob    = sBoom.getCurrentValue();     c.harshKnob   = sHarsh.getCurrentValue();
    c.lowKnob     = sLow.getCurrentValue();      c.highKnob    = sHigh.getCurrentValue();
    c.presenceDb  = sPresence.getCurrentValue();
    c.airDb       = sAir.getCurrentValue();      c.outputDb    = sOutput.getCurrentValue();
    c.fetOn = v (ID::fetOn) > 0.5f;   c.optoOn = v (ID::optoOn) > 0.5f;   c.controlOn = v (ID::controlOn) > 0.5f;
    c.eqOn  = v (ID::eqOn) > 0.5f;    c.airOn  = v (ID::airOn) > 0.5f;    c.gainMatch = v (ID::gainMatch) > 0.5f;
}

void VocalChainProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 1 || n == 0) return;
    if (dryBuf.getNumSamples() < n) { dryBuf.setSize (1, n, false, false, true); stageBuf.setSize (1, n, false, false, true); oversampler->initProcessing ((size_t) n); }
    float* io = buffer.getWritePointer (0);
    float* dry = dryBuf.getWritePointer (0);
    const float bypassTarget = apvts.getRawParameterValue (ID::bypass)->load() > 0.5f ? 1.0f : 0.0f;

    // Keep the raw input for the bypass crossfade
    juce::FloatVectorOperations::copy (dry, io, n);

    for (int start = 0; start < n; start += kSubBlock)
    {
        const int len = juce::jmin (kSubBlock, n - start);

        // parameters: smoothed, applied per sub-block, coefficients only on change
        vc::ChainParams p; pullSmoothedParams (p, len);
        chain.apply (p);

        // stage 1: highpass (base rate) -> stageBuf holds the post-HPF signal
        float* st = stageBuf.getWritePointer (0);
        for (int i = 0; i < len; ++i) st[i] = chain.stage1 (io[start + i]);

        // stage 2: compressors, oversampled
        juce::dsp::AudioBlock<float> block (&st, 1, 0, (size_t) len);
        auto up = oversampler->processSamplesUp (block);
        float* u = up.getChannelPointer (0);
        const size_t un = up.getNumSamples();
        for (size_t i = 0; i < un; ++i) u[i] = chain.stage2 (u[i]);
        float* ioArr[1] = { io };
        juce::dsp::AudioBlock<float> down (ioArr, 1, (size_t) start, (size_t) len);
        oversampler->processSamplesDown (down);

        // stage 3: control, EQ, air, gain match (reference = post-HPF signal), output
        for (int i = 0; i < len; ++i) io[start + i] = chain.stage3 (st[i], io[start + i]);
    }

    // Bypass crossfade to the raw input. The chain keeps running so meters and
    // gain-match state stay live; with Gain Match on, the processed path already
    // sits at input loudness so this is a level-matched A/B.
    for (int i = 0; i < n; ++i)
    {
        bypassRamp = bypassCoef * bypassRamp + (1 - bypassCoef) * bypassTarget;
        io[i] = io[i] + bypassRamp * (dry[i] - io[i]);
    }

    meters.fetGr.store (chain.fetGrDb());     meters.fetState.store (chain.fetState());
    meters.optoGr.store (chain.optoGrDb());   meters.optoState.store (chain.optoState());
    meters.harshGr.store (chain.harshGrDb()); meters.harshState.store (chain.harshState());
    meters.honkGr.store (chain.honkGrDb());   meters.honkState.store (chain.honkState());
    meters.matchDb.store (chain.matchDb());
}

juce::AudioProcessorEditor* VocalChainProcessor::createEditor() { return new VocalChainEditor (*this); }

void VocalChainProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void VocalChainProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalChainProcessor(); }
