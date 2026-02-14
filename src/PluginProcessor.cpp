#include "PluginProcessor.h"
#include "PluginEditor.h"

SpectrogramProcessor::SpectrogramProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

SpectrogramProcessor::~SpectrogramProcessor() {}

const juce::String SpectrogramProcessor::getName() const { return JucePlugin_Name; }
bool SpectrogramProcessor::acceptsMidi() const { return false; }
bool SpectrogramProcessor::producesMidi() const { return false; }
bool SpectrogramProcessor::isMidiEffect() const { return false; }
double SpectrogramProcessor::getTailLengthSeconds() const { return 0.0; }
int SpectrogramProcessor::getNumPrograms() { return 1; }
int SpectrogramProcessor::getCurrentProgram() { return 0; }
void SpectrogramProcessor::setCurrentProgram(int) {}
const juce::String SpectrogramProcessor::getProgramName(int) { return {}; }
void SpectrogramProcessor::changeProgramName(int, const juce::String&) {}

void SpectrogramProcessor::prepareToPlay(double, int)
{
}

void SpectrogramProcessor::releaseResources()
{
}

bool SpectrogramProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void SpectrogramProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Pure passthrough — audio goes in and out unchanged.
    // Future phases will copy samples into the analysis FIFO here.
    (void)buffer;
}

juce::AudioProcessorEditor* SpectrogramProcessor::createEditor()
{
    return new SpectrogramEditor(*this);
}

bool SpectrogramProcessor::hasEditor() const { return true; }

void SpectrogramProcessor::getStateInformation(juce::MemoryBlock&)
{
}

void SpectrogramProcessor::setStateInformation(const void*, int)
{
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectrogramProcessor();
}
