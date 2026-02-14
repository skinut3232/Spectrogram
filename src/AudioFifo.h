#pragma once

#include <juce_core/juce_core.h>

class AudioFifo
{
public:
    AudioFifo(int capacity)
        : fifo(capacity), buffer(1, capacity)
    {
    }

    void setSize(int newCapacity)
    {
        fifo.setTotalSize(newCapacity);
        buffer.setSize(1, newCapacity);
        fifo.reset();
    }

    void reset()
    {
        fifo.reset();
    }

    int getFreeSpace() const noexcept { return fifo.getFreeSpace(); }
    int getNumReady() const noexcept { return fifo.getNumReady(); }

    void push(const float* data, int numSamples) noexcept
    {
        const auto scope = fifo.write(numSamples);
        auto* dest = buffer.getWritePointer(0);

        if (scope.blockSize1 > 0)
            std::memcpy(dest + scope.startIndex1, data, sizeof(float) * (size_t)scope.blockSize1);

        if (scope.blockSize2 > 0)
            std::memcpy(dest + scope.startIndex2, data + scope.blockSize1, sizeof(float) * (size_t)scope.blockSize2);
    }

    int pop(float* dest, int numSamples) noexcept
    {
        const auto scope = fifo.read(numSamples);
        const auto* src = buffer.getReadPointer(0);

        if (scope.blockSize1 > 0)
            std::memcpy(dest, src + scope.startIndex1, sizeof(float) * (size_t)scope.blockSize1);

        if (scope.blockSize2 > 0)
            std::memcpy(dest + scope.blockSize1, src + scope.startIndex2, sizeof(float) * (size_t)scope.blockSize2);

        return scope.blockSize1 + scope.blockSize2;
    }

private:
    juce::AbstractFifo fifo;
    juce::AudioBuffer<float> buffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFifo)
};
