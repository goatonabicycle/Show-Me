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
    void mouseDown (const juce::MouseEvent& e) override;

private:
    AudioPluginAudioProcessor& processorRef;

    // Options panel visibility
    bool optionsPanelOpen = false;
    juce::Rectangle<int> optionsButtonBounds;
    juce::Rectangle<int> optionsPanelBounds;

    // Controls (in options panel)
    juce::Slider positionSlider;
    juce::Slider rangeSlider;
    juce::Slider stringsSlider;
    juce::Slider fretsSlider;
    juce::ComboBox keySelector;
    juce::ComboBox scaleSelector;
    juce::Label positionLabel;
    juce::Label rangeLabel;
    juce::Label stringsLabel;
    juce::Label fretsLabel;
    juce::Label keyLabel;
    juce::Label scaleLabel;

    // Current finger position for optimal note selection
    int currentString = 2;
    int currentFret = 5;

    struct NotePosition {
        int stringIndex;
        int fret;
        int midiNote;
    };

    NotePosition findOptimalPosition (int midiNote, int preferredPosition, int fingerRange,
                                      int numStrings, int totalFrets);
    bool isNoteInScale (int midiNote, int root, int scaleIndex);
    void setControlsVisible (bool visible);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
