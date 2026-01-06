#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
    const char* PLUGIN_VERSION = "v0.24";
    const char* PLUGIN_TITLE = "Billions of Notes";
    const char* NOTE_NAMES[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // Guitar tuning (standard 6-string, can extend to 8)
    const int GUITAR_TUNING[8] = { 64, 59, 55, 50, 45, 40, 35, 30 };  // E4, B3, G3, D3, A2, E2, B1, F#1

    // Scale definitions
    const char* SCALE_NAMES[] = {
        "Chromatic", "Major", "Minor", "Harmonic Minor", "Melodic Minor",
        "Pentatonic Maj", "Pentatonic Min", "Blues", "Dorian", "Phrygian",
        "Lydian", "Mixolydian", "Locrian"
    };
    const int NUM_SCALES = 13;

    const int SCALE_PATTERNS[13][12] = {
        { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },  // Chromatic
        { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 },  // Major
        { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 },  // Minor
        { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1 },  // Harmonic Minor
        { 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1 },  // Melodic Minor
        { 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0 },  // Pentatonic Maj
        { 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0 },  // Pentatonic Min
        { 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0 },  // Blues
        { 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0 },  // Dorian
        { 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0 },  // Phrygian
        { 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1 },  // Lydian
        { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0 },  // Mixolydian
        { 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0 },  // Locrian
    };

    // Colors - matching Show Me style
    const juce::Colour bgDark (18, 18, 22);
    const juce::Colour panelBg (28, 28, 34);
    const juce::Colour textBright (235, 235, 240);
    const juce::Colour textDim (120, 120, 130);
    const juce::Colour activeNoteColor (82, 209, 152);
    const juce::Colour flatColor (200, 100, 100);
    const juce::Colour sharpColor (100, 100, 200);

    // Fretboard colors
    const juce::Colour controlBg (38, 38, 46);
    const juce::Colour controlBorder (58, 58, 68);
    const juce::Colour accentBlue (100, 160, 220);
    const juce::Colour rootNoteColor (200, 110, 90);
    const juce::Colour scaleNoteColor (80, 140, 190);
    const juce::Colour outOfScaleColor (42, 40, 38);
    const juce::Colour fretboardCol (38, 32, 26);
    const juce::Colour fretMetal (120, 115, 105);
    const juce::Colour nutBone (220, 215, 200);

    juce::String getNoteName (int midiNote)
    {
        if (midiNote < 0) return "-";
        int octave = (midiNote / 12) - 1;
        return juce::String (NOTE_NAMES[midiNote % 12]) + juce::String (octave);
    }

    juce::String noteNameOnly (int midiNote)
    {
        return juce::String (NOTE_NAMES[midiNote % 12]);
    }

    bool isNoteInScale (int midiNote, int root, int scaleIndex)
    {
        int interval = (midiNote - root + 120) % 12;
        return SCALE_PATTERNS[scaleIndex][interval] == 1;
    }
}

// ModernLookAndFeel implementation
ModernLookAndFeel::ModernLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, controlBg);
    setColour (juce::ComboBox::outlineColourId, controlBorder);
    setColour (juce::ComboBox::textColourId, textBright);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, controlBg);
    setColour (juce::PopupMenu::textColourId, textBright);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentBlue);
    setColour (juce::PopupMenu::highlightedTextColourId, textBright);
}

void ModernLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                       int, int, int, int, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat();
    g.setColour (controlBg);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (controlBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    // Arrow
    auto arrowZone = bounds.removeFromRight (24).reduced (8, 10);
    juce::Path arrow;
    arrow.addTriangle (arrowZone.getX(), arrowZone.getY(),
                      arrowZone.getRight(), arrowZone.getY(),
                      arrowZone.getCentreX(), arrowZone.getBottom());
    g.setColour (textDim);
    g.fillPath (arrow);
}

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Apply modern look and feel
    sensitivitySlider.setLookAndFeel (&modernLookAndFeel);
    holdSlider.setLookAndFeel (&modernLookAndFeel);
    positionSlider.setLookAndFeel (&modernLookAndFeel);
    rangeSlider.setLookAndFeel (&modernLookAndFeel);
    stringsSlider.setLookAndFeel (&modernLookAndFeel);
    fretsSlider.setLookAndFeel (&modernLookAndFeel);
    keySelector.setLookAndFeel (&comboLookAndFeel);
    scaleSelector.setLookAndFeel (&comboLookAndFeel);

    // Tuner controls - sensitivity (shown in debug panel)
    sensitivitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    sensitivitySlider.setRange (0.0, 0.95, 0.01);
    sensitivitySlider.setValue (processorRef.sensitivityThreshold.load());
    sensitivitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 18);
    sensitivitySlider.setColour (juce::Slider::textBoxTextColourId, textDim);
    sensitivitySlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    sensitivitySlider.addListener (this);
    addChildComponent (sensitivitySlider);  // Hidden by default
    sensLabel.setText ("SENS", juce::dontSendNotification);
    sensLabel.setColour (juce::Label::textColourId, textDim);
    sensLabel.setFont (juce::Font (10.0f));
    addChildComponent (sensLabel);

    // Hold slider (shown in debug panel)
    holdSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    holdSlider.setRange (0, 10000, 100);
    holdSlider.setValue (processorRef.holdTimeMs.load());
    holdSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 18);
    holdSlider.setColour (juce::Slider::textBoxTextColourId, textDim);
    holdSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    holdSlider.addListener (this);
    addChildComponent (holdSlider);  // Hidden by default
    holdLabel.setText ("HOLD", juce::dontSendNotification);
    holdLabel.setColour (juce::Label::textColourId, textDim);
    holdLabel.setFont (juce::Font (10.0f));
    addChildComponent (holdLabel);

    // Key selector
    for (int i = 0; i < 12; ++i)
        keySelector.addItem (NOTE_NAMES[i], i + 1);
    keySelector.setSelectedId (1);
    addAndMakeVisible (keySelector);
    keyLabel.setText ("KEY", juce::dontSendNotification);
    keyLabel.setColour (juce::Label::textColourId, textDim);
    keyLabel.setFont (juce::Font (10.0f));
    addAndMakeVisible (keyLabel);

    // Scale selector
    for (int i = 0; i < NUM_SCALES; ++i)
        scaleSelector.addItem (SCALE_NAMES[i], i + 1);
    scaleSelector.setSelectedId (2);  // Default to Major
    addAndMakeVisible (scaleSelector);
    scaleLabel.setText ("SCALE", juce::dontSendNotification);
    scaleLabel.setColour (juce::Label::textColourId, textDim);
    scaleLabel.setFont (juce::Font (10.0f));
    addAndMakeVisible (scaleLabel);

    // Position slider
    positionSlider.setRange (0, 18, 1);
    positionSlider.setValue (0);
    positionSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    positionSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 28, 18);
    positionSlider.setColour (juce::Slider::textBoxTextColourId, textDim);
    positionSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (positionSlider);
    positionLabel.setText ("POSITION", juce::dontSendNotification);
    positionLabel.setColour (juce::Label::textColourId, textDim);
    positionLabel.setFont (juce::Font (10.0f));
    addAndMakeVisible (positionLabel);

    // Range slider
    rangeSlider.setRange (3, 8, 1);
    rangeSlider.setValue (5);
    rangeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    rangeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 28, 18);
    rangeSlider.setColour (juce::Slider::textBoxTextColourId, textDim);
    rangeSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (rangeSlider);
    rangeLabel.setText ("RANGE", juce::dontSendNotification);
    rangeLabel.setColour (juce::Label::textColourId, textDim);
    rangeLabel.setFont (juce::Font (10.0f));
    addAndMakeVisible (rangeLabel);

    // Strings slider
    stringsSlider.setRange (4, 8, 1);
    stringsSlider.setValue (6);
    stringsSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    stringsSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 28, 18);
    stringsSlider.setColour (juce::Slider::textBoxTextColourId, textDim);
    stringsSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (stringsSlider);
    stringsLabel.setText ("STRINGS", juce::dontSendNotification);
    stringsLabel.setColour (juce::Label::textColourId, textDim);
    stringsLabel.setFont (juce::Font (10.0f));
    addAndMakeVisible (stringsLabel);

    // Frets slider
    fretsSlider.setRange (12, 24, 1);
    fretsSlider.setValue (22);
    fretsSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    fretsSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 28, 18);
    fretsSlider.setColour (juce::Slider::textBoxTextColourId, textDim);
    fretsSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (fretsSlider);
    fretsLabel.setText ("FRETS", juce::dontSendNotification);
    fretsLabel.setColour (juce::Label::textColourId, textDim);
    fretsLabel.setFont (juce::Font (10.0f));
    addAndMakeVisible (fretsLabel);

    // Debug button
    debugButton.setButtonText ("Debug");
    debugButton.setColour (juce::TextButton::buttonColourId, juce::Colour (35, 35, 42));
    debugButton.setColour (juce::TextButton::textColourOffId, textDim);
    debugButton.onClick = [this] { showDebugMenu(); };
    addAndMakeVisible (debugButton);

    setResizable (true, true);
    setResizeLimits (800, 400, 1600, 700);
    setSize (1000, 480);
    startTimerHz (15);
}

void AudioPluginAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    if (slider == &sensitivitySlider)
        processorRef.sensitivityThreshold.store ((float) sensitivitySlider.getValue());
    else if (slider == &holdSlider)
        processorRef.holdTimeMs.store ((int) holdSlider.getValue());
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    sensitivitySlider.setLookAndFeel (nullptr);
    holdSlider.setLookAndFeel (nullptr);
    positionSlider.setLookAndFeel (nullptr);
    rangeSlider.setLookAndFeel (nullptr);
    stringsSlider.setLookAndFeel (nullptr);
    fretsSlider.setLookAndFeel (nullptr);
    keySelector.setLookAndFeel (nullptr);
    scaleSelector.setLookAndFeel (nullptr);
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
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
                // Show/hide sensitivity and hold controls with debug panel
                sensitivitySlider.setVisible (showDebugPanel);
                sensLabel.setVisible (showDebugPanel);
                holdSlider.setVisible (showDebugPanel);
                holdLabel.setVisible (showDebugPanel);
                resized();
                repaint();
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

void AudioPluginAudioProcessorEditor::drawTuner (juce::Graphics& g, juce::Rectangle<int> area, int midiNote, float pitch, float cents)
{
    g.setColour (panelBg);
    g.fillRoundedRectangle (area.toFloat(), 6.0f);

    auto tunerArea = area.reduced (10, 5);

    // Note name - large on left
    auto noteArea = tunerArea.removeFromLeft (100);
    if (midiNote >= 0)
    {
        g.setColour (activeNoteColor);
        g.setFont (juce::Font (48.0f, juce::Font::bold));
        g.drawText (getNoteName (midiNote), noteArea, juce::Justification::centred);
    }

    // Frequency
    auto freqArea = tunerArea.removeFromLeft (100);
    if (pitch > 0.0f)
    {
        g.setColour (textBright);
        g.setFont (18.0f);
        g.drawText (juce::String (pitch, 2) + " Hz", freqArea, juce::Justification::centred);
    }

    // Cents bar
    auto centsBarArea = tunerArea.reduced (20, 15);
    float barHeight = 16.0f;
    float barY = centsBarArea.getCentreY() - barHeight / 2;

    g.setColour (juce::Colour (40, 40, 50));
    g.fillRoundedRectangle ((float) centsBarArea.getX(), barY, (float) centsBarArea.getWidth(), barHeight, 4.0f);

    float centerX = (float) centsBarArea.getCentreX();
    g.setColour (textDim);
    g.drawVerticalLine ((int) centerX, barY, barY + barHeight);

    if (midiNote >= 0)
    {
        float maxCents = 50.0f;
        float normalizedCents = juce::jlimit (-maxCents, maxCents, cents) / maxCents;
        float indicatorX = centerX + normalizedCents * (centsBarArea.getWidth() / 2.0f - 8);

        juce::Colour indicatorColor = activeNoteColor;
        if (cents < -5.0f) indicatorColor = flatColor;
        else if (cents > 5.0f) indicatorColor = sharpColor;

        g.setColour (indicatorColor);
        g.fillEllipse (indicatorX - 6, barY + barHeight / 2 - 6, 12, 12);

        // Cents text
        g.setFont (14.0f);
        juce::String centsStr = (cents >= 0 ? "+" : "") + juce::String ((int) std::round(cents));
        g.drawText (centsStr, centsBarArea.removeFromRight (50), juce::Justification::centredRight);
    }
}

void AudioPluginAudioProcessorEditor::drawFretboard (juce::Graphics& g, juce::Rectangle<int> area, int midiNote)
{
    const int position = (int) positionSlider.getValue();
    const int range = (int) rangeSlider.getValue();
    const int key = keySelector.getSelectedId() - 1;
    const int scale = scaleSelector.getSelectedId() - 1;
    const int numStrings = (int) stringsSlider.getValue();
    const int numFrets = (int) fretsSlider.getValue();

    // Fretboard wood background
    g.setColour (fretboardCol);
    g.fillRoundedRectangle (area.toFloat(), 4.0f);

    float fretWidth = (float) area.getWidth() / (numFrets + 1);
    float stringSpacing = (float) area.getHeight() / (numStrings + 1);

    // Position zone highlighting
    float zoneX1 = area.getX() + position * fretWidth;
    float zoneX2 = area.getX() + (position + range) * fretWidth;

    g.setColour (juce::Colour (0, 0, 0).withAlpha (0.5f));
    if (position > 0)
        g.fillRect ((float) area.getX(), (float) area.getY(), zoneX1 - area.getX(), (float) area.getHeight());
    if (position + range <= numFrets)
        g.fillRect (zoneX2, (float) area.getY(), (float) area.getRight() - zoneX2, (float) area.getHeight());

    g.setColour (accentBlue.withAlpha (0.5f));
    g.drawRect (zoneX1, (float) area.getY(), zoneX2 - zoneX1, (float) area.getHeight(), 2.0f);

    // Nut
    float nutX = area.getX() + fretWidth;
    g.setColour (nutBone);
    g.fillRect (nutX - 2.0f, (float) area.getY(), 4.0f, (float) area.getHeight());

    // Fret lines
    g.setColour (fretMetal.withAlpha (0.4f));
    for (int f = 1; f <= numFrets; ++f)
    {
        float x = area.getX() + (f + 1) * fretWidth;
        g.drawLine (x, (float) area.getY(), x, (float) area.getBottom(), 1.0f);
    }

    // Find where the detected note would be on the fretboard
    int activeString = -1, activeFret = -1;
    if (midiNote >= 0)
    {
        // Find the best position for this note
        for (int s = 0; s < numStrings; ++s)
        {
            int fret = midiNote - GUITAR_TUNING[s];
            if (fret >= 0 && fret <= numFrets)
            {
                // Prefer positions in the current zone
                if (fret >= position && fret <= position + range - 1)
                {
                    activeString = s;
                    activeFret = fret;
                    break;
                }
                else if (activeString < 0)
                {
                    activeString = s;
                    activeFret = fret;
                }
            }
        }
    }

    // Draw notes - bigger and more visible
    float noteW = juce::jmin (fretWidth * 0.85f, 32.0f);
    float noteH = juce::jmin (stringSpacing * 0.75f, 26.0f);
    g.setFont (11.0f);

    for (int s = 0; s < numStrings; ++s)
    {
        float y = area.getY() + (s + 1) * stringSpacing;
        int openNote = GUITAR_TUNING[s];

        for (int f = 0; f <= numFrets; ++f)
        {
            int midi = openNote + f;
            float x = area.getX() + (f + 0.5f) * fretWidth;
            int noteClass = midi % 12;

            bool isActive = (s == activeString && f == activeFret);
            bool isRoot = (noteClass == key);
            bool inScale = isNoteInScale (midi, key, scale);

            juce::Colour bg, fg;
            if (isActive) {
                bg = activeNoteColor;
                fg = bgDark;
            } else if (isRoot) {
                bg = rootNoteColor;
                fg = textBright;
            } else if (inScale) {
                bg = scaleNoteColor;
                fg = textBright;
            } else {
                bg = outOfScaleColor;
                fg = textDim.withAlpha (0.5f);
            }

            juce::Rectangle<float> noteRect (x - noteW/2, y - noteH/2, noteW, noteH);

            // Glow for active note
            if (isActive)
            {
                g.setColour (activeNoteColor.withAlpha (0.4f));
                g.fillRoundedRectangle (noteRect.expanded (4), 5.0f);
            }

            g.setColour (bg);
            g.fillRoundedRectangle (noteRect, 3.0f);

            g.setColour (fg);
            g.drawText (noteNameOnly (midi), noteRect, juce::Justification::centred, false);
        }
    }

    // Fret numbers - all frets, traditional markers highlighted
    // Traditional dot marker positions: 3, 5, 7, 9, 12 (double), 15, 17, 19, 21, 24 (double)
    auto isMarkerFret = [](int f) {
        return f == 3 || f == 5 || f == 7 || f == 9 || f == 12 ||
               f == 15 || f == 17 || f == 19 || f == 21 || f == 24;
    };

    g.setFont (juce::Font (11.0f, juce::Font::bold));
    for (int f = 0; f <= numFrets; ++f)
    {
        float x = area.getX() + (f + 0.5f) * fretWidth;

        // Marker frets get brighter color
        if (isMarkerFret(f))
            g.setColour (textBright);
        else
            g.setColour (textDim);

        // High frets (15+) show numbers above the fretboard like modern guitars
        if (f >= 15)
            g.drawText (juce::String (f), (int)(x - 12), area.getY() - 16, 24, 14,
                        juce::Justification::centred);
        else
            g.drawText (juce::String (f), (int)(x - 12), area.getBottom() + 2, 24, 14,
                        juce::Justification::centred);
    }
}

void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgDark);

    float pitch = processorRef.detectedPitch.load();
    float cents = processorRef.detectedCents.load();
    int midiNote = processorRef.detectedMidiNote.load();

    auto bounds = getLocalBounds();

    // Top control bar area
    auto topBar = bounds.removeFromTop (36);
    g.setColour (panelBg);
    g.fillRect (topBar);
    g.setColour (controlBorder);
    g.drawHorizontalLine (topBar.getBottom() - 1, 0.0f, (float) getWidth());

    // Draw "Billions of Notes" branding and version on the right of top bar
    auto brandArea = topBar.removeFromRight (220).reduced (10, 0);
    g.setColour (activeNoteColor);
    g.setFont (juce::Font (16.0f, juce::Font::bold));
    g.drawText (PLUGIN_TITLE, brandArea.removeFromLeft (160), juce::Justification::centredRight);
    g.setColour (textDim);
    g.setFont (juce::Font (11.0f));
    g.drawText (PLUGIN_VERSION, brandArea, juce::Justification::centredLeft);

    // Bottom control bar
    auto bottomBar = bounds.removeFromBottom (36);
    g.setColour (panelBg);
    g.fillRect (bottomBar);
    g.setColour (controlBorder);
    g.drawHorizontalLine (bottomBar.getY(), 0.0f, (float) getWidth());

    // Debug panel if enabled
    if (showDebugPanel)
    {
        auto debugArea = bounds.removeFromBottom (70).reduced (10, 5);
        g.setColour (juce::Colour(30, 30, 35));
        g.fillRoundedRectangle (debugArea.toFloat(), 4.0f);

        // Debug info text at the bottom of debug area
        auto debugTextArea = debugArea.removeFromBottom (20);
        float debugRMS = processorRef.debugRMS.load();
        float debugPitch = processorRef.debugRawPitch.load();
        float debugConf = processorRef.debugConfidence.load();

        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
        g.setColour (juce::Colour(120, 120, 70));

        float threshold = processorRef.sensitivityThreshold.load();
        char debugStr[128];
        snprintf(debugStr, sizeof(debugStr), "RMS: %.6f  Pitch: %.1f Hz  Conf: %.2f (thresh: %.2f)",
                 debugRMS, debugPitch, debugConf, threshold);

        g.drawText (juce::String(debugStr), debugTextArea, juce::Justification::centred);
    }

    // Main content area
    bounds = bounds.reduced (10, 5);

    // Tuner area at top
    auto tunerArea = bounds.removeFromTop (70);
    drawTuner (g, tunerArea, midiNote, pitch, cents);

    bounds.removeFromTop (5);

    // Fretboard area
    auto fretArea = bounds.reduced (0, 5);
    fretArea.removeFromBottom (16);  // Space for fret numbers
    drawFretboard (g, fretArea, midiNote);

}

void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Top control bar
    auto topBar = bounds.removeFromTop (36).reduced (10, 6);

    int x = topBar.getX();
    int y = topBar.getY();
    int h = topBar.getHeight();

    // Fretboard controls: KEY, SCALE (now at the start of top bar)
    keyLabel.setBounds (x, y, 25, h);
    x += 25;
    keySelector.setBounds (x, y, 55, h);
    x += 65;

    scaleLabel.setBounds (x, y, 38, h);
    x += 38;
    scaleSelector.setBounds (x, y, 130, h);

    // Bottom control bar
    auto bottomBar = bounds.removeFromBottom (36).reduced (10, 6);

    x = bottomBar.getX();
    y = bottomBar.getY();
    h = bottomBar.getHeight();

    // POSITION, RANGE, STR, FRET - wider sliders
    positionLabel.setBounds (x, y, 58, h);
    x += 58;
    positionSlider.setBounds (x, y, 130, h);
    x += 140;

    rangeLabel.setBounds (x, y, 45, h);
    x += 45;
    rangeSlider.setBounds (x, y, 120, h);
    x += 130;

    stringsLabel.setBounds (x, y, 55, h);
    x += 55;
    stringsSlider.setBounds (x, y, 100, h);
    x += 110;

    fretsLabel.setBounds (x, y, 40, h);
    x += 40;
    fretsSlider.setBounds (x, y, 100, h);

    // Debug button on the right
    debugButton.setBounds (bottomBar.removeFromRight (60));

    // Debug panel controls (SENS, HOLD) - positioned in debug area when visible
    if (showDebugPanel)
    {
        auto debugBounds = getLocalBounds();
        debugBounds.removeFromTop (36);    // Top bar
        debugBounds.removeFromBottom (36); // Bottom bar
        auto debugArea = debugBounds.removeFromBottom (70).reduced (15, 10);
        auto controlRow = debugArea.removeFromTop (28);

        int dx = controlRow.getX();
        int dy = controlRow.getY();
        int dh = controlRow.getHeight();

        sensLabel.setBounds (dx, dy, 35, dh);
        dx += 35;
        sensitivitySlider.setBounds (dx, dy, 130, dh);
        dx += 145;

        holdLabel.setBounds (dx, dy, 35, dh);
        dx += 35;
        holdSlider.setBounds (dx, dy, 130, dh);
    }
}
