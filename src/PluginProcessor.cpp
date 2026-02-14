#include "PluginProcessor.h"
#include "PluginEditor.h"

SpectrogramProcessor::SpectrogramProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

SpectrogramProcessor::~SpectrogramProcessor()
{
    stopTimer();
}

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

void SpectrogramProcessor::prepareToPlay(double sampleRate, int)
{
    audioFifo.setSize(static_cast<int>(sampleRate) * 2);
    audioFifo.reset();

    analyser.prepare(sampleRate, SpectralAnalyser::FFTOrder::order4096);

    fifoReadBuffer.resize(static_cast<size_t>(analyser.getFFTSize()));

    startTimerHz(60);
}

void SpectrogramProcessor::releaseResources()
{
    stopTimer();
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

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels == 0 || numSamples == 0)
        return;

    // Mix all channels to mono and push into the FIFO (no allocation, no blocking)
    if (numChannels == 1)
    {
        audioFifo.push(buffer.getReadPointer(0), numSamples);
    }
    else
    {
        // Use channel 0 as scratch — we're passthrough so it doesn't matter
        // Actually, avoid modifying the buffer. Use a small stack buffer instead.
        const float* left = buffer.getReadPointer(0);
        const float* right = buffer.getReadPointer(1);

        // Process in small chunks to stay on the stack
        constexpr int chunkSize = 512;
        float mono[chunkSize];

        for (int offset = 0; offset < numSamples; offset += chunkSize)
        {
            const int count = std::min(chunkSize, numSamples - offset);
            for (int i = 0; i < count; ++i)
                mono[i] = (left[offset + i] + right[offset + i]) * 0.5f;

            audioFifo.push(mono, count);
        }
    }

    // Audio passes through unchanged (analyser-only plugin)
}

void SpectrogramProcessor::timerCallback()
{
    // Drain the FIFO and feed the analyser on the message thread
    const int available = audioFifo.getNumReady();
    if (available <= 0)
        return;

    const int toRead = std::min(available, static_cast<int>(fifoReadBuffer.size()));
    const int read = audioFifo.pop(fifoReadBuffer.data(), toRead);

    if (read > 0)
        analyser.pushSamples(fifoReadBuffer.data(), read);
}

juce::AudioProcessorEditor* SpectrogramProcessor::createEditor()
{
    return new SpectrogramEditor(*this);
}

bool SpectrogramProcessor::hasEditor() const { return true; }

void SpectrogramProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement>("SpectrogramState");
    xml->setAttribute("fftSizeId",   settings.fftSizeId);
    xml->setAttribute("overlapId",   settings.overlapId);
    xml->setAttribute("windowId",    settings.windowId);
    xml->setAttribute("colourMapId", settings.colourMapId);
    xml->setAttribute("logScale",    settings.logScale);
    xml->setAttribute("dbFloor",     static_cast<double>(settings.dbFloor));
    xml->setAttribute("dbCeiling",   static_cast<double>(settings.dbCeiling));
    xml->setAttribute("editorWidth", settings.editorWidth);
    xml->setAttribute("editorHeight", settings.editorHeight);
    copyXmlToBinary(*xml, destData);
}

void SpectrogramProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName("SpectrogramState"))
        return;

    settings.fftSizeId   = xml->getIntAttribute("fftSizeId",   settings.fftSizeId);
    settings.overlapId   = xml->getIntAttribute("overlapId",   settings.overlapId);
    settings.windowId    = xml->getIntAttribute("windowId",    settings.windowId);
    settings.colourMapId = xml->getIntAttribute("colourMapId", settings.colourMapId);
    settings.logScale    = xml->getBoolAttribute("logScale",   settings.logScale);
    settings.dbFloor     = static_cast<float>(xml->getDoubleAttribute("dbFloor",  settings.dbFloor));
    settings.dbCeiling   = static_cast<float>(xml->getDoubleAttribute("dbCeiling", settings.dbCeiling));
    settings.editorWidth = xml->getIntAttribute("editorWidth", settings.editorWidth);
    settings.editorHeight = xml->getIntAttribute("editorHeight", settings.editorHeight);

    // Apply analyser settings immediately
    SpectralAnalyser::FFTOrder order;
    switch (settings.fftSizeId)
    {
        case 1:  order = SpectralAnalyser::FFTOrder::order1024; break;
        case 2:  order = SpectralAnalyser::FFTOrder::order2048; break;
        case 4:  order = SpectralAnalyser::FFTOrder::order8192; break;
        default: order = SpectralAnalyser::FFTOrder::order4096; break;
    }
    analyser.prepare(analyser.getSampleRate(), order);
    analyser.setOverlap(settings.overlapId == 2 ? 0.75f : 0.5f);
    analyser.setWindowType(settings.windowId == 2
        ? SpectralAnalyser::WindowType::blackmanHarris
        : SpectralAnalyser::WindowType::hann);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectrogramProcessor();
}
