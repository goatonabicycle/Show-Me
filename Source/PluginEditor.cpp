#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <vector>
#include <climits>

namespace {
    const int GUITAR_TUNING[6] = {
        64,  // E4 (high E)
        59,  // B3
        55,  // G3
        50,  // D3
        45,  // A2
        40   // E2 (low E)
    };

    const char* NOTE_NAMES[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    juce::String midiNoteToName (int midiNote)
    {
        int noteIndex = midiNote % 12;
        int octave = (midiNote / 12) - 1;
        return juce::String (NOTE_NAMES[noteIndex]) + juce::String (octave);
    }

    bool hasFretMarker (int fret)
    {
        return fret == 3 || fret == 5 || fret == 7 || fret == 9 ||
               fret == 12 || fret == 15 || fret == 17 || fret == 19 ||
               fret == 21 || fret == 24;
    }

    bool isDoubleDot (int fret)
    {
        return fret == 12 || fret == 24;
    }
    
    const juce::Colour bgDark (18, 18, 22);
    const juce::Colour bgCard (28, 28, 35);
    const juce::Colour textPrimary (240, 240, 245);
    const juce::Colour textSecondary (140, 140, 155);
    const juce::Colour accentGreen (72, 199, 142);
    const juce::Colour accentBlue (99, 149, 238);
    const juce::Colour fretboardWood (38, 32, 26);
    const juce::Colour fretWire (160, 155, 145);
    const juce::Colour stringColor (190, 185, 175);
}

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label,
                               const juce::String& name, double min, double max, double def) {
        slider.setRange (min, max, 1);
        slider.setValue (def);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 35, 20);
        slider.setColour (juce::Slider::backgroundColourId, bgCard);
        slider.setColour (juce::Slider::trackColourId, accentBlue.withAlpha (0.6f));
        slider.setColour (juce::Slider::thumbColourId, accentBlue);
        slider.setColour (juce::Slider::textBoxTextColourId, textPrimary);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, bgDark);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, textSecondary);
        label.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (label);
    };

    setupSlider (positionSlider, positionLabel, "Position", 0, 24, 2);
    setupSlider (rangeSlider, rangeLabel, "Range", 4, 7, 6);
    setupSlider (stringsSlider, stringsLabel, "Strings", 6, 8, 6);
    setupSlider (fretsSlider, fretsLabel, "Frets", 12, 24, 24);

    setSize (1200, 380);
    startTimerHz (30);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
    repaint();
}

AudioPluginAudioProcessorEditor::NotePosition AudioPluginAudioProcessorEditor::findOptimalPosition (
    int midiNote, int preferredPosition, int fingerRange, int numStrings, int totalFrets)
{
    struct Candidate {
        int stringIndex;
        int fret;
        int distance;
        bool inZone;
    };
    std::vector<Candidate> candidates;

    int zoneStart = preferredPosition;
    int zoneEnd = preferredPosition + fingerRange - 1;

    for (int string = 0; string < numStrings; ++string)
    {
        int openStringNote = GUITAR_TUNING[string];
        int fret = midiNote - openStringNote;

        if (fret >= 0 && fret <= totalFrets)
        {
            int distance = std::abs (string - currentString) + std::abs (fret - currentFret);
            bool inZone = (fret >= zoneStart && fret <= zoneEnd);
            candidates.push_back ({ string, fret, distance, inZone });
        }
    }

    if (candidates.empty())
        return { -1, -1, midiNote };

    Candidate* best = nullptr;
    int bestDistance = INT_MAX;

    for (auto& c : candidates)
    {
        if (c.inZone && c.distance < bestDistance)
        {
            bestDistance = c.distance;
            best = &c;
        }
    }

    if (best == nullptr)
    {
        for (auto& c : candidates)
        {
            if (c.distance < bestDistance)
            {
                bestDistance = c.distance;
                best = &c;
            }
        }
    }

    return { best->stringIndex, best->fret, midiNote };
}

void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgDark);
    
    std::set<int> activeNotes;
    {
        std::lock_guard<std::mutex> lock (processorRef.notesMutex);
        activeNotes = processorRef.activeNotes;
    }
    
    const int position = (int) positionSlider.getValue();
    const int range = (int) rangeSlider.getValue();
    const int numStrings = (int) stringsSlider.getValue();
    const int totalFrets = (int) fretsSlider.getValue();

    const int startFret = 0;
    const int endFret = totalFrets;
    const int numFrets = endFret - startFret + 1;
    
    const float controlsHeight = 50.0f;
    const float headerHeight = 40.0f;
    const float padding = 20.0f;
    const float fretboardPadding = 15.0f;
    const float fretMarkerHeight = 18.0f;
    const float fretNumberHeight = 22.0f;

    auto bounds = getLocalBounds().toFloat();

    auto cardArea = bounds.reduced (padding, 0)
                          .withTrimmedTop (controlsHeight + 10)
                          .withTrimmedBottom (padding);

    g.setColour (bgCard);
    g.fillRoundedRectangle (cardArea, 8.0f);

    auto headerArea = cardArea.removeFromTop (headerHeight);

    g.setColour (textPrimary);
    g.setFont (16.0f);
    g.drawText ("Show Me v0.9", headerArea.withTrimmedLeft (fretboardPadding),
                juce::Justification::centredLeft, true);

    juce::String noteText;
    if (activeNotes.empty())
    {
        g.setColour (textSecondary);
        noteText = "No notes";
    }
    else
    {
        g.setColour (accentGreen);
        for (auto it = activeNotes.begin(); it != activeNotes.end(); ++it)
        {
            if (it != activeNotes.begin()) noteText += "  ";
            noteText += midiNoteToName (*it);
        }
    }
    g.setFont (18.0f);
    g.drawText (noteText, headerArea.withTrimmedRight (fretboardPadding),
                juce::Justification::centredRight, true);

    auto fretboardArea = cardArea.reduced (fretboardPadding, 0)
                                  .withTrimmedBottom (fretboardPadding);

    float fretboardTop = fretboardArea.getY() + fretMarkerHeight;
    float fretboardBottom = fretboardArea.getBottom() - fretNumberHeight;
    float fretboardHeight = fretboardBottom - fretboardTop;
    float stringSpacing = fretboardHeight / (numStrings - 1);
    float fretWidth = fretboardArea.getWidth() / numFrets;

    g.setColour (textSecondary.withAlpha (0.5f));
    for (int fret = startFret; fret <= endFret; ++fret)
    {
        if (hasFretMarker (fret))
        {
            float x = fretboardArea.getX() + (fret - startFret + 0.5f) * fretWidth;
            float markerY = fretboardArea.getY() + fretMarkerHeight / 2;

            if (isDoubleDot (fret))
            {
                g.fillEllipse (x - 3, markerY - 7, 6, 6);
                g.fillEllipse (x - 3, markerY + 1, 6, 6);
            }
            else
            {
                g.fillEllipse (x - 4, markerY - 4, 8, 8);
            }
        }
    }

    g.setColour (fretboardWood);
    g.fillRoundedRectangle (fretboardArea.getX(), fretboardTop,
                            fretboardArea.getWidth(), fretboardHeight, 4.0f);

    float zoneStartX = fretboardArea.getX() + position * fretWidth;
    float zoneEndX = fretboardArea.getX() + (position + range) * fretWidth;

    g.setColour (juce::Colour (0, 0, 0).withAlpha (0.55f));
    if (position > 0)
    {
        g.fillRoundedRectangle (fretboardArea.getX(), fretboardTop,
                                zoneStartX - fretboardArea.getX(), fretboardHeight, 4.0f);
    }
    if (position + range < totalFrets)
    {
        g.fillRoundedRectangle (zoneEndX, fretboardTop,
                                fretboardArea.getRight() - zoneEndX, fretboardHeight, 4.0f);
    }

    g.setColour (accentBlue.withAlpha (0.15f));
    g.fillRect (zoneStartX, fretboardTop, zoneEndX - zoneStartX, fretboardHeight);
    g.setColour (accentBlue.withAlpha (0.4f));
    g.drawRect (zoneStartX, fretboardTop, zoneEndX - zoneStartX, fretboardHeight, 2.0f);

    for (int fret = startFret; fret <= endFret; ++fret)
    {
        float x = fretboardArea.getX() + (fret - startFret + 1) * fretWidth;
        if (fret < endFret)
        {
            g.setColour (fret == 0 ? fretWire : fretWire.withAlpha (0.6f));
            g.drawLine (x, fretboardTop, x, fretboardBottom, fret == 0 ? 3.0f : 1.0f);
        }
    }

    for (int string = 0; string < numStrings; ++string)
    {
        float y = fretboardTop + string * stringSpacing;
        float thickness = 0.8f + (string * 0.35f);
        g.setColour (stringColor.withAlpha (0.8f));
        g.drawLine (fretboardArea.getX(), y, fretboardArea.getRight(), y, thickness);
    }

    std::vector<NotePosition> optimalPositions;
    for (int midiNote : activeNotes)
    {
        NotePosition pos = findOptimalPosition (midiNote, position, range, numStrings, totalFrets);
        if (pos.stringIndex >= 0)
        {
            optimalPositions.push_back (pos);
            if (optimalPositions.size() == 1)
            {
                currentString = pos.stringIndex;
                currentFret = pos.fret;
            }
        }
    }

    for (const auto& pos : optimalPositions)
    {
        float stringY = fretboardTop + pos.stringIndex * stringSpacing;
        float fretCenterX = fretboardArea.getX() + (pos.fret - startFret + 0.5f) * fretWidth;
        float circleSize = 26.0f;

        g.setColour (accentGreen.withAlpha (0.25f));
        g.fillEllipse (fretCenterX - circleSize/2 - 5, stringY - circleSize/2 - 5,
                       circleSize + 10, circleSize + 10);
        
        g.setColour (accentGreen);
        g.fillEllipse (fretCenterX - circleSize/2, stringY - circleSize/2, circleSize, circleSize);
        
        g.setColour (accentGreen.brighter (0.3f));
        g.fillEllipse (fretCenterX - circleSize/4, stringY - circleSize/3,
                       circleSize/3, circleSize/4);
        
        g.setColour (bgDark);
        g.setFont (10.0f);
        g.drawText (midiNoteToName (pos.midiNote),
                    (int)(fretCenterX - circleSize/2), (int)(stringY - circleSize/2),
                    (int)circleSize, (int)circleSize,
                    juce::Justification::centred, false);
    }

    g.setColour (textSecondary.withAlpha (0.6f));
    g.setFont (11.0f);
    for (int fret = startFret; fret <= endFret; ++fret)
    {
        float x = fretboardArea.getX() + (fret - startFret + 0.5f) * fretWidth;
        g.drawText (juce::String (fret),
                    (int)(x - 12), (int)(fretboardBottom + 4), 24, 18,
                    juce::Justification::centred, false);
    }
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto controlsArea = bounds.removeFromTop (50).reduced (25, 12);

    const int labelWidth = 55;
    const int sliderWidth = 100;
    const int spacing = 40;

    positionLabel.setBounds (controlsArea.removeFromLeft (labelWidth));
    positionSlider.setBounds (controlsArea.removeFromLeft (sliderWidth));
    controlsArea.removeFromLeft (spacing);

    rangeLabel.setBounds (controlsArea.removeFromLeft (labelWidth));
    rangeSlider.setBounds (controlsArea.removeFromLeft (sliderWidth));
    controlsArea.removeFromLeft (spacing);

    stringsLabel.setBounds (controlsArea.removeFromLeft (labelWidth));
    stringsSlider.setBounds (controlsArea.removeFromLeft (sliderWidth));
    controlsArea.removeFromLeft (spacing);

    fretsLabel.setBounds (controlsArea.removeFromLeft (labelWidth));
    fretsSlider.setBounds (controlsArea.removeFromLeft (sliderWidth));
}
