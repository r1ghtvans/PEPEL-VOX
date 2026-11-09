// PEPEL VOX - DSP engine. Intentionally JUCE-free so it can be unit-tested with plain g++.
// Audio thread rules: no allocation, no locks. All buffers are allocated in prepare().
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pepel
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float clampf (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float smoothstep (float x) { x = clampf (x, 0.f, 1.f); return x * x * (3.f - 2.f * x); }
inline float softClip (float x)
{
    const float t = 0.85f, a = std::fabs (x);
    if (a <= t) return x;
    const float y = t + (1.f - t) * std::tanh ((a - t) / (1.f - t));
    return x < 0 ? -y : y;
}

// ---------------------------------------------------------------- parameter snapshot
struct Params
{
    float inGain = 0, outGain = 0, mix = 100;
    int   atKey = 0, atScale = 1;
    float atSpeed = 25, atHuman = 35, atAmount = 100, atLock = 0;
    int   atLockMode = 0;
    float bcBits = 16, bcRate = 1, bcMix = 0;
    float trRate = 5, trDepth = 0, trShape = 0;
    float phRate = 0.4f, phDepth = 60, phFeed = 40, phMix = 0;
    float flRate = 0.25f, flDepth = 50, flFeed = 50, flMix = 0;
    float diDrive = 30, diTone = 60, diMix = 0;
    float exFreq = 4000, exDrive = 50, exMix = 0;
    float psShift = 0, psFine = 0, psMix = 0;
    float hmV1 = 2, hmV2 = 4, hmMix = 0, hmSpread = 70;
    float grSize = 90, grDens = 14, grPitch = 0, grScat = 30, grMix = 0;
};

// "How dirty is the sound" 0..1, used to drive the weather visualiser.
inline float computeDirt (const Params& p)
{
    auto n = [] (float v) { return clampf (v * 0.01f, 0.f, 1.f); };
    float s = 0.f;
    const float crush = std::max ((16.f - p.bcBits) / 14.f, (p.bcRate - 1.f) / 20.f);
    s += n (p.bcMix) * (0.45f + 0.55f * clampf (crush, 0.f, 1.f)) * 0.9f;
    s += n (p.diMix) * (0.35f + 0.65f * n (p.diDrive)) * 1.1f;
    s += n (p.exMix) * n (p.exDrive) * 0.45f;
    s += n (p.grMix) * (0.45f + 0.4f * n (p.grScat)) * 0.8f;
    s += n (p.phMix) * std::fabs (n (p.phFeed)) * 0.25f + n (p.phMix) * 0.06f;
    s += n (p.flMix) * std::fabs (n (p.flFeed)) * 0.3f + n (p.flMix) * 0.06f;
    s += n (p.trDepth) * 0.28f;
    s += n (p.psMix) * 0.2f + n (p.hmMix) * 0.2f;
    const float hard = n (p.atAmount) * (1.f - clampf (p.atSpeed / 120.f, 0.f, 1.f)) * (1.f - n (p.atHuman) * 0.7f);
    s += hard * 0.3f + n (p.atLock) * 0.35f;
    s *= (0.4f + 0.6f * n (p.mix));
    return 1.f - std::exp (-1.7f * s);
}

// ---------------------------------------------------------------- musical helpers
inline const std::array<int, 8>& scaleMasks()
{
    auto m = [] (std::initializer_list<int> d) { int r = 0; for (int x : d) r |= 1 << x; return r; };
    static const std::array<int, 8> masks {
        m ({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }), m ({ 0, 2, 4, 5, 7, 9, 11 }), m ({ 0, 2, 3, 5, 7, 8, 10 }),
        m ({ 0, 2, 3, 5, 7, 8, 11 }), m ({ 0, 2, 4, 7, 9 }), m ({ 0, 3, 5, 7, 10 }), m ({ 0, 3, 5, 6, 7, 10 }),
        m ({ 0, 2, 3, 5, 7, 9, 10 }) };
    return masks;
}
inline bool inScale (int scale, int key, int midi)
{
    const int pc = (((midi - key) % 12) + 12) % 12;
    return (scaleMasks()[(size_t) clampf ((float) scale, 0.f, 7.f)] >> pc) & 1;
}
inline int nearestNote (float m, int scale, int key, int current)
{
    if (current > 0 && inScale (scale, key, current) && std::fabs (m - (float) current) < 0.7f) return current;
    const int c = (int) std::lround (m);
    int best = c;
    float bd = 1e9f;
    for (int n = c - 6; n <= c + 6; ++n)
        if (inScale (scale, key, n))
        {
            const float d = std::fabs (m - (float) n);
            if (d < bd) { bd = d; best = n; }
        }
    return best;
}
// semitone offset for moving 'steps' scale degrees from 'note'
inline int stepOffset (int note, int scale, int key, int steps)
{
    if (steps == 0) return 0;
    const int dir = steps > 0 ? 1 : -1;
    int n = note;
    for (int i = 0; i < std::abs (steps); ++i)
    {
        int guard = 0;
        do { n += dir; } while (! inScale (scale, key, n) && ++guard < 12);
    }
    return n - note;
}

// ---------------------------------------------------------------- small blocks
struct Smooth
{
    float cur = 0, target = 0, coef = 1;
    void init (double sr, float ms, float v) { coef = 1.f - std::exp (-1.f / (std::max (ms, 0.01f) * 0.001f * (float) sr)); cur = target = v; }
    void set (float t) { target = t; }
    float next() { cur += coef * (target - cur); return cur; }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    void setBandpass (double sr, float f, float q)
    {
        const float w = kTwoPi * f / (float) sr, al = std::sin (w) / (2.f * q), a0 = 1.f + al;
        b0 = al / a0; b1 = 0; b2 = -al / a0; a1 = -2.f * std::cos (w) / a0; a2 = (1.f - al) / a0;
    }
    void setHighpass (double sr, float f, float q = 0.7071f)
    {
        const float w = kTwoPi * f / (float) sr, c = std::cos (w), al = std::sin (w) / (2.f * q), a0 = 1.f + al;
        b0 = (1.f + c) * 0.5f / a0; b1 = -(1.f + c) / a0; b2 = b0; a1 = -2.f * c / a0; a2 = (1.f - al) / a0;
    }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

struct OnePole
{
    float a = 0.5f, z = 0;
    void setCutoff (double sr, float f) { a = 1.f - std::exp (-kTwoPi * f / (float) sr); }
    float lp (float x) { z += a * (x - z); return z; }
};

struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void prepare (int minSize)
    {
        int n = 16;
        while (n < minSize) n <<= 1;
        buf.assign ((size_t) n, 0.f);
        mask = n - 1;
        w = 0;
    }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // d = 0 -> newest sample
    float read (float d) const
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const int i0 = (w - 1 - i) & mask, i1 = (i0 - 1) & mask;
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * f;
    }
};

struct Lfo
{
    float ph = 0;
    void advance (float hz, double sr) { ph += hz / (float) sr; ph -= std::floor (ph); }
};

// ---------------------------------------------------------------- YIN pitch detector
class PitchDetector
{
public:
    void prepare (double sr)
    {
        dec = std::max (1, (int) std::lround (sr / 22050.0));
        srD = (float) (sr / dec);
        tauMin = std::max (4, (int) (srD / 1000.f));
        tauMax = (int) (srD / 80.f);
        W = tauMax + tauMax / 4;
        ring.assign (4096, 0.f);
        scratch.assign ((size_t) (W + tauMax + 8), 0.f);
        d.assign ((size_t) tauMax + 2, 0.f);
        cm.assign ((size_t) tauMax + 2, 0.f);
        wpos = 0; acc = 0; cnt = 0;
    }
    void push (float x)
    {
        acc += x;
        if (++cnt >= dec) { ring[(size_t) (wpos & 4095)] = acc / (float) dec; ++wpos; acc = 0; cnt = 0; }
    }
    // returns true + f0 when voiced
    bool detect (float& f0)
    {
        const int total = W + tauMax;
        if (wpos < total + 8) return false;
        float e = 0;
        for (int i = 0; i < total; ++i)
        {
            scratch[(size_t) i] = ring[(size_t) ((wpos - total + i) & 4095)];
            e += scratch[(size_t) i] * scratch[(size_t) i];
        }
        if (std::sqrt (e / (float) total) < 0.0025f) return false; // gate ~ -52 dBFS
        float run = 0;
        cm[0] = 1.f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0;
            const float* a = scratch.data();
            const float* b = scratch.data() + tau;
            for (int j = 0; j < W; ++j) { const float df = a[j] - b[j]; s += df * df; }
            d[(size_t) tau] = s;
            run += s;
            cm[(size_t) tau] = run > 1e-12f ? s * (float) tau / run : 1.f;
        }
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (cm[(size_t) tau] < 0.16f)
            {
                while (tau + 1 < tauMax && cm[(size_t) tau + 1] < cm[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        if (best < 0) return false;
        float t = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float s0 = cm[(size_t) best - 1], s1 = cm[(size_t) best], s2 = cm[(size_t) best + 1];
            const float den = s0 + s2 - 2.f * s1;
            if (std::fabs (den) > 1e-9f) t += clampf (0.5f * (s0 - s2) / den, -1.f, 1.f);
        }
        f0 = srD / t;
        return f0 > 70.f && f0 < 1100.f;
    }

private:
    int dec = 2, tauMin = 20, tauMax = 300, W = 600, wpos = 0, cnt = 0;
    float srD = 22050, acc = 0;
    std::vector<float> ring, scratch, d, cm;
};

// ---------------------------------------------------------------- PSOLA style pitch corrector
// Grains of ~2 periods are cut from the input around pitch epochs and re-laid at period/ratio spacing.
// Constant latency (latency()) regardless of pitch.
class Psola
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        int n = 1;
        while ((double) n < sr * 0.25) n <<= 1;
        mask = n - 1;
        for (auto* v : { &inL, &inR, &ana, &accL, &accR, &wsum }) v->assign ((size_t) n, 0.f);
        D = (int) std::lround (sr * 0.040);
        uvPeriod = (float) (sr * 0.005);
        rc = 1.f - std::exp (-1.f / (0.004f * (float) sr));
        reset();
    }
    void reset()
    {
        for (auto* v : { &inL, &inR, &ana, &accL, &accR, &wsum }) std::fill (v->begin(), v->end(), 0.f);
        n = 0; tcNext = (double) D * 0.5; lp = 0; ratio = ratioTarget = 1.f; voiced = false; period = periodS = uvPeriod;
    }
    int latency() const { return D; }
    void setControl (bool v, float periodSamples, float r)
    {
        voiced = v;
        if (v)
        {
            period = periodSamples;
            if (periodS <= 0) periodS = period;
            ratioTarget = r;
        }
        else ratioTarget = 1.f;
    }
    void process (float l, float r, float& oL, float& oR)
    {
        const size_t idx = (size_t) (n & mask);
        inL[idx] = l; inR[idx] = r;
        lp += 0.19f * (0.5f * (l + r) - lp);
        ana[idx] = std::fabs (lp);

        ratio += rc * (ratioTarget - ratio);
        periodS += (voiced ? 0.25f : 0.05f) * ((voiced ? period : uvPeriod) - periodS);
        const float P = periodS;
        const float rEff = voiced ? ratio : 1.f;
        const float pOut = P / rEff;
        const int Pg = std::max (8, (int) std::lround (P));
        const int Hg = std::max (Pg, (int) std::ceil (pOut));

        while ((double) n >= tcNext + Hg + 0.5 * P + 3.0)
        {
            placeGrain (Pg, Hg, voiced);
            tcNext += pOut;
        }
        const int64_t rd = n - D;
        if (rd >= 0)
        {
            const size_t ri = (size_t) (rd & mask);
            const float den = std::max (wsum[ri], 0.25f);
            oL = accL[ri] / den; oR = accR[ri] / den;
            accL[ri] = accR[ri] = wsum[ri] = 0.f;
        }
        else oL = oR = 0.f;
        ++n;
    }

private:
    void placeGrain (int Pg, int Hg, bool snap)
    {
        const int64_t tc = (int64_t) std::llround (tcNext);
        int64_t ta = tc;
        if (snap)
        {
            const int h = Pg / 2;
            float best = -1.f; int bk = 0;
            for (int k = -h; k <= h; ++k)
            {
                const float v = ana[(size_t) ((ta + k) & mask)];
                if (v > best) { best = v; bk = k; }
            }
            ta += bk;
        }
        for (int i = -Hg; i <= Hg; ++i)
        {
            const float w = 0.5f + 0.5f * std::cos (kPi * (float) i / (float) Hg);
            const size_t src = (size_t) ((ta + i) & mask), dst = (size_t) ((tc + i) & mask);
            accL[dst] += w * inL[src]; accR[dst] += w * inR[src]; wsum[dst] += w;
        }
    }
    double sampleRate = 44100, tcNext = 0;
    int mask = 0, D = 1764;
    int64_t n = 0;
    float lp = 0, ratio = 1, ratioTarget = 1, period = 200, periodS = 200, uvPeriod = 220, rc = 0.05f;
    bool voiced = false;
    std::vector<float> inL, inR, ana, accL, accR, wsum;
};

// ---------------------------------------------------------------- auto-tune controller (per hop)
struct TuneController
{
    // outputs
    bool voiced = false;
    float ratio = 1.f, period = 200.f;
    int note = 60;

    void reset() { *this = TuneController(); }

    // call once per detection hop
    void update (bool det, float f0, double sr, float hopMs, const Params& p)
    {
        const float lock = clampf (p.atLock * 0.01f, 0.f, 1.f);
        const float lockTune = std::min (1.f, lock / 0.4f);
        const float strength = lerp (clampf (p.atAmount * 0.01f, 0.f, 1.f), 1.f, lockTune);
        const float speed = p.atSpeed * (1.f - lockTune);
        const float human = clampf (p.atHuman * 0.01f, 0.f, 1.f) * (1.f - lockTune);

        if (det)
        {
            const float m = 69.f + 12.f * std::log2 (f0 / 440.f);
            hist[1] = hist[0];
            // light de-glitch: reject a single frame that jumps more than 3 semitones away from the previous one
            float mMed = m;
            if (histN > 0 && std::fabs (m - hist[1]) > 3.f && std::fabs (hist[0] - hist[1]) < 1.f) mMed = hist[1];
            hist[0] = mMed; histN = std::min (histN + 1, 3);
            if (! voiced || std::fabs (mMed - mSlow) > 1.2f) { mSlow = mMed; corr = (note - mMed); }
            else mSlow += (1.f - std::exp (-hopMs / 80.f)) * (mMed - mSlow);
            voiced = true; unv = 0;
            note = nearestNote (mSlow, p.atScale, p.atKey, note);
            const float desired = ((float) note - mMed) + human * (mMed - mSlow);
            const float tau = speed * (1.f + 2.f * human);
            const float alpha = tau < 0.5f ? 1.f : 1.f - std::exp (-hopMs / tau);
            corr += alpha * (desired - corr);
            const float shift = clampf (strength * corr, -4.f, 4.f);
            ratio = std::exp2 (shift / 12.f);
            period = (float) sr / f0;
        }
        else if (voiced && ++unv > 4)
        {
            voiced = false; ratio = 1.f; corr = 0.f; histN = 0;
        }
    }

private:
    float mSlow = 60.f, corr = 0.f, hist[3] { 60.f, 60.f, 60.f };
    int histN = 0, unv = 0;
};

// ---------------------------------------------------------------- vocoder-style "synth lock"
class SynthLock
{
public:
    static constexpr int N = 16;
    void prepare (double sr_)
    {
        sr = sr_;
        for (int i = 0; i < N; ++i)
        {
            const float f = 110.f * std::pow (7500.f / 110.f, (float) i / (N - 1));
            for (int s = 0; s < 2; ++s)
            {
                modBP[s][i].setBandpass (sr, f, 4.5f);
                for (int c = 0; c < 2; ++c) carBP[c][s][i].setBandpass (sr, f, 4.5f);
            }
        }
        sib.setHighpass (sr, 5000.f);
        atk = 1.f - std::exp (-1.f / (0.0015f * (float) sr));
        rel = 1.f - std::exp (-1.f / (0.012f * (float) sr));
        gainA = 1.f - std::exp (-1.f / (0.08f * (float) sr));
        reset();
    }
    void reset()
    {
        for (auto& e : env) e = 0;
        for (auto& o : osc) o = 0;
        rmsIn = rmsOut = 1e-6f; gain = 1.f;
    }
    // notes: frequencies of chord tones (Hz), count 1..3
    void setNotes (const float* f, int count)
    {
        nNotes = count;
        for (int i = 0; i < count; ++i) freq[i] = f[i];
    }
    void process (float modIn, float& outL, float& outR)
    {
        // carriers: saw (polyBLEP), L slightly flat, R slightly sharp
        float cL = 0, cR = 0;
        for (int i = 0; i < nNotes; ++i)
        {
            cL += saw (osc[i * 2], freq[i] * 0.9965f);
            cR += saw (osc[i * 2 + 1], freq[i] * 1.0035f);
        }
        const float norm = 0.5f / (float) std::max (1, nNotes);
        cL *= norm; cR *= norm;

        float sumL = 0, sumR = 0;
        for (int i = 0; i < N; ++i)
        {
            const float m = modBP[1][i].process (modBP[0][i].process (modIn));
            const float a = std::fabs (m);
            env[i] += (a > env[i] ? atk : rel) * (a - env[i]);
            sumL += carBP[0][1][i].process (carBP[0][0][i].process (cL)) * env[i];
            sumR += carBP[1][1][i].process (carBP[1][0][i].process (cR)) * env[i];
        }
        // level match to the modulator so switching the knob doesn't change loudness
        rmsIn += gainA * (modIn * modIn - rmsIn);
        const float vo = 0.5f * (sumL * sumL + sumR * sumR);
        rmsOut += gainA * (vo - rmsOut);
        const float g = clampf (std::sqrt ((rmsIn + 1e-9f) / (rmsOut + 1e-9f)), 0.1f, 40.f);
        gain += 0.002f * (g - gain);
        const float s = sib.process (modIn) * 0.55f; // keep consonants intelligible
        outL = sumL * gain + s;
        outR = sumR * gain + s;
    }

private:
    float saw (float& ph, float f)
    {
        const float dt = clampf (f / (float) sr, 1e-5f, 0.45f);
        ph += dt; ph -= std::floor (ph);
        float y = 2.f * ph - 1.f;
        if (ph < dt) { const float t = ph / dt; y -= t + t - t * t - 1.f; }
        else if (ph > 1.f - dt) { const float t = (ph - 1.f) / dt; y -= t * t + t + t + 1.f; }
        return y;
    }
    double sr = 44100;
    Biquad modBP[2][N], carBP[2][2][N], sib;
    float env[N] {}, osc[6] {}, freq[3] { 220.f, 220.f, 220.f };
    int nNotes = 1;
    float atk = 0.1f, rel = 0.01f, gainA = 0.001f, rmsIn = 1e-6f, rmsOut = 1e-6f, gain = 1.f;
};

// ---------------------------------------------------------------- simple two-head delay pitch shifter
struct DualShifter
{
    DelayLine dl;
    float len = 1400, ph0 = 0.f, ph1 = 0.5f;
    void prepare (double sr) { len = (float) (0.032 * sr); dl.prepare ((int) len * 2 + 16); ph0 = 0.f; ph1 = 0.5f; }
    void push (float x) { dl.push (x); }
    static float win (float p) { return 0.5f - 0.5f * std::cos (kTwoPi * p); }
    float tick (float ratio)
    {
        const float inc = (1.f - ratio) / len;
        ph0 += inc; ph1 += inc;
        if (ph0 >= 1.f) ph0 -= 1.f; else if (ph0 < 0.f) ph0 += 1.f;
        if (ph1 >= 1.f) ph1 -= 1.f; else if (ph1 < 0.f) ph1 += 1.f;
        return win (ph0) * dl.read (ph0 * len + 1.f) + win (ph1) * dl.read (ph1 * len + 1.f);
    }
};

// ---------------------------------------------------------------- granular
class Granular
{
public:
    void prepare (double sr_)
    {
        sr = sr_;
        bl.prepare ((int) (sr * 2.0)); br.prepare ((int) (sr * 2.0));
        for (auto& g : grains) g.on = false;
        timer = 0;
    }
    void push (float l, float r) { bl.push (l); br.push (r); }
    void tick (float sizeMs, float dens, float pitchSt, float scat, bool spawn, float& oL, float& oR)
    {
        if (spawn && --timer <= 0.f)
        {
            const float interval = (float) sr / std::max (dens, 0.5f);
            timer = interval * (1.f - 0.6f * scat * (rnd() * 0.5f + 0.5f)) + 1.f;
            for (auto& g : grains)
                if (! g.on)
                {
                    const float st = pitchSt + (rnd()) * scat * 12.f;
                    g.inc = std::exp2 (st / 12.f);
                    g.len = std::max (64.f, sizeMs * 0.001f * (float) sr);
                    g.age = 0;
                    g.delay = 2.f + std::max (0.f, g.inc - 1.f) * g.len + (rnd() * 0.5f + 0.5f) * scat * 0.5f * (float) sr;
                    const float pan = clampf (rnd() * scat * 0.9f, -1.f, 1.f);
                    g.gl = std::cos ((pan + 1.f) * kPi * 0.25f) * 1.414f;
                    g.gr = std::sin ((pan + 1.f) * kPi * 0.25f) * 1.414f;
                    g.on = true;
                    break;
                }
        }
        const float overlap = std::max (1.f, dens * sizeMs * 0.001f);
        const float norm = 1.f / std::sqrt (overlap);
        float l = 0, r = 0;
        for (auto& g : grains)
        {
            if (! g.on) continue;
            const float w = 0.5f - 0.5f * std::cos (kTwoPi * g.age / g.len);
            l += w * bl.read (g.delay) * g.gl;
            r += w * br.read (g.delay) * g.gr;
            g.delay += 1.f - g.inc;
            g.delay = clampf (g.delay, 1.f, (float) (bl.mask - 4));
            if (++g.age >= g.len) g.on = false;
        }
        oL = l * norm; oR = r * norm;
    }

private:
    struct Grain { bool on = false; float inc = 1, len = 1000, age = 0, delay = 10, gl = 1, gr = 1; };
    float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) * (1.f / 8388608.f) - 1.f; }
    double sr = 44100;
    DelayLine bl, br;
    std::array<Grain, 40> grains;
    float timer = 0;
    uint32_t seed = 12345u;
};

// ---------------------------------------------------------------- distortion with first-order ADAA
struct AdaaTanh
{
    float x1 = 0;
    static float F (float x) { const float a = std::fabs (x); return a + std::log1p (std::exp (-2.f * a)) - 0.6931472f; }
    float process (float x)
    {
        const float d = x - x1;
        const float y = std::fabs (d) < 1e-4f ? std::tanh (0.5f * (x + x1)) : (F (x) - F (x1)) / d;
        x1 = x;
        return y;
    }
};

// ---------------------------------------------------------------- the whole engine
class Engine
{
public:
    void prepare (double sr_)
    {
        sr = sr_;
        det.prepare (sr);
        psola.prepare (sr);
        tune.reset();
        lock.prepare (sr);
        for (auto& s : psS) s.prepare (sr);
        for (auto& s : h1S) s.prepare (sr);
        for (auto& s : h2S) s.prepare (sr);
        gran.prepare (sr);
        for (int c = 0; c < 2; ++c)
        {
            fl[c].prepare ((int) (sr * 0.03));
            for (auto& z : apZ[c]) z = 0;
            exHp1[c] = Biquad(); exHp2[c] = Biquad(); diAd[c] = AdaaTanh();
            bcHold[c] = 0; phLast[c] = 0; flLast[c] = 0; diLp[c] = OnePole();
        }
        dryDl[0].prepare (psola.latency() + 8); dryDl[1].prepare (psola.latency() + 8);
        hop = std::max (128, (int) (sr * 0.0058));
        hopCount = 0;
        hopMs = 1000.f * (float) hop / (float) sr;
        for (auto& q : noteQ) q = 60.f;
        qDelay = std::max (1, (int) std::ceil ((double) psola.latency() / hop));
        qHead = 0;
        const float t20 = 20.f, t40 = 40.f;
        for (auto* s : { &sIn, &sOut, &sMix, &sBcMix, &sTrDepth, &sPhMix, &sFlMix, &sDiMix, &sExMix, &sPsMix, &sHmMix, &sGrMix, &sVoc })
            s->init (sr, t20, 0.f);
        for (auto* s : { &sBits, &sRate, &sTrRate, &sTrShape, &sPhRate, &sPhDepth, &sPhFeed, &sFlRate, &sFlDepth, &sFlFeed,
                         &sDrive, &sTone, &sExFreq, &sExDrive, &sPs, &sHm1, &sHm2, &sHmSpread, &sGrSize, &sGrDens, &sGrPitch, &sGrScat })
            s->init (sr, t40, 0.f);
        sHm1.init (sr, 60.f, 0.f); sHm2.init (sr, 60.f, 0.f);
        latencySamples = psola.latency();
        primed = false;
    }
    int latency() const { return latencySamples; }

    void process (float* L, float* R, int num, const Params& p)
    {
        // block-rate targets (smoothed per sample)
        sIn.set (dbToGain (p.inGain)); sOut.set (dbToGain (p.outGain)); sMix.set (p.mix * 0.01f);
        sBcMix.set (p.bcMix * 0.01f); sBits.set (p.bcBits); sRate.set (p.bcRate);
        sTrDepth.set (p.trDepth * 0.01f); sTrRate.set (p.trRate); sTrShape.set (p.trShape * 0.01f);
        sPhMix.set (p.phMix * 0.01f); sPhRate.set (p.phRate); sPhDepth.set (p.phDepth * 0.01f); sPhFeed.set (p.phFeed * 0.01f);
        sFlMix.set (p.flMix * 0.01f); sFlRate.set (p.flRate); sFlDepth.set (p.flDepth * 0.01f); sFlFeed.set (p.flFeed * 0.01f);
        sDiMix.set (p.diMix * 0.01f); sDrive.set (p.diDrive * 0.01f); sTone.set (p.diTone * 0.01f);
        sExMix.set (p.exMix * 0.01f); sExFreq.set (p.exFreq); sExDrive.set (p.exDrive * 0.01f);
        sPsMix.set (p.psMix * 0.01f); sPs.set (p.psShift + p.psFine * 0.01f);
        sHmMix.set (p.hmMix * 0.01f); sHmSpread.set (p.hmSpread * 0.01f);
        sGrMix.set (p.grMix * 0.01f); sGrSize.set (p.grSize); sGrDens.set (p.grDens); sGrPitch.set (p.grPitch); sGrScat.set (p.grScat * 0.01f);
        const float lockAmt = clampf (p.atLock * 0.01f, 0.f, 1.f);
        sVoc.set (smoothstep ((lockAmt - 0.25f) / 0.75f));
        if (! primed)
        {
            for (auto* s : { &sIn, &sOut, &sMix, &sBcMix, &sTrDepth, &sPhMix, &sFlMix, &sDiMix, &sExMix, &sPsMix, &sHmMix, &sGrMix, &sVoc,
                             &sBits, &sRate, &sTrRate, &sTrShape, &sPhRate, &sPhDepth, &sPhFeed, &sFlRate, &sFlDepth, &sFlFeed,
                             &sDrive, &sTone, &sExFreq, &sExDrive, &sPs, &sHmSpread, &sGrSize, &sGrDens, &sGrPitch, &sGrScat })
                s->cur = s->target;
            primed = true;
        }
        const int lockMode = p.atLockMode;

        for (int i = 0; i < num; ++i)
        {
            const float rawL = L[i], rawR = R[i];
            const float gIn = sIn.next();
            float l = rawL * gIn, r = rawR * gIn;

            // ---- detection / control
            det.push (0.5f * (l + r));
            if (++hopCount >= hop)
            {
                hopCount = 0;
                float f0 = 0;
                const bool ok = det.detect (f0);
                tune.update (ok, f0, sr, hopMs, p);
                psola.setControl (tune.voiced, tune.period, tune.ratio);
                noteQ[(size_t) (qHead & 15)] = (float) tune.note;
                ++qHead;
                const int dn = (int) noteQ[(size_t) ((qHead - 1 - qDelay) & 15)];
                curNote = dn;
                const float lf = 440.f * std::exp2 (((float) dn - 69.f) / 12.f);
                float fr[3] = { lf, lf, lf };
                int cnt = 1;
                if (lockMode == 1) { fr[1] = lf * 0.5f; cnt = 2; }
                else if (lockMode == 2) { fr[1] = lf * 1.4983f; cnt = 2; }
                else if (lockMode == 3)
                {
                    fr[1] = lf * std::exp2 ((float) stepOffset (dn, p.atScale, p.atKey, 2) / 12.f);
                    fr[2] = lf * std::exp2 ((float) stepOffset (dn, p.atScale, p.atKey, 4) / 12.f);
                    cnt = 3;
                }
                lock.setNotes (fr, cnt);
                hm1Target = (float) stepOffset (dn, p.atScale, p.atKey, (int) std::lround (p.hmV1));
                hm2Target = (float) stepOffset (dn, p.atScale, p.atKey, (int) std::lround (p.hmV2));
                sHm1.set (hm1Target); sHm2.set (hm2Target);
            }

            // ---- pitch correction
            float tl, tr;
            psola.process (l, r, tl, tr);
            dryDl[0].push (rawL); dryDl[1].push (rawR);

            // ---- synth lock (vocoder-like)
            const float vm = sVoc.next();
            if (vm > 0.0005f)
            {
                float vl, vr;
                lock.process (0.5f * (tl + tr), vl, vr);
                tl = lerp (tl, vl, vm); tr = lerp (tr, vr, vm);
            }

            l = tl; r = tr;

            // ---- pitch shift
            {
                psS[0].push (l); psS[1].push (r);
                const float m = sPsMix.next();
                const float ratio = std::exp2 (sPs.next () / 12.f);
                if (m > 0.0005f)
                {
                    const float wl = psS[0].tick (ratio), wr = psS[1].tick (ratio);
                    l = lerp (l, wl, m); r = lerp (r, wr, m);
                }
            }
            // ---- harmonizer (scale aware)
            {
                h1S[0].push (l); h1S[1].push (r); h2S[0].push (l); h2S[1].push (r);
                const float m = sHmMix.next(), sp = sHmSpread.next();
                const float r1 = std::exp2 (sHm1.next() / 12.f), r2 = std::exp2 (sHm2.next() / 12.f);
                if (m > 0.0005f)
                {
                    const float a1 = 0.5f * (h1S[0].tick (r1) + h1S[1].tick (r1)), a2 = 0.5f * (h2S[0].tick (r2) + h2S[1].tick (r2));
                    const float hl = a1 * (0.5f + 0.5f * sp) + a2 * (0.5f - 0.5f * sp);
                    const float hr = a1 * (0.5f - 0.5f * sp) + a2 * (0.5f + 0.5f * sp);
                    const float g = 0.9f * m;
                    l += hl * g; r += hr * g;
                    const float comp = 1.f / (1.f + 0.35f * m);
                    l *= comp; r *= comp;
                }
            }
            // ---- granular
            {
                gran.push (l, r);
                const float m = sGrMix.next();
                const float sz = sGrSize.next(), dn = sGrDens.next(), pt = sGrPitch.next(), sc = sGrScat.next();
                float gl = 0, gr = 0;
                gran.tick (sz, dn, pt, sc, m > 0.0005f, gl, gr);
                if (m > 0.0005f) { l = lerp (l, gl, m); r = lerp (r, gr, m); }
            }
            // ---- exciter
            {
                const float m = sExMix.next(), fr = sExFreq.next(), dr = sExDrive.next();
                if (++exCount >= 32) { exCount = 0; for (int c = 0; c < 2; ++c) { exHp1[c].setHighpass (sr, fr); exHp2[c].setHighpass (sr, std::max (400.f, fr * 0.8f)); } }
                if (m > 0.0005f)
                {
                    const float g = 1.f + dr * 9.f;
                    float* ch[2] = { &l, &r };
                    for (int c = 0; c < 2; ++c)
                    {
                        const float h = exHp1[c].process (*ch[c]);
                        const float t = std::tanh (g * h);
                        const float s = t + 0.35f * t * std::fabs (t);
                        *ch[c] += m * 0.7f * exHp2[c].process (s);
                    }
                }
            }
            // ---- bitcrusher
            {
                const float m = sBcMix.next(), bits = sBits.next(), rate = sRate.next();
                if (m > 0.0005f)
                {
                    const float lv = std::exp2 (std::max (1.f, bits - 1.f));
                    bcPhase += 1.f;
                    if (bcPhase >= rate) { bcPhase -= rate; bcHold[0] = l; bcHold[1] = r; }
                    const float cl = std::floor (bcHold[0] * lv + 0.5f) / lv, cr = std::floor (bcHold[1] * lv + 0.5f) / lv;
                    l = lerp (l, cl, m); r = lerp (r, cr, m);
                }
            }
            // ---- distortion
            {
                const float m = sDiMix.next(), dr = sDrive.next(), tn = sTone.next();
                if (++diCount >= 32) { diCount = 0; const float fc = 900.f * std::pow (18.f, tn); for (int c = 0; c < 2; ++c) diLp[c].setCutoff (sr, fc); }
                if (m > 0.0005f)
                {
                    const float g = dbToGain (dr * 36.f), comp = 1.f / (1.f + 0.05f * g);
                    float* ch[2] = { &l, &r };
                    for (int c = 0; c < 2; ++c)
                    {
                        const float y = diLp[c].lp (diAd[c].process (*ch[c] * g + 0.08f * dr)) * (0.6f + 0.4f * comp * 3.f);
                        *ch[c] = lerp (*ch[c], y * 0.9f, m);
                    }
                }
            }
            // ---- phaser
            {
                const float m = sPhMix.next(), rate = sPhRate.next(), dp = sPhDepth.next(), fb = sPhFeed.next();
                phLfo.advance (rate, sr);
                if (m > 0.0005f)
                {
                    float* ch[2] = { &l, &r };
                    for (int c = 0; c < 2; ++c)
                    {
                        const float lfo = std::sin (kTwoPi * (phLfo.ph + 0.25f * (float) c));
                        const float x = 0.5f + 0.5f * lfo * dp;
                        const float fc = 200.f * std::pow (20.f, x);
                        const float tn = std::tan (kPi * std::min (fc, 0.45f * (float) sr) / (float) sr);
                        const float a = (tn - 1.f) / (tn + 1.f);
                        float y = *ch[c] + fb * phLast[c];
                        for (int s = 0; s < 6; ++s) { const float o = a * y + apZ[c][s]; apZ[c][s] = y - a * o; y = o; }
                        phLast[c] = std::tanh (y);
                        *ch[c] = *ch[c] * (1.f - 0.5f * m) + y * 0.5f * m;
                    }
                }
            }
            // ---- flanger
            {
                const float m = sFlMix.next(), rate = sFlRate.next(), dp = sFlDepth.next(), fb = sFlFeed.next();
                flLfo.advance (rate, sr);
                float* ch[2] = { &l, &r };
                for (int c = 0; c < 2; ++c)
                {
                    const float lfo = std::sin (kTwoPi * (flLfo.ph + 0.25f * (float) c));
                    const float dMs = 3.4f + 2.8f * lfo * dp;
                    const float del = fl[c].read (clampf (dMs * 0.001f * (float) sr, 1.f, (float) fl[c].mask - 4.f));
                    fl[c].push (*ch[c] + clampf (fb, -0.95f, 0.95f) * std::tanh (del));
                    if (m > 0.0005f) *ch[c] = *ch[c] * (1.f - 0.5f * m) + del * 0.5f * m;
                }
            }
            // ---- tremolo
            {
                const float dp = sTrDepth.next(), rate = sTrRate.next(), sh = sTrShape.next();
                trLfo.advance (rate, sr);
                if (dp > 0.0005f)
                {
                    const float k = 1.f + sh * 12.f;
                    const float s = std::tanh (k * std::sin (kTwoPi * trLfo.ph)) / std::tanh (k);
                    const float g = 1.f - dp * (0.5f - 0.5f * s);
                    l *= g; r *= g;
                }
            }
            // ---- master
            const float gO = sOut.next(), mx = sMix.next();
            const float dl = dryDl[0].read ((float) latencySamples - 1.f), dr2 = dryDl[1].read ((float) latencySamples - 1.f);
            l = softClip ((dl + (l - dl) * mx) * gO);
            r = softClip ((dr2 + (r - dr2) * mx) * gO);
            L[i] = l; R[i] = r;
        }
    }
    int currentNote() const { return curNote; }

private:
    double sr = 44100;
    PitchDetector det;
    Psola psola;
    TuneController tune;
    SynthLock lock;
    DualShifter psS[2], h1S[2], h2S[2];
    Granular gran;
    DelayLine fl[2], dryDl[2];
    float apZ[2][6] {};
    float phLast[2] {}, flLast[2] {}, bcHold[2] {}, bcPhase = 0, hm1Target = 0, hm2Target = 0;
    Biquad exHp1[2], exHp2[2];
    AdaaTanh diAd[2];
    OnePole diLp[2];
    Lfo phLfo, flLfo, trLfo;
    int hop = 512, hopCount = 0, qDelay = 3, qHead = 0, exCount = 0, diCount = 0, latencySamples = 1764, curNote = 60;
    float hopMs = 5.8f;
    std::array<float, 16> noteQ {};
    bool primed = false;
    Smooth sIn, sOut, sMix, sBcMix, sTrDepth, sPhMix, sFlMix, sDiMix, sExMix, sPsMix, sHmMix, sGrMix, sVoc;
    Smooth sBits, sRate, sTrRate, sTrShape, sPhRate, sPhDepth, sPhFeed, sFlRate, sFlDepth, sFlFeed;
    Smooth sDrive, sTone, sExFreq, sExDrive, sPs, sHm1, sHm2, sHmSpread, sGrSize, sGrDens, sGrPitch, sGrScat;
};

} // namespace pepel
