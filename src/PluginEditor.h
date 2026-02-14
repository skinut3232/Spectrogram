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

    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    void drawSpectrogram(juce::Graphics& g, juce::Rectangle<int> area);
    void drawFrequencyAxis(juce::Graphics& g, juce::Rectangle<int> area);
    void drawTimeAxis(juce::Graphics& g, juce::Rectangle<int> area);
    void drawDbScale(juce::Graphics& g, juce::Rectangle<int> area);
    void drawHoverInfo(juce::Graphics& g, juce::Rectangle<int> area);

    void renderColumnToImage(const float* magnitudesDb, int numBins);

    // Frequency mapping helpers
    float freqToNorm(double freq) const;
    double normToFreq(float norm) const;

    void buildControls();
    void onFFTSizeChanged();
    void onOverlapChanged();
    void onWindowChanged();

    SpectrogramProcessor& processorRef;

    // Spectrogram image and scroll position
    juce::Image spectrogramImage;
    int writePosition = 0;

    // Scratch buffer for pulling frames
    std::vector<float> frameBuffer;

    // Last rendered frame for hover readout
    std::vector<float> lastFrame;

    // Display settings
    float dbFloor = -90.0f;
    float dbCeiling = 0.0f;
    bool logScale = true;
    bool frozen = false;
    ColourMap::Type colourMapType = ColourMap::Type::heat;

    // Hover state
    bool mouseInside = false;
    juce::Point<int> mousePos;

    // Layout
    static constexpr int leftMargin = 55;
    static constexpr int bottomMargin = 25;
    static constexpr int rightMargin = 15;
    static constexpr int topMargin = 10;
    static constexpr int controlBarHeight = 32;

    // Controls
    juce::ComboBox fftSizeBox;
    juce::ComboBox overlapBox;
    juce::ComboBox windowBox;
    juce::ComboBox colourMapBox;
    juce::TextButton scaleButton{"Log"};
    juce::TextButton freezeButton{"Freeze"};
    juce::Slider dbFloorSlider;
    juce::Slider dbCeilingSlider;

    juce::Label fftSizeLabel{{}, "FFT"};
    juce::Label overlapLabel{{}, "Overlap"};
    juce::Label windowLabel{{}, "Window"};
    juce::Label colourLabel{{}, "Colour"};
    juce::Label dbFloorLabel{{}, "Floor"};
    juce::Label dbCeilLabel{{}, "Ceil"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramEditor)
};
