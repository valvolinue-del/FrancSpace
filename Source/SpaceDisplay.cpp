#include "SpaceDisplay.h"

#include "Palette.h"

#include <cmath>

namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float maxCarveDb = 24.0f;

juce::String frequencyText (float frequency)
{
    if (frequency >= 1000.0f)
        return juce::String (frequency / 1000.0f, frequency >= 10000.0f ? 0 : 2) + " kHz";
    return juce::String (frequency, frequency < 100.0f ? 1 : 0) + " Hz";
}
} // namespace

SpaceDisplay::SpaceDisplay (FrancSpaceAudioProcessor& processorIn)
    : processor (processorIn)
{
    setOpaque (false);
    startTimerHz (60);
}

void SpaceDisplay::timerCallback()
{
    pullTargets();
    for (int band = 0; band < FrancSpaceAudioProcessor::numBands; ++band)
    {
        spectrum[band] += 0.42f * (targetSpectrum[band] - spectrum[band]);
        reduction[band] += 0.42f * (targetReduction[band] - reduction[band]);
    }
    repaint();
}

void SpaceDisplay::snapToTarget()
{
    pullTargets();
    for (int band = 0; band < FrancSpaceAudioProcessor::numBands; ++band)
    {
        spectrum[band] = targetSpectrum[band];
        reduction[band] = targetReduction[band];
    }
    repaint();
}

void SpaceDisplay::pullTargets()
{
    for (int band = 0; band < FrancSpaceAudioProcessor::numBands; ++band)
    {
        targetSpectrum[band] = processor.getSpectrum (band);
        targetReduction[band] = processor.getReductionDb (band);
    }
}

juce::Rectangle<float> SpaceDisplay::plotArea() const
{
    return getLocalBounds().toFloat().reduced (18.0f, 14.0f).withTrimmedLeft (36.0f).withTrimmedBottom (22.0f).withTrimmedTop (18.0f);
}

float SpaceDisplay::frequencyToX (float frequency, juce::Rectangle<float> area) const
{
    const float clamped = juce::jlimit (minFrequency, maxFrequency, frequency);
    const float norm = std::log (clamped / minFrequency) / std::log (maxFrequency / minFrequency);
    return area.getX() + norm * area.getWidth();
}

float SpaceDisplay::xToFrequency (float x, juce::Rectangle<float> area) const
{
    const float norm = juce::jlimit (0.0f, 1.0f, (x - area.getX()) / std::max (1.0f, area.getWidth()));
    return minFrequency * std::pow (maxFrequency / minFrequency, norm);
}

float SpaceDisplay::cutValue (int handle) const
{
    const char* id = handle == 0 ? ParamIDs::lowCut : ParamIDs::highCut;
    return processor.apvts.getRawParameterValue (id)->load();
}

int SpaceDisplay::handleAt (juce::Point<float> position) const
{
    auto area = plotArea();
    if (area.isEmpty())
        return -1;

    const float lowX = frequencyToX (cutValue (0), area);
    const float highX = frequencyToX (cutValue (1), area);
    const float lowDistance = std::abs (position.x - lowX);
    const float highDistance = std::abs (position.x - highX);
    const bool inPlot = position.y >= area.getY() - 8.0f && position.y <= area.getBottom() + 16.0f;

    if (! inPlot)
        return -1;

    if (lowDistance < 18.0f && lowDistance <= highDistance)
        return 0;
    if (highDistance < 18.0f)
        return 1;
    if (position.x < lowX)
        return 0;
    if (position.x > highX)
        return 1;
    return -1;
}

void SpaceDisplay::beginEdit (int handle)
{
    endEdit();
    const char* id = handle == 0 ? ParamIDs::lowCut : ParamIDs::highCut;
    dragParameter = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (id));
    if (dragParameter != nullptr)
    {
        dragParameter->beginChangeGesture();
        gestureOpen = true;
    }
}

void SpaceDisplay::endEdit()
{
    if (gestureOpen && dragParameter != nullptr)
        dragParameter->endChangeGesture();
    gestureOpen = false;
    dragParameter = nullptr;
}

void SpaceDisplay::setCut (int handle, float frequency)
{
    const float other = cutValue (handle == 0 ? 1 : 0);
    float value = juce::jlimit (minFrequency, maxFrequency, frequency);
    if (handle == 0)
        value = std::min (value, other / 1.12f);
    else
        value = std::max (value, other * 1.12f);

    if (dragParameter == nullptr)
        return;

    dragParameter->setValueNotifyingHost (dragParameter->convertTo0to1 (value));
}

void SpaceDisplay::mouseMove (const juce::MouseEvent& event)
{
    const int handle = handleAt (event.position);
    if (handle != hoverHandle)
    {
        hoverHandle = handle;
        setMouseCursor (handle >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SpaceDisplay::mouseDown (const juce::MouseEvent& event)
{
    dragHandle = handleAt (event.position);
    hoverHandle = dragHandle;
    if (dragHandle < 0)
        return;

    dragAnchorX = event.position.x;
    dragAnchorFrequency = cutValue (dragHandle);
    beginEdit (dragHandle);
}

void SpaceDisplay::mouseDrag (const juce::MouseEvent& event)
{
    if (dragHandle < 0)
        return;

    auto area = plotArea();
    float x = event.position.x;
    if (event.mods.isShiftDown() || event.mods.isCommandDown())
        x = dragAnchorX + (event.position.x - dragAnchorX) * 0.15f;

    setCut (dragHandle, xToFrequency (x, area));
}

void SpaceDisplay::mouseUp (const juce::MouseEvent&)
{
    endEdit();
    dragHandle = -1;
}

void SpaceDisplay::mouseDoubleClick (const juce::MouseEvent& event)
{
    const int handle = handleAt (event.position);
    if (handle < 0)
        return;

    beginEdit (handle);
    setCut (handle, handle == 0 ? minFrequency : maxFrequency);
    endEdit();
}

void SpaceDisplay::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const int handle = dragHandle >= 0 ? dragHandle : handleAt (event.position);
    if (handle < 0)
        return;

    const float delta = (std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? -wheel.deltaX : wheel.deltaY)
                      * (wheel.isReversed ? -1.0f : 1.0f);
    if (std::abs (delta) < 1.0e-6f)
        return;

    const bool fine = event.mods.isShiftDown() || event.mods.isCommandDown();
    const float scale = std::pow (fine ? 1.01f : 1.06f, delta * 8.0f);
    beginEdit (handle);
    setCut (handle, cutValue (handle) * scale);
    endEdit();
}

void SpaceDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (franc::panel);
    g.fillRoundedRectangle (bounds, 16.0f);
    g.setColour (franc::gold.withAlpha (0.22f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 16.0f, 1.0f);

    auto area = plotArea();
    if (area.isEmpty())
        return;

    g.setColour (juce::Colour::fromRGB (0x0c, 0x0c, 0x12));
    g.fillRoundedRectangle (area.expanded (8.0f, 6.0f), 10.0f);

    const float lowCut = cutValue (0);
    const float highCut = cutValue (1);
    const float lowX = frequencyToX (lowCut, area);
    const float highX = frequencyToX (highCut, area);

    auto shade = [&g] (juce::Rectangle<float> region)
    {
        if (region.getWidth() <= 1.0f)
            return;
        g.setColour (juce::Colour::fromRGB (0x05, 0x05, 0x08).withAlpha (0.72f));
        g.fillRect (region);
        g.setColour (franc::gold.withAlpha (0.07f));
        g.fillRect (region);
    };

    const float gridDb[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f };
    auto dbToY = [&area] (float db)
    {
        const float norm = juce::jlimit (0.0f, 1.0f, -db / maxCarveDb);
        return area.getY() + 8.0f + norm * (area.getHeight() - 16.0f);
    };

    g.setColour (franc::track.withAlpha (0.85f));
    const float gridHz[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    for (float frequency : gridHz)
    {
        const float x = frequencyToX (frequency, area);
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
    }
    for (float db : gridDb)
        g.drawHorizontalLine (juce::roundToInt (dbToY (db)), area.getX(), area.getRight());

    g.setColour (franc::muted.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    for (float db : gridDb)
    {
        const auto text = juce::String (juce::roundToInt (db));
        g.drawText (text, juce::Rectangle<float> (4.0f, dbToY (db) - 7.0f, 32.0f, 14.0f), juce::Justification::centredRight, false);
    }

    const char* hzLabel[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    for (int index = 0; index < 8; ++index)
    {
        const float x = frequencyToX (gridHz[index], area);
        g.drawText (hzLabel[index], juce::Rectangle<float> (x - 22.0f, area.getBottom() + 4.0f, 44.0f, 14.0f),
                    juce::Justification::centred, false);
    }

    const int bands = FrancSpaceAudioProcessor::numBands;
    for (int band = 0; band < bands; ++band)
    {
        const float frequency = processor.bandFrequency (band);
        const float x = frequencyToX (frequency, area);
        const float level = juce::jlimit (0.0f, 1.35f, spectrum[band]);
        const float height = std::sqrt (std::min (level, 1.0f)) * area.getHeight() * 0.72f;
        const float width = std::max (3.0f, area.getWidth() / 46.0f);
        juce::Rectangle<float> bar (x - width * 0.5f, area.getBottom() - height, width, height);
        g.setColour (franc::champagne.withAlpha (0.16f + 0.22f * std::min (level, 1.0f)));
        g.fillRoundedRectangle (bar, 1.5f);
    }

    juce::Path carve;
    juce::Path stroke;
    for (int band = 0; band < bands; ++band)
    {
        const float x = frequencyToX (processor.bandFrequency (band), area);
        const float y = dbToY (juce::jlimit (-maxCarveDb, 0.0f, reduction[band]));
        if (band == 0)
        {
            carve.startNewSubPath (area.getX(), dbToY (0.0f));
            carve.lineTo (x, y);
            stroke.startNewSubPath (x, y);
        }
        else
        {
            carve.lineTo (x, y);
            stroke.lineTo (x, y);
        }
    }
    carve.lineTo (area.getRight(), dbToY (0.0f));
    carve.closeSubPath();

    juce::ColourGradient fill (franc::gold.withAlpha (0.40f), area.getCentreX(), area.getY(),
                               franc::gold.withAlpha (0.05f), area.getCentreX(), area.getBottom(), false);
    g.setGradientFill (fill);
    g.fillPath (carve);

    g.setColour (franc::gold.withAlpha (0.28f));
    g.strokePath (stroke, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (franc::goldBright);
    g.strokePath (stroke, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    shade ({ area.getX(), area.getY(), std::max (0.0f, lowX - area.getX()), area.getHeight() });
    shade ({ highX, area.getY(), std::max (0.0f, area.getRight() - highX), area.getHeight() });

    auto drawHandle = [&] (float x, bool active, const juce::String& name)
    {
        g.setColour (active ? franc::goldBright : franc::gold.withAlpha (0.85f));
        g.drawLine (x, area.getY(), x, area.getBottom(), active ? 2.0f : 1.3f);

        juce::Path diamond;
        const float y = area.getY() + 2.0f;
        diamond.startNewSubPath (x, y - 6.0f);
        diamond.lineTo (x + 5.5f, y);
        diamond.lineTo (x, y + 6.0f);
        diamond.lineTo (x - 5.5f, y);
        diamond.closeSubPath();
        g.setColour (active ? franc::goldBright : franc::gold);
        g.fillPath (diamond);

        g.setFont (juce::Font (juce::FontOptions (10.0f).withKerningFactor (0.08f)));
        g.setColour (franc::ivory);
        auto caption = juce::Rectangle<float> (44.0f, 12.0f).withCentre ({ x, area.getY() + 20.0f });
        caption.setX (juce::jlimit (area.getX(), area.getRight() - caption.getWidth(), caption.getX()));
        g.drawText (name, caption, juce::Justification::centred, false);
    };

    drawHandle (lowX, hoverHandle == 0 || dragHandle == 0, "LOW");
    drawHandle (highX, hoverHandle == 1 || dragHandle == 1, "HIGH");

    if (dragHandle >= 0)
    {
        const float frequency = cutValue (dragHandle);
        const float x = dragHandle == 0 ? lowX : highX;
        const auto text = frequencyText (frequency);
        auto bubble = juce::Rectangle<float> (86.0f, 22.0f).withCentre ({ x, area.getY() - 16.0f });
        bubble = bubble.constrainedWithin (getLocalBounds().toFloat().reduced (4.0f));
        g.setColour (franc::panelRaised);
        g.fillRoundedRectangle (bubble, 5.0f);
        g.setColour (franc::gold);
        g.drawRoundedRectangle (bubble, 5.0f, 1.0f);
        g.setColour (franc::ivory);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText (text, bubble, juce::Justification::centred, false);
    }

    const int status = processor.getSidechainStatus();
    const bool live = status != FrancSpaceAudioProcessor::statusNone;

    if (! live)
    {
        g.setColour (franc::ivory.withAlpha (0.88f));
        g.setFont (juce::Font (juce::FontOptions (22.0f).withKerningFactor (0.12f)));
        g.drawText ("No sidechain", area.translated (0.0f, -8.0f), juce::Justification::centred, false);
        g.setColour (franc::muted);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("Route a sidechain, or switch the detector to Internal.",
                    area.translated (0.0f, 18.0f), juce::Justification::centred, false);
    }
}
