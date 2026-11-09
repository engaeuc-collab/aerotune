#pragma once
#include "PluginProcessor.h"
#include "UiComponents.h"

class AeroTuneAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AeroTuneAudioProcessorEditor (AeroTuneAudioProcessor&);
    ~AeroTuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void renderBackground();

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob { juce::Slider slider; juce::Label label; std::unique_ptr<SliderAtt> att; };
    struct Bubble { float x, y, r, speed, phase; };

    AeroTuneAudioProcessor& proc;
    AeroLookAndFeel laf;

    PitchDisplay display;
    NoteKeys keys;

    juce::ComboBox keyBox, scaleBox, rangeBox;
    juce::Label keyLabel, scaleLabel, rangeLabel, hintLabel;
    std::unique_ptr<ComboAtt> keyAtt, scaleAtt, rangeAtt;
    std::array<Knob, 6> knobs;

    juce::Image background;
    std::vector<Bubble> bubbles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AeroTuneAudioProcessorEditor)
};
