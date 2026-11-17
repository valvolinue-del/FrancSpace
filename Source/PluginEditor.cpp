#include "PluginEditor.h"

#include <cmath>

namespace
{
void drawTrackedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                      const juce::Font& font, juce::Colour colour, float tracking)
{
    float width = 0.0f;
    for (int index = 0; index < text.length(); ++index)
        width += juce::GlyphArrangement::getStringWidth (font, text.substring (index, index + 1));
    if (text.length() > 1)
        width += tracking * static_cast<float> (text.length() - 1);

    float x = area.getCentreX() - width * 0.5f;
    g.setFont (font);
    g.setColour (colour);
    for (int index = 0; index < text.length(); ++index)
    {
        const auto glyph = text.substring (index, index + 1);
        const float glyphWidth = juce::GlyphArrangement::getStringWidth (font, glyph);
        g.drawText (glyph, juce::Rectangle<float> (x, area.getY(), glyphWidth + tracking + 2.0f, area.getHeight()),
                    juce::Justification::centredLeft, false);
        x += glyphWidth + tracking;
    }
}
} // namespace

FrancLookAndFeel::FrancLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, franc::ivory);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, franc::gold.withAlpha (0.28f));
    setColour (juce::Slider::rotarySliderFillColourId, franc::gold);
    setColour (juce::Slider::rotarySliderOutlineColourId, franc::track);
    setColour (juce::Slider::thumbColourId, franc::goldBright);
    setColour (juce::Label::textColourId, franc::gold);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ResizableWindow::backgroundColourId, franc::background);
    setColour (juce::TextEditor::textColourId, franc::ivory);
    setColour (juce::TextEditor::backgroundColourId, franc::panelRaised);
    setColour (juce::TextEditor::outlineColourId, franc::gold);
    setColour (juce::TextEditor::focusedOutlineColourId, franc::goldBright);
    setColour (juce::TextEditor::highlightColourId, franc::gold.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, franc::gold);
    setColour (juce::TooltipWindow::backgroundColourId, franc::panelRaised);
    setColour (juce::TooltipWindow::textColourId, franc::ivory);
    setColour (juce::TooltipWindow::outlineColourId, franc::gold);
    setColour (juce::ComboBox::backgroundColourId, franc::panelRaised);
    setColour (juce::ComboBox::textColourId, franc::ivory);
    setColour (juce::ComboBox::outlineColourId, franc::gold);
    setColour (juce::ComboBox::arrowColourId, franc::gold);
    setColour (juce::PopupMenu::backgroundColourId, franc::panelRaised);
    setColour (juce::PopupMenu::textColourId, franc::ivory);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, franc::gold);
    setColour (juce::PopupMenu::highlightedTextColourId, franc::background);
    setColour (juce::TextButton::buttonColourId, franc::panelRaised);
    setColour (juce::TextButton::buttonOnColourId, franc::gold);
    setColour (juce::TextButton::textColourOffId, franc::gold);
    setColour (juce::TextButton::textColourOnId, franc::background);
}

void FrancLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                          static_cast<float> (width), static_cast<float> (height))
                      .reduced (6.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float arcRadius = radius - 5.0f;

    juce::Path trackPath;
    trackPath.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (franc::track);
    g.strokePath (trackPath, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valuePath;
    valuePath.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (franc::gold.withAlpha (0.28f));
    g.strokePath (valuePath, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (franc::gold);
    g.strokePath (valuePath, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobRadius = radius * 0.68f;
    juce::ColourGradient body (juce::Colour::fromRGB (0x2c, 0x2a, 0x32), centre.x, centre.y - knobRadius * 0.6f,
                               juce::Colour::fromRGB (0x0c, 0x0c, 0x10), centre.x, centre.y + knobRadius, true);
    g.setGradientFill (body);
    g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillEllipse (centre.x - knobRadius * 0.55f, centre.y - knobRadius * 0.72f, knobRadius * 1.05f, knobRadius * 0.48f);

    g.setColour (franc::gold.withAlpha (0.85f));
    g.drawEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.15f);

    juce::Path pointer;
    const float pointerLength = knobRadius * 0.72f;
    pointer.addRoundedRectangle (-1.15f, -pointerLength, 2.3f, pointerLength * 0.58f, 1.0f);
    g.setColour (franc::goldBright);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void FrancLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (2.0f, 1.0f);
    auto pillRow = bounds.removeFromTop (std::min (34.0f, bounds.getHeight() * 0.70f));
    auto pill = juce::Rectangle<float> (48.0f, 24.0f).withCentre (pillRow.getCentre());
    const bool on = button.getToggleState();

    g.setColour (on ? franc::gold : franc::track);
    g.fillRoundedRectangle (pill, 12.0f);
    g.setColour (franc::gold.withAlpha (on || highlighted ? 1.0f : 0.45f));
    g.drawRoundedRectangle (pill, 12.0f, highlighted || down ? 1.8f : 1.15f);

    auto thumb = pill.reduced (3.0f).withWidth (18.0f);
    if (on)
        thumb.setX (pill.getRight() - thumb.getWidth() - 3.0f);
    g.setColour (on ? franc::background : franc::gold);
    g.fillEllipse (thumb);

    if (bounds.getHeight() > 8.0f)
    {
        g.setColour (on ? franc::goldBright : franc::muted);
        g.setFont (juce::Font (juce::FontOptions (11.0f).withKerningFactor (0.08f)));
        g.drawText (on ? "ON" : "OFF", bounds, juce::Justification::centred, false);
    }
}

void FrancLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = button.getToggleState();
    g.setColour (on ? franc::gold : franc::panelRaised);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (franc::gold.withAlpha (on || highlighted ? 1.0f : 0.40f));
    g.drawRoundedRectangle (bounds, 7.0f, on || down ? 1.6f : 1.0f);
}

void FrancLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height)).reduced (1.0f);
    g.setColour (franc::panelRaised);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (franc::gold.withAlpha (box.isMouseOver() ? 0.95f : 0.45f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    juce::Path arrow;
    const float cx = bounds.getRight() - 14.0f;
    const float cy = bounds.getCentreY();
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 3.0f);
    arrow.closeSubPath();
    g.setColour (franc::gold);
    g.fillPath (arrow);
}

void FrancLookAndFeel::drawCornerResizer (juce::Graphics& g, int width, int height, bool, bool)
{
    g.setColour (franc::gold.withAlpha (0.55f));
    const float right = static_cast<float> (width);
    const float bottom = static_cast<float> (height);
    for (int index = 0; index < 3; ++index)
    {
        const float offset = static_cast<float> (index * 4 + 4);
        g.drawLine (right - offset - 8.0f, bottom - 3.0f, right - 3.0f, bottom - offset - 8.0f, 1.1f);
    }
}

juce::Label* FrancLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::Font (juce::FontOptions (12.5f)));
    label->setJustificationType (juce::Justification::centred);
    label->setColour (juce::Label::textColourId, franc::ivory);
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return label;
}

juce::Font FrancLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (12.5f));
}

juce::Font FrancLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions (12.0f).withKerningFactor (0.08f));
}

FrancSpaceAudioProcessorEditor::FrancSpaceAudioProcessorEditor (FrancSpaceAudioProcessor& processorIn)
    : AudioProcessorEditor (processorIn),
      processor (processorIn),
      display (processorIn)
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (false);

    addAndMakeVisible (display);

    configureLabel (amountLabel, "AMOUNT");
    configureLabel (lowLabel, "LOW CUT");
    configureLabel (highLabel, "HIGH CUT");
    configureLabel (attackLabel, "ATTACK");
    configureLabel (releaseLabel, "RELEASE");
    configureLabel (sidechainLabel, "SC GAIN");
    configureLabel (outputLabel, "OUTPUT");
    configureLabel (mixLabel, "MIX");
    configureLabel (modeLabel, "MODE");
    configureLabel (detectorLabel, "DETECTOR");
    configureLabel (bypassLabel, "BYPASS");

    configureKnob (amountSlider, 88);
    configureKnob (lowSlider, 76);
    configureKnob (highSlider, 76);
    configureKnob (attackSlider, 72);
    configureKnob (releaseSlider, 72);
    configureKnob (sidechainSlider, 72);
    configureKnob (outputSlider, 72);
    configureKnob (mixSlider, 72);

    amountSlider.setTooltip ("Depth of the carve. 100% is about -24 dB where the sidechain is loud. Shift or Cmd drags finer. Double-click resets. Scroll to nudge.");
    lowSlider.setTooltip ("Ignore sidechain energy below this frequency. Drag the LOW handle on the display, Shift or Cmd for a finer move.");
    highSlider.setTooltip ("Ignore sidechain energy above this frequency. Drag the HIGH handle on the display.");
    attackSlider.setTooltip ("How fast a band starts carving. Default is 5 ms.");
    releaseSlider.setTooltip ("How fast a band lets go. Default is 80 ms.");
    sidechainSlider.setTooltip ("Trim the sidechain before it is analysed. Raise it if the source is quiet.");
    outputSlider.setTooltip ("Gain after the carve and the mix.");
    mixSlider.setTooltip ("Dry/wet mix. 100% is the full carve. The dry path is delayed to match the spectral latency.");

    const char* modeNames[] = { "LINKED", "L / R", "MID / SIDE" };
    const char* modeTips[] = {
        "One detector for both channels, so the stereo image stays put.",
        "Left and right carve independently.",
        "Carve mid and side with separate detectors.",
    };
    for (int index = 0; index < 3; ++index)
    {
        modeButtons[index].setButtonText (modeNames[index]);
        modeButtons[index].setTooltip (modeTips[index]);
        modeButtons[index].setClickingTogglesState (false);
        configureChoiceButton (modeButtons[index]);
        modeButtons[index].onClick = [this, index]
        {
            if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (ParamIDs::mode)))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (index)));
                parameter->endChangeGesture();
            }
            syncChoiceButtons();
        };
    }

    externalButton.setButtonText ("EXT");
    internalButton.setButtonText ("INT");
    externalButton.setTooltip ("Listen to the sidechain input. With nothing routed, the plug-in stays transparent.");
    internalButton.setTooltip ("Use this track as its own sidechain, so Standalone can still show the carve.");
    externalButton.setClickingTogglesState (false);
    internalButton.setClickingTogglesState (false);
    configureChoiceButton (externalButton);
    configureChoiceButton (internalButton);
    externalButton.onClick = [this]
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (ParamIDs::detector)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (0.0f));
            parameter->endChangeGesture();
        }
        syncChoiceButtons();
    };
    internalButton.onClick = [this]
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (ParamIDs::detector)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (1.0f));
            parameter->endChangeGesture();
        }
        syncChoiceButtons();
    };

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass the carve, latency-aligned, so the host stays in time.");
    addAndMakeVisible (bypassButton);

    auto& state = processor.apvts;
    amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, ParamIDs::amount, amountSlider);
    lowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, ParamIDs::lowCut, lowSlider);
    highAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, ParamIDs::highCut, highSlider);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, ParamIDs::attack, attackSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, ParamIDs::release, releaseSlider);
    sidechainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, ParamIDs::sidechainGain, sidechainSlider);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, ParamIDs::outputGain, outputSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, ParamIDs::mix, mixSlider);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, ParamIDs::bypass, bypassButton);

    for (auto* slider : { &amountSlider, &lowSlider, &highSlider, &attackSlider, &releaseSlider,
                          &sidechainSlider, &outputSlider, &mixSlider })
        slider->setPopupDisplayEnabled (true, false, this);

    syncChoiceButtons();
    startTimerHz (30);

    setResizable (true, true);
    setResizeLimits (980, 640, 1800, 1120);
    setSize (1200, 760);
}

FrancSpaceAudioProcessorEditor::~FrancSpaceAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void FrancSpaceAudioProcessorEditor::configureKnob (juce::Slider& slider, int textBoxWidth)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxWidth, 18);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                juce::MathConstants<float>::pi * 2.8f,
                                true);
    slider.setColour (juce::Slider::textBoxTextColourId, franc::ivory);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);
}

void FrancSpaceAudioProcessorEditor::configureLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (11.0f).withKerningFactor (0.12f)));
    label.setColour (juce::Label::textColourId, franc::gold);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void FrancSpaceAudioProcessorEditor::configureChoiceButton (juce::TextButton& button)
{
    button.setColour (juce::TextButton::buttonColourId, franc::panelRaised);
    button.setColour (juce::TextButton::buttonOnColourId, franc::gold);
    button.setColour (juce::TextButton::textColourOffId, franc::gold);
    button.setColour (juce::TextButton::textColourOnId, franc::background);
    addAndMakeVisible (button);
}

void FrancSpaceAudioProcessorEditor::syncChoiceButtons()
{
    const int mode = juce::jlimit (0, 2, juce::roundToInt (processor.apvts.getRawParameterValue (ParamIDs::mode)->load()));
    for (int index = 0; index < 3; ++index)
        modeButtons[index].setToggleState (index == mode, juce::dontSendNotification);

    const bool internal = processor.apvts.getRawParameterValue (ParamIDs::detector)->load() >= 0.5f;
    externalButton.setToggleState (! internal, juce::dontSendNotification);
    internalButton.setToggleState (internal, juce::dontSendNotification);
}

void FrancSpaceAudioProcessorEditor::timerCallback()
{
    syncChoiceButtons();
}

void FrancSpaceAudioProcessorEditor::refreshForSnapshot()
{
    syncChoiceButtons();
    display.snapToTarget();
    resized();
}

void FrancSpaceAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (franc::background);

    juce::ColourGradient glow (franc::gold.withAlpha (0.09f), static_cast<float> (getWidth()) * 0.5f, 28.0f,
                               franc::background, static_cast<float> (getWidth()) * 0.5f, 260.0f, true);
    g.setGradientFill (glow);
    g.fillRect (getLocalBounds());

    if (! titleBounds.isEmpty())
    {
        auto title = titleBounds.toFloat();
        auto rule = title.removeFromTop (16.0f);
        const float centreX = rule.getCentreX();
        g.setColour (franc::gold.withAlpha (0.75f));
        g.drawLine (rule.getX() + 40.0f, rule.getCentreY(), centreX - 14.0f, rule.getCentreY(), 1.0f);
        g.drawLine (centreX + 14.0f, rule.getCentreY(), rule.getRight() - 40.0f, rule.getCentreY(), 1.0f);

        juce::Path diamond;
        const float y = rule.getCentreY();
        diamond.startNewSubPath (centreX, y - 4.0f);
        diamond.lineTo (centreX + 4.0f, y);
        diamond.lineTo (centreX, y + 4.0f);
        diamond.lineTo (centreX - 4.0f, y);
        diamond.closeSubPath();
        g.setColour (franc::gold);
        g.fillPath (diamond);

        const juce::Font titleFont (juce::FontOptions (34.0f, juce::Font::bold).withKerningFactor (0.045f));
        drawTrackedText (g, "FrancSpace", title.removeFromTop (40.0f), titleFont, franc::gold, 1.4f);

        const juce::Font subtitleFont (juce::FontOptions (11.0f).withKerningFactor (0.18f));
        drawTrackedText (g, "SPECTRAL SPACE", title, subtitleFont, franc::muted, 2.2f);

        const int status = processor.getSidechainStatus();
        const bool live = status != FrancSpaceAudioProcessor::statusNone;
        const char* statusLabel = status == FrancSpaceAudioProcessor::statusInternal ? "INTERNAL"
                                : status == FrancSpaceAudioProcessor::statusSidechain ? "SIDECHAIN"
                                                                                       : "NO SIDECHAIN";
        auto badge = juce::Rectangle<float> (static_cast<float> (getWidth()) - 168.0f, 18.0f, 148.0f, 24.0f);
        g.setColour (franc::panelRaised);
        g.fillRoundedRectangle (badge, 12.0f);
        g.setColour (live ? franc::gold : franc::muted);
        g.drawRoundedRectangle (badge, 12.0f, 1.4f);
        g.setFont (juce::Font (juce::FontOptions (11.0f).withKerningFactor (0.12f)));
        g.drawText (statusLabel, badge, juce::Justification::centred, false);
    }

    if (! controlsBounds.isEmpty())
    {
        auto panelBounds = controlsBounds.toFloat();
        g.setColour (franc::panel);
        g.fillRoundedRectangle (panelBounds, 16.0f);
        g.setColour (franc::gold.withAlpha (0.20f));
        g.drawRoundedRectangle (panelBounds.reduced (0.5f), 16.0f, 1.0f);
    }
}

void FrancSpaceAudioProcessorEditor::resized()
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    auto area = getLocalBounds().reduced (16, 12);
    titleBounds = area.removeFromTop (juce::jlimit (58, 74, getHeight() / 11));
    area.removeFromTop (6);

    const int controlHeight = juce::jlimit (188, 250, getHeight() / 4 + 24);
    controlsBounds = area.removeFromBottom (controlHeight);
    area.removeFromBottom (8);
    display.setBounds (area);

    auto controls = controlsBounds.reduced (12, 10);
    auto hero = controls.removeFromLeft (juce::jlimit (124, 176, controls.getWidth() / 7));
    controls.removeFromLeft (6);
    auto side = controls.removeFromRight (juce::jlimit (156, 196, getWidth() / 6));
    controls.removeFromRight (6);

    amountLabel.setBounds (hero.removeFromTop (16));
    amountSlider.setBounds (hero);

    juce::Slider* knobs[] = { &lowSlider, &highSlider, &attackSlider, &releaseSlider,
                              &sidechainSlider, &outputSlider, &mixSlider };
    juce::Label* labels[] = { &lowLabel, &highLabel, &attackLabel, &releaseLabel,
                              &sidechainLabel, &outputLabel, &mixLabel };
    for (int index = 0; index < 7; ++index)
    {
        auto cell = controls.removeFromLeft (controls.getWidth() / (7 - index));
        labels[index]->setBounds (cell.removeFromTop (16));
        knobs[index]->setBounds (cell.reduced (2, 0));
    }

    auto column = side;
    const int buttonHeight = juce::jlimit (24, 32, (column.getHeight() - 78) / 5);
    modeLabel.setBounds (column.removeFromTop (15));
    for (auto& button : modeButtons)
        button.setBounds (column.removeFromTop (buttonHeight).reduced (1, 2));

    column.removeFromTop (4);
    detectorLabel.setBounds (column.removeFromTop (15));
    auto detectorRow = column.removeFromTop (buttonHeight);
    externalButton.setBounds (detectorRow.removeFromLeft (detectorRow.getWidth() / 2).reduced (1, 2));
    internalButton.setBounds (detectorRow.reduced (1, 2));

    column.removeFromTop (4);
    bypassLabel.setBounds (column.removeFromTop (14));
    bypassButton.setBounds (column);
}
