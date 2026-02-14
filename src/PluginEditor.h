#pragma once

#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "ColourMap.h"
#include "CustomLookAndFeel.h"
#include "StereoSpectralAnalyser.h"

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
    void drawNebulaAxis(juce::Graphics& g, juce::Rectangle<int> area);
    void drawDbScale(juce::Graphics& g, juce::Rectangle<int> area);
    void drawHoverInfo(juce::Graphics& g, juce::Rectangle<int> area);
    void drawMagnitudeCurve(juce::Graphics& g, juce::Rectangle<int> area,
                            const std::vector<float>& data, juce::Colour colour,
                            bool filled);
    void drawGridLines(juce::Graphics& g, juce::Rectangle<int> area);

    float freqToNorm(double freq) const;
    double normToFreq(float norm) const;

    juce::Rectangle<int> getSpectrogramArea() const;

    void buildControls();
    void onFFTSizeChanged();
    void onOverlapChanged();
    void onWindowChanged();
    void updateModeVisibility();

    // Bloom FBO helpers
    void createBloomResources(int width, int height);
    void destroyBloomResources();
    void renderWithBloom(int vpX, int vpY, int vpW, int vpH);

    // Nebula helpers
    void updateNebulaTexture();

    SpectrogramProcessor& processorRef;
    CustomLookAndFeel customLnf;

    // OpenGL
    juce::OpenGLContext glContext;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    GLuint vao = 0, vbo = 0;
    GLuint textureId = 0;
    bool glInitialised = false;

    // Spectral texture data: [textureWidth * numBins] floats, circular columns
    // Double-buffered: front for GL thread, back for message thread
    std::vector<float> textureDataFront;
    std::vector<float> textureDataBack;
    int textureWidth = 0;
    int textureNumBins = 0;
    int writePosition = 0;
    std::atomic<bool> textureNeedsUpload{false};

    // Scratch buffer for pulling frames
    std::vector<float> frameBuffer;
    std::vector<float> lastFrame;

    // Display settings
    float dbFloor = -90.0f;
    float dbCeiling = 0.0f;
    bool logScale = true;
    bool frozen = false;
    ColourMap::Type colourMapType = ColourMap::Type::heat;

    // Phase 2: Zoom
    float zoomMinFreq = 20.0f;
    float zoomMaxFreq = 20000.0f;

    // Phase 3: Peak hold
    bool peakHoldEnabled = false;
    float peakDecayRate = 20.0f;
    std::vector<float> peakHoldData;
    double lastTimerTime = 0.0;

    // Phase 4: RTA
    bool rtaEnabled = false;

    // Phase 5: Bloom
    bool bloomEnabled = false;
    float bloomIntensity = 0.8f;
    float bloomThreshold = 0.3f;

    // Bloom FBO resources
    GLuint sceneFBO = 0, sceneTex = 0;
    GLuint bloomFBO1 = 0, bloomTex1 = 0;
    GLuint bloomFBO2 = 0, bloomTex2 = 0;
    int bloomWidth = 0, bloomHeight = 0;
    std::unique_ptr<juce::OpenGLShaderProgram> brightExtractShader;
    std::unique_ptr<juce::OpenGLShaderProgram> blurShader;
    std::unique_ptr<juce::OpenGLShaderProgram> compositeShader;
    std::unique_ptr<juce::OpenGLShaderProgram> nebulaShader;

    // Phase 6: Nebula
    bool nebulaMode = false;
    GLuint nebulaTexId = 0;
    std::vector<float> nebulaAccum;   // [nebulaTexW * nebulaTexH * 3] RGB
    static constexpr int nebulaTexW = 256;  // pan resolution
    static constexpr int nebulaTexH = 512;  // frequency resolution
    StereoFrame stereoFrame;

    // Hover state
    bool mouseInside = false;
    juce::Point<int> mousePos;

    // Layout
    static constexpr int leftMargin = 55;
    static constexpr int bottomMargin = 25;
    static constexpr int rightMargin = 15;
    static constexpr int topMargin = 10;
    static constexpr int controlBarHeight = 60;

    // Row 1 controls: Analysis + Display
    juce::ComboBox fftSizeBox;
    juce::ComboBox overlapBox;
    juce::ComboBox windowBox;
    juce::ComboBox colourMapBox;
    juce::TextButton scaleButton{"Log"};
    juce::TextButton freezeButton{"Freeze"};

    // Row 2 controls: Mode + Effects + Range
    juce::ComboBox modeBox;
    juce::TextButton bloomButton{"Bloom"};
    juce::TextButton peakButton{"Peak"};
    juce::TextButton rtaButton{"RTA"};
    juce::Slider dbFloorSlider;
    juce::Slider dbCeilingSlider;
    juce::Slider zoomMinSlider;
    juce::Slider zoomMaxSlider;
    juce::Slider bloomIntensitySlider;
    juce::Slider peakDecaySlider;

    // Labels
    juce::Label fftSizeLabel{{}, "FFT"};
    juce::Label overlapLabel{{}, "Overlap"};
    juce::Label windowLabel{{}, "Window"};
    juce::Label colourLabel{{}, "Colour"};
    juce::Label dbFloorLabel{{}, "Floor"};
    juce::Label dbCeilLabel{{}, "Ceil"};
    juce::Label modeLabel{{}, "Mode"};
    juce::Label zoomMinLabel{{}, "Lo"};
    juce::Label zoomMaxLabel{{}, "Hi"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramEditor)
};
