#pragma once

#include "SpectralEngine.h"

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamIDs
{
inline constexpr auto amount = "amount";
inline constexpr auto lowCut = "lowCut";
inline constexpr auto highCut = "highCut";
inline constexpr auto attack = "attack";
inline constexpr auto release = "release";
inline constexpr auto sidechainGain = "sidechainGain";
inline constexpr auto outputGain = "outputGain";
inline constexpr auto mix = "mix";
inline constexpr auto mode = "mode";
inline constexpr auto detector = "detector";
inline constexpr auto bypass = "bypass";
} // namespace ParamIDs

// Sidechain spectral carve. Meter atomics are written on the audio thread
// and read by the editor without taking a lock.
class FrancSpaceAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBands = franc::SpectralEngine::numBands;
    static constexpr int statusNone = 0;
    static constexpr int statusSidechain = 1;
    static constexpr int statusInternal = 2;

    FrancSpaceAudioProcessor();
    ~FrancSpaceAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FrancSpace"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    bool supportsDoublePrecisionProcessing() const override { return true; }
    double getTailLengthSeconds() const override { return 0.08; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    float getSpectrum (int band) const noexcept
    {
        return spectrum[static_cast<size_t> (juce::jlimit (0, numBands - 1, band))].load (std::memory_order_relaxed);
    }

    float getReductionDb (int band) const noexcept
    {
        return reductionDb[static_cast<size_t> (juce::jlimit (0, numBands - 1, band))].load (std::memory_order_relaxed);
    }

    int getSidechainStatus() const noexcept
    {
        return sidechainStatus.load (std::memory_order_relaxed);
    }

    float bandFrequency (int band) const noexcept { return engine.bandFrequency (band); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct Delay
    {
        void prepare (int maxDelay);
        void setDelay (int samples) noexcept;
        void reset() noexcept;
        float process (float input) noexcept;

        std::vector<float> buffer;
        int write = 0;
        int delay = 0;
    };

    void publishVisuals() noexcept;
    void updateStatus (bool sidechainEnabled, bool internal, float blockPeak, int numSamples) noexcept;

    franc::SpectralEngine engine;
    Delay dryLeft;
    Delay dryRight;
    juce::AudioBuffer<float> floatScratch;

    juce::SmoothedValue<float> amountSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> lowSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> highSmooth;
    juce::SmoothedValue<float> sidechainGainSmooth;
    juce::SmoothedValue<float> outputSmooth;
    juce::SmoothedValue<float> mixSmooth;
    juce::SmoothedValue<float> bypassSmooth;
    juce::SmoothedValue<float> internalSmooth;

    std::atomic<float>* amountPtr = nullptr;
    std::atomic<float>* lowCutPtr = nullptr;
    std::atomic<float>* highCutPtr = nullptr;
    std::atomic<float>* attackPtr = nullptr;
    std::atomic<float>* releasePtr = nullptr;
    std::atomic<float>* sidechainGainPtr = nullptr;
    std::atomic<float>* outputGainPtr = nullptr;
    std::atomic<float>* mixPtr = nullptr;
    std::atomic<float>* modePtr = nullptr;
    std::atomic<float>* detectorPtr = nullptr;
    std::atomic<float>* bypassPtr = nullptr;

    std::atomic<float> spectrum[numBands];
    std::atomic<float> reductionDb[numBands];
    std::atomic<int> sidechainStatus { statusNone };

    int silenceSamples = 0;
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrancSpaceAudioProcessor)
};
