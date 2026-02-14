#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AudioFifo.h"
#include "SpectralAnalyser.h"

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

private:
    void timerCallback() override;

    static constexpr int fifoCapacity = 48000;

    AudioFifo audioFifo{fifoCapacity};
    SpectralAnalyser analyser;

    std::vector<float> fifoReadBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramProcessor)
};
