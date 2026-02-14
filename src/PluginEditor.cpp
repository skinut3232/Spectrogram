#include "PluginEditor.h"
#include <cmath>

SpectrogramEditor::SpectrogramEditor(SpectrogramProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(800, 400);
    setResizable(true, true);
    setResizeLimits(400, 250, 1920, 1080);

    startTimerHz(60);
}

SpectrogramEditor::~SpectrogramEditor()
{
    stopTimer();
}

void SpectrogramEditor::timerCallback()
{
    auto& analyser = processorRef.getAnalyser();
    const int numBins = analyser.getNumBins();

    if (frameBuffer.size() != static_cast<size_t>(numBins))
        frameBuffer.resize(static_cast<size_t>(numBins));

    bool needsRepaint = false;

    while (analyser.pullNextFrame(frameBuffer.data(), numBins))
    {
        renderColumnToImage(frameBuffer.data(), numBins);
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

    // Draw one vertical column at writePosition
    for (int y = 0; y < imgH; ++y)
    {
        // Map pixel row to frequency bin (linear, bottom = low freq, top = high freq)
        const float binFloat = static_cast<float>(imgH - 1 - y) / static_cast<float>(imgH - 1)
                               * static_cast<float>(numBins - 1);
        const int bin = std::clamp(static_cast<int>(binFloat), 0, numBins - 1);

        const float db = magnitudesDb[bin];
        const auto colour = ColourMap::fromDb(db, dbFloor, dbCeiling);

        spectrogramImage.setPixelAt(writePosition, y, colour);
    }

    writePosition = (writePosition + 1) % imgW;
}

void SpectrogramEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    const auto bounds = getLocalBounds();
    const auto spectArea = bounds.reduced(0).withTrimmedLeft(leftMargin)
                                                .withTrimmedBottom(bottomMargin)
                                                .withTrimmedRight(rightMargin)
                                                .withTrimmedTop(topMargin);

    drawSpectrogram(g, spectArea);
    drawFrequencyAxis(g, spectArea);
    drawTimeAxis(g, spectArea);
    drawDbScale(g, spectArea);
}

void SpectrogramEditor::drawSpectrogram(juce::Graphics& g, juce::Rectangle<int> area)
{
    const int w = area.getWidth();
    const int h = area.getHeight();

    if (w <= 0 || h <= 0)
        return;

    // Resize image if needed
    if (spectrogramImage.getWidth() != w || spectrogramImage.getHeight() != h)
    {
        spectrogramImage = juce::Image(juce::Image::ARGB, w, h, true);
        writePosition = 0;
    }

    // Draw the image with the scroll seam handled:
    // Left part: from writePosition to end of image (older data)
    // Right part: from 0 to writePosition (newer data)
    const int rightWidth = writePosition;
    const int leftWidth = w - writePosition;

    if (leftWidth > 0)
    {
        g.drawImage(spectrogramImage,
                    area.getX(), area.getY(), leftWidth, h,
                    writePosition, 0, leftWidth, h);
    }

    if (rightWidth > 0)
    {
        g.drawImage(spectrogramImage,
                    area.getX() + leftWidth, area.getY(), rightWidth, h,
                    0, 0, rightWidth, h);
    }

    // Draw a thin border
    g.setColour(juce::Colours::grey);
    g.drawRect(area, 1);
}

void SpectrogramEditor::drawFrequencyAxis(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto& analyser = processorRef.getAnalyser();
    const double sampleRate = analyser.getSampleRate();
    const double nyquist = sampleRate / 2.0;

    g.setColour(juce::Colours::lightgrey);
    g.setFont(11.0f);

    // Frequency labels at nice intervals
    const double freqStops[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };

    for (double freq : freqStops)
    {
        if (freq >= nyquist)
            break;

        // Linear mapping: freq / nyquist → normalised position (0 = bottom, 1 = top)
        const float norm = static_cast<float>(freq / nyquist);
        const int y = area.getBottom() - static_cast<int>(norm * area.getHeight());

        if (y < area.getY() || y > area.getBottom())
            continue;

        // Tick mark
        g.setColour(juce::Colours::grey.withAlpha(0.4f));
        g.drawHorizontalLine(y, static_cast<float>(area.getX()),
                             static_cast<float>(area.getX() + 4));

        // Label
        g.setColour(juce::Colours::lightgrey);
        juce::String label;
        if (freq >= 1000.0)
            label = juce::String(freq / 1000.0, (freq >= 10000.0) ? 0 : 1) + " kHz";
        else
            label = juce::String(static_cast<int>(freq)) + " Hz";

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

    // Time per column (with 50% overlap default: hop = fftSize/2)
    const double hopSamples = fftSize * 0.5;
    const double secondsPerColumn = hopSamples / sampleRate;
    const double totalSeconds = area.getWidth() * secondsPerColumn;

    g.setColour(juce::Colours::lightgrey);
    g.setFont(11.0f);

    // Choose a nice tick interval
    double tickInterval = 0.5;
    if (totalSeconds > 20.0) tickInterval = 5.0;
    else if (totalSeconds > 10.0) tickInterval = 2.0;
    else if (totalSeconds > 5.0) tickInterval = 1.0;

    // Draw from right (now) to left (past)
    const int labelY = area.getBottom() + 3;

    for (double t = 0.0; t < totalSeconds; t += tickInterval)
    {
        const int x = area.getRight() - static_cast<int>(t / secondsPerColumn);
        if (x < area.getX())
            break;

        g.setColour(juce::Colours::grey.withAlpha(0.4f));
        g.drawVerticalLine(x, static_cast<float>(area.getBottom()),
                           static_cast<float>(area.getBottom() + 4));

        g.setColour(juce::Colours::lightgrey);
        juce::String label;
        if (t == 0.0)
            label = "now";
        else
            label = "-" + juce::String(t, 1) + "s";

        g.drawText(label, x - 25, labelY, 50, 16, juce::Justification::centred);
    }
}

void SpectrogramEditor::drawDbScale(juce::Graphics& g, juce::Rectangle<int> area)
{
    // Small colour bar on the right side showing the dB range
    const int barWidth = 8;
    const int barX = area.getRight() + 2;
    const int barH = area.getHeight();
    const int barY = area.getY();

    if (barX + barWidth > getWidth())
        return;

    for (int y = 0; y < barH; ++y)
    {
        float t = 1.0f - static_cast<float>(y) / static_cast<float>(barH - 1);
        g.setColour(ColourMap::heatMap(t));
        g.fillRect(barX, barY + y, barWidth, 1);
    }
}

void SpectrogramEditor::resized()
{
    // Image will be recreated on next paint if size changed
}
