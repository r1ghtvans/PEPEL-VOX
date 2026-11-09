// Plain C++ DSP checks (no JUCE needed): g++ -O2 -std=c++17 Tests/DspTest.cpp -o dsptest
#include "../Source/Dsp.h"
#include <chrono>
#include <cstdio>
#include <random>
using namespace pepel;

static float measure (const std::vector<float>& x, double sr, int from, int len)
{
    PitchDetector d; d.prepare (sr);
    float f = 0, last = 0; int ok = 0;
    for (int i = from; i < from + len; ++i)
    {
        d.push (x[(size_t) i]);
        if (i % 512 == 0) { if (d.detect (f)) { last = f; ++ok; } }
    }
    return ok > 3 ? last : 0.f;
}
static std::vector<float> voice (double sr, int n, float f0, float vibCents = 0)
{
    std::vector<float> x ((size_t) n);
    double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const double f = f0 * std::pow (2.0, vibCents / 1200.0 * std::sin (2 * M_PI * 5.5 * i / sr));
        ph += f / sr; ph -= std::floor (ph);
        float s = 0;
        for (int k = 1; k <= 18; ++k) s += std::sin (2 * M_PI * k * ph) / (float) k * (k < 6 ? 1.f : 0.6f);
        x[(size_t) i] = 0.3f * s;
    }
    return x;
}
static float rms (const std::vector<float>& x, int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += x[(size_t) i] * x[(size_t) i]; return (float) std::sqrt (s / (b - a)); }

int main()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* msg) { std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", msg); if (! ok) ++fails; };
    for (double sr : { 44100.0, 48000.0 })
    {
        std::printf ("--- sr %.0f\n", sr);
        const int N = (int) sr * 3;
        Engine e; e.prepare (sr);
        Params p; p.atKey = 0; p.atScale = 1; p.atSpeed = 0; p.atHuman = 0; p.atAmount = 100; p.mix = 100;
        auto in = voice (sr, N, 228.f);
        std::vector<float> L = in, R = in;
        for (int i = 0; i < N; i += 256) e.process (L.data() + i, R.data() + i, std::min (256, N - i), p);
        const float f = measure (L, sr, N / 2, N / 2 - 100);
        std::printf ("in 228.0 Hz -> out %.2f Hz (target A3 220.00)\n", f);
        check (std::fabs (f - 220.f) < 1.5f, "hard tune lands on 220 Hz");
        check (rms (L, N / 2, N) > 0.5f * rms (in, N / 2, N), "output level sane");

        Engine e2; e2.prepare (sr);
        Params q; q.atAmount = 0; q.atLock = 0; q.mix = 100;
        L = in; R = in;
        for (int i = 0; i < N; i += 256) e2.process (L.data() + i, R.data() + i, std::min (256, N - i), q);
        const float f2 = measure (L, sr, N / 2, N / 2 - 100);
        std::printf ("amount 0: out %.2f Hz (in 228)\n", f2);
        check (std::fabs (f2 - 228.f) < 1.5f, "amount 0 keeps pitch");

        // vibrato 50 cents, humanize 100 must keep some movement; speed 0 / human 0 must flatten
        auto vib = voice (sr, N, 220.f, 50.f);
        Engine e3; e3.prepare (sr);
        Params h; h.atSpeed = 0; h.atHuman = 0; h.atAmount = 100; h.atScale = 0;
        L = vib; R = vib;
        for (int i = 0; i < N; i += 256) e3.process (L.data() + i, R.data() + i, std::min (256, N - i), h);
        std::vector<float> mm;
        { PitchDetector d; d.prepare (sr); float ff;
          for (int i = N / 2; i < N; ++i) { d.push (L[(size_t) i]); if (i % 512 == 0 && d.detect (ff)) mm.push_back (12 * std::log2 (ff / 440.f)); } }
        float mn = 1e9f, mx = -1e9f; for (float v : mm) { mn = std::min (mn, v); mx = std::max (mx, v); }
        std::printf ("vibrato +-50ct, human 0: output spread %.2f semitones over %zu frames\n", mx - mn, mm.size());
        check (mx - mn < 0.45f, "hard tune flattens vibrato");

        Params lk; lk.atLock = 100; lk.atLockMode = 3; lk.atScale = 1;
        Engine e4; e4.prepare (sr);
        L = in; R = in;
        for (int i = 0; i < N; i += 256) e4.process (L.data() + i, R.data() + i, std::min (256, N - i), lk);
        bool finite = true; float peak = 0;
        for (float v : L) { if (! std::isfinite (v)) finite = false; peak = std::max (peak, std::fabs (v)); }
        std::printf ("lock 100 triad: rms in %.3f out %.3f peak %.3f\n", rms (in, N / 2, N), rms (L, N / 2, N), peak);
        check (finite && peak <= 1.0f, "lock output finite and limited");
        check (rms (L, N / 2, N) > 0.3f * rms (in, N / 2, N) && rms (L, N / 2, N) < 3.f * rms (in, N / 2, N), "lock level matched");

        // everything on, white-noise-ish torture + timing
        Params all; all.bcMix = all.trDepth = all.phMix = all.flMix = all.diMix = all.exMix = all.psMix = all.hmMix = all.grMix = 70; all.trDepth = 70;
        all.atLock = 60; all.bcBits = 6; all.bcRate = 4; all.diDrive = 90; all.psShift = 7; all.grDens = 40; all.grSize = 200; all.atLockMode = 3;
        Engine e5; e5.prepare (sr);
        std::mt19937 rng (1); std::uniform_real_distribution<float> u (-1, 1);
        std::vector<float> A ((size_t) N), B ((size_t) N);
        for (int i = 0; i < N; ++i) { A[(size_t) i] = in[(size_t) i] + 0.2f * u (rng); B[(size_t) i] = A[(size_t) i]; }
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < N; i += 128) e5.process (A.data() + i, B.data() + i, std::min (128, N - i), all);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        finite = true; peak = 0;
        for (int i = 0; i < N; ++i) { if (! std::isfinite (A[(size_t) i]) || ! std::isfinite (B[(size_t) i])) finite = false; peak = std::max (peak, std::fabs (A[(size_t) i])); }
        std::printf ("all effects: %.1f x realtime, peak %.3f\n", 3.0 / sec, peak);
        check (finite && peak <= 1.0f, "all effects on: finite, no overs");
    }
    std::printf (fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
    return fails ? 1 : 0;
}
