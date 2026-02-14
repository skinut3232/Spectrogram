#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AudioFifo.h"
#include "SpectralAnalyser.h"
#include "StereoSpectralAnalyser.h"
#include <atomic>

class SpectrogramProcessor : public juce::AudioProcessor,
                              private juce::Timer
{
public:
    SpectrogramProcessor();
    ~SpectrogramProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    SpectralAnalyser& getAnalyser() noexcept { return analyser; }
    StereoSpectralAnalyser& getStereoAnalyser() noexcept { return stereoAnalyser; }

    // Set by editor to enable stereo analysis (Nebula mode)
    std::atomic<bool> nebulaActive{false};

    // Persistent display settings (editor reads/writes these)
    struct Settings
    {
        int fftSizeId       = 3;    // ComboBox ID: 1=1024, 2=2048, 3=4096, 4=8192
        int overlapId       = 1;    // 1=50%, 2=75%
        int windowId        = 1;    // 1=Hann, 2=Blackman-Harris
        int colourMapId     = 1;    // 1..8
        bool logScale       = true;
        float dbFloor       = -90.0f;
        float dbCeiling     = 0.0f;
        int editorWidth     = 900;
        int editorHeight    = 520;

        // Phase 2: Zoom
        float zoomMinFreq   = 20.0f;
        float zoomMaxFreq   = 20000.0f;

        // Phase 3: Peak hold
        bool peakHoldEnabled  = false;
        float peakDecayRate   = 20.0f;  // dB/sec

        // Phase 4: RTA
        bool rtaEnabled       = false;

        // Phase 5: Bloom
        bool bloomEnabled     = false;
        float bloomIntensity  = 0.8f;
        float bloomThreshold  = 0.3f;

        // Phase 6: Nebula
        bool nebulaMode       = false;
    };

    Settings settings;

private:
    void timerCallback() override;

    static constexpr int fifoCapacity = 48000;

    AudioFifo audioFifo{fifoCapacity};
    SpectralAnalyser analyser;

    // Stereo FIFOs for Nebula mode
    AudioFifo stereoFifoL{fifoCapacity};
    AudioFifo stereoFifoR{fifoCapacity};
    StereoSpectralAnalyser stereoAnalyser;

    std::vector<float> fifoReadBuffer;
    std::vector<float> stereoReadBufL;
    std::vector<float> stereoReadBufR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramProcessor)
};
