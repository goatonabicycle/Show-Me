#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <chrono>
#include <algorithm>

AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    ringBuffer.resize (RING_BUFFER_SIZE, 0.0f);
    analysisBuffer.resize (ANALYSIS_SIZE, 0.0f);
    yinBuffer.resize (ANALYSIS_SIZE / 2, 0.0f);
    pitchHistory.resize (PITCH_HISTORY_SIZE, 0.0f);
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    threadRunning = false;
    if (pitchThread.joinable())
        pitchThread.join();
}

const juce::String AudioPluginAudioProcessor::getName() const { return JucePlugin_Name; }
bool AudioPluginAudioProcessor::acceptsMidi() const { return false; }
bool AudioPluginAudioProcessor::producesMidi() const { return false; }
bool AudioPluginAudioProcessor::isMidiEffect() const { return false; }
double AudioPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int AudioPluginAudioProcessor::getNumPrograms() { return 1; }
int AudioPluginAudioProcessor::getCurrentProgram() { return 0; }
void AudioPluginAudioProcessor::setCurrentProgram (int) {}
const juce::String AudioPluginAudioProcessor::getProgramName (int) { return {}; }
void AudioPluginAudioProcessor::changeProgramName (int, const juce::String&) {}

void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    writePos = 0;
    smoothedPitch = 0.0f;
    smoothedCents = 0.0f;
    std::fill (ringBuffer.begin(), ringBuffer.end(), 0.0f);
    std::fill (pitchHistory.begin(), pitchHistory.end(), 0.0f);

    // Start background analysis thread
    if (!threadRunning)
    {
        threadRunning = true;
        pitchThread = std::thread (&AudioPluginAudioProcessor::analyzerThread, this);
    }
}

void AudioPluginAudioProcessor::releaseResources()
{
    threadRunning = false;
    if (pitchThread.joinable())
        pitchThread.join();
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const float* inputL = buffer.getReadPointer (0);
    const float* inputR = totalNumInputChannels > 1 ? buffer.getReadPointer (1) : inputL;

    int numSamples = buffer.getNumSamples();

    // Calculate RMS level (cheap)
    float sumSquares = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = (inputL[i] + inputR[i]) * 0.5f;
        sumSquares += sample * sample;
    }
    signalLevel.store (std::sqrt (sumSquares / numSamples));

    // Write to ring buffer (lock-free)
    int wp = writePos.load();
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = (inputL[i] + inputR[i]) * 0.5f;
        ringBuffer[wp] = sample;
        wp = (wp + 1) % RING_BUFFER_SIZE;
    }
    writePos.store (wp);

    // Audio passes through unchanged
}

void AudioPluginAudioProcessor::analyzerThread()
{
    while (threadRunning)
    {
        // Run analysis ~50 times per second
        std::this_thread::sleep_for (std::chrono::milliseconds (20));

        if (!threadRunning) break;

        float rms = signalLevel.load();
        debugRMS.store(rms);

        // Copy from ring buffer regardless of level
        int wp = writePos.load();
        for (int i = 0; i < ANALYSIS_SIZE; ++i)
        {
            int idx = (wp - ANALYSIS_SIZE + i + RING_BUFFER_SIZE) % RING_BUFFER_SIZE;
            analysisBuffer[i] = ringBuffer[idx];
        }

        float confidence = 0.0f;
        float pitch = detectPitchYIN (analysisBuffer.data(), ANALYSIS_SIZE, confidence);

        // Store debug values
        debugRawPitch.store(pitch);
        debugConfidence.store(confidence);

        // Simple logic: if we have a valid pitch, show it
        // Use user-adjustable threshold
        float threshold = sensitivityThreshold.load();

        // Calculate hold counter based on user setting (ms to frames at 50fps)
        int holdFrames = (holdTimeMs.load() * 50) / 1000;

        if (pitch > 20.0f && pitch < 5000.0f && confidence > threshold)
        {
            float midiNoteFloat = 69.0f + 12.0f * std::log2 (pitch / 440.0f);
            int midiNote = (int) std::round (midiNoteFloat);
            float cents = (midiNoteFloat - midiNote) * 100.0f;

            // Octave jump protection: if we have a valid previous note,
            // and the new note is exactly 12 semitones away (octave),
            // require higher confidence to switch
            bool acceptNote = true;
            if (lastValidNote >= 0)
            {
                int noteDiff = std::abs(midiNote - lastValidNote);
                // If it's an octave jump (12 semitones) or close to it
                if (noteDiff == 12 || noteDiff == 11 || noteDiff == 13)
                {
                    // Require much higher confidence to accept octave jump
                    if (confidence < 0.85f)
                    {
                        acceptNote = false;
                    }
                }
                // For jumps of more than 7 semitones (a fifth), be more cautious
                else if (noteDiff > 7 && confidence < 0.75f)
                {
                    acceptNote = false;
                }
            }

            if (acceptNote)
            {
                // Store as last valid
                lastValidNote = midiNote;
                lastValidPitch = pitch;
                lastValidCents = cents;
                holdCounter = holdFrames;

                detectedPitch.store (pitch);
                detectedMidiNote.store (midiNote);
                detectedCents.store (cents);
            }
            else
            {
                // Rejected due to octave protection - keep showing last valid
                if (holdCounter > 0)
                {
                    detectedPitch.store (lastValidPitch);
                    detectedMidiNote.store (lastValidNote);
                    detectedCents.store (lastValidCents);
                }
            }
        }
        else
        {
            // No valid pitch - hold last note
            if (holdCounter > 0)
            {
                --holdCounter;
                detectedPitch.store (lastValidPitch);
                detectedMidiNote.store (lastValidNote);
                detectedCents.store (lastValidCents);
            }
            else
            {
                // Hold expired - show no signal
                detectedPitch.store (0.0f);
                detectedMidiNote.store (-1);
                detectedCents.store (0.0f);
            }
        }
    }
}

float AudioPluginAudioProcessor::detectPitchYIN (const float* buffer, int numSamples, float& confidence)
{
    // Based on aubio's pitchyin.c - proven implementation
    // YIN algorithm: de Cheveigné & Kawahara (2002)

    int halfSize = numSamples / 2;
    float tolerance = 0.50f;  // Much higher - allow more detections

    // Step 1 & 2 combined: Difference function + cumulative mean normalized
    // yinBuffer[0] is always 1.0 by definition
    yinBuffer[0] = 1.0f;

    // Compute difference function for all tau values
    for (int tau = 1; tau < halfSize; ++tau)
    {
        yinBuffer[tau] = 0.0f;
        for (int j = 0; j < halfSize; ++j)
        {
            float delta = buffer[j] - buffer[j + tau];
            yinBuffer[tau] += delta * delta;
        }
    }

    // Cumulative mean normalized difference function
    float runningSum = 0.0f;
    yinBuffer[0] = 1.0f;
    for (int tau = 1; tau < halfSize; ++tau)
    {
        runningSum += yinBuffer[tau];
        if (runningSum != 0.0f)
            yinBuffer[tau] = yinBuffer[tau] * tau / runningSum;
        else
            yinBuffer[tau] = 1.0f;
    }

    // Step 3: Absolute threshold - find first minimum below threshold
    // Start from tau=2 (aubio starts from 1, but 2 avoids edge issues)
    int tauEstimate = 0;
    float minValue = 1.0f;

    // Search in frequency range ~30Hz to ~2000Hz
    int minTau = (int)(currentSampleRate / 2000.0);
    int maxTau = (int)(currentSampleRate / 30.0);
    if (minTau < 2) minTau = 2;
    if (maxTau > halfSize - 1) maxTau = halfSize - 1;

    for (int tau = minTau; tau < maxTau; ++tau)
    {
        if (yinBuffer[tau] < tolerance)
        {
            // Found a value below threshold - now find the local minimum
            while (tau + 1 < maxTau && yinBuffer[tau + 1] < yinBuffer[tau])
            {
                ++tau;
            }
            tauEstimate = tau;
            minValue = yinBuffer[tau];
            break;
        }
    }

    // If no value below threshold, find the global minimum as fallback
    if (tauEstimate == 0)
    {
        for (int tau = minTau; tau < maxTau; ++tau)
        {
            if (yinBuffer[tau] < minValue)
            {
                minValue = yinBuffer[tau];
                tauEstimate = tau;
            }
        }
    }

    // Still nothing? Give up
    if (tauEstimate == 0)
    {
        confidence = 0.0f;
        return 0.0f;
    }

    // Confidence is 1 - d'(tau)
    confidence = 1.0f - minValue;

    // Step 4: Parabolic interpolation for sub-sample accuracy
    float betterTau;
    if (tauEstimate > 1 && tauEstimate < halfSize - 1)
    {
        float s0 = yinBuffer[tauEstimate - 1];
        float s1 = yinBuffer[tauEstimate];
        float s2 = yinBuffer[tauEstimate + 1];

        // Parabolic interpolation: find vertex of parabola through 3 points
        float denom = 2.0f * (2.0f * s1 - s2 - s0);
        if (std::abs(denom) > 1e-9f)
        {
            betterTau = tauEstimate + (s0 - s2) / denom;
        }
        else
        {
            betterTau = (float) tauEstimate;
        }
    }
    else
    {
        betterTau = (float) tauEstimate;
    }

    // Sanity check
    if (betterTau <= 0.0f)
    {
        confidence = 0.0f;
        return 0.0f;
    }

    return (float) currentSampleRate / betterTau;
}

bool AudioPluginAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock&) {}
void AudioPluginAudioProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
