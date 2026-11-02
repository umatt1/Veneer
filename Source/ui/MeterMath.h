#pragma once

#include <algorithm>

#include <juce_core/juce_core.h>

#include "Parameters.h"

namespace veneer::ui
{
// 0 at floorDb, 1 at ceilingDb
constexpr float levelPosition (float db, float floorDb, float ceilingDb) noexcept
{
    return std::clamp ((db - floorDb) / (ceilingDb - floorDb), 0.0f, 1.0f);
}

// rises at once, falls no faster than dbPerSecond
inline float fallen (float shown, float target, float seconds, float dbPerSecond) noexcept
{
    return target >= shown ? target : std::max (target, shown - dbPerSecond * seconds);
}

inline juce::String levelText (float db, float silenceBelowDb)
{
    return db > silenceBelowDb ? params::formatValue (db, params::Text::decimal) : juce::String ("--");
}
}
