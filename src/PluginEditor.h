#pragma once

#include "PluginProcessor.h"
#include "ColourMap.h"

class SpectrogramEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit SpectrogramEditor(SpectrogramProcessor&);
    ~SpectrogramEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void drawSpectrogram(juce::Graphics& g, juce::Rectangle<int> area);
    void drawFrequencyAxis(juce::Graphics& g, juce::Rectangle<int> area);
    void drawTimeAxis(juce::Graphics& g, juce::Rectangle<int> area);
    void drawDbScale(juce::Graphics& g, juce::Rectangle<int> area);

    void renderColumnToImage(const float* magnitudesDb, int numBins);

    SpectrogramProcessor& processorRef;

    // Spectrogram image and scroll position
    juce::Image spectrogramImage;
    int writePosition = 0;

    // Scratch buffer for pulling frames
    std::vector<float> frameBuffer;

    // Dynamic range
    float dbFloor = -90.0f;
    float dbCeiling = 0.0f;

    // Layout margins
    static constexpr int leftMargin = 55;
    static constexpr int bottomMargin = 25;
    static constexpr int rightMargin = 10;
    static constexpr int topMargin = 10;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramEditor)
};
