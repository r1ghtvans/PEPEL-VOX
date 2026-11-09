#pragma once
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"
#include "Params.h"

class PepelProcessor : public juce::AudioProcessor
{
public:
    PepelProcessor();
    ~PepelProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Pepel Vox"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.3; }

    int getNumPrograms() override { return (int) params::presets().size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    // read by the editor's animation (lock-free)
    std::atomic<float> inputLevel { 0.f }, outputLevel { 0.f }, dirt { 0.f };

private:
    pepel::Params readParams() const;
    struct Raw
    {
#define X(n) std::atomic<float>* n = nullptr;
        PEPEL_FLOAT_PARAMS (X)
        PEPEL_CHOICE_PARAMS (X)
#undef X
    } raw;
    pepel::Engine engine;
    std::vector<float> tmpR;
    int currentPreset = 0;
    double sr = 44100.0;
    float inEnv = 0.f, outEnv = 0.f, dirtEnv = 0.f;
};
