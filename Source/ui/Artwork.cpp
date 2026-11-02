#include "Artwork.h"

#include <cmath>

#include "Layout.h"
#include "Style.h"

namespace veneer::ui
{
namespace
{
void drawVeneer (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setGradientFill (juce::ColourGradient (colour (palette::walnutLight), 0.0f, area.getY(),
                                             colour (palette::walnutDark), 0.0f, area.getBottom(), false));
    g.fillRect (area);

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    juce::Random random (0x56454e);

    for (int strand = 0; strand < 110; ++strand)
    {
        const auto y0 = area.getY() + random.nextFloat() * area.getHeight();
        const auto wavelength = 160.0f + random.nextFloat() * 420.0f;
        const auto amplitude = 1.0f + random.nextFloat() * 4.0f;
        const auto phase = random.nextFloat() * juce::MathConstants<float>::twoPi;
        const auto slope = (random.nextFloat() - 0.5f) * 16.0f;

        juce::Path path;

        for (auto x = area.getX(); x <= area.getRight() + 6.0f; x += 6.0f)
        {
            const auto y = y0 + amplitude * std::sin (x / wavelength * juce::MathConstants<float>::twoPi + phase)
                         + slope * (x / area.getWidth() - 0.5f);

            if (x <= area.getX())
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }

        const auto light = random.nextBool();
        g.setColour (colour (light ? palette::walnutLight : palette::walnutDark).withAlpha (0.1f + random.nextFloat() * 0.2f));
        g.strokePath (path, juce::PathStrokeType (0.6f + random.nextFloat() * 1.4f));
    }

    g.restoreState();

    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillRect (area.getX(), area.getY(), area.getWidth(), 1.0f);
    g.setColour (colour (palette::brass).withAlpha (0.85f));
    g.fillRect (area.getX(), area.getBottom() - 2.0f, area.getWidth(), 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (area.getX(), area.getBottom() - 1.0f, area.getWidth(), 1.0f);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto area = bounds.toFloat();
    g.setColour (colour (palette::panelRaised));
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawLine (area.getX() + 10.0f, area.getY() + 1.5f, area.getRight() - 10.0f, area.getY() + 1.5f);
    g.setColour (colour (palette::edge));
    g.drawRoundedRectangle (area.reduced (0.5f), 10.0f, 1.0f);
}

void drawCaption (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                  float height = 11.0f, juce::uint32 argb = palette::textDim, bool bold = true)
{
    g.setColour (colour (argb));
    g.setFont (font (height, bold ? juce::Font::bold : juce::Font::plain).withExtraKerningFactor (0.14f));
    g.drawText (text, area, juce::Justification::centred, false);
}
}

Backdrop::Backdrop()
{
    setOpaque (true);
    setBufferedToImage (true);
    setInterceptsMouseClicks (false, false);
    setAccessible (false);
}

void Backdrop::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (colour (palette::panel).brighter (0.06f), 0.0f, 0.0f,
                                             colour (palette::panel).darker (0.25f), 0.0f, bounds.getBottom(), false));
    g.fillAll();

    drawVeneer (g, layout::header().toFloat());

    const auto wordmark = layout::wordmark();
    const auto title = font (30.0f, juce::Font::bold).withExtraKerningFactor (0.24f);
    g.setFont (title);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawText ("VENEER", wordmark.translated (0, 2), juce::Justification::centredLeft, false);
    g.setColour (colour (palette::text));
    g.drawText ("VENEER", wordmark, juce::Justification::centredLeft, false);

    g.setColour (colour (palette::text).withAlpha (0.7f));
    g.setFont (font (11.0f).withExtraKerningFactor (0.16f));
    g.drawText ("MIX-BUS FINISHER", layout::tagline(), juce::Justification::centredLeft, false);

    for (const auto& panel : { layout::inputPanel(), layout::tonePanel(), layout::finishPanel() })
        drawPanel (g, panel);

    drawCaption (g, "INPUT", layout::inputLabel());
    drawCaption (g, "AMOUNT", layout::amountLabel(), 12.0f, palette::text);
    drawCaption (g, "PUSH", layout::pushLabel());
    drawCaption (g, "LIM", layout::limiterLabel(), 9.0f);
    drawCaption (g, "OUTPUT", layout::outputLabel());

    static constexpr const char* bandNames[layout::bands] = { "WEIGHT", "BODY", "AIR" };
    static constexpr const char* bandRanges[layout::bands] = { "below 200 Hz", "200 Hz to 5 kHz", "above 5 kHz" };

    for (int band = 0; band < layout::bands; ++band)
    {
        drawCaption (g, bandNames[band], layout::bandLabel (band));
        drawCaption (g, bandRanges[band], layout::bandCaption (band), 9.5f, palette::textDim, false);
    }

    g.setColour (colour (palette::textDim));
    g.setFont (font (9.5f));
    g.drawFittedText ("Aim the level into the lit band.", layout::inputHint(), juce::Justification::centredTop, 3);
}
}
