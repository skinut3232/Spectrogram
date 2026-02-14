#include "SpectralAnalyser.h"
#include <cmath>
#include <algorithm>

SpectralAnalyser::SpectralAnalyser()
{
    frameBuffer.resize(maxFrames);
}

void SpectralAnalyser::prepare(double sampleRate, FFTOrder order)
{
    currentSampleRate = sampleRate;
    fftOrder = static_cast<int>(order);
    fftSize = 1 << fftOrder;
    hopSize = static_cast<int>(fftSize * (1.0f - overlapFraction));

    fft = std::make_unique<juce::dsp::FFT>(fftOrder);

    windowBuffer.resize(static_cast<size_t>(fftSize));
    buildWindow();

    inputBuffer.resize(static_cast<size_t>(fftSize), 0.0f);
    inputWritePos = 0;
    samplesUntilNextFrame = 0;

    fftWorkBuffer.resize(static_cast<size_t>(fftSize) * 2, 0.0f);

    const int numBins = getNumBins();
    for (auto& frame : frameBuffer)
        frame.resize(static_cast<size_t>(numBins), -100.0f);

    frameWritePos.store(0, std::memory_order_relaxed);
    frameReadPos.store(0, std::memory_order_relaxed);
}

void SpectralAnalyser::setWindowType(WindowType type)
{
    windowType = type;
    buildWindow();
}

void SpectralAnalyser::setOverlap(float fraction)
{
    overlapFraction = juce::jlimit(0.0f, 0.875f, fraction);
    hopSize = static_cast<int>(fftSize * (1.0f - overlapFraction));
    if (hopSize < 1) hopSize = 1;
}

void SpectralAnalyser::buildWindow()
{
    const auto N = static_cast<size_t>(fftSize);
    windowBuffer.resize(N);

    switch (windowType)
    {
        case WindowType::hann:
        {
            for (size_t i = 0; i < N; ++i)
            {
                double x = static_cast<double>(i) / static_cast<double>(N - 1);
                windowBuffer[i] = static_cast<float>(0.5 * (1.0 - std::cos(2.0 * juce::MathConstants<double>::pi * x)));
            }
            break;
        }
        case WindowType::blackmanHarris:
        {
            constexpr double a0 = 0.35875, a1 = 0.48829, a2 = 0.14128, a3 = 0.01168;
            for (size_t i = 0; i < N; ++i)
            {
                double x = static_cast<double>(i) / static_cast<double>(N - 1);
                double w = a0
                         - a1 * std::cos(2.0 * juce::MathConstants<double>::pi * x)
                         + a2 * std::cos(4.0 * juce::MathConstants<double>::pi * x)
                         - a3 * std::cos(6.0 * juce::MathConstants<double>::pi * x);
                windowBuffer[i] = static_cast<float>(w);
            }
            break;
        }
    }
}

void SpectralAnalyser::pushSamples(const float* data, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        inputBuffer[static_cast<size_t>(inputWritePos)] = data[i];
        ++inputWritePos;

        if (inputWritePos >= fftSize)
        {
            processNextFFTFrame();

            // Shift by hopSize: move the tail of the buffer to the front
            const int remaining = fftSize - hopSize;
            std::memmove(inputBuffer.data(),
                         inputBuffer.data() + hopSize,
                         sizeof(float) * static_cast<size_t>(remaining));
            inputWritePos = remaining;
        }
    }
}

void SpectralAnalyser::processNextFFTFrame()
{
    const auto N = static_cast<size_t>(fftSize);

    // Copy input and apply window into work buffer
    for (size_t i = 0; i < N; ++i)
        fftWorkBuffer[i] = inputBuffer[i] * windowBuffer[i];

    // Zero the imaginary part
    std::memset(fftWorkBuffer.data() + N, 0, sizeof(float) * N);

    // In-place FFT: input is fftSize reals, output is interleaved complex
    fft->performRealOnlyForwardTransform(fftWorkBuffer.data(), true);

    // Convert to magnitude dB
    const int numBins = getNumBins();
    const int writeIdx = frameWritePos.load(std::memory_order_relaxed);
    auto& destFrame = frameBuffer[static_cast<size_t>(writeIdx)];

    for (int bin = 0; bin < numBins; ++bin)
    {
        float real = fftWorkBuffer[static_cast<size_t>(bin * 2)];
        float imag = fftWorkBuffer[static_cast<size_t>(bin * 2 + 1)];
        float magnitude = std::sqrt(real * real + imag * imag);

        // Normalise by FFT size
        magnitude /= static_cast<float>(fftSize);

        // Convert to dB, clamped to a floor of -100 dB
        float db = (magnitude > 0.0f)
                       ? 20.0f * std::log10(magnitude)
                       : -100.0f;
        destFrame[static_cast<size_t>(bin)] = std::max(db, -100.0f);
    }

    frameWritePos.store((writeIdx + 1) % maxFrames, std::memory_order_release);
}

bool SpectralAnalyser::pullNextFrame(float* destMagnitudesDb, int numBins)
{
    int w = frameWritePos.load(std::memory_order_acquire);
    int r = frameReadPos.load(std::memory_order_relaxed);

    if (r == w)
        return false;

    const auto& srcFrame = frameBuffer[static_cast<size_t>(r)];
    const int toCopy = std::min(numBins, static_cast<int>(srcFrame.size()));
    std::memcpy(destMagnitudesDb, srcFrame.data(), sizeof(float) * static_cast<size_t>(toCopy));

    frameReadPos.store((r + 1) % maxFrames, std::memory_order_release);
    return true;
}
