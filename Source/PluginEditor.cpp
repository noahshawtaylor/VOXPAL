#include "PluginEditor.h"

namespace
{
    const juce::Colour bg       { 0xff1b1b1f };
    const juce::Colour panel    { 0xff26262c };
    const juce::Colour line     { 0xff3a3a42 };
    const juce::Colour text     { 0xffd8d4cc };
    const juce::Colour dim      { 0xff8a8690 };
    const juce::Colour accent   { 0xffe0a84a };
    const juce::Colour ledOff   { 0xff3a3f44 };
    const juce::Colour ledGreen { 0xff52d273 };
    const juce::Colour ledAmber { 0xffffb347 };
}

// ---------------------------------------------------------------- GrMeter
void GrMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto led = r.removeFromTop (16.0f).withSizeKeepingCentre (12.0f, 12.0f);
    r.removeFromTop (6.0f);

    // sweet-spot LED: dim (under), green (in zone), amber (over)
    juce::Colour c = spot == 1 ? ledGreen : (spot == 2 ? ledAmber : ledOff);
    if (spot != 0) { g.setColour (c.withAlpha (0.25f)); g.fillEllipse (led.expanded (4.0f)); }
    g.setColour (c); g.fillEllipse (led);

    // vertical GR bar, 0..12 dB, with the target zone drawn behind it
    auto bar = r.withSizeKeepingCentre (14.0f, r.getHeight());
    g.setColour (bg); g.fillRoundedRectangle (bar, 3.0f);
    const float maxDb = 12.0f;
    auto yFor = [&] (float db) { return bar.getY() + bar.getHeight() * juce::jlimit (0.0f, 1.0f, db / maxDb); };
    g.setColour (ledGreen.withAlpha (0.18f));
    g.fillRect (juce::Rectangle<float> (bar.getX(), yFor (zoneLo), bar.getWidth(), yFor (zoneHi) - yFor (zoneLo)));
    g.setColour (accent);
    g.fillRoundedRectangle (juce::Rectangle<float> (bar.getX() + 3, bar.getY(), bar.getWidth() - 6, yFor (gr) - bar.getY()), 2.0f);

    g.setColour (dim); g.setFont (10.0f);
    g.drawText (juce::String (gr, 1), getLocalBounds().removeFromBottom (14), juce::Justification::centred);
}

// ---------------------------------------------------------------- Knob / Toggle
void Knob::init (juce::Component& parent, juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& txt)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, line);
    slider.setColour (juce::Slider::textBoxTextColourId, text);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    label.setText (txt, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, dim);
    label.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    parent.addAndMakeVisible (slider);
    parent.addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
}

void Toggle::init (juce::Component& parent, juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& txt)
{
    button.setButtonText (txt);
    button.setColour (juce::ToggleButton::textColourId, text);
    button.setColour (juce::ToggleButton::tickColourId, accent);
    parent.addAndMakeVisible (button);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, button);
}

// ---------------------------------------------------------------- Section
void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (panel); g.fillRoundedRectangle (r, 6.0f);
    g.setColour (line);  g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (text);  g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (name, getLocalBounds().removeFromTop (24), juce::Justification::centred);
    if (footnote.isNotEmpty())
    {
        g.setColour (dim); g.setFont (10.0f);
        g.drawText (footnote, getLocalBounds().removeFromBottom (16), juce::Justification::centred);
    }
}

void Section::resized()
{
    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (24);
    if (onOff) onOff->button.setBounds (r.removeFromBottom (22).withSizeKeepingCentre (60, 22));
    if (footnote.isNotEmpty()) r.removeFromBottom (16);

    int meterW = 0;
    if (meter)  meterW += 40;
    if (meter2) meterW += 40;
    auto meterArea = r.removeFromRight (meterW);
    if (meter)  meter->setBounds (meterArea.removeFromLeft (40));
    if (meter2) meter2->setBounds (meterArea.removeFromLeft (40));

    const int nK = (int) knobs.size();
    if (nK == 0) return;
    int kw = r.getWidth() / nK;
    for (auto* k : knobs)
    {
        auto kr = r.removeFromLeft (kw);
        k->label.setBounds (kr.removeFromTop (16));
        k->slider.setBounds (kr.withSizeKeepingCentre (juce::jmin (kr.getWidth(), 84), juce::jmin (kr.getHeight(), 100)));
    }
}

// ---------------------------------------------------------------- Editor
VocalChainEditor::VocalChainEditor (VocalChainProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    // FET
    comp1.init (fetSec, s, ID::fetDrive, "COMPRESSOR 1");
    fetOn.init (fetSec, s, ID::fetOn, "ON");
    fetSec.knobs = { &comp1 }; fetSec.onOff = &fetOn;
    fetMeter.setRange (2, 5); fetSec.addAndMakeVisible (fetMeter); fetSec.meter = &fetMeter;
    fetSec.footnote = "4:1  ·  attack 3  ·  release 7";

    // OPTO
    comp2.init (optoSec, s, ID::optoPR, "COMPRESSOR 2");
    optoOn.init (optoSec, s, ID::optoOn, "ON");
    optoSec.knobs = { &comp2 }; optoSec.onOff = &optoOn;
    optoMeter.setRange (3, 5); optoSec.addAndMakeVisible (optoMeter); optoSec.meter = &optoMeter;
    optoSec.footnote = "gray  ·  compress";

    // CONTROL
    boom.init (ctrlSec, s, ID::boom, "BOOM");
    harsh.init (ctrlSec, s, ID::harsh, "HARSH");
    ctrlOn.init (ctrlSec, s, ID::controlOn, "ON");
    ctrlSec.knobs = { &boom, &harsh }; ctrlSec.onOff = &ctrlOn;
    boomMeter.setRange (2, 4); harshMeter.setRange (3, 4);
    ctrlSec.addAndMakeVisible (boomMeter); ctrlSec.addAndMakeVisible (harshMeter);
    ctrlSec.meter = &boomMeter; ctrlSec.meter2 = &harshMeter;
    ctrlSec.footnote = "250  ·  2.5–4.5k";

    // PASSIVE EQ
    low.init (eqSec, s, ID::low, "LOW");
    high.init (eqSec, s, ID::high, "HIGH");
    eqOn.init (eqSec, s, ID::eqOn, "ON");
    eqSec.knobs = { &low, &high }; eqSec.onOff = &eqOn;
    eqSec.footnote = "100 boost+atten  ·  10k";

    // AIR
    air.init (airSec, s, ID::air, "AIR");
    presence.init (airSec, s, ID::presence, "PRESENCE");
    airOn.init (airSec, s, ID::airOn, "ON");
    airSec.knobs = { &air, &presence }; airSec.onOff = &airOn;
    airSec.footnote = "q follows boost";

    for (auto* sec : { &fetSec, &optoSec, &ctrlSec, &eqSec, &airSec }) addAndMakeVisible (sec);

    // Header strip
    output.init (*this, s, ID::output, "OUTPUT");
    gainMatch.init (*this, s, ID::gainMatch, "GAIN MATCH");
    bypass.init (*this, s, ID::bypass, "BYPASS");
    matchReadout.setJustificationType (juce::Justification::centredLeft);
    matchReadout.setColour (juce::Label::textColourId, dim);
    matchReadout.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (matchReadout);

    setSize (1080, 330);
    startTimerHz (30);
}

void VocalChainEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (text); g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    g.drawText ("VOXPAL", 16, 10, 200, 28, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (10.0f);
    g.drawText ("hpf 100 hz / 18 db always on", 16, 34, 240, 14, juce::Justification::centredLeft);
}

void VocalChainEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (70);
    header.removeFromLeft (260);
    bypass.button.setBounds (header.removeFromRight (90).withSizeKeepingCentre (90, 24));
    gainMatch.button.setBounds (header.removeFromRight (110).withSizeKeepingCentre (110, 24));
    matchReadout.setBounds (header.removeFromRight (80).withSizeKeepingCentre (80, 24));
    auto outArea = header.removeFromRight (90);
    output.label.setBounds (outArea.removeFromTop (14)); output.slider.setBounds (outArea);

    r.reduce (8, 8);
    int w = r.getWidth();
    fetSec.setBounds  (r.removeFromLeft (w * 16 / 100));
    optoSec.setBounds (r.removeFromLeft (w * 16 / 100));
    ctrlSec.setBounds (r.removeFromLeft (w * 30 / 100));
    eqSec.setBounds   (r.removeFromLeft (w * 24 / 100));
    airSec.setBounds  (r);
}

void VocalChainEditor::timerCallback()
{
    auto& m = proc.meters;
    fetMeter.set   (m.fetGr.load(),   m.fetState.load());
    optoMeter.set  (m.optoGr.load(),  m.optoState.load());
    boomMeter.set  (m.honkGr.load(),  m.honkState.load());
    harshMeter.set (m.harshGr.load(), m.harshState.load());
    float md = m.matchDb.load();
    matchReadout.setText (proc.apvts.getRawParameterValue (ID::gainMatch)->load() > 0.5f
                              ? juce::String (md > 0 ? "+" : "") + juce::String (md, 1) + " dB" : "",
                          juce::dontSendNotification);
}
