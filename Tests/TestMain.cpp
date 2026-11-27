// Self-test: synthetic kick + bass through the analyzer and the full processor,
// preset / state round-trip, and a PNG snapshot of the editor.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

static int failures = 0;

static void check (bool ok, const char* what, double value = 0.0)
{
    std::printf ("%s %-58s %.2f\n", ok ? "ok  " : "FAIL", what, value);
    if (! ok) ++failures;
}

static std::vector<float> makeKick (double sr, int n, double interval)
{
    std::vector<float> x ((size_t) n, 0.0f);
    const int period = (int) (interval * sr);
    for (int start = 0; start < n; start += period)
    {
        double phase = 0.0;
        for (int i = 0; start + i < n && i < (int) (0.45 * sr); ++i)
        {
            const double t = i / sr;
            phase += juce::MathConstants<double>::twoPi * (50.0 + 100.0 * std::exp (-t / 0.03)) / sr; // 150 -> 50 Hz sweep
            x[(size_t) (start + i)] = (float) (0.9 * std::exp (-t / 0.08) * std::sin (phase));
        }
    }
    return x;
}

static std::vector<float> makeBass (double sr, int n, double hz, bool saw)
{
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double ph = std::fmod (i * hz / sr, 1.0);
        x[(size_t) i] = (float) (0.5 * (saw ? 2.0 * ph - 1.0 : std::sin (juce::MathConstants<double>::twoPi * ph)));
    }
    return x;
}

static float rmsDb (const std::vector<float>& x, int from, int to)
{
    double s = 0.0;
    for (int i = from; i < to; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
    return gsc::toDb ((float) std::sqrt (s / std::max (1, to - from)));
}

static float param (GSideChainProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const double sr = 48000.0;
    const int n4 = (int) (4 * sr);

    // ---- 1. analyzer ----
    const auto kick = makeKick (sr, n4, 0.5);
    const auto saw = makeBass (sr, n4, 55.0, true);
    const auto sine = makeBass (sr, n4, 55.0, false);

    const auto a = gsc::analyse (kick.data(), saw.data(), n4, sr, {});
    std::printf ("analysis: %s\n", a.message.toRawUTF8());
    check (a.valid, "kick + saw bass analysed");
    check (a.kickHz > 40 && a.kickHz < 80, "kick fundamental (Hz)", a.kickHz);
    check (std::abs (a.hitIntervalMs - 500) < 15, "hit interval ~500 ms", a.hitIntervalMs);
    check (a.numHits >= 7, "hits found in 4 s", a.numHits);
    check (a.kickDecayMs > 120 && a.kickDecayMs < 400, "kick decay (ms)", a.kickDecayMs);
    check (a.overlap > 0.3f, "low-end overlap", a.overlap);
    check (a.suggested.thresholdDb < a.kickPeakDb - 5, "threshold below kick peak (dB)", a.suggested.thresholdDb);
    check (a.suggested.depthDb >= 4 && a.suggested.depthDb <= 24, "depth (dB)", a.suggested.depthDb);
    check (a.suggested.attackMs >= 0.3f && a.suggested.attackMs <= 10, "attack (ms)", a.suggested.attackMs);
    check (a.suggested.releaseMs >= 15 && a.suggested.releaseMs <= 250, "release fits the groove (ms)", a.suggested.releaseMs);
    check (a.suggested.lowOnly, "harmonic saw -> duck lows only");
    check (std::abs (a.suggested.focusHz - a.kickHz) < 1, "sidechain focus = kick fundamental", a.suggested.focusHz);

    const auto b = gsc::analyse (kick.data(), sine.data(), n4, sr, {});
    check (b.valid && ! b.suggested.lowOnly, "pure sub bass -> full-band duck");

    const std::vector<float> silence ((size_t) n4, 0.0f);
    const auto c = gsc::analyse (silence.data(), saw.data(), n4, sr, {});
    check (! c.valid, "silent sidechain is rejected");

    // ---- 2. presets + state ----
    GSideChainProcessor proc;
    proc.prepareToPlay (sr, 512);
    const int autoSlot = proc.getNumPrograms() - 1;
    check (autoSlot >= 8, "factory presets", autoSlot);
    check (proc.getProgramName (autoSlot) == "Auto Detected", "last program slot is Auto Detected");
    proc.setCurrentProgram (1);
    check (std::abs (param (proc, "threshold") + 32.0f) < 0.05f && param (proc, "depth") > 23.9f, "EDM Pump preset applied");
    proc.setCurrentProgram (3);
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    proc.setCurrentProgram (0);
    proc.setStateInformation (state.getData(), (int) state.getSize());
    check (std::abs (param (proc, "threshold") + 26.0f) < 0.05f && proc.getCurrentProgram() == 3, "state round-trip");

    // ---- 3. full processor: auto-detect, then measure the ducking ----
    const int total = (int) (8 * sr), block = 512;
    const auto kickLong = makeKick (sr, total, 0.5);
    const auto bassLong = makeBass (sr, total, 55.0, true);
    std::vector<float> out ((size_t) total, 0.0f);

    juce::AudioBuffer<float> buf (4, block);
    juce::MidiBuffer midi;
    auto run = [&] (int from, int to, bool withKick)
    {
        for (int pos = from; pos + block <= to; pos += block)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                buf.copyFrom (ch, 0, bassLong.data() + pos, block);
                if (withKick) buf.copyFrom (ch + 2, 0, kickLong.data() + pos, block);
                else buf.clear (ch + 2, 0, block);
            }
            proc.processBlock (buf, midi);
            std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + block, out.begin() + pos);
            proc.pollLearn(); // what the processor's timer does in a host
        }
    };

    proc.startLearn();
    proc.cancelLearn();
    check (! proc.isLearning(), "cancel before any audio arrives");
    proc.startLearn();
    run (0, (int) sr, false);
    check (proc.isLearning() && proc.getLearnProgress() == 0.0f, "capture waits for the kick (bass alone doesn't start it)");
    proc.cancelLearn();
    run (0, block, false);
    check (! proc.isLearning(), "cancel while listening");

    proc.startLearn();
    check (proc.isLearning(), "learn armed");
    run (0, total, true);

    const auto& la = proc.getLastAnalysis();
    std::printf ("in-plugin analysis: %s\n", la.message.toRawUTF8());
    check (la.valid && ! proc.isLearning() && proc.getCurrentProgram() == autoSlot, "auto detect completed, Auto Detected slot active");
    check (std::abs (param (proc, "threshold") - la.suggested.thresholdDb) < 0.2f, "threshold parameter updated", param (proc, "threshold"));
    check (proc.isSidechainActive(), "sidechain detected as active");

    float duck = 0, recover = 0;
    int hits = 0;
    for (double t = 5.0; t < 7.6; t += 0.5, ++hits)
    {
        const int h = (int) (t * sr), ms = (int) (sr / 1000);
        duck += rmsDb (out, h + 10 * ms, h + 50 * ms) - rmsDb (bassLong, h + 10 * ms, h + 50 * ms);
        recover += rmsDb (out, h - 40 * ms, h - 5 * ms) - rmsDb (bassLong, h - 40 * ms, h - 5 * ms);
    }
    check (duck / hits < -3.0f, "bass ducked right after each kick (dB)", duck / hits);
    check (recover / hits > -1.5f, "bass recovered before next kick (dB)", recover / hits);

    // ---- 4. editor snapshot ----
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditorAndMakeActive());
        check (ed != nullptr && ed->getWidth() == 960, "editor created", ed ? ed->getWidth() : 0);
        GSideChainProcessor::Point leftover;
        check (! proc.popPoint (leftover), "points queued while the window was closed are dropped");
        if (auto* e = dynamic_cast<GSideChainEditor*> (ed.get()))
        {
            run (total - (int) (4 * sr), total, true); // fresh audio for the scope
            for (int i = 0; i < 30; ++i) e->refresh(); // let the meter ballistics settle
            const auto img = e->createComponentSnapshot (e->getLocalBounds(), true, 1.0f);
            const juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "gsidechain_ui.png");
            file.deleteFile();
            juce::FileOutputStream os (file);
            juce::PNGImageFormat png;
            check (os.openedOk() && png.writeImageToStream (img, os), "UI snapshot written");
            std::printf ("snapshot: %s\n", file.getFullPathName().toRawUTF8());
        }
    }

    // A host picking "Init" after Auto Detect must load it (JUCE skips program changes equal to getCurrentProgram()).
    proc.setCurrentProgram (0);
    check (proc.getCurrentProgram() == 0 && std::abs (param (proc, "threshold") + 24.0f) < 0.05f, "Init loads after Auto Detect");

    // The scope stops asking for redraws once its whole window is silent.
    {
        ui::ScopeView scope;
        const GSideChainProcessor::Point silent { -100.0f, -100.0f, 0.0f }, loud { -6.0f, -12.0f, 8.0f };
        bool changed = true;
        for (int i = 0; i <= 640; ++i) changed = scope.push (silent);
        check (! changed && scope.push (loud), "idle scope stops redrawing, wakes on signal");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
