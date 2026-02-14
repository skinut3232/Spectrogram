#pragma once

#include "PluginProcessor.h"

class SpectrogramEditor : public juce::AudioProcessorEditor
{
public:
    explicit SpectrogramEditor(SpectrogramProcessor&);
    ~SpectrogramEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SpectrogramProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramEditor)
};
