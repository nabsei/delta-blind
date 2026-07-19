#include "DeltaMatchProcessor.h"
#include "PluginEditor.h"
#include <cmath>

DeltaMatchProcessor::DeltaMatchProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createLayout())
{
    formatManager.registerBasicFormats();
}

juce::AudioProcessorValueTreeState::ParameterLayout DeltaMatchProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterBool>("useB", "Use B", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("matchEnabled", "Match", true));
    return { params.begin(), params.end() };
}

bool DeltaMatchProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    auto sidechain = layouts.getChannelSet(true, 1);
    if (! sidechain.isDisabled() && sidechain != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void DeltaMatchProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;

    levelADb.store(-100.0f);
    levelBDb.store(-100.0f);
    matchGainDb.store(0.0f);
    sidechainActive.store(false);
    meterEnvA = meterEnvB = -100.0f;
    matchEnvA = matchEnvB = -100.0f;

    mixSmoothed.reset(sampleRate, 0.008);   // ~8ms crossfade -- click-free A/B switch
    mixSmoothed.setCurrentAndTargetValue(*apvts.getRawParameterValue("useB") > 0.5f ? 1.0f : 0.0f);

    matchGainSmoothed.reset(sampleRate, 0.05); // ~50ms -- MATCH on/off doesn't jump
    matchGainSmoothed.setCurrentAndTargetValue(1.0f);

    testSampleCounter = 0;
}

void DeltaMatchProcessor::loadFileInto(const juce::File& file, juce::AudioBuffer<float>& destBuffer,
                                        bool& loadedFlag, juce::String& nameOut, juce::String& pathOut, int& playheadOut)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
        return;

    if (reader->sampleRate <= 0.0 || reader->numChannels < 1)
        return;

    constexpr juce::int64 maxSamples = (juce::int64) (30 * 60 * 192000);
    juce::int64 lengthSamples = juce::jmin(reader->lengthInSamples, maxSamples);
    const int numSamples = (int) lengthSamples;
    if (numSamples <= 0)
        return;

    juce::AudioBuffer<float> raw((int) reader->numChannels, numSamples);
    reader->read(&raw, 0, numSamples, 0, true, true);

    juce::AudioBuffer<float> stereo(2, numSamples);
    if (raw.getNumChannels() >= 2)
    {
        stereo.copyFrom(0, 0, raw, 0, 0, numSamples);
        stereo.copyFrom(1, 0, raw, 1, 0, numSamples);
    }
    else
    {
        stereo.copyFrom(0, 0, raw, 0, 0, numSamples);
        stereo.copyFrom(1, 0, raw, 0, 0, numSamples);
    }

    if (currentSampleRate > 0.0 && std::abs(reader->sampleRate - currentSampleRate) > 0.5)
    {
        double ratio = reader->sampleRate / currentSampleRate;
        int outLen = juce::jmax(1, (int) std::ceil((double) numSamples / ratio));
        juce::AudioBuffer<float> resampled(2, outLen);
        for (int ch = 0; ch < 2; ++ch)
        {
            juce::LagrangeInterpolator interp;
            interp.reset();
            interp.process(ratio, stereo.getReadPointer(ch), resampled.getWritePointer(ch), outLen);
        }
        stereo = std::move(resampled);
    }

    {
        const juce::SpinLock::ScopedLockType sl(fileLock);
        destBuffer = std::move(stereo);
        loadedFlag = true;
        nameOut = file.getFileName();
        pathOut = file.getFullPathName();
        playheadOut = 0;
    }
}

void DeltaMatchProcessor::loadFileIntoA(const juce::File& file)
{
    loadFileInto(file, fileBufferA, fileALoaded, fileNameA, filePathA, filePlayheadA);
    fileModeEnabled.store(fileALoaded && fileBLoaded);
}

void DeltaMatchProcessor::loadFileIntoB(const juce::File& file)
{
    loadFileInto(file, fileBufferB, fileBLoaded, fileNameB, filePathB, filePlayheadB);
    fileModeEnabled.store(fileALoaded && fileBLoaded);
}

void DeltaMatchProcessor::clearFiles()
{
    fileModeEnabled.store(false);
    const juce::SpinLock::ScopedLockType sl(fileLock);
    fileALoaded = false;
    fileBLoaded = false;
    fileNameA = {};
    fileNameB = {};
    filePathA = {};
    filePathB = {};
}

void DeltaMatchProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    auto mainIn = getBusBuffer(buffer, true, 0);
    auto mainOut = getBusBuffer(buffer, false, 0);

    bool sideAvailable = false;
    juce::AudioBuffer<float> sideIn;
    if (auto* scBus = getBus(true, 1))
    {
        if (scBus->isEnabled())
        {
            auto view = getBusBuffer(buffer, true, 1);
            if (view.getNumChannels() >= 2)
            {
                sideIn = view;
                sideAvailable = true;
            }
        }
    }

    juce::AudioBuffer<float> localA(2, numSamples);
    juce::AudioBuffer<float> localB(2, numSamples);
    localA.copyFrom(0, 0, mainIn, 0, 0, numSamples);
    localA.copyFrom(1, 0, mainIn, 1, 0, numSamples);
    if (sideAvailable)
    {
        localB.copyFrom(0, 0, sideIn, 0, 0, numSamples);
        localB.copyFrom(1, 0, sideIn, 1, 0, numSamples);
    }
    else
    {
        localB.clear();
    }

    if (fileModeEnabled.load())
    {
        const juce::SpinLock::ScopedTryLockType stl(fileLock);
        if (stl.isLocked() && fileALoaded && fileBLoaded
            && fileBufferA.getNumSamples() > 0 && fileBufferB.getNumSamples() > 0)
        {
            sideAvailable = true;
            for (int i = 0; i < numSamples; ++i)
            {
                localA.setSample(0, i, fileBufferA.getSample(0, filePlayheadA));
                localA.setSample(1, i, fileBufferA.getSample(1, filePlayheadA));
                localB.setSample(0, i, fileBufferB.getSample(0, filePlayheadB));
                localB.setSample(1, i, fileBufferB.getSample(1, filePlayheadB));
                filePlayheadA = (filePlayheadA + 1) % fileBufferA.getNumSamples();
                filePlayheadB = (filePlayheadB + 1) % fileBufferB.getNumSamples();
            }
        }
    }

    if (testSignalEnabled.load())
    {
        // Same tone on both, but B is deliberately 7dB louder -- with MATCH
        // off you should hear B jump in level on switch; with MATCH on the
        // jump should disappear, demonstrating exactly what this plugin does.
        sideAvailable = true;
        constexpr double freq = 440.0;
        constexpr double twoPi = juce::MathConstants<double>::twoPi;
        constexpr float bGain = 2.24f; // +7dB

        for (int i = 0; i < numSamples; ++i)
        {
            juce::int64 n = testSampleCounter + i;
            float s = (float) (0.3 * std::sin(twoPi * freq * (double) n / currentSampleRate));
            localA.setSample(0, i, s);
            localA.setSample(1, i, s);
            localB.setSample(0, i, s * bGain);
            localB.setSample(1, i, s * bGain);
        }
        testSampleCounter += numSamples;
    }

    sidechainActive.store(sideAvailable);

    // Block RMS -> dB for both sources.
    double sumSqA = 0.0, sumSqB = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        sumSqA += (double) localA.getSample(0, i) * localA.getSample(0, i)
                + (double) localA.getSample(1, i) * localA.getSample(1, i);
        sumSqB += (double) localB.getSample(0, i) * localB.getSample(0, i)
                + (double) localB.getSample(1, i) * localB.getSample(1, i);
    }
    float blockDbA = (float) juce::Decibels::gainToDecibels(std::sqrt(sumSqA / (2.0 * numSamples) + 1.0e-24), -100.0);
    float blockDbB = (float) juce::Decibels::gainToDecibels(std::sqrt(sumSqB / (2.0 * numSamples) + 1.0e-24), -100.0);

    if (sideAvailable)
    {
        meterEnvA += 0.15f * (blockDbA - meterEnvA);
        meterEnvB += 0.15f * (blockDbB - meterEnvB);
        levelADb.store(meterEnvA);
        levelBDb.store(meterEnvB);

        matchEnvA += 0.02f * (blockDbA - matchEnvA);
        matchEnvB += 0.02f * (blockDbB - matchEnvB);
        float targetMatchDb = juce::jlimit(-24.0f, 24.0f, matchEnvA - matchEnvB);
        matchGainDb.store(targetMatchDb);
    }
    else
    {
        levelADb.store(-100.0f);
        levelBDb.store(-100.0f);
        matchGainDb.store(0.0f);
        meterEnvA = meterEnvB = -100.0f;
        matchEnvA = matchEnvB = -100.0f;
    }

    bool matchOn = *apvts.getRawParameterValue("matchEnabled") > 0.5f;
    bool useB = *apvts.getRawParameterValue("useB") > 0.5f;
    matchGainSmoothed.setTargetValue(matchOn ? juce::Decibels::decibelsToGain(matchGainDb.load()) : 1.0f);
    mixSmoothed.setTargetValue(useB ? 1.0f : 0.0f);

    auto* left = mainOut.getNumChannels() > 0 ? mainOut.getWritePointer(0) : nullptr;
    auto* right = mainOut.getNumChannels() > 1 ? mainOut.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float gB = matchGainSmoothed.getNextValue();
        float mix = mixSmoothed.getNextValue();

        float aL = localA.getSample(0, i);
        float aR = localA.getSample(1, i);
        float bL = localB.getSample(0, i) * gB;
        float bR = localB.getSample(1, i) * gB;

        float outL = (1.0f - mix) * aL + mix * bL;
        float outR = (1.0f - mix) * aR + mix * bR;

        if (left != nullptr) left[i] = outL;
        if (right != nullptr) right[i] = outR;
    }

    // Final safety ceiling -- proportional rescale, never a per-sample clamp.
    const float ceiling = juce::Decibels::decibelsToGain(-0.3f);
    float blockPeak = 0.0f;
    for (int ch = 0; ch < mainOut.getNumChannels(); ++ch)
        blockPeak = juce::jmax(blockPeak, mainOut.getMagnitude(ch, 0, numSamples));
    if (blockPeak > ceiling)
        mainOut.applyGain(ceiling / blockPeak);
}

juce::AudioProcessorEditor* DeltaMatchProcessor::createEditor()
{
    return new DeltaMatchEditor(*this);
}

void DeltaMatchProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("testSignal", testSignalEnabled.load(), nullptr);

    if (fileModeEnabled.load())
    {
        const juce::SpinLock::ScopedLockType sl(fileLock);
        state.setProperty("filePathA", filePathA, nullptr);
        state.setProperty("filePathB", filePathB, nullptr);
    }

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DeltaMatchProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        testSignalEnabled.store((bool) state.getProperty("testSignal", false));
        apvts.replaceState(state);

        juce::String savedPathA = state.getProperty("filePathA", "");
        juce::String savedPathB = state.getProperty("filePathB", "");
        if (savedPathA.isNotEmpty() && savedPathB.isNotEmpty())
        {
            juce::File fa(savedPathA), fb(savedPathB);
            if (fa.existsAsFile()) loadFileIntoA(fa);
            if (fb.existsAsFile()) loadFileIntoB(fb);
        }
    }
}
