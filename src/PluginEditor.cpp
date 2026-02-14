#include "PluginEditor.h"
#include <cmath>

static constexpr double minLogFreq = 20.0;

SpectrogramEditor::SpectrogramEditor(SpectrogramProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(900, 480);
    setResizable(true, true);
    setResizeLimits(600, 350, 1920, 1080);

    buildControls();
    startTimerHz(60);
}

SpectrogramEditor::~SpectrogramEditor()
{
    stopTimer();
}

// ── Frequency mapping (linear or logarithmic) ──────────────────────────

float SpectrogramEditor::freqToNorm(double freq) const
{
    const double nyquist = processorRef.getAnalyser().getSampleRate() / 2.0;

    if (!logScale)
        return static_cast<float>(freq / nyquist);

    // Log scale: map [minLogFreq .. nyquist] → [0 .. 1]
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

    // FFT Size
    fftSizeBox.addItem("1024",  1);
    fftSizeBox.addItem("2048",  2);
    fftSizeBox.addItem("4096",  3);
    fftSizeBox.addItem("8192",  4);
    fftSizeBox.setSelectedId(3);
    fftSizeBox.onChange = [this] { onFFTSizeChanged(); };
    addAndMakeVisible(fftSizeBox);
    setupLabel(fftSizeLabel);

    // Overlap
    overlapBox.addItem("50%", 1);
    overlapBox.addItem("75%", 2);
    overlapBox.setSelectedId(1);
    overlapBox.onChange = [this] { onOverlapChanged(); };
    addAndMakeVisible(overlapBox);
    setupLabel(overlapLabel);

    // Window
    windowBox.addItem("Hann",           1);
    windowBox.addItem("Blackman-Harris", 2);
    windowBox.setSelectedId(1);
    windowBox.onChange = [this] { onWindowChanged(); };
    addAndMakeVisible(windowBox);
    setupLabel(windowLabel);

    // Colour map
    colourMapBox.addItem("Heat",      1);
    colourMapBox.addItem("Magma",     2);
    colourMapBox.addItem("Inferno",   3);
    colourMapBox.addItem("Grayscale", 4);
    colourMapBox.addItem("Rainbow",   5);
    colourMapBox.setSelectedId(1);
    colourMapBox.onChange = [this]
    {
        const int id = colourMapBox.getSelectedId();
        colourMapType = static_cast<ColourMap::Type>(id - 1);
        spectrogramImage = {};
        writePosition = 0;
        repaint();
    };
    addAndMakeVisible(colourMapBox);
    setupLabel(colourLabel);

    // Scale toggle
    scaleButton.setClickingTogglesState(true);
    scaleButton.setToggleState(logScale, juce::dontSendNotification);
    scaleButton.onClick = [this]
    {
        logScale = scaleButton.getToggleState();
        scaleButton.setButtonText(logScale ? "Log" : "Linear");
        spectrogramImage = {};
        writePosition = 0;
        repaint();
    };
    addAndMakeVisible(scaleButton);

    // Freeze toggle
    freezeButton.setClickingTogglesState(true);
    freezeButton.onClick = [this]
    {
        frozen = freezeButton.getToggleState();
        freezeButton.setButtonText(frozen ? "Resume" : "Freeze");
    };
    addAndMakeVisible(freezeButton);

    // dB floor slider
    dbFloorSlider.setRange(-120.0, -20.0, 1.0);
    dbFloorSlider.setValue(dbFloor, juce::dontSendNotification);
    dbFloorSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    dbFloorSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
    dbFloorSlider.onValueChange = [this]
    {
        dbFloor = static_cast<float>(dbFloorSlider.getValue());
        spectrogramImage = {};
        writePosition = 0;
    };
    addAndMakeVisible(dbFloorSlider);
    setupLabel(dbFloorLabel);

    // dB ceiling slider
    dbCeilingSlider.setRange(-30.0, 10.0, 1.0);
    dbCeilingSlider.setValue(dbCeiling, juce::dontSendNotification);
    dbCeilingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    dbCeilingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
    dbCeilingSlider.onValueChange = [this]
    {
        dbCeiling = static_cast<float>(dbCeilingSlider.getValue());
        spectrogramImage = {};
        writePosition = 0;
    };
    addAndMakeVisible(dbCeilingSlider);
    setupLabel(dbCeilLabel);
}

void SpectrogramEditor::onFFTSizeChanged()
{
    auto& analyser = processorRef.getAnalyser();
    const int id = fftSizeBox.getSelectedId();
    SpectralAnalyser::FFTOrder order;
    switch (id)
    {
        case 1:  order = SpectralAnalyser::FFTOrder::order1024; break;
        case 2:  order = SpectralAnalyser::FFTOrder::order2048; break;
        case 4:  order = SpectralAnalyser::FFTOrder::order8192; break;
        default: order = SpectralAnalyser::FFTOrder::order4096; break;
    }
    analyser.prepare(analyser.getSampleRate(), order);
    spectrogramImage = {};
    writePosition = 0;
}

void SpectrogramEditor::onOverlapChanged()
{
    auto& analyser = processorRef.getAnalyser();
    analyser.setOverlap(overlapBox.getSelectedId() == 2 ? 0.75f : 0.5f);
}

void SpectrogramEditor::onWindowChanged()
{
    auto& analyser = processorRef.getAnalyser();
    analyser.setWindowType(windowBox.getSelectedId() == 2
                               ? SpectralAnalyser::WindowType::blackmanHarris
                               : SpectralAnalyser::WindowType::hann);
}

// ── Timer / frame processing ────────────────────────────────────────────

void SpectrogramEditor::timerCallback()
{
    if (frozen)
        return;

    auto& analyser = processorRef.getAnalyser();
    const int numBins = analyser.getNumBins();

    if (frameBuffer.size() != static_cast<size_t>(numBins))
        frameBuffer.resize(static_cast<size_t>(numBins));

    bool needsRepaint = false;

    while (analyser.pullNextFrame(frameBuffer.data(), numBins))
    {
        renderColumnToImage(frameBuffer.data(), numBins);
        lastFrame.assign(frameBuffer.begin(), frameBuffer.end());
        needsRepaint = true;
    }

    if (needsRepaint)
        repaint();
}

void SpectrogramEditor::renderColumnToImage(const float* magnitudesDb, int numBins)
{
    const int imgW = spectrogramImage.getWidth();
    const int imgH = spectrogramImage.getHeight();

    if (imgW <= 0 || imgH <= 0)
        return;

    const double nyquist = processorRef.getAnalyser().getSampleRate() / 2.0;

    for (int y = 0; y < imgH; ++y)
    {
        // normalised position: 0 = bottom (low freq), 1 = top (high freq)
        const float norm = static_cast<float>(imgH - 1 - y) / static_cast<float>(imgH - 1);

        // Map normalised position to frequency, then to bin index
        const double freq = normToFreq(norm);
        const float binFloat = static_cast<float>(freq / nyquist) * static_cast<float>(numBins - 1);
        const int bin = std::clamp(static_cast<int>(binFloat), 0, numBins - 1);

        const float db = magnitudesDb[bin];
        const auto colour = ColourMap::fromDb(colourMapType, db, dbFloor, dbCeiling);

        spectrogramImage.setPixelAt(writePosition, y, colour);
    }

    writePosition = (writePosition + 1) % imgW;
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

// ── Paint ───────────────────────────────────────────────────────────────

void SpectrogramEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    const auto bounds = getLocalBounds();
    const auto spectArea = bounds.withTrimmedLeft(leftMargin)
                                  .withTrimmedBottom(bottomMargin)
                                  .withTrimmedRight(rightMargin)
                                  .withTrimmedTop(topMargin + controlBarHeight);

    drawSpectrogram(g, spectArea);
    drawFrequencyAxis(g, spectArea);
    drawTimeAxis(g, spectArea);
    drawDbScale(g, spectArea);

    if (mouseInside)
        drawHoverInfo(g, spectArea);
}

void SpectrogramEditor::drawSpectrogram(juce::Graphics& g, juce::Rectangle<int> area)
{
    const int w = area.getWidth();
    const int h = area.getHeight();

    if (w <= 0 || h <= 0)
        return;

    if (spectrogramImage.getWidth() != w || spectrogramImage.getHeight() != h)
    {
        spectrogramImage = juce::Image(juce::Image::ARGB, w, h, true);
        writePosition = 0;
    }

    const int rightWidth = writePosition;
    const int leftWidth = w - writePosition;

    if (leftWidth > 0)
        g.drawImage(spectrogramImage,
                    area.getX(), area.getY(), leftWidth, h,
                    writePosition, 0, leftWidth, h);

    if (rightWidth > 0)
        g.drawImage(spectrogramImage,
                    area.getX() + leftWidth, area.getY(), rightWidth, h,
                    0, 0, rightWidth, h);

    g.setColour(juce::Colours::grey);
    g.drawRect(area, 1);
}

void SpectrogramEditor::drawFrequencyAxis(juce::Graphics& g, juce::Rectangle<int> area)
{
    const double nyquist = processorRef.getAnalyser().getSampleRate() / 2.0;

    g.setFont(11.0f);

    const double freqStops[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };

    for (double freq : freqStops)
    {
        if (freq >= nyquist)
            break;

        const float norm = freqToNorm(freq);
        const int y = area.getBottom() - static_cast<int>(norm * area.getHeight());

        if (y < area.getY() + 8 || y > area.getBottom() - 4)
            continue;

        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawHorizontalLine(y, static_cast<float>(area.getX()),
                             static_cast<float>(area.getRight()));

        g.setColour(juce::Colours::lightgrey);
        juce::String label;
        if (freq >= 1000.0)
            label = juce::String(freq / 1000.0, (freq >= 10000.0) ? 0 : 1) + "k";
        else
            label = juce::String(static_cast<int>(freq));

        g.drawText(label,
                   area.getX() - leftMargin, y - 7,
                   leftMargin - 6, 14,
                   juce::Justification::centredRight);
    }
}

void SpectrogramEditor::drawTimeAxis(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto& analyser = processorRef.getAnalyser();
    const double sampleRate = analyser.getSampleRate();
    const int fftSize = analyser.getFFTSize();

    if (sampleRate <= 0.0 || fftSize <= 0)
        return;

    const double hopSamples = fftSize * 0.5;
    const double secondsPerColumn = hopSamples / sampleRate;
    const double totalSeconds = area.getWidth() * secondsPerColumn;

    g.setFont(11.0f);

    double tickInterval = 0.5;
    if (totalSeconds > 20.0) tickInterval = 5.0;
    else if (totalSeconds > 10.0) tickInterval = 2.0;
    else if (totalSeconds > 5.0) tickInterval = 1.0;

    const int labelY = area.getBottom() + 3;

    for (double t = 0.0; t < totalSeconds; t += tickInterval)
    {
        const int x = area.getRight() - static_cast<int>(t / secondsPerColumn);
        if (x < area.getX())
            break;

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

    if (barX + barWidth > getWidth())
        return;

    for (int y = 0; y < barH; ++y)
    {
        float t = 1.0f - static_cast<float>(y) / static_cast<float>(barH - 1);
        g.setColour(ColourMap::map(colourMapType, t));
        g.fillRect(barX, barY + y, barWidth, 1);
    }
}

void SpectrogramEditor::drawHoverInfo(juce::Graphics& g, juce::Rectangle<int> area)
{
    if (!area.contains(mousePos) || lastFrame.empty())
        return;

    const double nyquist = processorRef.getAnalyser().getSampleRate() / 2.0;
    const int numBins = static_cast<int>(lastFrame.size());

    // Map mouse Y to frequency
    const float norm = static_cast<float>(area.getBottom() - mousePos.y)
                     / static_cast<float>(area.getHeight());
    const double freq = normToFreq(std::clamp(norm, 0.0f, 1.0f));

    // Look up dB at that frequency bin
    const float binFloat = static_cast<float>(freq / nyquist) * static_cast<float>(numBins - 1);
    const int bin = std::clamp(static_cast<int>(binFloat), 0, numBins - 1);
    const float db = lastFrame[static_cast<size_t>(bin)];

    // Format label
    juce::String freqStr = (freq >= 1000.0)
        ? juce::String(freq / 1000.0, 2) + " kHz"
        : juce::String(static_cast<int>(freq)) + " Hz";
    juce::String dbStr = juce::String(db, 1) + " dB";
    juce::String text = freqStr + "  |  " + dbStr;

    // Draw crosshairs
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.drawHorizontalLine(mousePos.y, static_cast<float>(area.getX()),
                         static_cast<float>(area.getRight()));
    g.drawVerticalLine(mousePos.x, static_cast<float>(area.getY()),
                       static_cast<float>(area.getBottom()));

    // Draw info box
    const int boxW = 160;
    const int boxH = 20;
    int boxX = mousePos.x + 12;
    int boxY = mousePos.y - boxH - 4;

    // Keep on screen
    if (boxX + boxW > area.getRight()) boxX = mousePos.x - boxW - 12;
    if (boxY < area.getY()) boxY = mousePos.y + 8;

    g.setColour(juce::Colour(0xdd000000));
    g.fillRoundedRectangle(static_cast<float>(boxX), static_cast<float>(boxY),
                           static_cast<float>(boxW), static_cast<float>(boxH), 4.0f);

    g.setColour(juce::Colours::white);
    g.setFont(12.0f);
    g.drawText(text, boxX, boxY, boxW, boxH, juce::Justification::centred);
}

// ── Layout ──────────────────────────────────────────────────────────────

void SpectrogramEditor::resized()
{
    spectrogramImage = {};
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
