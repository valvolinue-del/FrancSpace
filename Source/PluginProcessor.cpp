#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
juce::String formatPercent (float value, int)
{
    return juce::String (value, 1) + " %";
}

juce::String formatDb (float value, int)
{
    const auto text = juce::String (value, 1);
    return (value > 0.0f ? "+" : "") + text + " dB";
}

juce::String formatMs (float value, int)
{
    return juce::String (value, value < 10.0f ? 1 : 0) + " ms";
}

juce::String formatHz (float value, int)
{
    if (value >= 1000.0f)
        return juce::String (value / 1000.0f, value >= 10000.0f ? 0 : 2) + " kHz";
    return juce::String (value, value < 100.0f ? 1 : 0) + " Hz";
}

float parseNumber (const juce::String& text)
{
    return text.trim().getFloatValue();
}

juce::NormalisableRange<float> logarithmic (float start, float end)
{
    return {
        start,
        end,
        [] (float rangeStart, float rangeEnd, float proportion)
        {
            return rangeStart * std::pow (rangeEnd / rangeStart, proportion);
        },
        [] (float rangeStart, float rangeEnd, float value)
        {
            const float clamped = juce::jlimit (rangeStart, rangeEnd, value);
            return std::log (clamped / rangeStart) / std::log (rangeEnd / rangeStart);
        },
    };
}

void addFloat (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const char* id, const juce::String& name,
               const juce::NormalisableRange<float>& range, float defaultValue, const juce::String& label,
               std::function<juce::String (float, int)> toText, std::function<float (const juce::String&)> fromText)
{
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 },
        name,
        range,
        defaultValue,
        juce::AudioParameterFloatAttributes()
            .withLabel (label)
            .withStringFromValueFunction (std::move (toText))
            .withValueFromStringFunction (std::move (fromText))));
}
} // namespace

void FrancSpaceAudioProcessor::Delay::prepare (int maxDelay)
{
    buffer.assign (static_cast<size_t> (std::max (1, maxDelay + 1)), 0.0f);
    write = 0;
    delay = 0;
}

void FrancSpaceAudioProcessor::Delay::setDelay (int samples) noexcept
{
    delay = juce::jlimit (0, static_cast<int> (buffer.size()) - 1, samples);
}

void FrancSpaceAudioProcessor::Delay::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    write = 0;
}

float FrancSpaceAudioProcessor::Delay::process (float input) noexcept
{
    const int size = static_cast<int> (buffer.size());
    int read = write - delay;
    if (read < 0)
        read += size;

    const float output = buffer[static_cast<size_t> (read)];
    buffer[static_cast<size_t> (write)] = input;
    write = (write + 1) % size;
    return output;
}

juce::AudioProcessorValueTreeState::ParameterLayout FrancSpaceAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    addFloat (layout, ParamIDs::amount, "Amount", { 0.0f, 100.0f, 0.01f }, 50.0f, "%", formatPercent, parseNumber);
    addFloat (layout, ParamIDs::lowCut, "Low Cut", logarithmic (20.0f, 20000.0f), 20.0f, "Hz", formatHz, parseNumber);
    addFloat (layout, ParamIDs::highCut, "High Cut", logarithmic (20.0f, 20000.0f), 20000.0f, "Hz", formatHz, parseNumber);
    addFloat (layout, ParamIDs::attack, "Attack", { 0.1f, 200.0f, 0.01f, 0.35f }, 5.0f, "ms", formatMs, parseNumber);
    addFloat (layout, ParamIDs::release, "Release", { 5.0f, 2000.0f, 0.01f, 0.4f }, 80.0f, "ms", formatMs, parseNumber);
    addFloat (layout, ParamIDs::sidechainGain, "Sidechain Gain", { -24.0f, 24.0f, 0.01f }, 0.0f, "dB", formatDb, parseNumber);
    addFloat (layout, ParamIDs::outputGain, "Output", { -24.0f, 24.0f, 0.01f }, 0.0f, "dB", formatDb, parseNumber);
    addFloat (layout, ParamIDs::mix, "Mix", { 0.0f, 100.0f, 0.01f }, 100.0f, "%", formatPercent, parseNumber);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::mode, 1 },
        "Mode",
        juce::StringArray { "Linked", "Left/Right", "Mid/Side" },
        0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::detector, 1 },
        "Detector",
        juce::StringArray { "External", "Internal" },
        0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::bypass, 1 },
        "Bypass",
        false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction (
            [] (bool enabled, int) { return enabled ? juce::String ("On") : juce::String ("Off"); })));

    return layout;
}

FrancSpaceAudioProcessor::FrancSpaceAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "FrancSpace", createParameterLayout())
{
    amountPtr = apvts.getRawParameterValue (ParamIDs::amount);
    lowCutPtr = apvts.getRawParameterValue (ParamIDs::lowCut);
    highCutPtr = apvts.getRawParameterValue (ParamIDs::highCut);
    attackPtr = apvts.getRawParameterValue (ParamIDs::attack);
    releasePtr = apvts.getRawParameterValue (ParamIDs::release);
    sidechainGainPtr = apvts.getRawParameterValue (ParamIDs::sidechainGain);
    outputGainPtr = apvts.getRawParameterValue (ParamIDs::outputGain);
    mixPtr = apvts.getRawParameterValue (ParamIDs::mix);
    modePtr = apvts.getRawParameterValue (ParamIDs::mode);
    detectorPtr = apvts.getRawParameterValue (ParamIDs::detector);
    bypassPtr = apvts.getRawParameterValue (ParamIDs::bypass);

    for (int band = 0; band < numBands; ++band)
    {
        spectrum[band].store (0.0f);
        reductionDb[band].store (0.0f);
    }
}

void FrancSpaceAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    engine.prepare (currentSampleRate);

    const int latency = engine.getLatencySamples();
    dryLeft.prepare (latency + 8);
    dryRight.prepare (latency + 8);
    dryLeft.setDelay (latency);
    dryRight.setDelay (latency);
    dryLeft.reset();
    dryRight.reset();
    setLatencySamples (latency);

    auto prime = [sampleRate] (auto& smoother, float value, double seconds)
    {
        smoother.reset (sampleRate, seconds);
        smoother.setCurrentAndTargetValue (value);
    };

    prime (amountSmooth, amountPtr->load(), 0.02);
    prime (lowSmooth, std::max (20.0f, lowCutPtr->load()), 0.03);
    prime (highSmooth, std::max (20.0f, highCutPtr->load()), 0.03);
    prime (sidechainGainSmooth, sidechainGainPtr->load(), 0.02);
    prime (outputSmooth, outputGainPtr->load(), 0.02);
    prime (mixSmooth, mixPtr->load() * 0.01f, 0.02);
    prime (bypassSmooth, bypassPtr->load() >= 0.5f ? 1.0f : 0.0f, 0.01);
    prime (internalSmooth, detectorPtr->load() >= 0.5f ? 1.0f : 0.0f, 0.02);

    const int channels = std::max (8, getTotalNumInputChannels());
    const int block = std::max (samplesPerBlock, 256);
    floatScratch.setSize (channels, block, false, false, true);
    silenceSamples = 0;
}

void FrancSpaceAudioProcessor::releaseResources()
{
    engine.reset();
    dryLeft.reset();
    dryRight.reset();
}

bool FrancSpaceAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto side = layouts.getChannelSet (true, 1);
        if (! side.isDisabled()
            && side != juce::AudioChannelSet::mono()
            && side != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void FrancSpaceAudioProcessor::publishVisuals() noexcept
{
    const auto& frame = engine.getVisuals();
    for (int band = 0; band < numBands; ++band)
    {
        spectrum[band].store (frame.spectrum[band], std::memory_order_relaxed);
        reductionDb[band].store (frame.reductionDb[band], std::memory_order_relaxed);
    }
}

void FrancSpaceAudioProcessor::updateStatus (bool sidechainEnabled, bool internal, float blockPeak, int numSamples) noexcept
{
    if (internal)
    {
        sidechainStatus.store (statusInternal, std::memory_order_relaxed);
        silenceSamples = 0;
        return;
    }

    if (! sidechainEnabled)
    {
        sidechainStatus.store (statusNone, std::memory_order_relaxed);
        silenceSamples = 0;
        return;
    }

    if (blockPeak > 1.0e-4f)
        silenceSamples = 0;
    else
        silenceSamples += numSamples;

    const int hold = static_cast<int> (currentSampleRate * 0.35);
    sidechainStatus.store (silenceSamples > hold ? statusNone : statusSidechain, std::memory_order_relaxed);
}

void FrancSpaceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    auto main = getBusBuffer (buffer, true, 0);
    const int mainChannels = main.getNumChannels();
    if (mainChannels <= 0)
        return;

    const bool sidechainEnabled = getBusCount (true) > 1
                               && getBus (true, 1)->isEnabled()
                               && getChannelCountOfBus (true, 1) > 0;
    juce::AudioBuffer<float> sidechain;
    if (sidechainEnabled)
        sidechain = getBusBuffer (buffer, true, 1);

    const int sideChannels = sidechainEnabled ? sidechain.getNumChannels() : 0;
    const bool internal = detectorPtr->load() >= 0.5f;

    float blockPeak = 0.0f;
    if (sidechainEnabled)
    {
        for (int channel = 0; channel < sideChannels; ++channel)
        {
            const float* samples = sidechain.getReadPointer (channel);
            for (int index = 0; index < numSamples; ++index)
                blockPeak = std::max (blockPeak, std::abs (samples[index]));
        }
    }

    updateStatus (sidechainEnabled, internal, blockPeak, numSamples);

    amountSmooth.setTargetValue (amountPtr->load());
    lowSmooth.setTargetValue (std::max (20.0f, lowCutPtr->load()));
    highSmooth.setTargetValue (std::max (20.0f, highCutPtr->load()));
    sidechainGainSmooth.setTargetValue (sidechainGainPtr->load());
    outputSmooth.setTargetValue (outputGainPtr->load());
    mixSmooth.setTargetValue (mixPtr->load() * 0.01f);
    bypassSmooth.setTargetValue (bypassPtr->load() >= 0.5f ? 1.0f : 0.0f);
    internalSmooth.setTargetValue (internal ? 1.0f : 0.0f);

    const float attack = attackPtr->load();
    const float release = releasePtr->load();
    const int mode = juce::jlimit (0, 2, juce::roundToInt (modePtr->load()));
    const bool analyse = internal || sidechainEnabled;

    auto* left = main.getWritePointer (0);
    auto* right = mainChannels > 1 ? main.getWritePointer (1) : nullptr;
    const float* sideLeft = sideChannels > 0 ? sidechain.getReadPointer (0) : nullptr;
    const float* sideRight = sideChannels > 1 ? sidechain.getReadPointer (1) : sideLeft;

    for (int index = 0; index < numSamples; ++index)
    {
        const float inLeft = left[index];
        const float inRight = right != nullptr ? right[index] : inLeft;

        const float trim = juce::Decibels::decibelsToGain (sidechainGainSmooth.getNextValue());
        float externalLeft = 0.0f;
        float externalRight = 0.0f;
        if (sideLeft != nullptr)
        {
            externalLeft = sideLeft[index] * trim;
            externalRight = sideRight[index] * trim;
        }

        const float detectorMix = internalSmooth.getNextValue();
        const float detectLeft = externalLeft + (inLeft - externalLeft) * detectorMix;
        const float detectRight = externalRight + (inRight - externalRight) * detectorMix;

        franc::SpectralParams params;
        params.amount = amountSmooth.getNextValue() * 0.01f;
        params.lowCutHz = lowSmooth.getNextValue();
        params.highCutHz = highSmooth.getNextValue();
        params.attackMs = attack;
        params.releaseMs = release;
        params.mode = mode;
        params.analyse = analyse;

        const auto wet = engine.processSample (inLeft, inRight, detectLeft, detectRight, params);
        const float dryL = dryLeft.process (inLeft);
        const float dryR = dryRight.process (inRight);
        const float mix = mixSmooth.getNextValue();
        const float outputGain = juce::Decibels::decibelsToGain (outputSmooth.getNextValue());
        const float bypass = bypassSmooth.getNextValue();

        const float mixedLeft = dryL + (wet.left - dryL) * mix;
        const float mixedRight = dryR + (wet.right - dryR) * mix;
        const float gainedLeft = mixedLeft * outputGain;
        const float gainedRight = mixedRight * outputGain;

        left[index] = dryL + (gainedLeft - dryL) * (1.0f - bypass);
        if (right != nullptr)
            right[index] = dryR + (gainedRight - dryR) * (1.0f - bypass);
    }

    publishVisuals();
}

void FrancSpaceAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();
    if (channels <= 0 || samples <= 0 || floatScratch.getNumSamples() <= 0)
        return;

    const int scratchChannels = std::min (channels, floatScratch.getNumChannels());
    const int chunk = floatScratch.getNumSamples();
    const int mainOut = std::min (getMainBusNumOutputChannels(), scratchChannels);

    int offset = 0;
    while (offset < samples)
    {
        const int count = std::min (chunk, samples - offset);
        for (int channel = 0; channel < scratchChannels; ++channel)
            for (int index = 0; index < count; ++index)
                floatScratch.setSample (channel, index, static_cast<float> (buffer.getSample (channel, offset + index)));

        juce::AudioBuffer<float> slice (floatScratch.getArrayOfWritePointers(), scratchChannels, count);
        processBlock (slice, midi);

        for (int channel = 0; channel < mainOut; ++channel)
            for (int index = 0; index < count; ++index)
                buffer.setSample (channel, offset + index, static_cast<double> (slice.getSample (channel, index)));

        offset += count;
    }
}

void FrancSpaceAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (const auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FrancSpaceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* FrancSpaceAudioProcessor::createEditor()
{
    return new FrancSpaceAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FrancSpaceAudioProcessor();
}
