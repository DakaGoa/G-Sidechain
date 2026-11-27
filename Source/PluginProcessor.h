#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SidechainCore.h"

class GSideChainProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    struct Point { float scDb, inDb, grDb; };

    GSideChainProcessor();
    ~GSideChainProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "G-SideChain"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return displayProgram.load(); } // last index = "Auto Detected"
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Auto detect (message thread) ----
    void startLearn();
    void cancelLearn();
    bool isLearning() const;
    float getLearnProgress() const { return learnProgress.load(); }
    void pollLearn(); // runs the analysis once capture is complete
    const gsc::Analysis& getLastAnalysis() const { return lastAnalysis; }
    int getAnalysisSerial() const { return analysisSerial; }

    // ---- Metering (message thread) ----
    bool popPoint (Point&);
    float getCurrentGr() const { return currentGr.load(); }
    bool isSidechainActive() const { return sidechainActive.load(); }

    static juce::StringArray getPresetNames();
    juce::AudioProcessorValueTreeState apvts;

private:
    void timerCallback() override { pollLearn(); }
    gsc::Settings readSettings() const;
    void applySettings (const gsc::Settings&);

    gsc::Ducker ducker;
    double sr = 44100.0;

    // display decimation (audio thread)
    int samplesPerPoint = 240, pointCount = 0, scSilent = 0;
    Point acc { -100.0f, -100.0f, 0.0f };
    float inPeak = 0.0f; // raw input peak, converted to dB once per display point
    juce::AbstractFifo fifo { 4096 };
    std::array<Point, 4096> points {};
    std::atomic<float> currentGr { 0.0f };
    std::atomic<bool> sidechainActive { false };

    // capture for auto detect
    enum { learnIdle, learnCapturing, learnReady };
    std::atomic<int> learnState { learnIdle };
    std::atomic<bool> startRequest { false }, cancelRequest { false };
    std::atomic<float> learnProgress { 0.0f };
    std::vector<float> learnKick, learnBass;
    int learnPos = 0;
    bool learnStarted = false;

    gsc::Analysis lastAnalysis;
    int analysisSerial = 0;
    std::atomic<int> displayProgram { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GSideChainProcessor)
};
