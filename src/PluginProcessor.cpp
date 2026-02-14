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
    const int bufSize = static_cast<int>(sampleRate) * 2;
    audioFifo.setSize(bufSize);
    audioFifo.reset();

    stereoFifoL.setSize(bufSize);
    stereoFifoL.reset();
    stereoFifoR.setSize(bufSize);
    stereoFifoR.reset();

    analyser.prepare(sampleRate, SpectralAnalyser::FFTOrder::order4096);
    stereoAnalyser.prepare(sampleRate, StereoSpectralAnalyser::FFTOrder::order4096);

    fifoReadBuffer.resize(static_cast<size_t>(analyser.getFFTSize()));
    stereoReadBufL.resize(static_cast<size_t>(analyser.getFFTSize()));
    stereoReadBufR.resize(static_cast<size_t>(analyser.getFFTSize()));

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

    // Mix to mono for standard analyser
    if (numChannels == 1)
    {
        audioFifo.push(buffer.getReadPointer(0), numSamples);
    }
    else
    {
        const float* left = buffer.getReadPointer(0);
        const float* right = buffer.getReadPointer(1);

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

    // Push L/R separately for Nebula mode
    if (nebulaActive.load(std::memory_order_relaxed) && numChannels >= 2)
    {
        stereoFifoL.push(buffer.getReadPointer(0), numSamples);
        stereoFifoR.push(buffer.getReadPointer(1), numSamples);
    }

    // Audio passes through unchanged
}

void SpectrogramProcessor::timerCallback()
{
    // Drain mono FIFO -> analyser
    {
        const int available = audioFifo.getNumReady();
        if (available > 0)
        {
            const int toRead = std::min(available, static_cast<int>(fifoReadBuffer.size()));
            const int read = audioFifo.pop(fifoReadBuffer.data(), toRead);
            if (read > 0)
                analyser.pushSamples(fifoReadBuffer.data(), read);
        }
    }

    // Drain stereo FIFOs -> stereo analyser
    if (nebulaActive.load(std::memory_order_relaxed))
    {
        const int availL = stereoFifoL.getNumReady();
        const int availR = stereoFifoR.getNumReady();
        const int avail = std::min(availL, availR);
        if (avail > 0)
        {
            const int toRead = std::min(avail, static_cast<int>(stereoReadBufL.size()));
            const int readL = stereoFifoL.pop(stereoReadBufL.data(), toRead);
            const int readR = stereoFifoR.pop(stereoReadBufR.data(), toRead);
            const int read = std::min(readL, readR);
            if (read > 0)
                stereoAnalyser.pushSamples(stereoReadBufL.data(), stereoReadBufR.data(), read);
        }
    }
}

juce::AudioProcessorEditor* SpectrogramProcessor::createEditor()
{
    return new SpectrogramEditor(*this);
}

bool SpectrogramProcessor::hasEditor() const { return true; }

void SpectrogramProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement>("SpectrogramState");
    xml->setAttribute("fftSizeId",      settings.fftSizeId);
    xml->setAttribute("overlapId",      settings.overlapId);
    xml->setAttribute("windowId",       settings.windowId);
    xml->setAttribute("colourMapId",    settings.colourMapId);
    xml->setAttribute("logScale",       settings.logScale);
    xml->setAttribute("dbFloor",        static_cast<double>(settings.dbFloor));
    xml->setAttribute("dbCeiling",      static_cast<double>(settings.dbCeiling));
    xml->setAttribute("editorWidth",    settings.editorWidth);
    xml->setAttribute("editorHeight",   settings.editorHeight);
    xml->setAttribute("zoomMinFreq",    static_cast<double>(settings.zoomMinFreq));
    xml->setAttribute("zoomMaxFreq",    static_cast<double>(settings.zoomMaxFreq));
    xml->setAttribute("peakHoldEnabled", settings.peakHoldEnabled);
    xml->setAttribute("peakDecayRate",  static_cast<double>(settings.peakDecayRate));
    xml->setAttribute("rtaEnabled",     settings.rtaEnabled);
    xml->setAttribute("bloomEnabled",   settings.bloomEnabled);
    xml->setAttribute("bloomIntensity", static_cast<double>(settings.bloomIntensity));
    xml->setAttribute("bloomThreshold", static_cast<double>(settings.bloomThreshold));
    xml->setAttribute("nebulaMode",     settings.nebulaMode);
    copyXmlToBinary(*xml, destData);
}

void SpectrogramProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName("SpectrogramState"))
        return;

    settings.fftSizeId      = xml->getIntAttribute("fftSizeId",      settings.fftSizeId);
    settings.overlapId      = xml->getIntAttribute("overlapId",      settings.overlapId);
    settings.windowId       = xml->getIntAttribute("windowId",       settings.windowId);
    settings.colourMapId    = xml->getIntAttribute("colourMapId",    settings.colourMapId);
    settings.logScale       = xml->getBoolAttribute("logScale",      settings.logScale);
    settings.dbFloor        = static_cast<float>(xml->getDoubleAttribute("dbFloor",      settings.dbFloor));
    settings.dbCeiling      = static_cast<float>(xml->getDoubleAttribute("dbCeiling",    settings.dbCeiling));
    settings.editorWidth    = xml->getIntAttribute("editorWidth",    settings.editorWidth);
    settings.editorHeight   = xml->getIntAttribute("editorHeight",   settings.editorHeight);
    settings.zoomMinFreq    = static_cast<float>(xml->getDoubleAttribute("zoomMinFreq",  settings.zoomMinFreq));
    settings.zoomMaxFreq    = static_cast<float>(xml->getDoubleAttribute("zoomMaxFreq",  settings.zoomMaxFreq));
    settings.peakHoldEnabled = xml->getBoolAttribute("peakHoldEnabled", settings.peakHoldEnabled);
    settings.peakDecayRate  = static_cast<float>(xml->getDoubleAttribute("peakDecayRate", settings.peakDecayRate));
    settings.rtaEnabled     = xml->getBoolAttribute("rtaEnabled",    settings.rtaEnabled);
    settings.bloomEnabled   = xml->getBoolAttribute("bloomEnabled",  settings.bloomEnabled);
    settings.bloomIntensity = static_cast<float>(xml->getDoubleAttribute("bloomIntensity", settings.bloomIntensity));
    settings.bloomThreshold = static_cast<float>(xml->getDoubleAttribute("bloomThreshold", settings.bloomThreshold));
    settings.nebulaMode     = xml->getBoolAttribute("nebulaMode",    settings.nebulaMode);

    // Apply analyser settings
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
