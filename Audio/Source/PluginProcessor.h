#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

class AudioPluginAudioProcessor : public juce::AudioProcessor
{
public:
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Pitch detection results - thread-safe access
    std::atomic<float> detectedPitch { 0.0f };
    std::atomic<float> detectedCents { 0.0f };
    std::atomic<int> detectedMidiNote { -1 };
    std::atomic<float> signalLevel { 0.0f };

    // Debug info
    std::atomic<float> debugRawPitch { 0.0f };
    std::atomic<float> debugConfidence { 0.0f };
    std::atomic<float> debugRMS { 0.0f };

    // User-adjustable sensitivity (confidence threshold)
    std::atomic<float> sensitivityThreshold { 0.62f };  // 0.0 = most sensitive, 1.0 = least sensitive

    // User-adjustable hold time in milliseconds
    std::atomic<int> holdTimeMs { 400 };  // How long to hold note after signal drops

private:
    // Background pitch detection
    void analyzerThread();
    float detectPitchYIN (const float* buffer, int numSamples, float& confidence);

    double currentSampleRate = 44100.0;

    // Lock-free ring buffer for audio data
    static constexpr int RING_BUFFER_SIZE = 16384;
    std::vector<float> ringBuffer;
    std::atomic<int> writePos { 0 };

    // Analysis buffer (used by background thread)
    static constexpr int ANALYSIS_SIZE = 4096;
    std::vector<float> analysisBuffer;
    std::vector<float> yinBuffer;

    // Median filter for stability
    std::vector<float> pitchHistory;
    static constexpr int PITCH_HISTORY_SIZE = 5;

    // Background thread
    std::thread pitchThread;
    std::atomic<bool> threadRunning { false };

    // Smoothing and display stability
    float smoothedPitch = 0.0f;
    float smoothedCents = 0.0f;
    int lastValidNote = -1;
    float lastValidPitch = 0.0f;
    float lastValidCents = 0.0f;
    int holdCounter = 0;
    static constexpr int HOLD_TIME = 25;  // Hold note for ~500ms after signal drops

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
