#include "MeterViews.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "MeterMath.h"
#include "Style.h"
#include "dsp/Voicing.h"

namespace veneer::ui
{
namespace
{
constexpr float reductionFallDbPerSecond = 40.0f;
constexpr int levelLabelWidth = 26, levelBarWidth = 24, reductionBarWidth = 12, reductionTextHeight = 16;
constexpr float hotDb = -9.0f;

bool differs (float a, float b)
{
    return std::abs (a - b) >= 0.05f;
}
}

LevelMeter::LevelMeter()
{
    setAccessible (false);
}

juce::Rectangle<float> LevelMeter::barArea() const
{
    return getLocalBounds().removeFromRight (levelBarWidth).reduced (0, 8).toFloat();
}

void LevelMeter::setLevelDb (float db)
{
    const auto clamped = std::clamp (db, floorDb, ceilingDb);

    if (differs (clamped, shownDb))
    {
        shownDb = clamped;
        repaint();
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto bar = barArea();
    auto yAt = [&bar] (float db) { return bar.getBottom() - levelPosition (db, floorDb, ceilingDb) * bar.getHeight(); };
    const auto zoneLow = voicing::targetLevelDb - voicing::targetHalfWidthDb;
    const auto zoneHigh = voicing::targetLevelDb + voicing::targetHalfWidthDb;

    juce::Path well;
    well.addRoundedRectangle (bar, 3.0f);
    g.setColour (colour (palette::well));
    g.fillPath (well);

    g.saveState();
    g.reduceClipRegion (well);

    g.setColour (colour (palette::teal).withAlpha (0.2f));
    g.fillRect (bar.getX(), yAt (zoneHigh), bar.getWidth(), yAt (zoneLow) - yAt (zoneHigh));

    auto fill = [&] (float lowDb, float highDb, juce::uint32 argb)
    {
        const auto top = std::min (shownDb, highDb);

        if (top > lowDb)
        {
            g.setColour (colour (argb));
            g.fillRect (bar.getX(), yAt (top), bar.getWidth(), yAt (lowDb) - yAt (top));
        }
    };

    fill (floorDb, zoneLow, palette::tealDim);
    fill (zoneLow, zoneHigh, palette::teal);
    fill (zoneHigh, hotDb, palette::amber);
    fill (hotDb, ceilingDb, palette::coral);
    g.restoreState();

    g.setColour (colour (palette::teal).withAlpha (0.6f));
    g.fillRect (bar.getX(), yAt (zoneHigh), bar.getWidth(), 1.0f);
    g.fillRect (bar.getX(), yAt (zoneLow), bar.getWidth(), 1.0f);

    g.setColour (colour (palette::edge));
    g.drawRoundedRectangle (bar.reduced (0.5f), 3.0f, 1.0f);

    g.setFont (font (9.0f));

    for (auto db = (int) ceilingDb; db >= (int) floorDb; db -= 12)
    {
        const auto y = yAt ((float) db);
        g.setColour (colour (palette::textDim));
        g.fillRect (bar.getX() - 4.0f, y, 3.0f, 1.0f);
        g.drawText (juce::String (db), juce::Rectangle<float> (0.0f, y - 6.0f, (float) levelLabelWidth - 3.0f, 12.0f),
                    juce::Justification::centredRight, false);
    }
}

GainReductionMeter::GainReductionMeter (Kind k) : kind (k)
{
    setAccessible (false);
}

void GainReductionMeter::setReductionDb (float db, float seconds)
{
    const auto next = std::clamp (fallen (shownDb, db, seconds, reductionFallDbPerSecond), 0.0f, 99.0f);

    if (differs (next, shownDb))
    {
        shownDb = next;
        repaint();
    }
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    auto textArea = area.removeFromBottom (reductionTextHeight);
    const auto bar = area.withSizeKeepingCentre (reductionBarWidth, area.getHeight()).reduced (0, 2).toFloat();

    juce::Path well;
    well.addRoundedRectangle (bar, 3.0f);
    g.setColour (colour (palette::well));
    g.fillPath (well);

    g.saveState();
    g.reduceClipRegion (well);

    g.setColour (colour (kind == Kind::limiter ? palette::coral : palette::amber));
    g.fillRect (bar.getX(), bar.getY(), bar.getWidth(), levelPosition (shownDb, 0.0f, rangeDb) * bar.getHeight());

    g.setColour (juce::Colours::white.withAlpha (0.1f));

    for (auto db = 3.0f; db < rangeDb; db += 3.0f)
        g.fillRect (bar.getX(), bar.getY() + levelPosition (db, 0.0f, rangeDb) * bar.getHeight(), bar.getWidth(), 1.0f);

    g.restoreState();

    g.setColour (colour (palette::edge));
    g.drawRoundedRectangle (bar.reduced (0.5f), 3.0f, 1.0f);

    g.setColour (colour (shownDb < 0.05f ? palette::textDim : palette::text));
    g.setFont (font (10.5f, juce::Font::bold));
    g.drawText (params::formatValue (-shownDb, params::Text::decimal), textArea, juce::Justification::centred, false);
}

LoudnessReadout::LoudnessReadout()
{
    setAccessible (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void LoudnessReadout::setValues (const Values& next)
{
    const auto changed = differs (next.momentary, values.momentary) || differs (next.shortTerm, values.shortTerm)
                      || differs (next.integrated, values.integrated) || differs (next.truePeak, values.truePeak);

    if (changed)
    {
        values = next;
        repaint();
    }
}

void LoudnessReadout::setCeilingDb (float db)
{
    if (differs (db, ceilingDb))
    {
        ceilingDb = db;
        repaint();
    }
}

void LoudnessReadout::mouseDown (const juce::MouseEvent&)
{
    if (onClick)
        onClick();
}

void LoudnessReadout::paint (juce::Graphics& g)
{
    struct Row { const char* caption; float value; const char* unit; bool isPeak; };

    const Row rows[] = {
        { "MOMENTARY", values.momentary, "LUFS", false },
        { "SHORT TERM", values.shortTerm, "LUFS", false },
        { "INTEGRATED", values.integrated, "LUFS", false },
        { "TRUE PEAK", values.truePeak, "dBTP", true },
    };

    const auto rowHeight = getHeight() / (int) std::size (rows);
    auto area = getLocalBounds();

    for (const auto& row : rows)
    {
        auto line = area.removeFromTop (rowHeight);
        const auto caption = line.removeFromLeft (64);
        const auto unit = line.removeFromRight (28);
        const auto over = row.isPeak && row.value > ceilingDb + 0.05f;

        g.setColour (colour (palette::textDim));
        g.setFont (font (9.0f, juce::Font::bold).withExtraKerningFactor (0.08f));
        g.drawText (row.caption, caption, juce::Justification::centredLeft, false);
        g.drawText (row.unit, unit, juce::Justification::centredRight, false);

        g.setColour (colour (over ? palette::coral : palette::text));
        g.setFont (font (14.0f, juce::Font::bold));
        g.drawText (levelText (row.value, -70.0f), line, juce::Justification::centredRight, false);
    }
}
}
