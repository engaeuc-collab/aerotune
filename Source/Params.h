#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace AeroParams
{
    inline const juce::StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    inline const juce::StringArray scaleNames { "Chromatic", "Major", "Natural Minor", "Harmonic Minor",
                                                "Major Pentatonic", "Minor Pentatonic", "Blues", "Dorian", "Mixolydian" };
    // bit n = semitone n above the tonic is in the scale
    inline constexpr int scaleMasks[] = { 0xFFF, 0xAB5, 0x5AD, 0x9AD, 0x295, 0x4A9, 0x4E9, 0x6AD, 0x6B5 };

    inline const juce::StringArray rangeNames { "Wide (70-1100 Hz)", "Low Male (70-350 Hz)", "Male / Tenor (100-500 Hz)",
                                                "Female / Alto (150-800 Hz)", "Soprano (220-1100 Hz)" };
    struct Range { float lo, hi; };
    inline constexpr Range ranges[] = { { 70.f, 1100.f }, { 70.f, 350.f }, { 100.f, 500.f }, { 150.f, 800.f }, { 220.f, 1100.f } };
    inline constexpr int numRanges = 5;

    inline juce::String noteId (int i) { return "note" + juce::String (i); }

    inline juce::String noteLabel (int midi)
    {
        const int pc  = ((midi % 12) + 12) % 12;
        const int oct = (int) std::floor ((double) midi / 12.0) - 1;
        return noteNames[pc] + juce::String (oct);
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        auto fmt = [] (const char* unit, int decimals)
        {
            return [=] (float v, int) { return String (v, decimals) + unit; };
        };

        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 },   "Key",   noteNames,  0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scaleNames, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "range", 1 }, "Vocal Range", rangeNames, 0));

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "speed", 1 }, "Retune Speed",
                    NormalisableRange<float> (0.0f, 400.0f, 0.1f, 0.4f), 20.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (fmt (" ms", 0))));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount",
                    NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (fmt (" %", 0))));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "detune", 1 }, "Detune",
                    NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (fmt (" ct", 1))));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "ref", 1 }, "Reference A",
                    NormalisableRange<float> (430.0f, 450.0f, 0.1f), 440.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (fmt (" Hz", 1))));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                    NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (fmt (" %", 0))));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                    NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (fmt (" dB", 1))));

        for (int i = 0; i < 12; ++i)
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { noteId (i), 1 }, "Note " + noteNames[i], true));

        return layout;
    }
}
