#pragma once
#include <juce_dsp/juce_dsp.h>

namespace gsc
{

struct Settings
{
    float thresholdDb = -24.0f;
    float ratio       = 8.0f;
    float depthDb     = 12.0f;  // maximum gain reduction
    float attackMs    = 1.0f;
    float holdMs      = 10.0f;
    float releaseMs   = 120.0f;
    float focusHz     = 70.0f;  // sidechain band-pass centre (kick fundamental)
    float crossoverHz = 120.0f; // split point when only the lows are ducked
    float mix         = 1.0f;
    float outputDb    = 0.0f;
    bool  lowOnly     = false;
    bool  listen      = false;
};

inline float toDb (float gain) { return 20.0f * std::log10 (std::max (gain, 1.0e-6f)); }

// Soft-knee (6 dB) downward gain computer, result in dB of reduction (>= 0), capped at depth.
inline float computeGr (float detDb, float thresholdDb, float slope, float depthDb)
{
    constexpr float knee = 6.0f;
    const float over = detDb - thresholdDb;
    float gr = 0.0f;
    if (over >= knee * 0.5f)       gr = slope * over;
    else if (over > -knee * 0.5f)  gr = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
    return std::min (gr, depthDb);
}

// Band-passed peak follower. Shared by the realtime engine and the analyzer so
// auto-detected thresholds match exactly what the engine sees.
struct Detector
{
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        bp.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        bp.setResonance (juce::MathConstants<float>::sqrt2 * 0.5f);
        bp.prepare ({ sampleRate, 4096u, 1u });
        freq = -1.0f;
        reset();
    }

    void reset() { bp.reset(); env = 0.0f; }

    void setFocus (float hz)
    {
        hz = juce::jlimit (20.0f, (float) sr * 0.45f, hz);
        if (hz == freq) return;
        freq = hz;
        bp.setCutoffFrequency (hz);
        // release ~2 periods of the focus frequency keeps ripple low on sub-heavy kicks
        const float relMs = std::max (10.0f, 2000.0f / hz);
        rel = std::exp (-1.0f / (0.001f * relMs * (float) sr));
    }

    float process (float x)
    {
        const float f = bp.processSample (0, x) * juce::MathConstants<float>::sqrt2; // 0 dB at centre
        const float a = std::abs (f);
        env = a > env ? a : env * rel;
        return f;
    }

    juce::dsp::StateVariableTPTFilter<float> bp;
    double sr = 44100.0;
    float env = 0.0f, rel = 0.0f, freq = -1.0f;
};

class Ducker
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        detector.prepare (sampleRate);
        xover.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        xover.prepare ({ sampleRate, (juce::uint32) std::max (1, maxBlock), 2u });
        xoverHz = -1.0f;
        mixSm.reset (sampleRate, 0.02);
        outSm.reset (sampleRate, 0.02);
        reset();
    }

    void reset()
    {
        detector.reset();
        xover.reset();
        gr = 0.0f;
        holdLeft = 0;
    }

    // io: main bus (1-2 channels, processed in place). sc: sidechain channels (may be null).
    // sink (scMono, inMono, detDb, grDb) is called once per sample for metering / capture.
    template <typename Sink>
    void process (juce::AudioBuffer<float>& io, const float* const* sc, int numSc, const Settings& s, Sink&& sink)
    {
        const int numCh = std::min (io.getNumChannels(), 2);
        const int n = io.getNumSamples();
        if (numCh == 0) return;

        detector.setFocus (s.focusHz);
        if (s.crossoverHz != xoverHz) { xoverHz = s.crossoverHz; xover.setCutoffFrequency (xoverHz); }
        if (s.lowOnly != lastLowOnly) { xover.reset(); lastLowOnly = s.lowOnly; }

        const float atk = coef (s.attackMs), rel = coef (s.releaseMs);
        const int holdLen = (int) (s.holdMs * 0.001f * (float) sr);
        const float slope = 1.0f - 1.0f / std::max (1.0f, s.ratio);
        mixSm.setTargetValue (s.mix);
        outSm.setTargetValue (juce::Decibels::decibelsToGain (s.outputDb));

        float* ch[2] = { io.getWritePointer (0), numCh > 1 ? io.getWritePointer (1) : nullptr };

        for (int i = 0; i < n; ++i)
        {
            float x = 0.0f;
            for (int c = 0; c < numSc; ++c) x += sc[c][i];
            if (numSc > 0) x /= (float) numSc;

            const float filtered = detector.process (x);
            const float detDb = toDb (detector.env);
            const float target = computeGr (detDb, s.thresholdDb, slope, s.depthDb);

            if (target >= gr)          { gr = target + atk * (gr - target); holdLeft = holdLen; }
            else if (holdLeft > 0)     { --holdLeft; }
            else                       { gr = target + rel * (gr - target); }

            const float g = 1.0f - mixSm.getNextValue() * (1.0f - std::exp (-gr * 0.11512925f));
            const float out = outSm.getNextValue();

            float inMono = 0.0f;
            for (int c = 0; c < numCh; ++c)
            {
                float& smp = ch[c][i];
                inMono += smp;
                float y;
                if (s.lowOnly)
                {
                    float lo, hi;
                    xover.processSample (c, smp, lo, hi);
                    y = lo * g + hi;
                }
                else
                {
                    y = smp * g;
                }
                smp = s.listen ? filtered : y * out;
            }

            sink (x, inMono / (float) numCh, detDb, gr);
        }
    }

private:
    float coef (float ms) const { return std::exp (-1.0f / (std::max (0.01f, ms) * 0.001f * (float) sr)); }

    double sr = 44100.0;
    Detector detector;
    juce::dsp::LinkwitzRileyFilter<float> xover;
    juce::SmoothedValue<float> mixSm { 1.0f }, outSm { 1.0f };
    float xoverHz = -1.0f, gr = 0.0f;
    int holdLeft = 0;
    bool lastLowOnly = false;
};

struct Analysis
{
    bool valid = false;
    juce::String message;
    float kickHz = 0, bassHz = 0, hitIntervalMs = 0, kickDecayMs = 0, kickPeakDb = -100, overlap = 0;
    int numHits = 0;
    bool hasBass = false;
    Settings suggested;
};

// Inspect captured kick (sidechain) and bass (main input) audio and derive ducking settings.
Analysis analyse (const float* kick, const float* bass, int numSamples, double sampleRate, const Settings& current);

} // namespace gsc
