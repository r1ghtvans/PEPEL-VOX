// PEPEL VOX - parameter IDs, layout and factory presets
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"

// X-macro lists. Names are identical to the fields of pepel::Params.
#define PEPEL_FLOAT_PARAMS(X)                                                                          \
    X (inGain) X (outGain) X (mix) X (atSpeed) X (atHuman) X (atAmount) X (atLock)                     \
    X (bcBits) X (bcRate) X (bcMix) X (trRate) X (trDepth) X (trShape)                                 \
    X (phRate) X (phDepth) X (phFeed) X (phMix) X (flRate) X (flDepth) X (flFeed) X (flMix)            \
    X (diDrive) X (diTone) X (diMix) X (exFreq) X (exDrive) X (exMix)                                  \
    X (psShift) X (psFine) X (psMix) X (hmV1) X (hmV2) X (hmMix) X (hmSpread)                          \
    X (grSize) X (grDens) X (grPitch) X (grScat) X (grMix)
#define PEPEL_CHOICE_PARAMS(X) X (atKey) X (atScale) X (atLockMode)

namespace pid
{
#define X(n) inline constexpr const char* n = #n;
PEPEL_FLOAT_PARAMS (X)
PEPEL_CHOICE_PARAMS (X)
#undef X
}

namespace params
{
using FP = juce::AudioParameterFloat;

inline std::unique_ptr<FP> mk (const char* id, const juce::String& name, float lo, float hi, float def,
                               const juce::String& unit, int dec, bool sgn = false, float skew = 1.f, float step = 0.f)
{
    juce::NormalisableRange<float> r (lo, hi, step);
    r.skew = skew;
    auto attr = juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([=] (float v, int) {
                        if (std::fabs (v) < 0.5f * std::pow (10.f, (float) -dec)) v = 0.f;
                        juce::String s;
                        if (sgn && v > 0.f) s << "+";
                        s << juce::String (v, dec) << unit;
                        return s;
                    })
                    .withValueFromStringFunction ([] (const juce::String& t) { return t.retainCharacters ("0123456789.-").getFloatValue(); });
    return std::make_unique<FP> (juce::ParameterID { id, 1 }, name, r, def, attr);
}
inline std::unique_ptr<juce::AudioParameterChoice> mkChoice (const char* id, const juce::String& name, juce::StringArray c, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, c, def);
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    // master
    l.add (mk (pid::inGain, "Input", -24, 24, 0, " dB", 1, true));
    l.add (mk (pid::outGain, "Output", -24, 12, 0, " dB", 1, true));
    l.add (mk (pid::mix, "Dry/Wet", 0, 100, 100, "%", 0));
    // auto-tune
    l.add (mkChoice (pid::atKey, "Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    l.add (mkChoice (pid::atScale, "Scale", { "Chroma", "Major", "Minor", "HarmMin", "MajPent", "MinPent", "Blues", "Dorian" }, 1));
    l.add (mk (pid::atSpeed, "Speed", 0, 400, 25, " ms", 0, false, 0.4f));
    l.add (mk (pid::atHuman, "Humanize", 0, 100, 35, "%", 0));
    l.add (mk (pid::atAmount, "Strength", 0, 100, 100, "%", 0));
    l.add (mk (pid::atLock, "Syn-Lock", 0, 100, 0, "%", 0));
    l.add (mkChoice (pid::atLockMode, "Lock Mode", { "Unison", "Octave", "Power", "Triad" }, 0));
    // bitcrusher
    l.add (mk (pid::bcBits, "Bits", 2, 16, 16, " bit", 1));
    l.add (mk (pid::bcRate, "Downsmp", 1, 64, 1, " x", 1, false, 0.4f));
    l.add (mk (pid::bcMix, "Crush Mix", 0, 100, 0, "%", 0));
    // tremolo
    l.add (mk (pid::trRate, "Trem Rate", 0.1f, 30, 5, " Hz", 1, false, 0.4f));
    l.add (mk (pid::trDepth, "Trem Depth", 0, 100, 0, "%", 0));
    l.add (mk (pid::trShape, "Trem Shape", 0, 100, 0, "%", 0));
    // phaser
    l.add (mk (pid::phRate, "Phaser Rate", 0.02f, 8, 0.4f, " Hz", 2, false, 0.4f));
    l.add (mk (pid::phDepth, "Phaser Depth", 0, 100, 60, "%", 0));
    l.add (mk (pid::phFeed, "Phaser Feedback", -95, 95, 40, "%", 0, true));
    l.add (mk (pid::phMix, "Phaser Mix", 0, 100, 0, "%", 0));
    // flanger
    l.add (mk (pid::flRate, "Flanger Rate", 0.02f, 8, 0.25f, " Hz", 2, false, 0.4f));
    l.add (mk (pid::flDepth, "Flanger Depth", 0, 100, 50, "%", 0));
    l.add (mk (pid::flFeed, "Flanger Feedback", -95, 95, 50, "%", 0, true));
    l.add (mk (pid::flMix, "Flanger Mix", 0, 100, 0, "%", 0));
    // distortion
    l.add (mk (pid::diDrive, "Dist Drive", 0, 100, 30, "%", 0));
    l.add (mk (pid::diTone, "Dist Tone", 0, 100, 60, "%", 0));
    l.add (mk (pid::diMix, "Dist Mix", 0, 100, 0, "%", 0));
    // exciter
    l.add (mk (pid::exFreq, "Exciter Freq", 1000, 12000, 4000, " Hz", 0, false, 0.4f));
    l.add (mk (pid::exDrive, "Exciter Drive", 0, 100, 50, "%", 0));
    l.add (mk (pid::exMix, "Exciter Mix", 0, 100, 0, "%", 0));
    // pitch shift
    l.add (mk (pid::psShift, "Pitch Shift", -24, 24, 0, " st", 1, true, 1.f, 0.1f));
    l.add (mk (pid::psFine, "Pitch Fine", -100, 100, 0, " ct", 0, true, 1.f, 1.f));
    l.add (mk (pid::psMix, "Pitch Mix", 0, 100, 0, "%", 0));
    // harmonizer (scale steps)
    l.add (mk (pid::hmV1, "Harm Voice 1", -7, 7, 2, " stp", 0, true, 1.f, 1.f));
    l.add (mk (pid::hmV2, "Harm Voice 2", -7, 7, 4, " stp", 0, true, 1.f, 1.f));
    l.add (mk (pid::hmMix, "Harm Mix", 0, 100, 0, "%", 0));
    l.add (mk (pid::hmSpread, "Harm Spread", 0, 100, 70, "%", 0));
    // granular
    l.add (mk (pid::grSize, "Grain Size", 10, 400, 90, " ms", 0, false, 0.5f));
    l.add (mk (pid::grDens, "Grain Density", 1, 60, 14, " /s", 0, false, 0.6f));
    l.add (mk (pid::grPitch, "Grain Pitch", -24, 24, 0, " st", 1, true, 1.f, 0.1f));
    l.add (mk (pid::grScat, "Grain Scatter", 0, 100, 30, "%", 0));
    l.add (mk (pid::grMix, "Grain Mix", 0, 100, 0, "%", 0));
    return l;
}

// ---------------------------------------------------------------- factory presets (real units, anything not listed = default)
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> v;
};

inline const std::vector<Preset>& presets()
{
    using namespace pid;
    static const std::vector<Preset> p {
        { "INIT", {} },
        { "CLEAN POP TUNE",   { { atKey, 0 }, { atScale, 1 }, { atSpeed, 45 }, { atHuman, 55 }, { atAmount, 85 },
                                { exMix, 25 }, { exDrive, 35 }, { exFreq, 5000 } } },
        { "HARD TUNE T-PAIN", { { atScale, 1 }, { atSpeed, 0 }, { atHuman, 0 }, { atAmount, 100 },
                                { exMix, 20 }, { diMix, 10 }, { diDrive, 15 } } },
        { "TRAP MELODIC",     { { atKey, 9 }, { atScale, 2 }, { atSpeed, 8 }, { atHuman, 25 }, { atAmount, 100 },
                                { hmMix, 24 }, { hmV1, 2 }, { hmV2, 4 }, { exMix, 30 }, { exDrive, 45 },
                                { flMix, 10 }, { diMix, 12 }, { diDrive, 20 } } },
        { "HYPERPOP GLITCH",  { { atScale, 4 }, { atSpeed, 0 }, { atHuman, 0 }, { atAmount, 100 },
                                { psShift, 12 }, { psMix, 28 }, { bcBits, 9 }, { bcRate, 5 }, { bcMix, 30 },
                                { grMix, 22 }, { grSize, 40 }, { grDens, 30 }, { grScat, 60 },
                                { exMix, 45 }, { exDrive, 70 }, { exFreq, 6500 }, { diMix, 25 }, { diDrive, 55 },
                                { trDepth, 25 }, { trRate, 12 }, { trShape, 80 } } },
        { "CLOUD RAP DREAM",  { { atScale, 2 }, { atSpeed, 15 }, { atHuman, 40 }, { atAmount, 90 },
                                { grMix, 35 }, { grSize, 180 }, { grDens, 10 }, { grScat, 25 },
                                { phMix, 35 }, { phRate, 0.25f }, { phDepth, 70 }, { phFeed, 50 },
                                { hmMix, 35 }, { hmV1, 2 }, { hmV2, 4 }, { hmSpread, 90 },
                                { psShift, 12 }, { psMix, 15 }, { trDepth, 15 }, { trRate, 3 } } },
        { "ROBOT CHOIR",      { { atSpeed, 0 }, { atHuman, 0 }, { atAmount, 100 }, { atLock, 80 }, { atLockMode, 3 },
                                { hmMix, 40 }, { flMix, 20 }, { exMix, 20 } } },
        { "VOCODER LEAD",     { { atSpeed, 0 }, { atAmount, 100 }, { atLock, 100 }, { atLockMode, 1 },
                                { exMix, 35 }, { phMix, 20 }, { diMix, 8 } } },
        { "PLUGG UNDERWATER", { { atScale, 5 }, { atSpeed, 18 }, { atAmount, 90 },
                                { bcBits, 12 }, { bcRate, 3 }, { bcMix, 40 },
                                { phMix, 55 }, { phRate, 0.2f }, { phDepth, 85 }, { phFeed, 70 },
                                { trDepth, 20 }, { trRate, 4.5f }, { grMix, 18 }, { grSize, 120 }, { grDens, 18 } } },
        { "RAGE BLOWN OUT",   { { atSpeed, 4 }, { atHuman, 5 }, { atAmount, 100 },
                                { diMix, 70 }, { diDrive, 80 }, { diTone, 55 },
                                { bcBits, 10 }, { bcRate, 2 }, { bcMix, 25 },
                                { exMix, 50 }, { exDrive, 70 }, { exFreq, 3500 }, { hmMix, 18 }, { outGain, -4 } } },
        { "PHONK TAPE",       { { atScale, 2 }, { atSpeed, 30 }, { atHuman, 40 },
                                { diMix, 40 }, { diDrive, 45 }, { diTone, 35 },
                                { flMix, 30 }, { flRate, 0.15f }, { flDepth, 60 }, { flFeed, 40 },
                                { trDepth, 20 }, { trRate, 6 }, { bcBits, 12 }, { bcRate, 1.5f }, { bcMix, 20 } } },
        { "HYPER CHIPMUNK",   { { atSpeed, 10 }, { atAmount, 90 }, { psShift, 7 }, { psMix, 70 }, { exMix, 30 } } },
        { "WIDE DOUBLE",      { { atSpeed, 90 }, { atHuman, 70 }, { atAmount, 40 },
                                { hmMix, 30 }, { hmV1, -2 }, { hmV2, 2 }, { hmSpread, 100 },
                                { flMix, 15 }, { flRate, 0.1f }, { flDepth, 30 }, { flFeed, 20 } } },
        { "GRAIN SPACE",      { { atSpeed, 40 }, { grMix, 70 }, { grSize, 250 }, { grDens, 8 }, { grScat, 80 }, { grPitch, 12 },
                                { phMix, 25 } } },
        { "ALT-POP DARK",     { { atKey, 5 }, { atScale, 2 }, { atSpeed, 60 }, { atHuman, 60 }, { atAmount, 70 },
                                { diMix, 18 }, { diDrive, 30 }, { exMix, 25 }, { hmMix, 20 }, { hmV1, -2 }, { hmV2, 2 } } },
    };
    return p;
}
} // namespace params
