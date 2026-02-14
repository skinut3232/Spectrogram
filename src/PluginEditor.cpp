#include "PluginEditor.h"

SpectrogramEditor::SpectrogramEditor(SpectrogramProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(800, 400);
    setResizable(true, true);
}

SpectrogramEditor::~SpectrogramEditor() {}

void SpectrogramEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawFittedText("Spectrogram Plugin — Phase 0 OK",
                     getLocalBounds(), juce::Justification::centred, 1);
}

void SpectrogramEditor::resized()
{
}
