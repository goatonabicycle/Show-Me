#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
    const char* PLUGIN_VERSION = "v0.19";
    const char* NOTE_NAMES[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // Colors - matching Show Me style
    const juce::Colour bgDark (18, 18, 22);
    const juce::Colour panelBg (28, 28, 34);
    const juce::Colour textBright (235, 235, 240);
    const juce::Colour textDim (120, 120, 130);
    const juce::Colour activeNote (82, 209, 152);
    const juce::Colour flatColor (200, 100, 100);
    const juce::Colour sharpColor (100, 100, 200);

    juce::String getNoteName (int midiNote)
    {
        if (midiNote < 0) return "-";
        int octave = (midiNote / 12) - 1;
        return juce::String (NOTE_NAMES[midiNote % 12]) + juce::String (octave);
    }
}

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Apply modern look and feel to sliders
    sensitivitySlider.setLookAndFeel (&modernLookAndFeel);
    holdSlider.setLookAndFeel (&modernLookAndFeel);

    // Setup sensitivity slider - horizontal at bottom
    sensitivitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    sensitivitySlider.setRange (0.0, 0.95, 0.01);
    sensitivitySlider.setValue (processorRef.sensitivityThreshold.load());
    sensitivitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 18);
    sensitivitySlider.setColour (juce::Slider::textBoxTextColourId, textDim);
    sensitivitySlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    sensitivitySlider.addListener (this);
    addAndMakeVisible (sensitivitySlider);

    // Setup hold time slider - horizontal at bottom
    holdSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    holdSlider.setRange (0, 10000, 100);  // 0 to 10 seconds in 100ms steps
    holdSlider.setValue (processorRef.holdTimeMs.load());
    holdSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 18);
    holdSlider.setColour (juce::Slider::textBoxTextColourId, textDim);
    holdSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    holdSlider.addListener (this);
    addAndMakeVisible (holdSlider);

    // Setup debug button
    debugButton.setButtonText ("Debug");
    debugButton.setColour (juce::TextButton::buttonColourId, juce::Colour (35, 35, 42));
    debugButton.setColour (juce::TextButton::textColourOffId, textDim);
    debugButton.onClick = [this] { showDebugMenu(); };
    addAndMakeVisible (debugButton);

    setSize (400, 290);  // Cleaner layout with bottom controls
    startTimerHz (10);   // Slower refresh - 10 fps for stable display
}

void AudioPluginAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &sensitivitySlider)
    {
        processorRef.sensitivityThreshold.store ((float) sensitivitySlider.getValue());
    }
    else if (slider == &holdSlider)
    {
        processorRef.holdTimeMs.store ((int) holdSlider.getValue());
    }
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    sensitivitySlider.setLookAndFeel (nullptr);
    holdSlider.setLookAndFeel (nullptr);
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
    // Log current state
    DebugSample sample;
    sample.rms = processorRef.debugRMS.load();
    sample.pitch = processorRef.debugRawPitch.load();
    sample.confidence = processorRef.debugConfidence.load();
    sample.displayedNote = processorRef.detectedMidiNote.load();

    debugLog.push_back(sample);
    if (debugLog.size() > MAX_LOG_SIZE)
        debugLog.pop_front();

    repaint();
}

void AudioPluginAudioProcessorEditor::showDebugMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Copy debug logs to clipboard");
    menu.addSeparator();
    menu.addItem (2, "Show debug panel", true, showDebugPanel);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&debugButton),
        [this] (int result)
        {
            if (result == 1)
                copyLogToClipboard();
            else if (result == 2)
            {
                showDebugPanel = !showDebugPanel;
                setSize (400, showDebugPanel ? 340 : 290);
            }
        });
}

void AudioPluginAudioProcessorEditor::copyLogToClipboard()
{
    juce::String log;
    log += "=== Show Me Audio Debug Log ===\n";
    log += "Sample Rate: " + juce::String(processorRef.getSampleRate()) + " Hz\n";
    log += "Samples: " + juce::String((int)debugLog.size()) + "\n\n";
    log += "RMS,Pitch,Confidence,DisplayedNote\n";

    for (const auto& s : debugLog)
    {
        log += juce::String(s.rms, 8) + ",";
        log += juce::String(s.pitch, 1) + ",";
        log += juce::String(s.confidence, 3) + ",";
        log += juce::String(s.displayedNote) + "\n";
    }

    juce::SystemClipboard::copyTextToClipboard(log);
}

void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgDark);

    float pitch = processorRef.detectedPitch.load();
    float cents = processorRef.detectedCents.load();
    int midiNote = processorRef.detectedMidiNote.load();
    float level = processorRef.signalLevel.load();

    auto bounds = getLocalBounds().reduced (20);

    // Title bar
    g.setColour (panelBg);
    auto titleBar = bounds.removeFromTop (30);
    g.fillRoundedRectangle (titleBar.toFloat(), 4.0f);
    g.setColour (textDim);
    g.setFont (14.0f);
    g.drawText (juce::String ("AUDIO PITCH DETECTOR  ") + PLUGIN_VERSION, titleBar, juce::Justification::centred);

    bounds.removeFromTop (10);

    // Main display area
    auto mainArea = bounds;
    g.setColour (panelBg);
    g.fillRoundedRectangle (mainArea.toFloat(), 6.0f);

    // Note name area
    auto noteArea = mainArea.removeFromTop (90);
    if (midiNote >= 0)
    {
        juce::String noteName = getNoteName (midiNote);
        g.setColour (activeNote);
        g.setFont (juce::Font (72.0f, juce::Font::bold));
        g.drawText (noteName, noteArea, juce::Justification::centred);
    }
    // else: just leave it empty - no "No Signal" text

    // Frequency area
    auto freqArea = mainArea.removeFromTop (30);
    if (pitch > 0.0f)
    {
        g.setColour (textBright);
        g.setFont (24.0f);
        juce::String freqStr = juce::String (pitch, 2) + " Hz";  // Two decimal places like GTune
        g.drawText (freqStr, freqArea, juce::Justification::centred);
    }

    // Cents deviation bar - ALWAYS show this
    auto centsBarArea = mainArea.reduced (30, 10);
    float barHeight = 20.0f;
    float barY = centsBarArea.getCentreY() - barHeight / 2;

    // Background bar
    g.setColour (juce::Colour (40, 40, 50));
    g.fillRoundedRectangle (centsBarArea.getX(), barY, centsBarArea.getWidth(), barHeight, 4.0f);

    // Center line
    float centerX = centsBarArea.getCentreX();
    g.setColour (textDim);
    g.drawVerticalLine ((int) centerX, barY, barY + barHeight);

    // Cents indicator - only show if we have a note
    if (midiNote >= 0)
    {
        float maxCents = 50.0f;
        float normalizedCents = juce::jlimit (-maxCents, maxCents, cents) / maxCents;
        float indicatorX = centerX + normalizedCents * (centsBarArea.getWidth() / 2 - 10);

        juce::Colour indicatorColor = activeNote;
        if (cents < -5.0f) indicatorColor = flatColor;
        else if (cents > 5.0f) indicatorColor = sharpColor;

        g.setColour (indicatorColor);
        g.fillEllipse (indicatorX - 8, barY + barHeight / 2 - 8, 16, 16);
    }

    // Cents text - show below bar if we have a note
    if (midiNote >= 0)
    {
        juce::Colour centsColor = activeNote;
        if (cents < -5.0f) centsColor = flatColor;
        else if (cents > 5.0f) centsColor = sharpColor;

        g.setFont (16.0f);
        juce::String centsStr = (cents >= 0 ? "+" : "") + juce::String ((int) std::round(cents));
        g.setColour (centsColor);
        g.drawText (centsStr, centsBarArea.removeFromBottom (24), juce::Justification::centred);
    }

    // Debug panel - only show if enabled
    if (showDebugPanel)
    {
        auto debugArea = getLocalBounds().removeFromBottom(50).reduced(20, 5);
        g.setColour (juce::Colour(30, 30, 35));
        g.fillRoundedRectangle (debugArea.toFloat(), 4.0f);

        float debugRMS = processorRef.debugRMS.load();
        float debugPitch = processorRef.debugRawPitch.load();
        float debugConf = processorRef.debugConfidence.load();

        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
        g.setColour (juce::Colour(120, 120, 70));

        float threshold = processorRef.sensitivityThreshold.load();
        char debugStr[128];
        snprintf(debugStr, sizeof(debugStr), "RMS: %.6f  Pitch: %.1f Hz  Conf: %.2f (thresh: %.2f)",
                 debugRMS, debugPitch, debugConf, threshold);

        g.drawText (juce::String(debugStr), debugArea, juce::Justification::centred);
    }

    // Slider labels at bottom
    auto controlsArea = getLocalBounds().removeFromBottom (showDebugPanel ? 90 : 40);
    if (showDebugPanel)
        controlsArea.removeFromBottom (50);  // Debug panel space

    g.setColour (textDim);
    g.setFont (11.0f);
    auto labelArea = controlsArea.reduced (20, 0);
    g.drawText ("SENS", labelArea.removeFromLeft (35), juce::Justification::centredLeft);
    labelArea.removeFromLeft (120);  // Skip slider space
    g.drawText ("HOLD", labelArea.removeFromLeft (35), juce::Justification::centredLeft);
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Debug panel at very bottom if enabled
    if (showDebugPanel)
        area.removeFromBottom (50);

    // Controls bar at bottom
    auto controlsArea = area.removeFromBottom (40).reduced (20, 8);

    // Debug button on the right
    debugButton.setBounds (controlsArea.removeFromRight (60));
    controlsArea.removeFromRight (10);  // Spacing

    // Sliders side by side
    auto sensArea = controlsArea.removeFromLeft (155);
    sensArea.removeFromLeft (35);  // Label space
    sensitivitySlider.setBounds (sensArea);

    auto holdArea = controlsArea.removeFromLeft (175);
    holdArea.removeFromLeft (35);  // Label space
    holdSlider.setBounds (holdArea);
}
