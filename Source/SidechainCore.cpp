#include "SidechainCore.h"

namespace gsc
{
namespace
{
constexpr int fftOrder = 13; // 8192-point frames, ~5.9 Hz bins at 48 kHz

// Welch-averaged power spectrum (bins 0..N/2).
std::vector<float> powerSpectrum (const float* x, int n)
{
    constexpr int N = 1 << fftOrder;
    juce::dsp::FFT fft (fftOrder);
    juce::dsp::WindowingFunction<float> window ((size_t) N, juce::dsp::WindowingFunction<float>::hann, false);
    std::vector<float> acc (N / 2 + 1, 0.0f), buf (2 * N);
    int frames = 0;

    for (int start = 0; frames == 0 || start + N <= n; start += N / 2)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        std::copy (x + start, x + std::min (n, start + N), buf.begin());
        window.multiplyWithWindowingTable (buf.data(), (size_t) N);
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);
        for (int k = 0; k <= N / 2; ++k) acc[(size_t) k] += buf[(size_t) k] * buf[(size_t) k];
        ++frames;
    }

    for (auto& a : acc) a /= (float) frames;
    return acc;
}

float bandSum (const std::vector<float>& p, float binHz, float lo, float hi)
{
    float s = 0.0f;
    for (int k = std::max (1, (int) (lo / binHz)); k <= std::min ((int) p.size() - 1, (int) (hi / binHz)); ++k)
        s += p[(size_t) k];
    return s;
}

// Strongest bin in [lo, hi] refined by parabolic interpolation on log magnitude.
float peakFreq (const std::vector<float>& p, float binHz, float lo, float hi)
{
    const int k0 = std::max (2, (int) (lo / binHz)), k1 = std::min ((int) p.size() - 2, (int) (hi / binHz));
    int best = k0;
    for (int k = k0; k <= k1; ++k)
        if (p[(size_t) k] > p[(size_t) best]) best = k;

    const float a = std::log (p[(size_t) best - 1] + 1e-12f), b = std::log (p[(size_t) best] + 1e-12f),
                c = std::log (p[(size_t) best + 1] + 1e-12f);
    const float denom = a - 2.0f * b + c;
    const float delta = std::abs (denom) > 1e-9f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (a - c) / denom) : 0.0f;
    return ((float) best + delta) * binHz;
}

float median (std::vector<float> v)
{
    if (v.empty()) return 0.0f;
    std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
    return v[v.size() / 2];
}

float rms (const float* x, int n)
{
    double s = 0.0;
    for (int i = 0; i < n; ++i) s += (double) x[i] * x[i];
    return (float) std::sqrt (s / std::max (1, n));
}
} // namespace

Analysis analyse (const float* kick, const float* bass, int n, double sr, const Settings& current)
{
    Analysis a;
    a.suggested = current;

    if (n < (int) (sr * 0.5))
    {
        a.message = "Not enough audio captured - play your kick and bass for a few seconds.";
        return a;
    }

    const float binHz = (float) (sr / (1 << fftOrder));
    const auto ks = powerSpectrum (kick, n), bs = powerSpectrum (bass, n);

    a.hasBass = rms (bass, n) > 0.001f; // -60 dBFS
    a.kickHz = peakFreq (ks, binHz, 30.0f, 200.0f);

    // ---- Kick envelope, seen through the exact detector the engine uses ----
    Detector det;
    det.prepare (sr);
    det.setFocus (a.kickHz);
    const int hop = std::max (1, (int) (sr / 1000.0)); // 1 ms resolution
    std::vector<float> env;
    env.reserve ((size_t) (n / hop + 1));
    float blockMax = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        det.process (kick[i]);
        blockMax = std::max (blockMax, det.env);
        if ((i + 1) % hop == 0) { env.push_back (toDb (blockMax)); blockMax = 0.0f; }
    }

    const int len = (int) env.size();
    const float globalPeak = *std::max_element (env.begin(), env.end());
    a.kickPeakDb = globalPeak;

    if (globalPeak < -50.0f)
    {
        a.message = "No kick found on the sidechain input. Route your kick track to G-SideChain's sidechain and try again.";
        return a;
    }

    // ---- Hit detection: rising past (peak - 9 dB), re-armed after a 12 dB fall ----
    std::vector<int> hits;
    bool armed = true;
    float lastHitPeak = globalPeak;
    for (int i = 1; i < len; ++i)
    {
        if (armed && env[(size_t) i] > globalPeak - 9.0f && env[(size_t) i] >= env[(size_t) i - 1]
            && (hits.empty() || i - hits.back() > 80))
        {
            hits.push_back (i);
            armed = false;
            lastHitPeak = env[(size_t) i];
        }
        else if (! armed)
        {
            lastHitPeak = std::max (lastHitPeak, env[(size_t) i]);
            if (env[(size_t) i] < lastHitPeak - 12.0f) armed = true;
        }
    }

    a.numHits = (int) hits.size();
    if (hits.empty())
    {
        a.message = "Couldn't find clear kick hits on the sidechain. Try a section where the kick plays on its own beat.";
        return a;
    }

    std::vector<float> peaks, rises, bodies, decays, intervals, levelsRel;
    std::vector<int> peakIdx;
    for (size_t h = 0; h < hits.size(); ++h)
    {
        const int start = hits[h];
        const int end = h + 1 < hits.size() ? hits[h + 1] : len;

        int p = start;
        for (int i = start; i < std::min (end, start + 40); ++i)
            if (env[(size_t) i] > env[(size_t) p]) p = i;
        const float pk = env[(size_t) p];
        peaks.push_back (pk);
        peakIdx.push_back (p);

        int r = p;
        while (r > std::max (0, start - 30) && env[(size_t) r - 1] > pk - 20.0f) --r;
        rises.push_back ((float) (p - r));

        int b = p;
        while (b < end && env[(size_t) b] > pk - 6.0f) ++b;
        bodies.push_back ((float) (b - p));

        int d = p;
        while (d < end && env[(size_t) d] > pk - 24.0f) ++d;
        if (d < end || decays.empty()) decays.push_back ((float) (d - p)); // ignore truncated tails when possible

        if (h > 0) intervals.push_back ((float) (hits[h] - hits[h - 1]));
    }

    const float decay = std::max (20.0f, median (decays));
    const float interval = intervals.empty() ? std::max (decay * 2.0f, 500.0f) : median (intervals);
    const float peakDb = median (peaks);
    a.kickDecayMs = decay;
    a.hitIntervalMs = interval;

    // Ducking should be finished well before the next hit.
    const float tEnd = std::min (decay, 0.6f * interval);
    for (size_t h = 0; h < peakIdx.size(); ++h)
    {
        const int at = peakIdx[h] + (int) tEnd;
        const int limit = h + 1 < hits.size() ? hits[h + 1] : len;
        if (at < limit) levelsRel.push_back (env[(size_t) at] - peaks[h]);
    }
    const float relDb = juce::jlimit (-30.0f, -6.0f, levelsRel.empty() ? -24.0f : median (levelsRel));

    // ---- Bass vs kick in the low end ----
    float depth = 10.0f;
    if (a.hasBass)
    {
        a.bassHz = peakFreq (bs, binHz, 25.0f, 300.0f);

        float sk = 0, sb = 0, inter = 0;
        const int k0 = (int) (20.0f / binHz), k1 = (int) (300.0f / binHz);
        for (int k = k0; k <= k1; ++k) { sk += std::sqrt (ks[(size_t) k]); sb += std::sqrt (bs[(size_t) k]); }
        for (int k = k0; k <= k1 && sk > 0 && sb > 0; ++k)
            inter += std::min (std::sqrt (ks[(size_t) k]) / sk, std::sqrt (bs[(size_t) k]) / sb);
        a.overlap = inter;

        const float lowDiffDb = 10.0f * std::log10 ((bandSum (bs, binHz, 20, 250) + 1e-12f) / (bandSum (ks, binHz, 20, 250) + 1e-12f));
        depth = 6.0f + 12.0f * a.overlap + 0.5f * juce::jlimit (-6.0f, 12.0f, lowDiffDb);
    }
    depth = juce::jlimit (4.0f, 24.0f, depth);

    auto& s = a.suggested;
    s.focusHz     = juce::jlimit (30.0f, 500.0f, a.kickHz);
    s.thresholdDb = juce::jlimit (-60.0f, 0.0f, peakDb + relDb);
    s.depthDb     = depth;
    const float slope = std::min (0.95f, depth / std::max (6.0f, -relDb - 6.0f)); // full depth ~6 dB under the peak
    s.ratio       = juce::jlimit (1.5f, 20.0f, 1.0f / (1.0f - slope));
    s.attackMs    = juce::jlimit (0.3f, 10.0f, 0.3f + median (rises) * 0.25f);
    s.holdMs      = juce::jlimit (0.0f, 60.0f, median (bodies) * 0.25f);
    s.releaseMs   = juce::jlimit (15.0f, 500.0f, (interval - tEnd) * 0.25f);
    s.mix         = 1.0f;
    s.listen      = false;

    if (a.hasBass)
    {
        s.crossoverHz = juce::jlimit (60.0f, 300.0f, 2.2f * std::max (a.kickHz, a.bassHz));
        // keep the bass's upper harmonics untouched when it has real energy above the split
        const float upper = bandSum (bs, binHz, s.crossoverHz, 4000.0f) / (bandSum (bs, binHz, 20.0f, 4000.0f) + 1e-12f);
        s.lowOnly = upper > 0.15f;
    }

    a.valid = true;
    a.message = "Kick " + juce::String (juce::roundToInt (a.kickHz)) + " Hz, hit every "
              + juce::String (juce::roundToInt (interval)) + " ms, decay " + juce::String (juce::roundToInt (decay)) + " ms";
    a.message += a.hasBass ? "   |   Bass " + juce::String (juce::roundToInt (a.bassHz)) + " Hz, low-end overlap "
                                 + juce::String (juce::roundToInt (a.overlap * 100.0f)) + "%"
                           : juce::String ("   |   No bass on the main input - generic depth used");
    return a;
}

} // namespace gsc
