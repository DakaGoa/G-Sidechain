#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct Preset
{
    const char* name;
    float thr, ratio, depth, attack, hold, release, focus, xover;
    bool lowOnly;
    float mix;
};

// clang-format off
const Preset presets[] = {
    //  name                 thr   ratio depth  atk  hold  rel  focus xover lowOnly mix
    { "Init",               -24.f,  8.f, 12.f, 1.0f, 10.f, 120.f, 70.f, 120.f, false, 100.f },
    { "EDM Pump",           -32.f, 20.f, 24.f, 0.5f, 20.f, 180.f, 60.f, 120.f, false, 100.f },
    { "Techno Tight",       -28.f, 10.f, 12.f, 0.3f,  5.f,  70.f, 55.f, 150.f, true,  100.f },
    { "House Groove",       -26.f,  6.f,  9.f, 2.0f, 10.f, 140.f, 65.f, 120.f, false, 100.f },
    { "Hip-Hop 808",        -30.f, 12.f, 18.f, 0.3f, 30.f, 250.f, 50.f, 120.f, true,  100.f },
    { "Subtle Glue",        -22.f,  3.f,  4.f, 5.0f,  0.f,  90.f, 70.f, 120.f, false, 100.f },
    { "Sub Only Duck",      -28.f, 10.f, 15.f, 0.5f, 10.f, 100.f, 55.f,  90.f, true,  100.f },
    { "Future Bass Swell",  -34.f, 20.f, 30.f, 1.0f, 40.f, 350.f, 60.f, 120.f, false, 100.f },
    { "Drum & Bass Snap",   -26.f, 10.f, 14.f, 0.2f,  5.f,  60.f, 60.f, 140.f, true,  100.f },
    { "Parallel Pump",      -32.f, 20.f, 24.f, 0.5f, 20.f, 180.f, 60.f, 120.f, false,  60.f },
};
// clang-format on

const int autoProgram = (int) std::size (presets);
const char* const autoProgramName = "Auto Detected";

juce::String fmtMs (float v, int)
{
    return (v < 10.0f ? juce::String (v, 2) : v < 100.0f ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + " ms";
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using Attr = AudioParameterFloatAttributes;
    auto parse = [] (const String& t) { return t.getFloatValue(); };
    auto db  = Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; }).withValueFromStringFunction (parse);
    auto ms  = Attr().withStringFromValueFunction (fmtMs).withValueFromStringFunction (parse);
    auto hz  = Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " Hz"; }).withValueFromStringFunction (parse);
    auto rat = Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + ":1"; }).withValueFromStringFunction (parse);
    auto pct = Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; }).withValueFromStringFunction (parse);

    auto range = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "threshold", 1 }, "Threshold", NormalisableRange<float> (-60.f, 0.f, 0.1f), -24.f, db));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "ratio", 1 }, "Ratio", range (1.f, 20.f, 4.f), 8.f, rat));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "depth", 1 }, "Depth", NormalisableRange<float> (0.f, 36.f, 0.1f), 12.f, db));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack", range (0.1f, 50.f, 3.f), 1.f, ms));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "hold", 1 }, "Hold", range (0.f, 200.f, 30.f), 10.f, ms));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release", range (10.f, 1000.f, 150.f), 120.f, ms));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "focus", 1 }, "SC Focus", range (30.f, 500.f, 100.f), 70.f, hz));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "crossover", 1 }, "Crossover", range (40.f, 500.f, 140.f), 120.f, hz));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "lowOnly", 1 }, "Lows Only", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "listen", 1 }, "SC Listen", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24.f, 24.f, 0.1f), 0.f, db));
    return layout;
}

float raw (const juce::AudioProcessorValueTreeState& s, const char* id) { return s.getRawParameterValue (id)->load(); }
} // namespace

GSideChainProcessor::GSideChainProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GSideChain", createLayout())
{
    startTimerHz (15);
}

GSideChainProcessor::~GSideChainProcessor() { stopTimer(); }

void GSideChainProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    ducker.prepare (sampleRate, samplesPerBlock);
    samplesPerPoint = std::max (1, juce::roundToInt (sampleRate / 200.0)); // 200 display points per second

    learnState = learnIdle;
    startRequest = false;
    learnKick.assign ((size_t) (sampleRate * 4.0), 0.0f); // 4 s capture: ~8 bars of 4/4 kicks at 120 BPM
    learnBass.assign (learnKick.size(), 0.0f);
}

bool GSideChainProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet() != out) return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

gsc::Settings GSideChainProcessor::readSettings() const
{
    gsc::Settings s;
    s.thresholdDb = raw (apvts, "threshold");
    s.ratio       = raw (apvts, "ratio");
    s.depthDb     = raw (apvts, "depth");
    s.attackMs    = raw (apvts, "attack");
    s.holdMs      = raw (apvts, "hold");
    s.releaseMs   = raw (apvts, "release");
    s.focusHz     = raw (apvts, "focus");
    s.crossoverHz = raw (apvts, "crossover");
    s.lowOnly     = raw (apvts, "lowOnly") > 0.5f;
    s.listen      = raw (apvts, "listen") > 0.5f;
    s.mix         = raw (apvts, "mix") * 0.01f;
    s.outputDb    = raw (apvts, "output");
    return s;
}

void GSideChainProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto main = getBusBuffer (buffer, true, 0);

    const float* sc[2] = { nullptr, nullptr };
    int numSc = 0;
    if (auto* scBus = getBus (true, 1); scBus != nullptr && scBus->isEnabled())
    {
        auto scBuf = getBusBuffer (buffer, true, 1);
        numSc = std::min (2, scBuf.getNumChannels());
        for (int c = 0; c < numSc; ++c) sc[c] = scBuf.getReadPointer (c);
    }

    // auto-detect capture requests are handled here so the audio thread owns the capture buffers
    if (cancelRequest.exchange (false) && learnState == learnCapturing) learnState = learnIdle;
    if (learnState != learnReady && startRequest.exchange (false))
    {
        learnPos = 0;
        learnStarted = false;
        learnProgress = 0.0f;
        learnState = learnCapturing;
    }
    const bool capturing = learnState == learnCapturing && ! learnKick.empty();
    const int learnLen = (int) learnKick.size();

    float blockGr = 0.0f;
    const float silentDb = -60.0f;

    ducker.process (main, sc, numSc, readSettings(), [&] (float scMono, float inMono, float detDb, float grDb)
    {
        if (capturing)
        {
            if (! learnStarted && std::abs (scMono) > 0.001f) learnStarted = true; // wait for the kick, not the bass
            if (learnStarted && learnPos < learnLen)
            {
                learnKick[(size_t) learnPos] = scMono;
                learnBass[(size_t) learnPos] = inMono;
                ++learnPos;
            }
        }

        blockGr = std::max (blockGr, grDb);
        scSilent = detDb > silentDb ? 0 : std::min (scSilent + 1, 1 << 30);

        acc.scDb = std::max (acc.scDb, detDb);
        inPeak = std::max (inPeak, std::abs (inMono));
        acc.grDb = std::max (acc.grDb, grDb);
        if (++pointCount >= samplesPerPoint)
        {
            acc.inDb = gsc::toDb (inPeak);
            inPeak = 0.0f;
            int s1, n1, s2, n2;
            fifo.prepareToWrite (1, s1, n1, s2, n2);
            if (n1 > 0) { points[(size_t) s1] = acc; fifo.finishedWrite (1); }
            acc = { -100.0f, -100.0f, 0.0f };
            pointCount = 0;
        }
    });

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    currentGr = blockGr;
    sidechainActive = numSc > 0 && scSilent < (int) sr;

    if (capturing)
    {
        learnProgress = (float) learnPos / (float) std::max (1, learnLen);
        if (learnPos >= learnLen) learnState = learnReady;
    }
}

bool GSideChainProcessor::popPoint (Point& p)
{
    int s1, n1, s2, n2;
    fifo.prepareToRead (1, s1, n1, s2, n2);
    if (n1 == 0) return false;
    p = points[(size_t) s1];
    fifo.finishedRead (1);
    return true;
}

// ---------------------------------------------------------------------------
void GSideChainProcessor::startLearn()
{
    if (isLearning()) return;
    learnProgress = 0.0f;
    startRequest = true;
}

void GSideChainProcessor::cancelLearn()
{
    startRequest = false;
    if (learnState == learnCapturing) cancelRequest = true;
}

bool GSideChainProcessor::isLearning() const
{
    return startRequest.load() || learnState.load() != learnIdle;
}

void GSideChainProcessor::pollLearn()
{
    if (learnState.load() != learnReady) return;

    lastAnalysis = gsc::analyse (learnKick.data(), learnBass.data(), (int) learnKick.size(), sr, readSettings());
    learnState = learnIdle;
    learnProgress = 0.0f;

    if (lastAnalysis.valid)
    {
        applySettings (lastAnalysis.suggested);
        displayProgram = autoProgram;
        updateHostDisplay (ChangeDetails().withProgramChanged (true));
    }
    ++analysisSerial;
}

void GSideChainProcessor::applySettings (const gsc::Settings& s)
{
    auto set = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    };
    set ("threshold", s.thresholdDb);
    set ("ratio", s.ratio);
    set ("depth", s.depthDb);
    set ("attack", s.attackMs);
    set ("hold", s.holdMs);
    set ("release", s.releaseMs);
    set ("focus", s.focusHz);
    set ("crossover", s.crossoverHz);
    set ("lowOnly", s.lowOnly ? 1.0f : 0.0f);
    set ("listen", s.listen ? 1.0f : 0.0f);
    set ("mix", s.mix * 100.0f);
    set ("output", s.outputDb);
}

// ---------------------------------------------------------------------------
juce::StringArray GSideChainProcessor::getPresetNames()
{
    juce::StringArray names;
    for (auto& p : presets) names.add (p.name);
    names.add (autoProgramName);
    return names;
}

// The last program is "Auto Detected", a real slot so the host's program list stays in sync after a detect.
int GSideChainProcessor::getNumPrograms() { return autoProgram + 1; }

const juce::String GSideChainProcessor::getProgramName (int index)
{
    return index == autoProgram ? autoProgramName : juce::isPositiveAndBelow (index, autoProgram) ? presets[index].name : "";
}

void GSideChainProcessor::setCurrentProgram (int index)
{
    if (index == autoProgram) { displayProgram = index; return; } // keeps the current (detected) settings
    if (! juce::isPositiveAndBelow (index, autoProgram)) return;
    const auto& p = presets[index];

    gsc::Settings s = readSettings();
    s.thresholdDb = p.thr;   s.ratio = p.ratio;     s.depthDb = p.depth;
    s.attackMs = p.attack;   s.holdMs = p.hold;     s.releaseMs = p.release;
    s.focusHz = p.focus;     s.crossoverHz = p.xover;
    s.lowOnly = p.lowOnly;   s.listen = false;      s.mix = p.mix * 0.01f;
    applySettings (s);
    displayProgram = index;
}

void GSideChainProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", displayProgram.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void GSideChainProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        displayProgram = juce::jlimit (0, autoProgram, (int) state.getProperty ("program", 0));
        apvts.replaceState (state);
    }
}

juce::AudioProcessorEditor* GSideChainProcessor::createEditor() { return new GSideChainEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GSideChainProcessor(); }
