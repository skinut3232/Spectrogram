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
    uniform float zoomMinFreq;
    uniform float zoomMaxFreq;

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

    vec3 viridisMap(float t)
    {
        if (t < 0.25) return lerp3(t / 0.25,           vec3(0.267, 0.004, 0.329), vec3(0.282, 0.140, 0.458));
        if (t < 0.5)  return lerp3((t - 0.25) / 0.25,  vec3(0.282, 0.140, 0.458), vec3(0.127, 0.566, 0.551));
        if (t < 0.75) return lerp3((t - 0.5) / 0.25,   vec3(0.127, 0.566, 0.551), vec3(0.554, 0.812, 0.246));
                       return lerp3((t - 0.75) / 0.25,  vec3(0.554, 0.812, 0.246), vec3(0.993, 0.906, 0.144));
    }

    vec3 plasmaMap(float t)
    {
        if (t < 0.25) return lerp3(t / 0.25,           vec3(0.050, 0.030, 0.528), vec3(0.417, 0.001, 0.658));
        if (t < 0.5)  return lerp3((t - 0.25) / 0.25,  vec3(0.417, 0.001, 0.658), vec3(0.748, 0.149, 0.475));
        if (t < 0.75) return lerp3((t - 0.5) / 0.25,   vec3(0.748, 0.149, 0.475), vec3(0.963, 0.467, 0.165));
                       return lerp3((t - 0.75) / 0.25,  vec3(0.963, 0.467, 0.165), vec3(0.940, 0.975, 0.131));
    }

    vec3 turboMap(float t)
    {
        if (t < 0.25) return lerp3(t / 0.25,           vec3(0.190, 0.072, 0.232), vec3(0.133, 0.570, 0.902));
        if (t < 0.5)  return lerp3((t - 0.25) / 0.25,  vec3(0.133, 0.570, 0.902), vec3(0.341, 0.890, 0.298));
        if (t < 0.75) return lerp3((t - 0.5) / 0.25,   vec3(0.341, 0.890, 0.298), vec3(0.951, 0.651, 0.039));
                       return lerp3((t - 0.75) / 0.25,  vec3(0.951, 0.651, 0.039), vec3(0.600, 0.040, 0.098));
    }

    void main()
    {
        float x = vTexCoord.x + scrollOffset;
        if (x >= 1.0) x -= 1.0;

        // Frequency mapping with zoom support
        float y = vTexCoord.y;

        // Map from display [0,1] to frequency using zoom range
        float freq;
        if (useLogScale == 1)
        {
            freq = zoomMinFreq * pow(zoomMaxFreq / zoomMinFreq, y);
        }
        else
        {
            freq = zoomMinFreq + (zoomMaxFreq - zoomMinFreq) * y;
        }
        // Map frequency to texture coordinate (linear 0..nyquist)
        y = freq / nyquist;

        float db = texture(magnitudeTexture, vec2(x, y)).r;
        float t = clamp((db - dbFloor) / (dbCeiling - dbFloor), 0.0, 1.0);

        vec3 colour;
        if      (colourMapType == 0) colour = heatMap(t);
        else if (colourMapType == 1) colour = magmaMap(t);
        else if (colourMapType == 2) colour = infernoMap(t);
        else if (colourMapType == 3) colour = grayscaleMap(t);
        else if (colourMapType == 4) colour = rainbowMap(t);
        else if (colourMapType == 5) colour = viridisMap(t);
        else if (colourMapType == 6) colour = plasmaMap(t);
        else                         colour = turboMap(t);

        fragColour = vec4(colour, 1.0);
    }
)";

// ── Nebula fragment shader ──────────────────────────────────────────────

static const char* nebulaFragmentShaderSource = R"(
    #version 330 core
    in vec2 vTexCoord;
    out vec4 fragColour;

    uniform sampler2D nebulaTexture;

    void main()
    {
        vec3 rgb = texture(nebulaTexture, vTexCoord).rgb;
        fragColour = vec4(rgb, 1.0);
    }
)";

// ── Bloom shaders ───────────────────────────────────────────────────────

static const char* brightExtractFragSource = R"(
    #version 330 core
    in vec2 vTexCoord;
    out vec4 fragColour;
    uniform sampler2D sceneTexture;
    uniform float threshold;

    void main()
    {
        vec3 colour = texture(sceneTexture, vTexCoord).rgb;
        float brightness = dot(colour, vec3(0.2126, 0.7152, 0.0722));
        if (brightness > threshold)
            fragColour = vec4(colour * (brightness - threshold), 1.0);
        else
            fragColour = vec4(0.0, 0.0, 0.0, 1.0);
    }
)";

static const char* blurFragSource = R"(
    #version 330 core
    in vec2 vTexCoord;
    out vec4 fragColour;
    uniform sampler2D inputTexture;
    uniform vec2 direction;
    uniform vec2 texelSize;

    void main()
    {
        float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
        vec3 result = texture(inputTexture, vTexCoord).rgb * weights[0];
        for (int i = 1; i < 5; ++i)
        {
            vec2 offset = direction * texelSize * float(i);
            result += texture(inputTexture, vTexCoord + offset).rgb * weights[i];
            result += texture(inputTexture, vTexCoord - offset).rgb * weights[i];
        }
        fragColour = vec4(result, 1.0);
    }
)";

static const char* compositeFragSource = R"(
    #version 330 core
    in vec2 vTexCoord;
    out vec4 fragColour;
    uniform sampler2D sceneTexture;
    uniform sampler2D bloomTexture;
    uniform float bloomIntensity;

    void main()
    {
        vec3 scene = texture(sceneTexture, vTexCoord).rgb;
        vec3 bloom = texture(bloomTexture, vTexCoord).rgb;
        fragColour = vec4(scene + bloom * bloomIntensity, 1.0);
    }
)";

// ── Constructor / Destructor ────────────────────────────────────────────

SpectrogramEditor::SpectrogramEditor(SpectrogramProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setLookAndFeel(&customLnf);

    // Restore display settings from processor state
    const auto& s = processorRef.settings;
    colourMapType    = static_cast<ColourMap::Type>(std::clamp(s.colourMapId, 1, ColourMap::numTypes) - 1);
    logScale         = s.logScale;
    dbFloor          = s.dbFloor;
    dbCeiling        = s.dbCeiling;
    zoomMinFreq      = s.zoomMinFreq;
    zoomMaxFreq      = s.zoomMaxFreq;
    peakHoldEnabled  = s.peakHoldEnabled;
    peakDecayRate    = s.peakDecayRate;
    rtaEnabled       = s.rtaEnabled;
    bloomEnabled     = s.bloomEnabled;
    bloomIntensity   = s.bloomIntensity;
    bloomThreshold   = s.bloomThreshold;
    nebulaMode       = s.nebulaMode;

    processorRef.nebulaActive.store(nebulaMode, std::memory_order_relaxed);

    setSize(s.editorWidth, s.editorHeight);
    setResizable(true, true);
    setResizeLimits(700, 400, 1920, 1080);

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
    zoomMinSlider.setValue(zoomMinFreq, juce::dontSendNotification);
    zoomMaxSlider.setValue(zoomMaxFreq, juce::dontSendNotification);
    modeBox.setSelectedId(nebulaMode ? 2 : 1, juce::dontSendNotification);
    bloomButton.setToggleState(bloomEnabled, juce::dontSendNotification);
    peakButton.setToggleState(peakHoldEnabled, juce::dontSendNotification);
    rtaButton.setToggleState(rtaEnabled, juce::dontSendNotification);
    bloomIntensitySlider.setValue(bloomIntensity, juce::dontSendNotification);
    peakDecaySlider.setValue(peakDecayRate, juce::dontSendNotification);

    updateModeVisibility();

    glContext.setRenderer(this);
    glContext.setContinuousRepainting(false);
    glContext.attachTo(*this);

    lastTimerTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    startTimerHz(60);
}

SpectrogramEditor::~SpectrogramEditor()
{
    stopTimer();
    glContext.detach();
    setLookAndFeel(nullptr);
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

    // Compile bloom shaders
    brightExtractShader = std::make_unique<juce::OpenGLShaderProgram>(glContext);
    if (!(brightExtractShader->addVertexShader(vertexShaderSource)
          && brightExtractShader->addFragmentShader(brightExtractFragSource)
          && brightExtractShader->link()))
    {
        DBG("Bright extract shader error: " + brightExtractShader->getLastError());
        brightExtractShader.reset();
    }

    blurShader = std::make_unique<juce::OpenGLShaderProgram>(glContext);
    if (!(blurShader->addVertexShader(vertexShaderSource)
          && blurShader->addFragmentShader(blurFragSource)
          && blurShader->link()))
    {
        DBG("Blur shader error: " + blurShader->getLastError());
        blurShader.reset();
    }

    compositeShader = std::make_unique<juce::OpenGLShaderProgram>(glContext);
    if (!(compositeShader->addVertexShader(vertexShaderSource)
          && compositeShader->addFragmentShader(compositeFragSource)
          && compositeShader->link()))
    {
        DBG("Composite shader error: " + compositeShader->getLastError());
        compositeShader.reset();
    }

    // Compile nebula shader
    nebulaShader = std::make_unique<juce::OpenGLShaderProgram>(glContext);
    if (!(nebulaShader->addVertexShader(vertexShaderSource)
          && nebulaShader->addFragmentShader(nebulaFragmentShaderSource)
          && nebulaShader->link()))
    {
        DBG("Nebula shader error: " + nebulaShader->getLastError());
        nebulaShader.reset();
    }

    // Fullscreen quad
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

    // Create spectrogram texture
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create nebula texture
    glGenTextures(1, &nebulaTexId);
    glBindTexture(GL_TEXTURE_2D, nebulaTexId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    nebulaAccum.assign(static_cast<size_t>(nebulaTexW) * nebulaTexH * 3, 0.0f);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, nebulaTexW, nebulaTexH, 0,
                 GL_RGB, GL_FLOAT, nebulaAccum.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SpectrogramEditor::createBloomResources(int width, int height)
{
    destroyBloomResources();

    bloomWidth = width / 2;
    bloomHeight = height / 2;

    // Scene FBO (full res)
    glGenFramebuffers(1, &sceneFBO);
    glGenTextures(1, &sceneTex);
    glBindTexture(GL_TEXTURE_2D, sceneTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTex, 0);

    // Bloom ping-pong FBOs (half res)
    glGenFramebuffers(1, &bloomFBO1);
    glGenTextures(1, &bloomTex1);
    glBindTexture(GL_TEXTURE_2D, bloomTex1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bloomWidth, bloomHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO1);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomTex1, 0);

    glGenFramebuffers(1, &bloomFBO2);
    glGenTextures(1, &bloomTex2);
    glBindTexture(GL_TEXTURE_2D, bloomTex2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bloomWidth, bloomHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO2);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomTex2, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SpectrogramEditor::destroyBloomResources()
{
    if (sceneFBO) { glDeleteFramebuffers(1, &sceneFBO); sceneFBO = 0; }
    if (sceneTex) { glDeleteTextures(1, &sceneTex); sceneTex = 0; }
    if (bloomFBO1) { glDeleteFramebuffers(1, &bloomFBO1); bloomFBO1 = 0; }
    if (bloomTex1) { glDeleteTextures(1, &bloomTex1); bloomTex1 = 0; }
    if (bloomFBO2) { glDeleteFramebuffers(1, &bloomFBO2); bloomFBO2 = 0; }
    if (bloomTex2) { glDeleteTextures(1, &bloomTex2); bloomTex2 = 0; }
    bloomWidth = 0;
    bloomHeight = 0;
}

void SpectrogramEditor::renderWithBloom(int vpX, int vpY, int vpW, int vpH)
{
    if (!brightExtractShader || !blurShader || !compositeShader)
        return;

    // Save JUCE's default FBO
    GLint defaultFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFBO);

    // Recreate bloom FBOs if size changed
    if (bloomWidth != vpW / 2 || bloomHeight != vpH / 2)
        createBloomResources(vpW, vpH);

    // Pass 1: Render spectrogram to scene FBO
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
    glViewport(0, 0, vpW, vpH);

    const auto& analyser = processorRef.getAnalyser();
    const float nyquist = static_cast<float>(analyser.getSampleRate() / 2.0);

    if (nebulaMode && nebulaShader)
    {
        // Render nebula to scene FBO using nebula shader
        nebulaShader->use();
        nebulaShader->setUniform("nebulaTexture", 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, nebulaTexId);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else
    {

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
    shader->setUniform("zoomMinFreq", zoomMinFreq);
    shader->setUniform("zoomMaxFreq", std::min(zoomMaxFreq, nyquist));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    } // end else (spectrogram path)

    // Pass 2: Bright extract to bloom FBO1 (half res)
    glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO1);
    glViewport(0, 0, bloomWidth, bloomHeight);

    brightExtractShader->use();
    brightExtractShader->setUniform("sceneTexture", 0);
    brightExtractShader->setUniform("threshold", bloomThreshold);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTex);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    // Pass 3: Ping-pong Gaussian blur (3 iterations)
    blurShader->use();
    blurShader->setUniform("inputTexture", 0);
    float texelW = 1.0f / static_cast<float>(bloomWidth);
    float texelH = 1.0f / static_cast<float>(bloomHeight);

    for (int i = 0; i < 3; ++i)
    {
        // Horizontal blur: bloomFBO1 -> bloomFBO2
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO2);
        glViewport(0, 0, bloomWidth, bloomHeight);
        blurShader->setUniform("direction", 1.0f, 0.0f);
        blurShader->setUniform("texelSize", texelW, texelH);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, bloomTex1);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        // Vertical blur: bloomFBO2 -> bloomFBO1
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO1);
        glViewport(0, 0, bloomWidth, bloomHeight);
        blurShader->setUniform("direction", 0.0f, 1.0f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, bloomTex2);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }

    // Pass 4: Composite scene + bloom to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(defaultFBO));
    glViewport(vpX, vpY, vpW, vpH);

    compositeShader->use();
    compositeShader->setUniform("sceneTexture", 0);
    compositeShader->setUniform("bloomTexture", 1);
    compositeShader->setUniform("bloomIntensity", bloomIntensity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomTex1);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
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

    // Upload spectrogram texture data if needed
    if (textureNeedsUpload.load(std::memory_order_acquire) && textureWidth > 0 && textureNumBins > 0)
    {
        // Swap front buffer from the back buffer that was written by message thread
        textureDataFront = textureDataBack;

        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F,
                     textureWidth, textureNumBins, 0,
                     GL_RED, GL_FLOAT, textureDataFront.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        textureNeedsUpload.store(false, std::memory_order_release);
    }

    // Upload nebula texture if in nebula mode
    if (nebulaMode && !nebulaAccum.empty())
    {
        glBindTexture(GL_TEXTURE_2D, nebulaTexId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, nebulaTexW, nebulaTexH,
                        GL_RGB, GL_FLOAT, nebulaAccum.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    if (bloomEnabled && brightExtractShader && blurShader && compositeShader)
    {
        renderWithBloom(vpX, vpY, vpW, vpH);
    }
    else if (nebulaMode && nebulaShader)
    {
        // Render nebula texture with nebula shader
        nebulaShader->use();
        nebulaShader->setUniform("nebulaTexture", 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, nebulaTexId);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else
    {
        // Standard spectrogram render
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
        shader->setUniform("zoomMinFreq", zoomMinFreq);
        shader->setUniform("zoomMaxFreq", std::min(zoomMaxFreq, nyquist));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureId);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glDisable(GL_SCISSOR_TEST);
}

void SpectrogramEditor::openGLContextClosing()
{
    destroyBloomResources();

    if (nebulaTexId != 0) { glDeleteTextures(1, &nebulaTexId); nebulaTexId = 0; }
    if (textureId != 0)   { glDeleteTextures(1, &textureId);   textureId = 0; }
    if (vbo != 0)         { glDeleteBuffers(1, &vbo);          vbo = 0; }
    if (vao != 0)         { glDeleteVertexArrays(1, &vao);     vao = 0; }

    shader.reset();
    nebulaShader.reset();
    brightExtractShader.reset();
    blurShader.reset();
    compositeShader.reset();
    glInitialised = false;
}

// ── Frequency mapping (zoom-aware) ──────────────────────────────────────

float SpectrogramEditor::freqToNorm(double freq) const
{
    const double lo = static_cast<double>(zoomMinFreq);
    const double hi = static_cast<double>(zoomMaxFreq);

    if (logScale)
    {
        if (freq <= lo) return 0.0f;
        if (freq >= hi) return 1.0f;
        return static_cast<float>((std::log(freq) - std::log(lo))
                                 / (std::log(hi) - std::log(lo)));
    }
    else
    {
        return static_cast<float>((freq - lo) / (hi - lo));
    }
}

double SpectrogramEditor::normToFreq(float norm) const
{
    const double lo = static_cast<double>(zoomMinFreq);
    const double hi = static_cast<double>(zoomMaxFreq);
    norm = std::clamp(norm, 0.0f, 1.0f);

    if (logScale)
        return lo * std::pow(hi / lo, static_cast<double>(norm));
    else
        return lo + (hi - lo) * static_cast<double>(norm);
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
        label.setColour(juce::Label::textColourId, CustomLookAndFeel::textSecondary);
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
    colourMapBox.addItem("Viridis",   6);
    colourMapBox.addItem("Plasma",    7);
    colourMapBox.addItem("Turbo",     8);
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

    // Row 2: Mode
    modeBox.addItem("Spectrogram", 1);
    modeBox.addItem("Nebula", 2);
    modeBox.setSelectedId(1);
    modeBox.onChange = [this]
    {
        nebulaMode = (modeBox.getSelectedId() == 2);
        processorRef.settings.nebulaMode = nebulaMode;
        processorRef.nebulaActive.store(nebulaMode, std::memory_order_relaxed);
        if (nebulaMode)
            nebulaAccum.assign(static_cast<size_t>(nebulaTexW) * nebulaTexH * 3, 0.0f);
        updateModeVisibility();
        repaint();
    };
    addAndMakeVisible(modeBox);
    setupLabel(modeLabel);

    // Bloom toggle + intensity
    bloomButton.setClickingTogglesState(true);
    bloomButton.onClick = [this]
    {
        bloomEnabled = bloomButton.getToggleState();
        processorRef.settings.bloomEnabled = bloomEnabled;
        repaint();
    };
    addAndMakeVisible(bloomButton);

    bloomIntensitySlider.setRange(0.0, 2.0, 0.05);
    bloomIntensitySlider.setValue(bloomIntensity, juce::dontSendNotification);
    bloomIntensitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bloomIntensitySlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    bloomIntensitySlider.onValueChange = [this]
    {
        bloomIntensity = static_cast<float>(bloomIntensitySlider.getValue());
        processorRef.settings.bloomIntensity = bloomIntensity;
        repaint();
    };
    addAndMakeVisible(bloomIntensitySlider);

    // Peak hold toggle + decay
    peakButton.setClickingTogglesState(true);
    peakButton.onClick = [this]
    {
        peakHoldEnabled = peakButton.getToggleState();
        processorRef.settings.peakHoldEnabled = peakHoldEnabled;
        if (!peakHoldEnabled) peakHoldData.clear();
        repaint();
    };
    addAndMakeVisible(peakButton);

    peakDecaySlider.setRange(5.0, 60.0, 1.0);
    peakDecaySlider.setValue(peakDecayRate, juce::dontSendNotification);
    peakDecaySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    peakDecaySlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    peakDecaySlider.onValueChange = [this]
    {
        peakDecayRate = static_cast<float>(peakDecaySlider.getValue());
        processorRef.settings.peakDecayRate = peakDecayRate;
    };
    addAndMakeVisible(peakDecaySlider);

    // RTA toggle
    rtaButton.setClickingTogglesState(true);
    rtaButton.onClick = [this]
    {
        rtaEnabled = rtaButton.getToggleState();
        processorRef.settings.rtaEnabled = rtaEnabled;
        repaint();
    };
    addAndMakeVisible(rtaButton);

    // Floor / Ceiling sliders
    dbFloorSlider.setRange(-120.0, -20.0, 1.0);
    dbFloorSlider.setValue(dbFloor, juce::dontSendNotification);
    dbFloorSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    dbFloorSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 18);
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
    dbCeilingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 18);
    dbCeilingSlider.onValueChange = [this]
    {
        dbCeiling = static_cast<float>(dbCeilingSlider.getValue());
        processorRef.settings.dbCeiling = dbCeiling;
        repaint();
    };
    addAndMakeVisible(dbCeilingSlider);
    setupLabel(dbCeilLabel);

    // Zoom sliders with log skew
    zoomMinSlider.setRange(20.0, 20000.0, 1.0);
    zoomMinSlider.setSkewFactorFromMidPoint(1000.0);
    zoomMinSlider.setValue(zoomMinFreq, juce::dontSendNotification);
    zoomMinSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    zoomMinSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    zoomMinSlider.onValueChange = [this]
    {
        zoomMinFreq = static_cast<float>(zoomMinSlider.getValue());
        if (zoomMinFreq >= zoomMaxFreq)
        {
            zoomMinFreq = zoomMaxFreq * 0.5f;
            zoomMinSlider.setValue(zoomMinFreq, juce::dontSendNotification);
        }
        processorRef.settings.zoomMinFreq = zoomMinFreq;
        repaint();
    };
    addAndMakeVisible(zoomMinSlider);
    setupLabel(zoomMinLabel);

    zoomMaxSlider.setRange(20.0, 20000.0, 1.0);
    zoomMaxSlider.setSkewFactorFromMidPoint(1000.0);
    zoomMaxSlider.setValue(zoomMaxFreq, juce::dontSendNotification);
    zoomMaxSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    zoomMaxSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    zoomMaxSlider.onValueChange = [this]
    {
        zoomMaxFreq = static_cast<float>(zoomMaxSlider.getValue());
        if (zoomMaxFreq <= zoomMinFreq)
        {
            zoomMaxFreq = zoomMinFreq * 2.0f;
            zoomMaxSlider.setValue(zoomMaxFreq, juce::dontSendNotification);
        }
        processorRef.settings.zoomMaxFreq = zoomMaxFreq;
        repaint();
    };
    addAndMakeVisible(zoomMaxSlider);
    setupLabel(zoomMaxLabel);
}

void SpectrogramEditor::updateModeVisibility()
{
    // In nebula mode, hide peak hold and RTA (they don't apply)
    peakButton.setVisible(!nebulaMode);
    peakDecaySlider.setVisible(!nebulaMode && peakHoldEnabled);
    rtaButton.setVisible(!nebulaMode);
    colourMapBox.setVisible(!nebulaMode);
    colourLabel.setVisible(!nebulaMode);
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
    textureDataBack.clear();
    textureDataFront.clear();
    textureWidth = 0;
    writePosition = 0;
    peakHoldData.clear();
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

// ── Nebula texture update ───────────────────────────────────────────────

void SpectrogramEditor::updateNebulaTexture()
{
    auto& stereoAnalyser = processorRef.getStereoAnalyser();
    const int numBins = stereoAnalyser.getNumBins();
    const double nyquist = stereoAnalyser.getSampleRate() / 2.0;

    if (numBins <= 0 || nyquist <= 0.0)
        return;

    // Decay existing accumulation
    const float decay = 0.96f; // per-frame decay
    for (auto& v : nebulaAccum)
        v *= decay;

    while (stereoAnalyser.pullNextFrame(stereoFrame))
    {
        const int frameBins = static_cast<int>(stereoFrame.magnitudeDb.size());
        for (int bin = 0; bin < frameBins; ++bin)
        {
            float db = stereoFrame.magnitudeDb[static_cast<size_t>(bin)];
            float pan = stereoFrame.pan[static_cast<size_t>(bin)];

            // Map dB to brightness
            float t = (db - dbFloor) / (dbCeiling - dbFloor);
            if (t <= 0.0f) continue;
            t = std::clamp(t, 0.0f, 1.0f);

            // Map frequency to Y position
            float freq = static_cast<float>(bin) / static_cast<float>(frameBins - 1) * static_cast<float>(nyquist);
            float yNorm = freqToNorm(freq);
            int yIdx = std::clamp(static_cast<int>(yNorm * (nebulaTexH - 1)), 0, nebulaTexH - 1);

            // Map pan (-1..+1) to X position (0..nebulaTexW-1)
            float xNorm = (pan + 1.0f) * 0.5f;
            int xIdx = std::clamp(static_cast<int>(xNorm * (nebulaTexW - 1)), 0, nebulaTexW - 1);

            // Gaussian-ish splat: center + neighbors
            float energy = t * t * 2.0f;

            // Frequency-based rainbow colour: low = red, mid = green, high = blue
            float hue = yNorm * 0.8f;
            float r, g, b;
            if (hue < 0.333f)
            {
                float s = hue / 0.333f;
                r = 1.0f - s; g = s; b = 0.0f;
            }
            else if (hue < 0.666f)
            {
                float s = (hue - 0.333f) / 0.333f;
                r = 0.0f; g = 1.0f - s; b = s;
            }
            else
            {
                float s = (hue - 0.666f) / 0.334f;
                r = s * 0.5f; g = 0.0f; b = 1.0f - s * 0.3f;
            }

            // Splat with small spread
            for (int dy = -1; dy <= 1; ++dy)
            {
                int yy = yIdx + dy;
                if (yy < 0 || yy >= nebulaTexH) continue;
                float yWeight = (dy == 0) ? 1.0f : 0.3f;

                for (int dx = -2; dx <= 2; ++dx)
                {
                    int xx = xIdx + dx;
                    if (xx < 0 || xx >= nebulaTexW) continue;
                    float xWeight = 1.0f / (1.0f + static_cast<float>(dx * dx));

                    float w = energy * xWeight * yWeight;
                    size_t idx = (static_cast<size_t>(yy) * nebulaTexW + static_cast<size_t>(xx)) * 3;
                    nebulaAccum[idx + 0] += r * w;
                    nebulaAccum[idx + 1] += g * w;
                    nebulaAccum[idx + 2] += b * w;
                }
            }
        }
    }

    // Clamp to prevent overflow
    for (auto& v : nebulaAccum)
        v = std::min(v, 1.5f);
}

// ── Timer / frame processing ────────────────────────────────────────────

void SpectrogramEditor::timerCallback()
{
    if (frozen)
        return;

    double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    double dt = now - lastTimerTime;
    lastTimerTime = now;

    if (nebulaMode)
    {
        updateNebulaTexture();
        glContext.triggerRepaint();
        repaint();
        return;
    }

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
        textureDataBack.assign(static_cast<size_t>(w) * static_cast<size_t>(numBins), -100.0f);
        textureDataFront.assign(static_cast<size_t>(w) * static_cast<size_t>(numBins), -100.0f);
        writePosition = 0;
    }

    if (frameBuffer.size() != static_cast<size_t>(numBins))
        frameBuffer.resize(static_cast<size_t>(numBins));

    bool gotNewData = false;

    while (analyser.pullNextFrame(frameBuffer.data(), numBins))
    {
        for (int bin = 0; bin < numBins; ++bin)
        {
            textureDataBack[static_cast<size_t>(bin) * static_cast<size_t>(textureWidth)
                        + static_cast<size_t>(writePosition)] = frameBuffer[static_cast<size_t>(bin)];
        }

        lastFrame.assign(frameBuffer.begin(), frameBuffer.end());
        writePosition = (writePosition + 1) % textureWidth;
        gotNewData = true;
    }

    // Update peak hold data
    if (peakHoldEnabled && !lastFrame.empty())
    {
        if (peakHoldData.size() != lastFrame.size())
            peakHoldData.assign(lastFrame.size(), -100.0f);

        float decayAmount = peakDecayRate * static_cast<float>(dt);
        for (size_t i = 0; i < peakHoldData.size(); ++i)
        {
            if (lastFrame[i] > peakHoldData[i])
                peakHoldData[i] = lastFrame[i];
            else
                peakHoldData[i] -= decayAmount;

            peakHoldData[i] = std::max(peakHoldData[i], -100.0f);
        }
    }

    if (gotNewData)
    {
        textureNeedsUpload.store(true, std::memory_order_release);
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

// ── Drawing helpers ─────────────────────────────────────────────────────

void SpectrogramEditor::drawMagnitudeCurve(juce::Graphics& g, juce::Rectangle<int> area,
                                           const std::vector<float>& data, juce::Colour colour,
                                           bool filled)
{
    if (data.empty()) return;

    const int numBins = static_cast<int>(data.size());
    const double nyquist = processorRef.getAnalyser().getSampleRate() / 2.0;
    const float areaW = static_cast<float>(area.getWidth());
    const float areaH = static_cast<float>(area.getHeight());

    juce::Path path;
    bool started = false;

    for (int y = 0; y < area.getHeight(); ++y)
    {
        float norm = 1.0f - static_cast<float>(y) / areaH;
        double freq = normToFreq(norm);
        float binFloat = static_cast<float>(freq / nyquist) * static_cast<float>(numBins - 1);
        int bin = std::clamp(static_cast<int>(binFloat), 0, numBins - 1);

        float db = data[static_cast<size_t>(bin)];
        float t = (db - dbFloor) / (dbCeiling - dbFloor);
        t = std::clamp(t, 0.0f, 1.0f);

        float xPos = static_cast<float>(area.getX()) + t * areaW;
        float yPos = static_cast<float>(area.getY() + y);

        if (!started)
        {
            path.startNewSubPath(xPos, yPos);
            started = true;
        }
        else
        {
            path.lineTo(xPos, yPos);
        }
    }

    if (filled)
    {
        juce::Path fillPath(path);
        fillPath.lineTo(static_cast<float>(area.getX()), static_cast<float>(area.getBottom()));
        fillPath.lineTo(static_cast<float>(area.getX()), static_cast<float>(area.getY()));
        fillPath.closeSubPath();

        g.setColour(colour.withAlpha(0.15f));
        g.fillPath(fillPath);
    }

    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(filled ? 1.5f : 2.0f));
}

void SpectrogramEditor::drawGridLines(juce::Graphics& g, juce::Rectangle<int> area)
{
    const double nyquist = processorRef.getAnalyser().getSampleRate() / 2.0;

    // Major frequency grid lines
    const double majorFreqs[] = { 100, 1000, 10000 };
    for (double freq : majorFreqs)
    {
        if (freq < zoomMinFreq || freq > zoomMaxFreq || freq >= nyquist) continue;
        float norm = freqToNorm(freq);
        int y = area.getBottom() - static_cast<int>(norm * area.getHeight());
        if (y < area.getY() + 2 || y > area.getBottom() - 2) continue;

        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawHorizontalLine(y, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
    }

    // Minor frequency grid lines
    const double minorFreqs[] = { 50, 200, 500, 2000, 5000, 20000 };
    for (double freq : minorFreqs)
    {
        if (freq < zoomMinFreq || freq > zoomMaxFreq || freq >= nyquist) continue;
        float norm = freqToNorm(freq);
        int y = area.getBottom() - static_cast<int>(norm * area.getHeight());
        if (y < area.getY() + 2 || y > area.getBottom() - 2) continue;

        g.setColour(juce::Colours::white.withAlpha(0.04f));
        g.drawHorizontalLine(y, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
    }
}

// ── Paint ───────────────────────────────────────────────────────────────

void SpectrogramEditor::paint(juce::Graphics& g)
{
    const auto spectArea = getSpectrogramArea();

    // Fill areas outside the spectrogram
    g.setColour(CustomLookAndFeel::bgDark);
    g.fillRect(getLocalBounds().removeFromTop(spectArea.getY()));
    g.fillRect(getLocalBounds().removeFromBottom(getHeight() - spectArea.getBottom()));
    g.fillRect(0, spectArea.getY(), spectArea.getX(), spectArea.getHeight());
    g.fillRect(spectArea.getRight(), spectArea.getY(),
               getWidth() - spectArea.getRight(), spectArea.getHeight());

    // Separators between control groups
    g.setColour(CustomLookAndFeel::separator);
    // Between row 1 and row 2
    int sepY = topMargin + controlBarHeight / 2;
    g.drawHorizontalLine(sepY, 4.0f, static_cast<float>(getWidth() - 4));

    // Border around spectrogram
    g.setColour(CustomLookAndFeel::border);
    g.drawRect(spectArea, 1);

    // Grid lines
    drawGridLines(g, spectArea);

    drawFrequencyAxis(g, spectArea);

    if (nebulaMode)
        drawNebulaAxis(g, spectArea);
    else
        drawTimeAxis(g, spectArea);

    drawDbScale(g, spectArea);

    // Draw RTA curve overlay
    if (rtaEnabled && !nebulaMode && !lastFrame.empty())
        drawMagnitudeCurve(g, spectArea, lastFrame, juce::Colour(0x8800d4ff), true);

    // Draw peak hold overlay
    if (peakHoldEnabled && !nebulaMode && !peakHoldData.empty())
        drawMagnitudeCurve(g, spectArea, peakHoldData, juce::Colour(0xccffdd44), false);

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
        if (freq < zoomMinFreq || freq > zoomMaxFreq) continue;

        const float norm = freqToNorm(freq);
        const int y = area.getBottom() - static_cast<int>(norm * area.getHeight());

        if (y < area.getY() + 8 || y > area.getBottom() - 4) continue;

        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawHorizontalLine(y, static_cast<float>(area.getX()),
                             static_cast<float>(area.getRight()));

        g.setColour(CustomLookAndFeel::textSecondary);
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

        g.setColour(CustomLookAndFeel::textSecondary);
        juce::String label = (t == 0.0) ? "now" : ("-" + juce::String(t, 1) + "s");
        g.drawText(label, x - 25, labelY, 50, 16, juce::Justification::centred);
    }
}

void SpectrogramEditor::drawNebulaAxis(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setFont(juce::FontOptions(11.0f));
    g.setColour(CustomLookAndFeel::textSecondary);

    const int labelY = area.getBottom() + 3;
    const int cx = area.getCentreX();

    g.drawText("L", area.getX(), labelY, 30, 16, juce::Justification::centred);
    g.drawText("C", cx - 15, labelY, 30, 16, juce::Justification::centred);
    g.drawText("R", area.getRight() - 30, labelY, 30, 16, juce::Justification::centred);

    // Draw centre line
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawVerticalLine(cx, static_cast<float>(area.getY()), static_cast<float>(area.getBottom()));
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

    g.setColour(juce::Colours::white.withAlpha(0.3f));
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

    textureDataBack.clear();
    textureDataFront.clear();
    textureWidth = 0;
    writePosition = 0;
    peakHoldData.clear();

    auto area = getLocalBounds();

    // Two-row control bar
    const int rowH = controlBarHeight / 2;

    // ─── Row 1: Analysis + Display ───
    auto row1 = area.removeFromTop(rowH).reduced(4, 2);

    const int labelW = 42;
    const int gap = 4;
    const int buttonW = 52;
    const int sliderW = 70;

    auto placeCombo = [&](juce::Label& label, juce::ComboBox& box, int w, juce::Rectangle<int>& row)
    {
        label.setBounds(row.removeFromLeft(labelW));
        box.setBounds(row.removeFromLeft(w));
        row.removeFromLeft(gap);
    };

    placeCombo(fftSizeLabel, fftSizeBox, 60, row1);
    placeCombo(overlapLabel, overlapBox, 50, row1);
    placeCombo(windowLabel, windowBox, 105, row1);
    placeCombo(colourLabel, colourMapBox, 80, row1);

    scaleButton.setBounds(row1.removeFromLeft(buttonW));
    row1.removeFromLeft(gap);
    freezeButton.setBounds(row1.removeFromLeft(buttonW));
    row1.removeFromLeft(gap);

    dbFloorLabel.setBounds(row1.removeFromLeft(32));
    dbFloorSlider.setBounds(row1.removeFromLeft(sliderW));
    row1.removeFromLeft(gap);
    dbCeilLabel.setBounds(row1.removeFromLeft(26));
    dbCeilingSlider.setBounds(row1.removeFromLeft(sliderW));

    // ─── Row 2: Mode + Effects + Range ───
    auto row2 = area.removeFromTop(rowH).reduced(4, 2);

    modeLabel.setBounds(row2.removeFromLeft(34));
    modeBox.setBounds(row2.removeFromLeft(90));
    row2.removeFromLeft(gap + 4);

    bloomButton.setBounds(row2.removeFromLeft(buttonW));
    row2.removeFromLeft(2);
    bloomIntensitySlider.setBounds(row2.removeFromLeft(50));
    row2.removeFromLeft(gap);

    peakButton.setBounds(row2.removeFromLeft(buttonW));
    row2.removeFromLeft(2);
    peakDecaySlider.setBounds(row2.removeFromLeft(50));
    row2.removeFromLeft(gap);

    rtaButton.setBounds(row2.removeFromLeft(buttonW));
    row2.removeFromLeft(gap + 4);

    zoomMinLabel.setBounds(row2.removeFromLeft(18));
    zoomMinSlider.setBounds(row2.removeFromLeft(60));
    row2.removeFromLeft(gap);
    zoomMaxLabel.setBounds(row2.removeFromLeft(18));
    zoomMaxSlider.setBounds(row2.removeFromLeft(60));
}
