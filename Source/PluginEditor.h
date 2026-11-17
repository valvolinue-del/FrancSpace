#pragma once

#include "Palette.h"
#include "PluginProcessor.h"
#include "SpaceDisplay.h"

#include <cmath>
#include <memory>

class FrancSlider : public juce::Slider
{
public:
    FrancSlider()
    {
        setVelocityModeParameters (1.0, 1, 0.0, false);
        setScrollWheelEnabled (true);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        setMouseDragSensitivity (fine (event) ? 1600 : 220);
        juce::Slider::mouseDown (event);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        setMouseDragSensitivity (fine (event) ? 1600 : 220);
        juce::Slider::mouseDrag (event);
    }

    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        const float raw = (std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? -wheel.deltaX : wheel.deltaY)
                        * (wheel.isReversed ? -1.0f : 1.0f);
        if (std::abs (raw) < 1.0e-6f || ! isEnabled() || event.mods.isAnyMouseButtonDown())
            return;

        const bool precise = fine (event);
        const double step = (precise ? 0.004 : 0.02) * static_cast<double> (raw);
        const double position = juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + step);
        startedDragging();
        setValue (proportionOfLengthToValue (position), juce::sendNotificationSync);
        stoppedDragging();
    }

private:
    static bool fine (const juce::MouseEvent& event)
    {
        return event.mods.isShiftDown() || event.mods.isCommandDown();
    }
};

class FrancLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FrancLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

class FrancSpaceAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit FrancSpaceAudioProcessorEditor (FrancSpaceAudioProcessor&);
    ~FrancSpaceAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshForSnapshot();

private:
    void timerCallback() override;
    void configureKnob (juce::Slider& slider, int textBoxWidth);
    void configureLabel (juce::Label& label, const juce::String& text);
    void configureChoiceButton (juce::TextButton& button);
    void syncChoiceButtons();

    FrancSpaceAudioProcessor& processor;
    FrancLookAndFeel lookAndFeel;
    SpaceDisplay display;
    juce::TooltipWindow tooltipWindow;

    juce::Label amountLabel;
    juce::Label lowLabel;
    juce::Label highLabel;
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label sidechainLabel;
    juce::Label outputLabel;
    juce::Label mixLabel;
    juce::Label modeLabel;
    juce::Label detectorLabel;
    juce::Label bypassLabel;

    FrancSlider amountSlider;
    FrancSlider lowSlider;
    FrancSlider highSlider;
    FrancSlider attackSlider;
    FrancSlider releaseSlider;
    FrancSlider sidechainSlider;
    FrancSlider outputSlider;
    FrancSlider mixSlider;
    juce::TextButton modeButtons[3];
    juce::TextButton externalButton;
    juce::TextButton internalButton;
    juce::ToggleButton bypassButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sidechainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    juce::Rectangle<int> titleBounds;
    juce::Rectangle<int> controlsBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrancSpaceAudioProcessorEditor)
};
