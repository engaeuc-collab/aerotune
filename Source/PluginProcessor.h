#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "Params.h"
#include "DSP/AutoTuneEngine.h"

class AeroTuneAudioProcessor : public juce::AudioProcessor
{
public:
    AeroTuneAudioProcessor();
    ~AeroTuneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    AutoTuneEngine& getEngine() { return engine; }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> uiNoteMask { 0xFFF };

private:
    AutoTuneEngine engine;
    std::atomic<float> *pSpeed, *pAmount, *pDetune, *pRef, *pMix, *pOutput, *pRange;
    std::array<std::atomic<float>*, 12> pNote;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AeroTuneAudioProcessor)
};
