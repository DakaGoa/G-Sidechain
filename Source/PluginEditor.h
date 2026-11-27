#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
namespace col
{
const juce::Colour bg       { 0xff0b0e14 };
const juce::Colour bg2      { 0xff121722 };
const juce::Colour panel    { 0xff161c28 };
const juce::Colour panelHi  { 0xff1d2535 };
const juce::Colour border   { 0xff262f41 };
const juce::Colour text     { 0xffe8ecf4 };
const juce::Colour textDim  { 0xff8590a6 };
const juce::Colour textFaint{ 0xff4c566b };
const juce::Colour accent   { 0xff22d3ee }; // cyan
const juce::Colour accent2  { 0xff8b5cf6 }; // violet
const juce::Colour kick     { 0xfff59e0b }; // amber
const juce::Colour input    { 0xff5b7cfa };
const juce::Colour warn     { 0xfff43f5e };
} // namespace col

juce::Font font (float height, bool bold = false);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (15.0f, true); }
    juce::Font getPopupMenuFont() override { return font (14.5f); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
};

class ScopeView : public juce::Component
{
public:
    // Setters return true when the picture changes.
    bool push (const GSideChainProcessor::Point& p);
    bool setThreshold (float db) { return std::exchange (thresholdDb, db) != db; }
    bool setSidechainActive (bool b) { return std::exchange (scActive, b) != b; }
    void paint (juce::Graphics&) override;

private:
    static constexpr int numPoints = 640; // 3.2 s of history
    std::array<GSideChainProcessor::Point, numPoints> hist {};
    int head = 0, quietRun = 0;
    float thresholdDb = -24.0f;
    bool scActive = false;
};

class GainMeter : public juce::Component
{
public:
    bool setGr (float db) // returns true when the meter moved
    {
        const auto before = std::make_pair (value, peak);
        value = std::max (db, value - 0.6f);
        peak = std::max (db, peak - 0.15f);
        return before != std::make_pair (value, peak);
    }
    void paint (juce::Graphics&) override;

private:
    float value = 0.0f, peak = 0.0f;
};

class AutoButton : public juce::Button
{
public:
    AutoButton() : juce::Button ("Auto Detect") {}
    void setLearning (bool isLearning, float progress);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    bool learning = false;
    float progress = 0.0f;
};

class ChevronButton : public juce::Button
{
public:
    explicit ChevronButton (bool pointsRight) : juce::Button ({}), right (pointsRight) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    bool right;
};
} // namespace ui

class GSideChainEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GSideChainEditor (GSideChainProcessor&);
    ~GSideChainEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh(); // pulls meter data from the processor (timer body, public for the self-test)

private:
    void timerCallback() override { refresh(); }
    void stepPreset (int delta);

    struct Knob
    {
        juce::Slider slider;
        juce::String name;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct Card
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name, bool bipolar = false);
    void setupToggle (juce::TextButton&, std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>&,
                      const juce::String& paramId, const juce::String& text);

    GSideChainProcessor& proc;
    ui::LookAndFeel lnf;

    ui::ScopeView scope;
    ui::GainMeter meter;
    ui::AutoButton autoButton;
    ui::ChevronButton prevButton { false }, nextButton { true };
    juce::ComboBox presetBox;
    juce::Label infoLabel;

    Knob threshold, ratio, focus, attack, hold, release, depth, crossover, mix, output;
    juce::TextButton listenButton, lowOnlyButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> listenAttachment, lowOnlyAttachment;

    std::array<Card, 4> cards;
    int seenSerial = 0, seenProgram = -2;
    bool wasLearning = false;
    std::tuple<double, double, double> lastEnvelope { -1.0, -1.0, -1.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GSideChainEditor)
};
