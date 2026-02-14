#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <atomic>

struct StereoFrame
{
    std::vector<float> magnitudeDb;  // per-bin magnitude in dB
    std::vector<float> pan;          // per-bin stereo pan: -1 = full L, 0 = centre, +1 = full R
};

class StereoSpectralAnalyser
{
public:
    enum class WindowType { hann, blackmanHarris };
    enum class FFTOrder { order1024 = 10, order2048 = 11, order4096 = 12, order8192 = 13 };

    StereoSpectralAnalyser();

    void prepare(double sampleRate, FFTOrder order);
    void setWindowType(WindowType type);
    void setOverlap(float overlapFraction);

    void pushSamples(const float* leftData, const float* rightData, int numSamples);

    bool pullNextFrame(StereoFrame& dest);

    int getFFTSize() const noexcept { return fftSize; }
    int getNumBins() const noexcept { return fftSize / 2 + 1; }
    double getSampleRate() const noexcept { return currentSampleRate; }

private:
    void buildWindow();
    void processNextFFTFrame();

    double currentSampleRate = 44100.0;
    int fftOrder = 12;
    int fftSize = 4096;

    std::unique_ptr<juce::dsp::FFT> fft;

    WindowType windowType = WindowType::hann;
    std::vector<float> windowBuffer;

    float overlapFrac = 0.5f;
    int hopSize = 2048;

    // Separate input buffers for L and R
    std::vector<float> inputBufferL;
    std::vector<float> inputBufferR;
    int inputWritePos = 0;
    int samplesUntilNextFrame = 0;

    std::vector<float> fftWorkL;
    std::vector<float> fftWorkR;

    static constexpr int maxFrames = 512;
    std::vector<StereoFrame> frameBuffer;
    std::atomic<int> frameWritePos{0};
    std::atomic<int> frameReadPos{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoSpectralAnalyser)
};
