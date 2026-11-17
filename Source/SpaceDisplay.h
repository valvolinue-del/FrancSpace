#pragma once

#include "PluginProcessor.h"

// Lock-free spectrum and carve. The audio thread publishes atomics; this
// component only reads them and edits parameters from the message thread.
class SpaceDisplay : public juce::Component,
                     private juce::Timer
{
public:
    explicit SpaceDisplay (FrancSpaceAudioProcessor&);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void snapToTarget();

private:
    void timerCallback() override;
    juce::Rectangle<float> plotArea() const;
    float frequencyToX (float frequency, juce::Rectangle<float> area) const;
    float xToFrequency (float x, juce::Rectangle<float> area) const;
    void pullTargets();
    int handleAt (juce::Point<float> position) const;
    void beginEdit (int handle);
    void endEdit();
    void setCut (int handle, float frequency);
    float cutValue (int handle) const;

    FrancSpaceAudioProcessor& processor;
    float spectrum[FrancSpaceAudioProcessor::numBands] {};
    float reduction[FrancSpaceAudioProcessor::numBands] {};
    float targetSpectrum[FrancSpaceAudioProcessor::numBands] {};
    float targetReduction[FrancSpaceAudioProcessor::numBands] {};

    int hoverHandle = -1;
    int dragHandle = -1;
    float dragAnchorX = 0.0f;
    float dragAnchorFrequency = 20.0f;
    bool gestureOpen = false;
    juce::RangedAudioParameter* dragParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceDisplay)
};
