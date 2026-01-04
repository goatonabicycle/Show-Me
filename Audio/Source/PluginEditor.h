#pragma once

#include "PluginProcessor.h"
#include <deque>

// Modern slider look and feel
class ModernSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModernSliderLookAndFeel()
    {
        setColour (juce::Slider::backgroundColourId, juce::Colour (40, 40, 50));
        setColour (juce::Slider::trackColourId, juce::Colour (82, 209, 152));
        setColour (juce::Slider::thumbColourId, juce::Colour (235, 235, 240));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        auto trackWidth = 6.0f;
        auto thumbRadius = 8.0f;

        // Track background
        juce::Rectangle<float> track;
        if (style == juce::Slider::LinearVertical)
        {
            float trackX = x + width * 0.5f - trackWidth * 0.5f;
            track = { trackX, (float) y, trackWidth, (float) height };
        }
        else
        {
            float trackY = y + height * 0.5f - trackWidth * 0.5f;
            track = { (float) x, trackY, (float) width, trackWidth };
        }

        g.setColour (slider.findColour (juce::Slider::backgroundColourId));
        g.fillRoundedRectangle (track, trackWidth * 0.5f);

        // Filled portion
        juce::Rectangle<float> filledTrack;
        if (style == juce::Slider::LinearVertical)
        {
            filledTrack = { track.getX(), sliderPos, track.getWidth(), track.getBottom() - sliderPos };
        }
        else
        {
            filledTrack = { track.getX(), track.getY(), sliderPos - x, track.getHeight() };
        }

        g.setColour (slider.findColour (juce::Slider::trackColourId));
        g.fillRoundedRectangle (filledTrack, trackWidth * 0.5f);

        // Thumb
        float thumbX, thumbY;
        if (style == juce::Slider::LinearVertical)
        {
            thumbX = x + width * 0.5f;
            thumbY = sliderPos;
        }
        else
        {
            thumbX = sliderPos;
            thumbY = y + height * 0.5f;
        }

        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.fillEllipse (thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);

        // Subtle shadow/outline on thumb
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.drawEllipse (thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f, 1.0f);
    }
};

class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer,
                                         private juce::Slider::Listener
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void sliderValueChanged (juce::Slider* slider) override;
    void copyLogToClipboard();
    void showDebugMenu();

    AudioPluginAudioProcessor& processorRef;

    // Modern look and feel
    ModernSliderLookAndFeel modernLookAndFeel;

    // Sliders
    juce::Slider sensitivitySlider;
    juce::Slider holdSlider;

    // Debug button
    juce::TextButton debugButton;
    bool showDebugPanel = false;

    // Debug log - stores last N samples
    struct DebugSample {
        float rms;
        float pitch;
        float confidence;
        int displayedNote;
    };
    std::deque<DebugSample> debugLog;
    static constexpr int MAX_LOG_SIZE = 100;  // ~10 seconds at 10fps

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
