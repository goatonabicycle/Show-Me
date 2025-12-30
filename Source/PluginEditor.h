#pragma once

#include "PluginProcessor.h"

class AudioPluginAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    AudioPluginAudioProcessor& processorRef;

    juce::Slider positionSlider;
    juce::Slider rangeSlider;
    juce::Slider stringsSlider;
    juce::Slider fretsSlider;
    
    juce::Label positionLabel;
    juce::Label rangeLabel;
    juce::Label stringsLabel;
    juce::Label fretsLabel;
    
    int currentString = 0;
    int currentFret = 0;
    
    struct NotePosition {
        int stringIndex;
        int fret;
        int midiNote;
    };
    
    NotePosition findOptimalPosition (int midiNote, int preferredPosition, int fingerRange,
                                      int numStrings, int totalFrets);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
