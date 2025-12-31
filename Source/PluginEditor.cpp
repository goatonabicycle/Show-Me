#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <vector>
#include <climits>

namespace {
    const int GUITAR_TUNING[6] = { 64, 59, 55, 50, 45, 40 };
    const char* NOTE_NAMES[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const char* SCALE_NAMES[] = {
        "Chromatic", "Major", "Minor", "Harmonic Minor", "Melodic Minor",
        "Pentatonic Maj", "Pentatonic Min", "Blues", "Dorian", "Phrygian",
        "Lydian", "Mixolydian", "Locrian", "Whole Tone", "Diminished",
        "Phrygian Dominant", "Hungarian Minor", "Double Harmonic"
    };
    const int NUM_SCALES = 18;

    const int SCALE_PATTERNS[18][12] = {
        { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 },
        { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 },
        { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1 },
        { 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1 },
        { 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0 },
        { 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0 },
        { 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0 },
        { 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0 },
        { 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0 },
        { 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1 },
        { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0 },
        { 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0 },
        { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 },
        { 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1 },
        { 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0 },
        { 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1 },
        { 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1 },
    };

    juce::String noteNameOnly (int midiNote)
    {
        return juce::String (NOTE_NAMES[midiNote % 12]);
    }

    // Colors
    const juce::Colour bgDark (18, 18, 22);
    const juce::Colour panelBg (28, 28, 34);
    const juce::Colour controlBg (38, 38, 46);
    const juce::Colour controlBorder (58, 58, 68);
    const juce::Colour textBright (235, 235, 240);
    const juce::Colour textDim (120, 120, 130);
    const juce::Colour accentBlue (100, 160, 220);
    const juce::Colour activeNote (82, 209, 152);
    const juce::Colour rootNote (200, 110, 90);
    const juce::Colour scaleNote (80, 140, 190);
    const juce::Colour outOfScale (42, 40, 38);
    const juce::Colour fretboardCol (38, 32, 26);
    const juce::Colour fretMetal (120, 115, 105);
    const juce::Colour nutBone (220, 215, 200);

    const int MENU_BAR_HEIGHT = 44;
}

// Custom LookAndFeel for modern controls
class ModernLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ModernLookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, controlBg);
        setColour (juce::ComboBox::outlineColourId, controlBorder);
        setColour (juce::ComboBox::textColourId, textBright);
        setColour (juce::ComboBox::arrowColourId, textDim);
        setColour (juce::PopupMenu::backgroundColourId, controlBg);
        setColour (juce::PopupMenu::textColourId, textBright);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accentBlue);
        setColour (juce::PopupMenu::highlightedTextColourId, textBright);
        setColour (juce::Slider::backgroundColourId, controlBg);
        setColour (juce::Slider::trackColourId, accentBlue);
        setColour (juce::Slider::thumbColourId, textBright);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
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

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
        auto trackBounds = bounds.reduced (0, bounds.getHeight() * 0.35f);

        // Track background
        g.setColour (controlBg);
        g.fillRoundedRectangle (trackBounds, 3.0f);
        g.setColour (controlBorder);
        g.drawRoundedRectangle (trackBounds.reduced (0.5f), 3.0f, 1.0f);

        // Filled portion
        float fillWidth = sliderPos - x;
        if (fillWidth > 0)
        {
            auto fillBounds = trackBounds.withWidth (fillWidth);
            g.setColour (accentBlue);
            g.fillRoundedRectangle (fillBounds, 3.0f);
        }

        // Thumb
        float thumbX = sliderPos;
        float thumbSize = trackBounds.getHeight() + 6;
        auto thumbBounds = juce::Rectangle<float> (thumbX - thumbSize/2,
                                                    trackBounds.getCentreY() - thumbSize/2,
                                                    thumbSize, thumbSize);
        g.setColour (textBright);
        g.fillEllipse (thumbBounds);
        g.setColour (accentBlue);
        g.drawEllipse (thumbBounds.reduced (1), 2.0f);
    }
};

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Set custom look and feel
    static ModernLookAndFeel modernLF;
    setLookAndFeel (&modernLF);

    // Key selector
    for (int i = 0; i < 12; ++i)
        keySelector.addItem (NOTE_NAMES[i], i + 1);
    keySelector.setSelectedId (1);
    addAndMakeVisible (keySelector);
    keyLabel.setText ("KEY", juce::dontSendNotification);
    keyLabel.setColour (juce::Label::textColourId, textDim);
    keyLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    addAndMakeVisible (keyLabel);

    // Scale selector
    for (int i = 0; i < NUM_SCALES; ++i)
        scaleSelector.addItem (SCALE_NAMES[i], i + 1);
    scaleSelector.setSelectedId (2);
    addAndMakeVisible (scaleSelector);
    scaleLabel.setText ("SCALE", juce::dontSendNotification);
    scaleLabel.setColour (juce::Label::textColourId, textDim);
    scaleLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    addAndMakeVisible (scaleLabel);

    // Position slider
    positionSlider.setRange (0, 18, 1);
    positionSlider.setValue (0);
    positionSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    positionSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 28, 20);
    positionSlider.setColour (juce::Slider::textBoxTextColourId, textBright);
    positionSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (positionSlider);
    positionLabel.setText ("POSITION", juce::dontSendNotification);
    positionLabel.setColour (juce::Label::textColourId, textDim);
    positionLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    addAndMakeVisible (positionLabel);

    // Range slider
    rangeSlider.setRange (3, 8, 1);
    rangeSlider.setValue (5);
    rangeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    rangeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 28, 20);
    rangeSlider.setColour (juce::Slider::textBoxTextColourId, textBright);
    rangeSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (rangeSlider);
    rangeLabel.setText ("RANGE", juce::dontSendNotification);
    rangeLabel.setColour (juce::Label::textColourId, textDim);
    rangeLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    addAndMakeVisible (rangeLabel);

    // Strings slider
    stringsSlider.setRange (4, 8, 1);
    stringsSlider.setValue (6);
    stringsSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    stringsSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 28, 20);
    stringsSlider.setColour (juce::Slider::textBoxTextColourId, textBright);
    stringsSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (stringsSlider);
    stringsLabel.setText ("STRINGS", juce::dontSendNotification);
    stringsLabel.setColour (juce::Label::textColourId, textDim);
    stringsLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    addAndMakeVisible (stringsLabel);

    // Frets slider
    fretsSlider.setRange (12, 24, 1);
    fretsSlider.setValue (24);
    fretsSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    fretsSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 28, 20);
    fretsSlider.setColour (juce::Slider::textBoxTextColourId, textBright);
    fretsSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (fretsSlider);
    fretsLabel.setText ("FRETS", juce::dontSendNotification);
    fretsLabel.setColour (juce::Label::textColourId, textDim);
    fretsLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    addAndMakeVisible (fretsLabel);

    setResizable (true, true);
    setResizeLimits (1000, 240, 1800, 500);
    setSize (1200, 300);
    startTimerHz (30);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void AudioPluginAudioProcessorEditor::timerCallback() { repaint(); }

void AudioPluginAudioProcessorEditor::setControlsVisible (bool) {}

void AudioPluginAudioProcessorEditor::mouseDown (const juce::MouseEvent&) {}

bool AudioPluginAudioProcessorEditor::isNoteInScale (int midiNote, int root, int scaleIndex)
{
    int interval = (midiNote - root + 120) % 12;
    return SCALE_PATTERNS[scaleIndex][interval] == 1;
}

AudioPluginAudioProcessorEditor::NotePosition AudioPluginAudioProcessorEditor::findOptimalPosition (
    int midiNote, int preferredPosition, int fingerRange, int numStrings, int totalFrets)
{
    struct Candidate { int s; int f; int d; bool z; };
    std::vector<Candidate> c;

    for (int s = 0; s < numStrings; ++s)
    {
        int f = midiNote - GUITAR_TUNING[s];
        if (f >= 0 && f <= totalFrets)
        {
            int d = std::abs(s - currentString) + std::abs(f - currentFret);
            bool z = (f >= preferredPosition && f <= preferredPosition + fingerRange - 1);
            c.push_back({s, f, d, z});
        }
    }

    if (c.empty()) return {-1, -1, midiNote};

    Candidate* best = nullptr;
    int bestD = INT_MAX;
    for (auto& x : c) if (x.z && x.d < bestD) { bestD = x.d; best = &x; }
    if (!best) for (auto& x : c) if (x.d < bestD) { bestD = x.d; best = &x; }

    return {best->s, best->f, midiNote};
}

void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgDark);

    const int position = (int) positionSlider.getValue();
    const int range = (int) rangeSlider.getValue();
    const int key = keySelector.getSelectedId() - 1;
    const int scale = scaleSelector.getSelectedId() - 1;

    std::set<int> activeNotes;
    {
        std::lock_guard<std::mutex> lock (processorRef.notesMutex);
        activeNotes = processorRef.activeNotes;
    }

    const int numStrings = (int) stringsSlider.getValue();
    const int numFrets = (int) fretsSlider.getValue();

    // Menu bar background
    auto menuBar = getLocalBounds().removeFromTop (MENU_BAR_HEIGHT);
    g.setColour (panelBg);
    g.fillRect (menuBar);

    // Subtle bottom border
    g.setColour (controlBorder);
    g.drawHorizontalLine (MENU_BAR_HEIGHT - 1, 0.0f, (float) getWidth());

    // Scale notes panel - positioned after the scale selector (around x=207)
    // Draw it in the space after the controls
    auto scaleNotesArea = juce::Rectangle<int> (210, 10, 196, 24);
    g.setColour (controlBg);
    g.fillRoundedRectangle (scaleNotesArea.toFloat(), 4.0f);
    g.setColour (controlBorder);
    g.drawRoundedRectangle (scaleNotesArea.toFloat().reduced (0.5f), 4.0f, 1.0f);

    // Draw scale notes as small square boxes
    float noteBoxW = 15.0f;
    float noteBoxH = 18.0f;
    float noteBoxGap = 1.0f;
    float startX = scaleNotesArea.getX() + 4.0f;
    float noteY = scaleNotesArea.getY() + 3.0f;

    g.setFont (9.0f);
    for (int i = 0; i < 12; ++i)
    {
        int noteIndex = (key + i) % 12;
        bool inScale = SCALE_PATTERNS[scale][i] == 1;
        bool isRoot = (i == 0);

        float x = startX + i * (noteBoxW + noteBoxGap);
        juce::Rectangle<float> noteBox (x, noteY, noteBoxW, noteBoxH);

        if (isRoot) {
            g.setColour (rootNote);
        } else if (inScale) {
            g.setColour (scaleNote);
        } else {
            g.setColour (outOfScale);
        }
        g.fillRoundedRectangle (noteBox, 2.0f);

        g.setColour (inScale ? textBright : textDim.withAlpha (0.4f));
        g.drawText (juce::String (NOTE_NAMES[noteIndex]), noteBox, juce::Justification::centred, false);
    }

    // Active notes display on the right
    if (!activeNotes.empty())
    {
        juce::String noteStr;
        for (int n : activeNotes)
        {
            if (!noteStr.isEmpty()) noteStr += " ";
            noteStr += noteNameOnly (n);
        }
        g.setColour (activeNote);
        g.setFont (juce::Font (16.0f, juce::Font::bold));
        g.drawText (noteStr, menuBar.removeFromRight (100).reduced (8, 0),
                    juce::Justification::centredRight);
    }

    // Fretboard area - explicitly below menu bar with gap
    const int fretboardTop = MENU_BAR_HEIGHT + 6;
    const int padding = 10;
    const int fretNumSpace = 16;

    // Fixed note sizes
    const float fixedNoteH = 22.0f;
    const float fixedStringSpacing = 28.0f;
    float fretboardHeight = fixedStringSpacing * (numStrings - 1) + fixedNoteH;

    // Calculate fretboard bounds - vertically center
    int availableHeight = getHeight() - fretboardTop - fretNumSpace - 4;
    int yOffset = (availableHeight - (int)fretboardHeight) / 2;
    if (yOffset < 0) yOffset = 0;

    juce::Rectangle<int> fretArea (padding, fretboardTop + yOffset,
                                    getWidth() - padding * 2, (int)fretboardHeight);

    float stringSpacing = fixedStringSpacing;
    float fretWidth = (float) fretArea.getWidth() / (numFrets + 1);

    // Fretboard wood background
    g.setColour (fretboardCol);
    g.fillRoundedRectangle (fretArea.toFloat(), 4.0f);

    // Position zone highlighting - use fretArea bounds directly
    float zoneX1 = fretArea.getX() + position * fretWidth;
    float zoneX2 = fretArea.getX() + (position + range) * fretWidth;
    float fretAreaY = (float) fretArea.getY();
    float fretAreaH = (float) fretArea.getHeight();

    // Dim outside zone
    g.setColour (juce::Colour (0, 0, 0).withAlpha (0.65f));
    if (position > 0)
        g.fillRect ((float)fretArea.getX(), fretAreaY, zoneX1 - fretArea.getX(), fretAreaH);
    if (position + range <= numFrets)
        g.fillRect (zoneX2, fretAreaY, (float)fretArea.getRight() - zoneX2, fretAreaH);

    // Zone border
    g.setColour (accentBlue);
    g.drawRect (zoneX1, fretAreaY, zoneX2 - zoneX1, fretAreaH, 2.0f);

    // Nut
    float nutX = fretArea.getX() + fretWidth;
    g.setColour (nutBone);
    g.fillRect (nutX - 2.0f, fretAreaY, 4.0f, fretAreaH);

    // Fret lines (subtle, between note columns)
    g.setColour (fretMetal.withAlpha (0.4f));
    for (int f = 1; f <= numFrets; ++f)
    {
        float x = fretArea.getX() + (f + 1) * fretWidth;
        g.drawLine (x, fretAreaY, x, fretAreaY + fretAreaH, 1.0f);
    }

    // Get optimal positions for active notes
    std::vector<NotePosition> optimalPos;
    for (int n : activeNotes)
    {
        auto pos = findOptimalPosition (n, position, range, numStrings, numFrets);
        if (pos.stringIndex >= 0)
        {
            optimalPos.push_back (pos);
            if (optimalPos.size() == 1) { currentString = pos.stringIndex; currentFret = pos.fret; }
        }
    }

    // Draw notes - set font ONCE before loop to prevent layout shifts
    float noteW = juce::jmin (fretWidth * 0.85f, 28.0f);
    float noteH = fixedNoteH;
    juce::Font noteFont (10.0f);
    g.setFont (noteFont);

    // Notes are positioned so first row starts at top of fretArea + half note height
    float firstNoteY = fretAreaY + noteH / 2.0f;

    for (int s = 0; s < numStrings; ++s)
    {
        float y = firstNoteY + s * stringSpacing;
        int openNote = GUITAR_TUNING[s];

        for (int f = 0; f <= numFrets; ++f)
        {
            int midi = openNote + f;
            float x = fretArea.getX() + (f + 0.5f) * fretWidth;
            int noteClass = midi % 12;

            bool isActive = false;
            for (auto& p : optimalPos)
                if (p.stringIndex == s && p.fret == f) { isActive = true; break; }

            bool isRoot = (noteClass == key);
            bool inScale = isNoteInScale (midi, key, scale);

            juce::Colour bg, fg;
            if (isActive) {
                bg = activeNote;
                fg = bgDark;
            } else if (isRoot) {
                bg = rootNote;
                fg = textBright;
            } else if (inScale) {
                bg = scaleNote;
                fg = textBright;
            } else {
                bg = outOfScale;
                fg = textDim.withAlpha (0.5f);
            }

            juce::Rectangle<float> noteRect (x - noteW/2, y - noteH/2, noteW, noteH);

            if (isActive)
            {
                g.setColour (activeNote.withAlpha (0.4f));
                g.fillRoundedRectangle (noteRect.expanded (3), 4.0f);
            }

            g.setColour (bg);
            g.fillRoundedRectangle (noteRect, 3.0f);

            g.setColour (fg);
            g.drawText (noteNameOnly (midi), noteRect, juce::Justification::centred, false);
        }
    }

    // Fret numbers
    g.setColour (textDim);
    g.setFont (10.0f);
    for (int f = 0; f <= numFrets; f += 3)
    {
        float x = fretArea.getX() + (f + 0.5f) * fretWidth;
        g.drawText (juce::String (f), (int)(x - 10), fretArea.getBottom() + 2, 20, 14,
                    juce::Justification::centred);
    }
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto menuBar = bounds.removeFromTop (MENU_BAR_HEIGHT).reduced (12, 6);

    int h = menuBar.getHeight();
    int labelH = 12;
    int ctrlH = h - labelH;
    int y = menuBar.getY();
    int x = menuBar.getX();

    // Key
    keyLabel.setBounds (x, y, 30, labelH);
    keySelector.setBounds (x, y + labelH, 55, ctrlH);
    x += 65;

    // Scale
    scaleLabel.setBounds (x, y, 40, labelH);
    scaleSelector.setBounds (x, y + labelH, 130, ctrlH);
    x += 142;

    // Skip space for scale notes panel (196 + padding)
    x += 206;

    // Position
    positionLabel.setBounds (x, y, 60, labelH);
    positionSlider.setBounds (x, y + labelH, 110, ctrlH);
    x += 120;

    // Range
    rangeLabel.setBounds (x, y, 50, labelH);
    rangeSlider.setBounds (x, y + labelH, 100, ctrlH);
    x += 110;

    // Strings
    stringsLabel.setBounds (x, y, 55, labelH);
    stringsSlider.setBounds (x, y + labelH, 95, ctrlH);
    x += 105;

    // Frets
    fretsLabel.setBounds (x, y, 50, labelH);
    fretsSlider.setBounds (x, y + labelH, 100, ctrlH);
}
