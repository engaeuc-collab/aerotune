#include "PluginProcessor.h"
#include "PluginEditor.h"

AeroTuneAudioProcessor::AeroTuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AeroTuneState", AeroParams::createLayout())
{
    pSpeed  = apvts.getRawParameterValue ("speed");
    pAmount = apvts.getRawParameterValue ("amount");
    pDetune = apvts.getRawParameterValue ("detune");
    pRef    = apvts.getRawParameterValue ("ref");
    pMix    = apvts.getRawParameterValue ("mix");
    pOutput = apvts.getRawParameterValue ("output");
    pRange  = apvts.getRawParameterValue ("range");
    for (int i = 0; i < 12; ++i)
        pNote[(size_t) i] = apvts.getRawParameterValue (AeroParams::noteId (i));
}

void AeroTuneAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
}

bool AeroTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

void AeroTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
    if (buffer.getNumChannels() < 1 || n < 1) return;

    TuneSettings s;
    s.speedMs         = pSpeed->load();
    s.amount          = pAmount->load() * 0.01f;
    s.detuneSemitones = pDetune->load() * 0.01f;
    s.refHz           = pRef->load();
    s.mix             = pMix->load() * 0.01f;
    s.outputGain      = juce::Decibels::decibelsToGain (pOutput->load());

    const int ri = juce::jlimit (0, AeroParams::numRanges - 1, (int) pRange->load());
    s.rangeLoHz = AeroParams::ranges[ri].lo;
    s.rangeHiHz = AeroParams::ranges[ri].hi;

    int mask = 0;
    for (int i = 0; i < 12; ++i)
        if (pNote[(size_t) i]->load() > 0.5f) mask |= (1 << i);
    s.noteMask = mask;
    uiNoteMask.store (mask != 0 ? mask : 0xFFF);

    float* left  = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    engine.process (left, right, n, s);
}

juce::AudioProcessorEditor* AeroTuneAudioProcessor::createEditor()
{
    return new AeroTuneAudioProcessorEditor (*this);
}

void AeroTuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AeroTuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AeroTuneAudioProcessor();
}
