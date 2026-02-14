#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <atomic>

class SpectralAnalyser
{
public:
    enum class WindowType { hann, blackmanHarris };
    enum class FFTOrder { order1024 = 10, order2048 = 11, order4096 = 12, order8192 = 13 };

    SpectralAnalyser();

    void prepare(double sampleRate, FFTOrder order);

    void setWindowType(WindowType type);
    void setOverlap(float overlapFraction);

    void pushSamples(const float* data, int numSamples);

    bool pullNextFrame(float* destMagnitudesDb, int numBins);

    int getFFTSize() const noexcept { return fftSize; }
    int getNumBins() const noexcept { return fftSize / 2 + 1; }
    double getSampleRate() const noexcept { return currentSampleRate; }

    int getNumFramesAvailable() const noexcept
    {
        int w = frameWritePos.load(std::memory_order_acquire);
        int r = frameReadPos.load(std::memory_order_acquire);
        return (w - r + maxFrames) % maxFrames;
    }

private:
    void buildWindow();
    void processNextFFTFrame();

    double currentSampleRate = 44100.0;

    int fftOrder = 12;
    int fftSize = 4096;

    std::unique_ptr<juce::dsp::FFT> fft;

    WindowType windowType = WindowType::hann;
    std::vector<float> windowBuffer;

    float overlapFraction = 0.5f;
    int hopSize = 2048;

    std::vector<float> inputBuffer;
    int inputWritePos = 0;
    int samplesUntilNextFrame = 0;

    std::vector<float> fftWorkBuffer;

    static constexpr int maxFrames = 512;
    static constexpr int maxBins = 8192 / 2 + 1;

    std::vector<std::vector<float>> frameBuffer;
    std::atomic<int> frameWritePos{0};
    std::atomic<int> frameReadPos{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralAnalyser)
};
