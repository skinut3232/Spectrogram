#include "StereoSpectralAnalyser.h"
#include <cmath>
#include <algorithm>

StereoSpectralAnalyser::StereoSpectralAnalyser()
{
    frameBuffer.resize(maxFrames);
}

void StereoSpectralAnalyser::prepare(double sampleRate, FFTOrder order)
{
    currentSampleRate = sampleRate;
    fftOrder = static_cast<int>(order);
    fftSize = 1 << fftOrder;
    hopSize = static_cast<int>(fftSize * (1.0f - overlapFrac));

    fft = std::make_unique<juce::dsp::FFT>(fftOrder);

    windowBuffer.resize(static_cast<size_t>(fftSize));
    buildWindow();

    inputBufferL.resize(static_cast<size_t>(fftSize), 0.0f);
    inputBufferR.resize(static_cast<size_t>(fftSize), 0.0f);
    inputWritePos = 0;
    samplesUntilNextFrame = 0;

    fftWorkL.resize(static_cast<size_t>(fftSize) * 2, 0.0f);
    fftWorkR.resize(static_cast<size_t>(fftSize) * 2, 0.0f);

    const int numBins = getNumBins();
    for (auto& frame : frameBuffer)
    {
        frame.magnitudeDb.resize(static_cast<size_t>(numBins), -100.0f);
        frame.pan.resize(static_cast<size_t>(numBins), 0.0f);
    }

    frameWritePos.store(0, std::memory_order_relaxed);
    frameReadPos.store(0, std::memory_order_relaxed);
}

void StereoSpectralAnalyser::setWindowType(WindowType type)
{
    windowType = type;
    buildWindow();
}

void StereoSpectralAnalyser::setOverlap(float fraction)
{
    overlapFrac = juce::jlimit(0.0f, 0.875f, fraction);
    hopSize = static_cast<int>(fftSize * (1.0f - overlapFrac));
    if (hopSize < 1) hopSize = 1;
}

void StereoSpectralAnalyser::buildWindow()
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

void StereoSpectralAnalyser::pushSamples(const float* leftData, const float* rightData, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        inputBufferL[static_cast<size_t>(inputWritePos)] = leftData[i];
        inputBufferR[static_cast<size_t>(inputWritePos)] = rightData[i];
        ++inputWritePos;

        if (inputWritePos >= fftSize)
        {
            processNextFFTFrame();

            const int remaining = fftSize - hopSize;
            std::memmove(inputBufferL.data(),
                         inputBufferL.data() + hopSize,
                         sizeof(float) * static_cast<size_t>(remaining));
            std::memmove(inputBufferR.data(),
                         inputBufferR.data() + hopSize,
                         sizeof(float) * static_cast<size_t>(remaining));
            inputWritePos = remaining;
        }
    }
}

void StereoSpectralAnalyser::processNextFFTFrame()
{
    const auto N = static_cast<size_t>(fftSize);

    // Window both channels
    for (size_t i = 0; i < N; ++i)
    {
        fftWorkL[i] = inputBufferL[i] * windowBuffer[i];
        fftWorkR[i] = inputBufferR[i] * windowBuffer[i];
    }
    std::memset(fftWorkL.data() + N, 0, sizeof(float) * N);
    std::memset(fftWorkR.data() + N, 0, sizeof(float) * N);

    fft->performRealOnlyForwardTransform(fftWorkL.data(), true);
    fft->performRealOnlyForwardTransform(fftWorkR.data(), true);

    const int numBins = getNumBins();
    const int writeIdx = frameWritePos.load(std::memory_order_relaxed);
    auto& dest = frameBuffer[static_cast<size_t>(writeIdx)];

    for (int bin = 0; bin < numBins; ++bin)
    {
        float realL = fftWorkL[static_cast<size_t>(bin * 2)];
        float imagL = fftWorkL[static_cast<size_t>(bin * 2 + 1)];
        float magL = std::sqrt(realL * realL + imagL * imagL) / static_cast<float>(fftSize);

        float realR = fftWorkR[static_cast<size_t>(bin * 2)];
        float imagR = fftWorkR[static_cast<size_t>(bin * 2 + 1)];
        float magR = std::sqrt(realR * realR + imagR * imagR) / static_cast<float>(fftSize);

        float totalMag = magL + magR;

        // Combined magnitude (average of L+R)
        float combinedMag = totalMag * 0.5f;
        float db = (combinedMag > 0.0f) ? 20.0f * std::log10(combinedMag) : -100.0f;
        dest.magnitudeDb[static_cast<size_t>(bin)] = std::max(db, -100.0f);

        // Pan: -1 = full L, 0 = centre, +1 = full R
        if (totalMag > 1e-10f)
            dest.pan[static_cast<size_t>(bin)] = (magR - magL) / totalMag;
        else
            dest.pan[static_cast<size_t>(bin)] = 0.0f;
    }

    frameWritePos.store((writeIdx + 1) % maxFrames, std::memory_order_release);
}

bool StereoSpectralAnalyser::pullNextFrame(StereoFrame& dest)
{
    int w = frameWritePos.load(std::memory_order_acquire);
    int r = frameReadPos.load(std::memory_order_relaxed);

    if (r == w)
        return false;

    const auto& src = frameBuffer[static_cast<size_t>(r)];
    dest.magnitudeDb = src.magnitudeDb;
    dest.pan = src.pan;

    frameReadPos.store((r + 1) % maxFrames, std::memory_order_release);
    return true;
}
