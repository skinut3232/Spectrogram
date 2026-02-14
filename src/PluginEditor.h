#pragma once

#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "ColourMap.h"

class SpectrogramEditor : public juce::AudioProcessorEditor,
                           private juce::Timer,
                           private juce::OpenGLRenderer
{
public:
    explicit SpectrogramEditor(SpectrogramProcessor&);
    ~SpectrogramEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    // OpenGLRenderer
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

private:
    void timerCallback() override;

    void drawFrequencyAxis(juce::Graphics& g, juce::Rectangle<int> area);
    void drawTimeAxis(juce::Graphics& g, juce::Rectangle<int> area);
    void drawDbScale(juce::Graphics& g, juce::Rectangle<int> area);
    void drawHoverInfo(juce::Graphics& g, juce::Rectangle<int> area);

    float freqToNorm(double freq) const;
    double normToFreq(float norm) const;

    juce::Rectangle<int> getSpectrogramArea() const;

    void buildControls();
    void onFFTSizeChanged();
    void onOverlapChanged();
    void onWindowChanged();

    SpectrogramProcessor& processorRef;

    // OpenGL
    juce::OpenGLContext glContext;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    GLuint vao = 0, vbo = 0;
    GLuint textureId = 0;
    bool glInitialised = false;

    // Spectral texture data: [textureWidth * numBins] floats, circular columns
    std::vector<float> textureData;
    int textureWidth = 0;
    int textureNumBins = 0;
    int writePosition = 0;
    bool textureNeedsUpload = false;

    // Scratch buffer for pulling frames
    std::vector<float> frameBuffer;
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
