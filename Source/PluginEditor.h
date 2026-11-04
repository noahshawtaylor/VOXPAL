#pragma once
#include "PluginProcessor.h"

// Functional UI: dark panel, one row of five sections plus a header strip.
// Every knob is an APVTS attachment, so this editor can be replaced with a
// fully skinned one later without touching the processor.

class GrMeter : public juce::Component
{
public:
    void set (float grDb, int state) { gr = grDb; spot = state; repaint(); }
    void setRange (float lo, float hi) { zoneLo = lo; zoneHi = hi; }
    void paint (juce::Graphics& g) override;
private:
    float gr = 0, zoneLo = 2, zoneHi = 5; int spot = 0;
};

struct Knob
{
    juce::Slider slider; juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    void init (juce::Component& parent, juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text);
};

struct Toggle
{
    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
    void init (juce::Component& parent, juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text);
};

class Section : public juce::Component
{
public:
    Section (const juce::String& title) : name (title) {}
    void paint (juce::Graphics& g) override;
    void resized() override;
    juce::String name;
    std::vector<Knob*> knobs;      // laid out left to right
    Toggle* onOff = nullptr;
    GrMeter* meter = nullptr;
    GrMeter* meter2 = nullptr;     // Control has two
    juce::String footnote;
};

class VocalChainEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VocalChainEditor (VocalChainProcessor&);
    ~VocalChainEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    VocalChainProcessor& proc;
    juce::LookAndFeel_V4 lnf;

    Section fetSec { "TOM" }, optoSec { "SPIKE" }, ctrlSec { "CONTROL" }, eqSec { "PASSIVE EQ" }, airSec { "MAGIC" };
    Knob comp1, comp2, low, high, air, presence, boom, harsh, output;
    Toggle fetOn, optoOn, ctrlOn, eqOn, airOn, gainMatch, bypass;
    GrMeter fetMeter, optoMeter, boomMeter, harshMeter;
    juce::Label matchReadout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalChainEditor)
};
