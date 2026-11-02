#include "LookAndFeel.h"

#include <algorithm>
#include <cmath>

#include "Style.h"

namespace veneer::ui
{
VeneerLookAndFeel::VeneerLookAndFeel()
{
    setColourScheme (juce::LookAndFeel_V4::ColourScheme (colour (palette::panel), colour (palette::panelRaised),
                                                         colour (palette::panelRaised), colour (palette::edge),
                                                         colour (palette::text), colour (palette::brass),
                                                         colour (palette::ink), colour (palette::brass),
                                                         colour (palette::text)));

    setColour (juce::Slider::textBoxTextColourId, colour (palette::text));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, colour (palette::brass).withAlpha (0.35f));
    setColour (juce::TooltipWindow::backgroundColourId, colour (palette::panelRaised));
    setColour (juce::TooltipWindow::textColourId, colour (palette::text));
    setColour (juce::TooltipWindow::outlineColourId, colour (palette::edge));
}

void VeneerLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const auto hero = (bool) slider.getProperties().getWithDefault ("hero", false);
    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto centre = area.getCentre();
    const auto outer = std::min (area.getWidth(), area.getHeight()) * 0.5f - 4.0f;
    const auto trackWidth = std::max (3.0f, outer * (hero ? 0.09f : 0.11f));
    const auto trackRadius = outer - trackWidth * 0.5f;
    const auto capRadius = trackRadius - trackWidth * 1.2f;
    const auto sweep = endAngle - startAngle;
    const auto angle = startAngle + position * sweep;

    auto arc = [&] (float from, float to, juce::uint32 argb)
    {
        juce::Path p;
        p.addCentredArc (centre.x, centre.y, trackRadius, trackRadius, 0.0f, from, to, true);
        g.setColour (colour (argb));
        g.strokePath (p, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    arc (startAngle, endAngle, palette::well);

    const auto bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const auto zeroAngle = bipolar ? startAngle + (float) slider.valueToProportionOfLength (0.0) * sweep : startAngle;

    if (std::abs (angle - zeroAngle) > 0.01f)
        arc (std::min (zeroAngle, angle), std::max (zeroAngle, angle), palette::brass);

    if (slider.isDoubleClickReturnEnabled())
    {
        const auto home = startAngle + (float) slider.valueToProportionOfLength (slider.getDoubleClickReturnValue()) * sweep;
        const auto dx = std::sin (home), dy = -std::cos (home);
        g.setColour (colour (palette::textDim));
        g.drawLine (centre.x + dx * (outer + 0.5f), centre.y + dy * (outer + 0.5f),
                    centre.x + dx * (outer + 3.5f), centre.y + dy * (outer + 3.5f), 1.5f);
    }

    const auto cap = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (colour (hero ? palette::walnutLight : 0xff3d424a),
                                             centre.x - capRadius * 0.45f, centre.y - capRadius * 0.65f,
                                             colour (hero ? palette::walnutDark : 0xff1a1c20),
                                             centre.x + capRadius * 0.5f, centre.y + capRadius * 0.8f, false));
    g.fillEllipse (cap);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawEllipse (cap.reduced (0.5f), 1.0f);

    const auto pointerWidth = std::max (2.5f, capRadius * 0.09f);
    juce::Path pointer;
    pointer.addRoundedRectangle (-pointerWidth * 0.5f, -capRadius * 0.9f, pointerWidth, capRadius * 0.46f, pointerWidth * 0.5f);
    g.setColour (colour (palette::brass));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));

    if (slider.hasKeyboardFocus (false))
    {
        g.setColour (colour (palette::brass).withAlpha (0.7f));
        g.drawEllipse (juce::Rectangle<float> (outer * 2.0f + 6.0f, outer * 2.0f + 6.0f).withCentre (centre), 1.5f);
    }
}

juce::Label* VeneerLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (font ((float) slider.getTextBoxHeight() * 0.72f, juce::Font::bold));
    label->setJustificationType (juce::Justification::centred);
    return label;
}

void VeneerLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down)
{
    const auto area = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto on = button.getToggleState();
    const auto radius = area.getHeight() * 0.5f;

    g.setColour (on ? colour (palette::brass) : juce::Colours::black.withAlpha (highlighted || down ? 0.55f : 0.4f));
    g.fillRoundedRectangle (area, radius);
    g.setColour (on ? colour (palette::brass) : colour (palette::brass).withAlpha (highlighted ? 0.9f : 0.55f));
    g.drawRoundedRectangle (area, radius, 1.2f);

    g.setColour (on ? colour (palette::ink) : colour (palette::text));
    g.setFont (font (11.5f, juce::Font::bold).withExtraKerningFactor (0.14f));
    g.drawText (button.getButtonText().toUpperCase(), area, juce::Justification::centred, false);

    if (button.hasKeyboardFocus (false))
    {
        g.setColour (colour (palette::text).withAlpha (0.8f));
        g.drawRoundedRectangle (area.expanded (2.0f), radius + 2.0f, 1.2f);
    }
}

void VeneerLookAndFeel::drawCornerResizer (juce::Graphics& g, int width, int height, bool mouseOver, bool dragging)
{
    g.setColour (colour (palette::textDim).withAlpha (mouseOver || dragging ? 0.9f : 0.5f));
    const auto thickness = (float) std::min (width, height) * 0.075f;

    for (auto i = 0.3f; i < 1.0f; i += 0.3f)
        g.drawLine ((float) width * i, (float) height + 1.0f, (float) width + 1.0f, (float) height * i, thickness);
}
}
