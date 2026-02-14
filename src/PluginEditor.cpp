#include "PluginEditor.h"
#include <cmath>

using namespace juce::gl;

static constexpr double minLogFreq = 20.0;

// ── GLSL Shaders ────────────────────────────────────────────────────────

static const char* vertexShaderSource = R"(
    #version 330 core
    layout(location = 0) in vec2 position;
    out vec2 vTexCoord;
    void main()
    {
        gl_Position = vec4(position, 0.0, 1.0);
        vTexCoord = position * 0.5 + 0.5;
    }
)";

static const char* fragmentShaderSource = R"(
    #version 330 core
    in vec2 vTexCoord;
    out vec4 fragColour;

    uniform sampler2D magnitudeTexture;
    uniform float scrollOffset;
    uniform int colourMapType;
    uniform float dbFloor;
    uniform float dbCeiling;
    uniform int useLogScale;
    uniform float logMinFreq;
    uniform float nyquist;

    vec3 heatMap(float t)
    {
        if (t < 0.2) { float s = t / 0.2;          return vec3(0.0, 0.0, s); }
        if (t < 0.4) { float s = (t - 0.2) / 0.2;  return vec3(0.0, s, 1.0); }
        if (t < 0.6) { float s = (t - 0.4) / 0.2;  return vec3(s, 1.0, 1.0 - s); }
        if (t < 0.8) { float s = (t - 0.6) / 0.2;  return vec3(1.0, 1.0 - s, 0.0); }
                       float s = (t - 0.8) / 0.2;  return vec3(1.0, s, s);
    }

    vec3 lerp3(float t, vec3 a, vec3 b) { return mix(a, b, t); }

    vec3 magmaMap(float t)
    {
        if (t < 0.25) return lerp3(t / 0.25,           vec3(0.0, 0.0, 0.02),   vec3(0.27, 0.0, 0.33));
        if (t < 0.5)  return lerp3((t - 0.25) / 0.25,  vec3(0.27, 0.0, 0.33),  vec3(0.73, 0.21, 0.47));
        if (t < 0.75) return lerp3((t - 0.5) / 0.25,   vec3(0.73, 0.21, 0.47), vec3(0.99, 0.57, 0.25));
                       return lerp3((t - 0.75) / 0.25,  vec3(0.99, 0.57, 0.25), vec3(0.99, 0.99, 0.75));
    }

    vec3 infernoMap(float t)
    {
        if (t < 0.25) return lerp3(t / 0.25,           vec3(0.0, 0.0, 0.02),   vec3(0.34, 0.06, 0.38));
        if (t < 0.5)  return lerp3((t - 0.25) / 0.25,  vec3(0.34, 0.06, 0.38), vec3(0.85, 0.21, 0.16));
        if (t < 0.75) return lerp3((t - 0.5) / 0.25,   vec3(0.85, 0.21, 0.16), vec3(0.99, 0.64, 0.03));
                       return lerp3((t - 0.75) / 0.25,  vec3(0.99, 0.64, 0.03), vec3(0.98, 0.99, 0.64));
    }

    vec3 grayscaleMap(float t) { return vec3(t); }

    vec3 rainbowMap(float t)
    {
        float hue = (1.0 - t) * 0.75;
        float v = t > 0.01 ? 1.0 : 0.0;
        // HSV to RGB
        float c = v;
        float h6 = hue * 6.0;
        float x = c * (1.0 - abs(mod(h6, 2.0) - 1.0));
        vec3 rgb;
        if      (h6 < 1.0) rgb = vec3(c, x, 0.0);
        else if (h6 < 2.0) rgb = vec3(x, c, 0.0);
        else if (h6 < 3.0) rgb = vec3(0.0, c, x);
        else if (h6 < 4.0) rgb = vec3(0.0, x, c);
        else if (h6 < 5.0) rgb = vec3(x, 0.0, c);
        else                rgb = vec3(c, 0.0, x);
        return rgb;
    }

    void main()
    {
        // Scroll: shift x coordinate
        float x = vTexCoord.x + scrollOffset;
        if (x >= 1.0) x -= 1.0;

        // Frequency mapping: vTexCoord.y goes 0 (bottom) to 1 (top)
        float y = vTexCoord.y;
        if (useLogScale == 1)
        {
            float freq = logMinFreq * pow(nyquist / logMinFreq, y);
            y = freq / nyquist;
        }

        float db = texture(magnitudeTexture, vec2(x, y)).r;
        float t = clamp((db - dbFloor) / (dbCeiling - dbFloor), 0.0, 1.0);

        vec3 colour;
        if      (colourMapType == 0) colour = heatMap(t);
        else if (colourMapType == 1) colour = magmaMap(t);
        else if (colourMapType == 2) colour = infernoMap(t);
        else if (colourMapType == 3) colour = grayscaleMap(t);
        else                         colour = rainbowMap(t);

        fragColour = vec4(colour, 1.0);
    }
)";

// ── Constructor / Destructor ────────────────────────────────────────────

SpectrogramEditor::SpectrogramEditor(SpectrogramProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // Restore display settings from processor state
    const auto& s = processorRef.settings;
    colourMapType = static_cast<ColourMap::Type>(std::clamp(s.colourMapId, 1, 5) - 1);
    logScale      = s.logScale;
    dbFloor       = s.dbFloor;
    dbCeiling     = s.dbCeiling;

    setSize(s.editorWidth, s.editorHeight);
    setResizable(true, true);
    setResizeLimits(600, 350, 1920, 1080);

    buildControls();

    // Set control states to match restored settings
    fftSizeBox.setSelectedId(s.fftSizeId, juce::dontSendNotification);
    overlapBox.setSelectedId(s.overlapId, juce::dontSendNotification);
    windowBox.setSelectedId(s.windowId, juce::dontSendNotification);
    colourMapBox.setSelectedId(s.colourMapId, juce::dontSendNotification);
    scaleButton.setToggleState(logScale, juce::dontSendNotification);
    scaleButton.setButtonText(logScale ? "Log" : "Linear");
    dbFloorSlider.setValue(dbFloor, juce::dontSendNotification);
    dbCeilingSlider.setValue(dbCeiling, juce::dontSendNotification);

    glContext.setRenderer(this);
    glContext.setContinuousRepainting(false);
    glContext.attachTo(*this);

    startTimerHz(60);
}

SpectrogramEditor::~SpectrogramEditor()
{
    stopTimer();
    glContext.detach();
}

// ── OpenGL ──────────────────────────────────────────────────────────────

void SpectrogramEditor::newOpenGLContextCreated()
{
    shader = std::make_unique<juce::OpenGLShaderProgram>(glContext);

    if (shader->addVertexShader(vertexShaderSource)
        && shader->addFragmentShader(fragmentShaderSource)
        && shader->link())
    {
        glInitialised = true;
    }
    else
    {
        DBG("Shader compile error: " + shader->getLastError());
        glInitialised = false;
        return;
    }

    // Fullscreen quad: two triangles covering [-1,1]
    static const GLfloat quadVertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);

    glBindVertexArray(0);

    // Create texture
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SpectrogramEditor::renderOpenGL()
{
    if (!glInitialised)
        return;

    const auto area = getSpectrogramArea();
    const float scale = static_cast<float>(glContext.getRenderingScale());
    const int vpX = static_cast<int>(area.getX() * scale);
    const int vpY = static_cast<int>((getHeight() - area.getBottom()) * scale);
    const int vpW = static_cast<int>(area.getWidth() * scale);
    const int vpH = static_cast<int>(area.getHeight() * scale);

    glEnable(GL_SCISSOR_TEST);
    glScissor(vpX, vpY, vpW, vpH);
    glViewport(vpX, vpY, vpW, vpH);

    // Upload texture data if needed
    if (textureNeedsUpload && textureWidth > 0 && textureNumBins > 0)
    {
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F,
                     textureWidth, textureNumBins, 0,
                     GL_RED, GL_FLOAT, textureData.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        textureNeedsUpload = false;
    }

    const auto& analyser = processorRef.getAnalyser();
    const float nyquist = static_cast<float>(analyser.getSampleRate() / 2.0);

    shader->use();
    shader->setUniform("magnitudeTexture", 0);
    shader->setUniform("scrollOffset", textureWidth > 0
        ? static_cast<float>(writePosition) / static_cast<float>(textureWidth)
        : 0.0f);
    shader->setUniform("colourMapType", static_cast<GLint>(colourMapType));
    shader->setUniform("dbFloor", dbFloor);
    shader->setUniform("dbCeiling", dbCeiling);
    shader->setUniform("useLogScale", logScale ? 1 : 0);
    shader->setUniform("logMinFreq", static_cast<float>(minLogFreq));
    shader->setUniform("nyquist", nyquist);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_SCISSOR_TEST);
}

void SpectrogramEditor::openGLContextClosing()
{
    if (textureId != 0) { glDeleteTextures(1, &textureId); textureId = 0; }
    if (vbo != 0)       { glDeleteBuffers(1, &vbo);        vbo = 0; }
    if (vao != 0)       { glDeleteVertexArrays(1, &vao);   vao = 0; }
    shader.reset();
    glInitialised = false;
}

// ── Frequency mapping ───────────────────────────────────────────────────

float SpectrogramEditor::freqToNorm(double freq) const
{
    const double nyquist = processorRef.getAnalyser().getSampleRate() / 2.0;

    if (!logScale)
        return static_cast<float>(freq / nyquist);

    if (freq <= minLogFreq) return 0.0f;
    if (freq >= nyquist)    return 1.0f;

    return static_cast<float>((std::log(freq) - std::log(minLogFreq))
                             / (std::log(nyquist) - std::log(minLogFreq)));
}

double SpectrogramEditor::normToFreq(float norm) const
{
    const double nyquist = processorRef.getAnalyser().getSampleRate() / 2.0;

    if (!logScale)
        return norm * nyquist;

    norm = std::clamp(norm, 0.0f, 1.0f);
    return minLogFreq * std::pow(nyquist / minLogFreq, static_cast<double>(norm));
}

juce::Rectangle<int> SpectrogramEditor::getSpectrogramArea() const
{
    return getLocalBounds().withTrimmedLeft(leftMargin)
                           .withTrimmedBottom(bottomMargin)
                           .withTrimmedRight(rightMargin)
                           .withTrimmedTop(topMargin + controlBarHeight);
}

// ── Controls ────────────────────────────────────────────────────────────

void SpectrogramEditor::buildControls()
{
    auto setupLabel = [this](juce::Label& label)
    {
        label.setFont(juce::FontOptions(11.0f));
        label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
    };

    fftSizeBox.addItem("1024",  1);
    fftSizeBox.addItem("2048",  2);
    fftSizeBox.addItem("4096",  3);
    fftSizeBox.addItem("8192",  4);
    fftSizeBox.setSelectedId(3);
    fftSizeBox.onChange = [this] { onFFTSizeChanged(); };
    addAndMakeVisible(fftSizeBox);
    setupLabel(fftSizeLabel);

    overlapBox.addItem("50%", 1);
    overlapBox.addItem("75%", 2);
    overlapBox.setSelectedId(1);
    overlapBox.onChange = [this] { onOverlapChanged(); };
    addAndMakeVisible(overlapBox);
    setupLabel(overlapLabel);

    windowBox.addItem("Hann",           1);
    windowBox.addItem("Blackman-Harris", 2);
    windowBox.setSelectedId(1);
    windowBox.onChange = [this] { onWindowChanged(); };
    addAndMakeVisible(windowBox);
    setupLabel(windowLabel);

    colourMapBox.addItem("Heat",      1);
    colourMapBox.addItem("Magma",     2);
    colourMapBox.addItem("Inferno",   3);
    colourMapBox.addItem("Grayscale", 4);
    colourMapBox.addItem("Rainbow",   5);
    colourMapBox.setSelectedId(1);
    colourMapBox.onChange = [this]
    {
        colourMapType = static_cast<ColourMap::Type>(colourMapBox.getSelectedId() - 1);
        processorRef.settings.colourMapId = colourMapBox.getSelectedId();
        repaint();
    };
    addAndMakeVisible(colourMapBox);
    setupLabel(colourLabel);

    scaleButton.setClickingTogglesState(true);
    scaleButton.setToggleState(logScale, juce::dontSendNotification);
    scaleButton.onClick = [this]
    {
        logScale = scaleButton.getToggleState();
        scaleButton.setButtonText(logScale ? "Log" : "Linear");
        processorRef.settings.logScale = logScale;
        repaint();
    };
    addAndMakeVisible(scaleButton);

    freezeButton.setClickingTogglesState(true);
    freezeButton.onClick = [this]
    {
        frozen = freezeButton.getToggleState();
        freezeButton.setButtonText(frozen ? "Resume" : "Freeze");
    };
    addAndMakeVisible(freezeButton);

    dbFloorSlider.setRange(-120.0, -20.0, 1.0);
    dbFloorSlider.setValue(dbFloor, juce::dontSendNotification);
    dbFloorSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    dbFloorSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
    dbFloorSlider.onValueChange = [this]
    {
        dbFloor = static_cast<float>(dbFloorSlider.getValue());
        processorRef.settings.dbFloor = dbFloor;
        repaint();
    };
    addAndMakeVisible(dbFloorSlider);
    setupLabel(dbFloorLabel);

    dbCeilingSlider.setRange(-30.0, 10.0, 1.0);
    dbCeilingSlider.setValue(dbCeiling, juce::dontSendNotification);
    dbCeilingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    dbCeilingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
    dbCeilingSlider.onValueChange = [this]
    {
        dbCeiling = static_cast<float>(dbCeilingSlider.getValue());
        processorRef.settings.dbCeiling = dbCeiling;
        repaint();
    };
    addAndMakeVisible(dbCeilingSlider);
    setupLabel(dbCeilLabel);
}

void SpectrogramEditor::onFFTSizeChanged()
{
    auto& analyser = processorRef.getAnalyser();
    SpectralAnalyser::FFTOrder order;
    switch (fftSizeBox.getSelectedId())
    {
        case 1:  order = SpectralAnalyser::FFTOrder::order1024; break;
        case 2:  order = SpectralAnalyser::FFTOrder::order2048; break;
        case 4:  order = SpectralAnalyser::FFTOrder::order8192; break;
        default: order = SpectralAnalyser::FFTOrder::order4096; break;
    }
    analyser.prepare(analyser.getSampleRate(), order);
    processorRef.settings.fftSizeId = fftSizeBox.getSelectedId();
    textureData.clear();
    textureWidth = 0;
    writePosition = 0;
}

void SpectrogramEditor::onOverlapChanged()
{
    processorRef.getAnalyser().setOverlap(overlapBox.getSelectedId() == 2 ? 0.75f : 0.5f);
    processorRef.settings.overlapId = overlapBox.getSelectedId();
}

void SpectrogramEditor::onWindowChanged()
{
    processorRef.getAnalyser().setWindowType(windowBox.getSelectedId() == 2
        ? SpectralAnalyser::WindowType::blackmanHarris
        : SpectralAnalyser::WindowType::hann);
    processorRef.settings.windowId = windowBox.getSelectedId();
}

// ── Timer / frame processing ────────────────────────────────────────────

void SpectrogramEditor::timerCallback()
{
    if (frozen)
        return;

    auto& analyser = processorRef.getAnalyser();
    const int numBins = analyser.getNumBins();
    const auto area = getSpectrogramArea();
    const int w = area.getWidth();

    if (w <= 0 || numBins <= 0)
        return;

    // Resize texture data if needed
    if (textureWidth != w || textureNumBins != numBins)
    {
        textureWidth = w;
        textureNumBins = numBins;
        textureData.assign(static_cast<size_t>(w) * static_cast<size_t>(numBins), -100.0f);
        writePosition = 0;
    }

    if (frameBuffer.size() != static_cast<size_t>(numBins))
        frameBuffer.resize(static_cast<size_t>(numBins));

    bool gotNewData = false;

    while (analyser.pullNextFrame(frameBuffer.data(), numBins))
    {
        // Write one column into the texture: column writePosition, all bins
        for (int bin = 0; bin < numBins; ++bin)
        {
            textureData[static_cast<size_t>(bin) * static_cast<size_t>(textureWidth)
                        + static_cast<size_t>(writePosition)] = frameBuffer[static_cast<size_t>(bin)];
        }

        lastFrame.assign(frameBuffer.begin(), frameBuffer.end());
        writePosition = (writePosition + 1) % textureWidth;
        gotNewData = true;
    }

    if (gotNewData)
    {
        textureNeedsUpload = true;
        glContext.triggerRepaint();
        repaint();
    }
}

// ── Mouse interaction ───────────────────────────────────────────────────

void SpectrogramEditor::mouseMove(const juce::MouseEvent& e)
{
    mouseInside = true;
    mousePos = e.getPosition();
    repaint();
}

void SpectrogramEditor::mouseExit(const juce::MouseEvent&)
{
    mouseInside = false;
    repaint();
}

// ── Paint (overlay: axes, labels, hover — spectrogram rendered by GL) ───

void SpectrogramEditor::paint(juce::Graphics& g)
{
    const auto spectArea = getSpectrogramArea();
    const auto bg = juce::Colour(0xff1a1a2e);

    // Fill only the areas outside the spectrogram so we don't cover GL rendering
    g.setColour(bg);
    g.fillRect(getLocalBounds().removeFromTop(spectArea.getY()));                    // top strip
    g.fillRect(getLocalBounds().removeFromBottom(getHeight() - spectArea.getBottom())); // bottom strip
    g.fillRect(spectArea.getX(), spectArea.getY(), 0, spectArea.getHeight());        // already covered
    g.fillRect(0, spectArea.getY(), spectArea.getX(), spectArea.getHeight());        // left margin
    g.fillRect(spectArea.getRight(), spectArea.getY(),
               getWidth() - spectArea.getRight(), spectArea.getHeight());            // right margin

    // Border around spectrogram
    g.setColour(juce::Colours::grey);
    g.drawRect(spectArea, 1);

    drawFrequencyAxis(g, spectArea);
    drawTimeAxis(g, spectArea);
    drawDbScale(g, spectArea);

    if (mouseInside)
        drawHoverInfo(g, spectArea);
}

void SpectrogramEditor::drawFrequencyAxis(juce::Graphics& g, juce::Rectangle<int> area)
{
    const double nyquist = processorRef.getAnalyser().getSampleRate() / 2.0;
    g.setFont(juce::FontOptions(11.0f));

    const double freqStops[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };

    for (double freq : freqStops)
    {
        if (freq >= nyquist) break;

        const float norm = freqToNorm(freq);
        const int y = area.getBottom() - static_cast<int>(norm * area.getHeight());

        if (y < area.getY() + 8 || y > area.getBottom() - 4) continue;

        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawHorizontalLine(y, static_cast<float>(area.getX()),
                             static_cast<float>(area.getRight()));

        g.setColour(juce::Colours::lightgrey);
        juce::String label = (freq >= 1000.0)
            ? juce::String(freq / 1000.0, (freq >= 10000.0) ? 0 : 1) + "k"
            : juce::String(static_cast<int>(freq));

        g.drawText(label, area.getX() - leftMargin, y - 7,
                   leftMargin - 6, 14, juce::Justification::centredRight);
    }
}

void SpectrogramEditor::drawTimeAxis(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto& analyser = processorRef.getAnalyser();
    const double sampleRate = analyser.getSampleRate();
    const int fftSize = analyser.getFFTSize();
    if (sampleRate <= 0.0 || fftSize <= 0) return;

    const double hopSamples = fftSize * 0.5;
    const double secondsPerColumn = hopSamples / sampleRate;
    const double totalSeconds = area.getWidth() * secondsPerColumn;

    g.setFont(juce::FontOptions(11.0f));

    double tickInterval = 0.5;
    if (totalSeconds > 20.0) tickInterval = 5.0;
    else if (totalSeconds > 10.0) tickInterval = 2.0;
    else if (totalSeconds > 5.0) tickInterval = 1.0;

    const int labelY = area.getBottom() + 3;

    for (double t = 0.0; t < totalSeconds; t += tickInterval)
    {
        const int x = area.getRight() - static_cast<int>(t / secondsPerColumn);
        if (x < area.getX()) break;

        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawVerticalLine(x, static_cast<float>(area.getY()),
                           static_cast<float>(area.getBottom()));

        g.setColour(juce::Colours::lightgrey);
        juce::String label = (t == 0.0) ? "now" : ("-" + juce::String(t, 1) + "s");
        g.drawText(label, x - 25, labelY, 50, 16, juce::Justification::centred);
    }
}

void SpectrogramEditor::drawDbScale(juce::Graphics& g, juce::Rectangle<int> area)
{
    const int barWidth = 8;
    const int barX = area.getRight() + 3;
    const int barH = area.getHeight();
    const int barY = area.getY();
    if (barX + barWidth > getWidth()) return;

    for (int y = 0; y < barH; ++y)
    {
        float t = 1.0f - static_cast<float>(y) / static_cast<float>(barH - 1);
        g.setColour(ColourMap::map(colourMapType, t));
        g.fillRect(barX, barY + y, barWidth, 1);
    }
}

void SpectrogramEditor::drawHoverInfo(juce::Graphics& g, juce::Rectangle<int> area)
{
    if (!area.contains(mousePos) || lastFrame.empty()) return;

    const double nyquist = processorRef.getAnalyser().getSampleRate() / 2.0;
    const int numBins = static_cast<int>(lastFrame.size());

    const float norm = static_cast<float>(area.getBottom() - mousePos.y)
                     / static_cast<float>(area.getHeight());
    const double freq = normToFreq(std::clamp(norm, 0.0f, 1.0f));

    const float binFloat = static_cast<float>(freq / nyquist) * static_cast<float>(numBins - 1);
    const int bin = std::clamp(static_cast<int>(binFloat), 0, numBins - 1);
    const float db = lastFrame[static_cast<size_t>(bin)];

    juce::String freqStr = (freq >= 1000.0)
        ? juce::String(freq / 1000.0, 2) + " kHz"
        : juce::String(static_cast<int>(freq)) + " Hz";
    juce::String text = freqStr + "  |  " + juce::String(db, 1) + " dB";

    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.drawHorizontalLine(mousePos.y, static_cast<float>(area.getX()),
                         static_cast<float>(area.getRight()));
    g.drawVerticalLine(mousePos.x, static_cast<float>(area.getY()),
                       static_cast<float>(area.getBottom()));

    const int boxW = 160, boxH = 20;
    int boxX = mousePos.x + 12;
    int boxY = mousePos.y - boxH - 4;
    if (boxX + boxW > area.getRight()) boxX = mousePos.x - boxW - 12;
    if (boxY < area.getY()) boxY = mousePos.y + 8;

    g.setColour(juce::Colour(0xdd000000));
    g.fillRoundedRectangle(static_cast<float>(boxX), static_cast<float>(boxY),
                           static_cast<float>(boxW), static_cast<float>(boxH), 4.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(text, boxX, boxY, boxW, boxH, juce::Justification::centred);
}

// ── Layout ──────────────────────────────────────────────────────────────

void SpectrogramEditor::resized()
{
    processorRef.settings.editorWidth = getWidth();
    processorRef.settings.editorHeight = getHeight();

    textureData.clear();
    textureWidth = 0;
    writePosition = 0;

    auto area = getLocalBounds();
    auto controlArea = area.removeFromTop(controlBarHeight).reduced(4, 4);

    const int labelW = 42;
    const int gap = 4;
    const int buttonW = 55;
    const int sliderW = 80;

    auto placeCombo = [&](juce::Label& label, juce::ComboBox& box, int w)
    {
        label.setBounds(controlArea.removeFromLeft(labelW));
        box.setBounds(controlArea.removeFromLeft(w));
        controlArea.removeFromLeft(gap);
    };

    placeCombo(fftSizeLabel, fftSizeBox, 64);
    placeCombo(overlapLabel, overlapBox, 56);
    placeCombo(windowLabel, windowBox, 110);
    placeCombo(colourLabel, colourMapBox, 80);

    scaleButton.setBounds(controlArea.removeFromLeft(buttonW));
    controlArea.removeFromLeft(gap);
    freezeButton.setBounds(controlArea.removeFromLeft(buttonW));
    controlArea.removeFromLeft(gap);

    dbFloorLabel.setBounds(controlArea.removeFromLeft(34));
    dbFloorSlider.setBounds(controlArea.removeFromLeft(sliderW));
    controlArea.removeFromLeft(gap);
    dbCeilLabel.setBounds(controlArea.removeFromLeft(28));
    dbCeilingSlider.setBounds(controlArea.removeFromLeft(sliderW));
}
