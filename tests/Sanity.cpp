#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        std::cerr << "FAIL " << message << "\n";
        ++failures;
    }
    else
    {
        std::cout << "ok   " << message << "\n";
    }
}

void setRaw (FrancSpaceAudioProcessor& processor, const juce::String& id, float raw)
{
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (id));
    if (parameter == nullptr)
    {
        expect (false, "missing parameter " + id.toStdString());
        return;
    }

    parameter->setValueNotifyingHost (parameter->convertTo0to1 (raw));
}

float getRaw (FrancSpaceAudioProcessor& processor, const juce::String& id)
{
    return processor.apvts.getRawParameterValue (id)->load();
}

struct Rendered
{
    std::vector<float> left;
    int latency = 0;
};

Rendered render (FrancSpaceAudioProcessor& processor, const std::vector<float>& input, const std::vector<float>* sidechain,
                 double sampleRate, int block)
{
    if (sidechain != nullptr)
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
        expect (processor.setBusesLayout (layout), "stereo sidechain layout is accepted");
    }

    processor.prepareToPlay (sampleRate, block);

    Rendered rendered;
    rendered.latency = processor.getLatencySamples();
    rendered.left.assign (input.size(), 0.0f);

    const int channels = sidechain != nullptr ? 4 : 2;
    juce::AudioBuffer<float> buffer (channels, block);
    juce::MidiBuffer midi;
    size_t offset = 0;
    while (offset < input.size())
    {
        const int count = static_cast<int> (std::min (static_cast<size_t> (block), input.size() - offset));
        buffer.clear();
        for (int sample = 0; sample < count; ++sample)
        {
            const float value = input[offset + static_cast<size_t> (sample)];
            buffer.setSample (0, sample, value);
            buffer.setSample (1, sample, value);
            if (sidechain != nullptr)
            {
                const float side = (*sidechain)[offset + static_cast<size_t> (sample)];
                buffer.setSample (2, sample, side);
                buffer.setSample (3, sample, side);
            }
        }

        juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(), channels, count);
        processor.processBlock (slice, midi);

        for (int sample = 0; sample < count; ++sample)
            rendered.left[offset + static_cast<size_t> (sample)] = slice.getSample (0, sample);

        offset += static_cast<size_t> (count);
    }

    return rendered;
}

std::vector<float> sine (int count, double sampleRate, double frequency, float amplitude)
{
    std::vector<float> data (static_cast<size_t> (count));
    for (int sample = 0; sample < count; ++sample)
        data[static_cast<size_t> (sample)] = amplitude * std::sin (2.0 * juce::MathConstants<double>::pi * frequency
                                                                    * static_cast<double> (sample) / sampleRate);
    return data;
}

std::vector<float> tones (int count, double sampleRate)
{
    std::vector<float> data (static_cast<size_t> (count));
    for (int sample = 0; sample < count; ++sample)
    {
        const double time = static_cast<double> (sample) / sampleRate;
        data[static_cast<size_t> (sample)] = 0.2f * std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * time)
                                           + 0.2f * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * time)
                                           + 0.2f * std::sin (2.0 * juce::MathConstants<double>::pi * 10000.0 * time);
    }
    return data;
}

bool allFinite (const std::vector<float>& data)
{
    for (float value : data)
        if (! std::isfinite (value))
            return false;
    return true;
}

double goertzel (const std::vector<float>& data, int start, int count, double frequency, double sampleRate)
{
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double coefficient = 2.0 * std::cos (omega);
    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;
    for (int sample = 0; sample < count; ++sample)
    {
        s0 = static_cast<double> (data[static_cast<size_t> (start + sample)]) + coefficient * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - coefficient * s1 * s2));
}

double bandLevel (const std::vector<float>& data, int start, int count, double lowHz, double highHz, double sampleRate)
{
    const double df = sampleRate / static_cast<double> (count);
    const int first = std::max (1, static_cast<int> (std::floor (lowHz / df)));
    const int last = std::min (count / 2 - 1, static_cast<int> (std::ceil (highHz / df)));
    double energy = 0.0;
    int bins = 0;
    for (int bin = first; bin <= last; ++bin)
    {
        const double omega = 2.0 * juce::MathConstants<double>::pi * static_cast<double> (bin) / static_cast<double> (count);
        double real = 0.0;
        double imag = 0.0;
        for (int sample = 0; sample < count; ++sample)
        {
            const double window = 0.5 * (1.0 - std::cos (2.0 * juce::MathConstants<double>::pi * sample / static_cast<double> (count - 1)));
            const double value = static_cast<double> (data[static_cast<size_t> (start + sample)]) * window;
            real += value * std::cos (omega * sample);
            imag -= value * std::sin (omega * sample);
        }
        energy += real * real + imag * imag;
        ++bins;
    }

    return bins > 0 ? std::sqrt (energy / static_cast<double> (bins)) : 0.0;
}

double dbOfRatio (double output, double input)
{
    return 20.0 * std::log10 (std::max (1.0e-12, output) / std::max (1.0e-12, input));
}

float maxAbsError (const std::vector<float>& output, const std::vector<float>& input, int latency)
{
    float error = 0.0f;
    for (int sample = latency; sample < static_cast<int> (input.size()); ++sample)
        error = std::max (error, std::abs (output[static_cast<size_t> (sample)] - input[static_cast<size_t> (sample - latency)]));
    return error;
}

struct ToneDelta
{
    double low = 0.0;
    double mid = 0.0;
    double high = 0.0;
};

ToneDelta measureTones (const std::vector<float>& input, const Rendered& rendered, double sampleRate)
{
    const int start = rendered.latency + static_cast<int> (sampleRate * 0.35);
    const int count = static_cast<int> (sampleRate * 0.4);
    ToneDelta delta;
    delta.low = dbOfRatio (goertzel (rendered.left, start, count, 100.0, sampleRate),
                           goertzel (input, start - rendered.latency, count, 100.0, sampleRate));
    delta.mid = dbOfRatio (goertzel (rendered.left, start, count, 1000.0, sampleRate),
                           goertzel (input, start - rendered.latency, count, 1000.0, sampleRate));
    delta.high = dbOfRatio (goertzel (rendered.left, start, count, 10000.0, sampleRate),
                            goertzel (input, start - rendered.latency, count, 10000.0, sampleRate));
    return delta;
}
} // namespace

int runTests()
{
    constexpr double sampleRate = 48000.0;
    constexpr int block = 256;

    {
        FrancSpaceAudioProcessor processor;
        expect (processor.getName() == "FrancSpace", "plug-in name is FrancSpace");
        expect (processor.getLatencySamples() == 0, "latency is unset before prepare");
        expect (processor.getBusCount (true) == 2, "main input plus a sidechain bus");
        expect (! processor.getBus (true, 1)->isEnabledByDefault(), "sidechain bus starts disabled");
    }

    {
        FrancSpaceAudioProcessor processor;
        const auto input = sine (static_cast<int> (sampleRate), sampleRate, 1000.0, 0.25f);
        const auto rendered = render (processor, input, nullptr, sampleRate, block);
        expect (rendered.latency == 2047, "reported latency is one frame minus one sample (" + std::to_string (rendered.latency) + ")");
        const float error = maxAbsError (rendered.left, input, rendered.latency);
        std::cout << "     no-sidechain max error " << error << "\n";
        expect (error < 1.0e-4f, "no sidechain output matches the input within latency");
        expect (allFinite (rendered.left), "no-sidechain output is finite");
        expect (processor.getSidechainStatus() == FrancSpaceAudioProcessor::statusNone, "UI reports no sidechain");
    }

    {
        FrancSpaceAudioProcessor processor;
        std::vector<float> impulse (block * 16, 0.0f);
        impulse[0] = 1.0f;
        const auto rendered = render (processor, impulse, nullptr, sampleRate, block);
        int peakAt = 0;
        float peak = 0.0f;
        for (int sample = 0; sample < static_cast<int> (rendered.left.size()); ++sample)
        {
            const float value = std::abs (rendered.left[static_cast<size_t> (sample)]);
            if (value > peak)
            {
                peak = value;
                peakAt = sample;
            }
        }
        expect (peakAt == rendered.latency, "impulse lands on the reported latency (peak " + std::to_string (peakAt) + ")");
        expect (std::abs (peak - 1.0f) < 1.0e-4f, "impulse peak is unity (" + std::to_string (peak) + ")");
    }

    const auto main = tones (static_cast<int> (sampleRate * 1.2), sampleRate);
    const auto side = sine (static_cast<int> (main.size()), sampleRate, 1000.0, 0.9f);

    ToneDelta full {};
    {
        FrancSpaceAudioProcessor processor;
        setRaw (processor, ParamIDs::amount, 100.0f);
        const auto rendered = render (processor, main, &side, sampleRate, block);
        expect (allFinite (rendered.left), "carved tones are finite");
        expect (processor.getSidechainStatus() == FrancSpaceAudioProcessor::statusSidechain, "UI reports an active sidechain");
        full = measureTones (main, rendered, sampleRate);
        std::cout << "     amount 100%  100 Hz " << full.low << " dB, 1 kHz " << full.mid << " dB, 10 kHz " << full.high << " dB\n";
        expect (full.mid < -18.0 && full.mid > -30.0, "1 kHz is carved by about the full amount");
        expect (std::abs (full.low) < 1.5, "100 Hz barely moves");
        expect (std::abs (full.high) < 1.5, "10 kHz barely moves");
    }

    {
        FrancSpaceAudioProcessor processor;
        setRaw (processor, ParamIDs::amount, 0.0f);
        const auto rendered = render (processor, main, &side, sampleRate, block);
        const float error = maxAbsError (rendered.left, main, rendered.latency);
        std::cout << "     amount 0 max error " << error << "\n";
        expect (error < 1.0e-4f, "amount 0 stays transparent with a loud sidechain");
    }

    {
        FrancSpaceAudioProcessor processor;
        setRaw (processor, ParamIDs::amount, 50.0f);
        const auto rendered = render (processor, main, &side, sampleRate, block);
        const auto half = measureTones (main, rendered, sampleRate);
        std::cout << "     amount 50%   1 kHz " << half.mid << " dB\n";
        expect (half.mid < -8.0 && half.mid > -16.0, "amount 50% carves about half as deep");
        expect (half.mid > full.mid + 4.0, "amount 50% removes less than amount 100%");
    }

    {
        FrancSpaceAudioProcessor processor;
        setRaw (processor, ParamIDs::amount, 100.0f);
        setRaw (processor, ParamIDs::lowCut, 4000.0f);
        const auto rendered = render (processor, main, &side, sampleRate, block);
        const auto delta = measureTones (main, rendered, sampleRate);
        std::cout << "     low cut 4 kHz  1 kHz " << delta.mid << " dB\n";
        expect (std::abs (delta.mid) < 1.5, "low cut keeps 1 kHz out of the carve");
    }

    {
        FrancSpaceAudioProcessor processor;
        setRaw (processor, ParamIDs::amount, 100.0f);
        setRaw (processor, ParamIDs::highCut, 500.0f);
        const auto rendered = render (processor, main, &side, sampleRate, block);
        const auto delta = measureTones (main, rendered, sampleRate);
        std::cout << "     high cut 500 Hz  1 kHz " << delta.mid << " dB, 100 Hz " << delta.low << " dB\n";
        expect (std::abs (delta.mid) < 1.5, "high cut keeps 1 kHz out of the carve");
    }

    {
        FrancSpaceAudioProcessor processor;
        setRaw (processor, ParamIDs::amount, 100.0f);
        setRaw (processor, ParamIDs::bypass, 1.0f);
        const auto rendered = render (processor, main, &side, sampleRate, 128);
        const float error = maxAbsError (rendered.left, main, rendered.latency);
        std::cout << "     bypass max error " << error << "\n";
        expect (error < 1.0e-4f, "bypass ignores the carve and stays latency-aligned");
    }

    {
        FrancSpaceAudioProcessor processor;
        setRaw (processor, ParamIDs::amount, 100.0f);
        setRaw (processor, ParamIDs::mix, 0.0f);
        const auto rendered = render (processor, main, &side, sampleRate, 64);
        const float error = maxAbsError (rendered.left, main, rendered.latency);
        expect (error < 1.0e-4f, "mix 0 is the delayed dry signal");
    }

    {
        std::mt19937 rng (17);
        std::uniform_real_distribution<float> noise (-0.25f, 0.25f);
        std::vector<float> white (static_cast<size_t> (sampleRate * 1.1));
        for (float& sample : white)
            sample = noise (rng);
        const auto sideNoise = sine (static_cast<int> (white.size()), sampleRate, 1000.0, 0.9f);

        FrancSpaceAudioProcessor processor;
        setRaw (processor, ParamIDs::amount, 100.0f);
        const auto rendered = render (processor, white, &sideNoise, sampleRate, 512);
        const int start = rendered.latency + 8000;
        constexpr int count = 8192;
        const double low = dbOfRatio (bandLevel (rendered.left, start, count, 80.0, 140.0, sampleRate),
                                      bandLevel (white, start - rendered.latency, count, 80.0, 140.0, sampleRate));
        const double mid = dbOfRatio (bandLevel (rendered.left, start, count, 800.0, 1200.0, sampleRate),
                                      bandLevel (white, start - rendered.latency, count, 800.0, 1200.0, sampleRate));
        const double high = dbOfRatio (bandLevel (rendered.left, start, count, 8000.0, 12000.0, sampleRate),
                                       bandLevel (white, start - rendered.latency, count, 8000.0, 12000.0, sampleRate));
        std::cout << "     white noise  100 Hz " << low << " dB, 1 kHz " << mid << " dB, 10 kHz " << high << " dB\n";
        expect (allFinite (rendered.left), "white-noise carve is finite");
        expect (mid < -10.0, "white noise is attenuated around 1 kHz");
        expect (std::abs (low) < 3.0, "white noise around 100 Hz barely changes");
        expect (std::abs (high) < 3.0, "white noise around 10 kHz barely changes");
    }

    {
        FrancSpaceAudioProcessor processor;
        setRaw (processor, ParamIDs::detector, 1.0f);
        setRaw (processor, ParamIDs::amount, 100.0f);
        const auto input = sine (static_cast<int> (sampleRate), sampleRate, 1000.0, 0.8f);
        const auto rendered = render (processor, input, nullptr, sampleRate, block);
        const int start = rendered.latency + 8000;
        const double delta = dbOfRatio (goertzel (rendered.left, start, 16000, 1000.0, sampleRate),
                                        goertzel (input, start - rendered.latency, 16000, 1000.0, sampleRate));
        std::cout << "     internal 1 kHz " << delta << " dB\n";
        expect (processor.getSidechainStatus() == FrancSpaceAudioProcessor::statusInternal, "UI reports internal detector");
        expect (delta < -12.0, "internal mode carves the track against itself");
    }

    {
        FrancSpaceAudioProcessor processor;
        setRaw (processor, ParamIDs::amount, 37.5f);
        setRaw (processor, ParamIDs::lowCut, 180.0f);
        setRaw (processor, ParamIDs::attack, 12.0f);
        setRaw (processor, ParamIDs::mode, 2.0f);
        setRaw (processor, ParamIDs::detector, 1.0f);
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        setRaw (processor, ParamIDs::amount, 0.0f);
        setRaw (processor, ParamIDs::lowCut, 20.0f);
        setRaw (processor, ParamIDs::attack, 5.0f);
        setRaw (processor, ParamIDs::mode, 0.0f);
        setRaw (processor, ParamIDs::detector, 0.0f);
        processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        expect (std::abs (getRaw (processor, ParamIDs::amount) - 37.5f) < 0.05f, "state restore keeps amount");
        expect (std::abs (getRaw (processor, ParamIDs::lowCut) - 180.0f) < 1.0f, "state restore keeps low cut");
        expect (std::abs (getRaw (processor, ParamIDs::attack) - 12.0f) < 0.05f, "state restore keeps attack");
        expect (std::abs (getRaw (processor, ParamIDs::mode) - 2.0f) < 0.01f, "state restore keeps mode");
        expect (std::abs (getRaw (processor, ParamIDs::detector) - 1.0f) < 0.01f, "state restore keeps detector");
    }

    {
        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            for (int oddBlock : { 1, 7, 128, 1024 })
            {
                FrancSpaceAudioProcessor processor;
                setRaw (processor, ParamIDs::amount, 70.0f);
                const auto input = sine (3000, rate, 1000.0, 0.3f);
                const auto sidechain = sine (3000, rate, 1000.0, 0.6f);
                const auto rendered = render (processor, input, &sidechain, rate, oddBlock);
                expect (allFinite (rendered.left), "finite at " + std::to_string (static_cast<int> (rate)) + " Hz, block " + std::to_string (oddBlock));
            }
        }
    }

    {
        FrancSpaceAudioProcessor processor;
        processor.prepareToPlay (sampleRate, 128);
        juce::AudioBuffer<double> buffer (2, 128);
        for (int sample = 0; sample < 128; ++sample)
            buffer.setSample (0, sample, 0.2 * std::sin (0.1 * sample));
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
        bool finite = true;
        for (int sample = 0; sample < 128; ++sample)
            finite = finite && std::isfinite (buffer.getSample (0, sample));
        expect (finite, "double-precision processing is finite");
    }

    {
        FrancSpaceAudioProcessor processor;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.inputBuses.add (juce::AudioChannelSet::mono());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
        expect (processor.setBusesLayout (layout), "mono sidechain layout is accepted");
        setRaw (processor, ParamIDs::amount, 100.0f);
        processor.prepareToPlay (sampleRate, 256);
        const int count = static_cast<int> (sampleRate);
        juce::AudioBuffer<float> buffer (3, 256);
        juce::MidiBuffer midi;
        std::vector<float> output (static_cast<size_t> (count));
        const auto input = tones (count, sampleRate);
        for (int offset = 0; offset < count; offset += 256)
        {
            const int n = std::min (256, count - offset);
            buffer.clear();
            for (int sample = 0; sample < n; ++sample)
            {
                buffer.setSample (0, sample, input[static_cast<size_t> (offset + sample)]);
                buffer.setSample (1, sample, input[static_cast<size_t> (offset + sample)]);
                buffer.setSample (2, sample, 0.9f * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * (offset + sample) / sampleRate));
            }
            juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(), 3, n);
            processor.processBlock (slice, midi);
            for (int sample = 0; sample < n; ++sample)
                output[static_cast<size_t> (offset + sample)] = slice.getSample (0, sample);
        }
        const int latency = processor.getLatencySamples();
        const int start = latency + 16000;
        const double delta = dbOfRatio (goertzel (output, start, 16000, 1000.0, sampleRate),
                                        goertzel (input, start - latency, 16000, 1000.0, sampleRate));
        std::cout << "     mono sidechain 1 kHz " << delta << " dB\n";
        expect (delta < -18.0, "a mono sidechain still carves 1 kHz");
    }

    if (failures == 0)
        std::cout << "All sanity checks passed.\n";
    else
        std::cout << failures << " sanity check(s) failed.\n";

    return failures == 0 ? 0 : 1;
}

int writeSnapshot (FrancSpaceAudioProcessor& processor, const char* path, int width, int height)
{
    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());
    editor->setSize (width, height);
    if (auto* francSpace = dynamic_cast<FrancSpaceAudioProcessorEditor*> (editor.get()))
    {
        francSpace->refreshForSnapshot();
        francSpace->resized();
    }

    juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
    {
        juce::Graphics graphics (image);
        editor->paintEntireComponent (graphics, true);
    }

    juce::File output (path);
    output.getParentDirectory().createDirectory();
    if (auto stream = output.createOutputStream())
    {
        juce::PNGImageFormat png;
        if (png.writeImageToStream (image, *stream))
        {
            std::cout << "Wrote " << path << " (" << width << "x" << height << ")\n";
            return 0;
        }
    }

    std::cerr << "Could not write " << path << "\n";
    return 2;
}

int renderSnapshots (const char* widePath, const char* compactPath)
{
    juce::ScopedJuceInitialiser_GUI gui;

    {
        FrancSpaceAudioProcessor processor;
        setRaw (processor, ParamIDs::detector, 1.0f);
        setRaw (processor, ParamIDs::amount, 82.0f);
        setRaw (processor, ParamIDs::lowCut, 140.0f);
        setRaw (processor, ParamIDs::highCut, 8500.0f);
        setRaw (processor, ParamIDs::attack, 5.0f);
        setRaw (processor, ParamIDs::release, 80.0f);

        std::vector<float> input (static_cast<size_t> (48000));
        for (size_t sample = 0; sample < input.size(); ++sample)
        {
            const double time = static_cast<double> (sample) / 48000.0;
            input[sample] = 0.16f * std::sin (2.0 * juce::MathConstants<double>::pi * 140.0 * time)
                          + 0.22f * std::sin (2.0 * juce::MathConstants<double>::pi * 880.0 * time)
                          + 0.18f * std::sin (2.0 * juce::MathConstants<double>::pi * 2400.0 * time)
                          + 0.08f * std::sin (2.0 * juce::MathConstants<double>::pi * 6400.0 * time);
        }
        render (processor, input, nullptr, 48000.0, 256);
        if (writeSnapshot (processor, widePath, 1280, 800) != 0)
            return 2;
    }

    if (compactPath != nullptr)
    {
        FrancSpaceAudioProcessor processor;
        if (writeSnapshot (processor, compactPath, 980, 640) != 0)
            return 2;
    }

    return 0;
}

int main (int argc, char** argv)
{
    const int result = runTests();
    const char* wide = nullptr;
    const char* compact = nullptr;
    for (int index = 1; index < argc; ++index)
    {
        if (std::strcmp (argv[index], "--snapshot") == 0 && index + 1 < argc)
            wide = argv[++index];
        else if (std::strcmp (argv[index], "--snapshot-compact") == 0 && index + 1 < argc)
            compact = argv[++index];
    }

    if (wide != nullptr)
        return renderSnapshots (wide, compact) == 0 ? result : 2;

    return result;
}
