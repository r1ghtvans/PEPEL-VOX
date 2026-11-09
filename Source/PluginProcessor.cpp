#include "PluginProcessor.h"
#include "PluginEditor.h"

PepelProcessor::PepelProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PEPEL", params::createLayout())
{
#define X(n) raw.n = apvts.getRawParameterValue (pid::n);
    PEPEL_FLOAT_PARAMS (X)
    PEPEL_CHOICE_PARAMS (X)
#undef X
    tmpR.assign (4096, 0.f);
}

bool PepelProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::stereo() || o == juce::AudioChannelSet::mono()) && o == l.getMainInputChannelSet();
}

void PepelProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    engine.prepare (sampleRate);
    setLatencySamples (engine.latency());
}

pepel::Params PepelProcessor::readParams() const
{
    pepel::Params p;
#define X(n) p.n = raw.n->load();
    PEPEL_FLOAT_PARAMS (X)
#undef X
#define X(n) p.n = (int) std::lround (raw.n->load());
    PEPEL_CHOICE_PARAMS (X)
#undef X
    return p;
}

void PepelProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = buffer.getNumChannels(), n = buffer.getNumSamples();
    if (numCh < 1 || n < 1) return;
    const auto p = readParams();

    float* l = buffer.getWritePointer (0);
    float* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    float inPk = 0.f;
    for (int i = 0; i < n; ++i) inPk = std::max (inPk, std::fabs (l[i]));

    for (int s = 0; s < n; s += (int) tmpR.size())
    {
        const int c = std::min ((int) tmpR.size(), n - s);
        if (r != nullptr) engine.process (l + s, r + s, c, p);
        else
        {
            std::copy (l + s, l + s + c, tmpR.begin());
            engine.process (l + s, tmpR.data(), c, p);
            for (int i = 0; i < c; ++i) l[s + i] = 0.5f * (l[s + i] + tmpR[(size_t) i]);
        }
    }
    float outPk = 0.f;
    for (int i = 0; i < n; ++i) outPk = std::max (outPk, std::fabs (l[i]));

    const float blockSec = (float) n / (float) sr;
    const float a = 1.f - std::exp (-blockSec / 0.12f);
    inEnv = std::max (inPk, inEnv + a * (inPk - inEnv));
    outEnv = std::max (outPk, outEnv + a * (outPk - outEnv));
    dirtEnv += (1.f - std::exp (-blockSec / 0.35f)) * (pepel::computeDirt (p) - dirtEnv);
    inputLevel.store (inEnv); outputLevel.store (outEnv); dirt.store (dirtEnv);
}

juce::AudioProcessorEditor* PepelProcessor::createEditor() { return new Editor (*this); }

const juce::String PepelProcessor::getProgramName (int i)
{
    const auto& p = params::presets();
    return i >= 0 && i < (int) p.size() ? juce::String (p[(size_t) i].name) : juce::String();
}

void PepelProcessor::setCurrentProgram (int index)
{
    const auto& all = params::presets();
    if (index < 0 || index >= (int) all.size()) return;
    currentPreset = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& kv : all[(size_t) index].v)
        if (auto* rp = apvts.getParameter (kv.first))
            rp->setValueNotifyingHost (rp->convertTo0to1 (kv.second));
}

void PepelProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PepelProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto t = juce::ValueTree::fromXml (*xml);
            currentPreset = juce::jlimit (0, getNumPrograms() - 1, (int) t.getProperty ("preset", 0));
            apvts.replaceState (t);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PepelProcessor(); }
