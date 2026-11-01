#include "BandCompressor.h"

#include <algorithm>
#include <cmath>

namespace veneer
{
namespace
{
constexpr float sustainTimeMs = 300.0f;
constexpr float sustainFullDb = 3.0f;
constexpr float dbToNeper = 0.11512925f;
}

void BandCompressor::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    sustainCoef = coefficient (sustainTimeMs);
    setSettings (settings);
    reset();
}

void BandCompressor::reset() noexcept
{
    meanSquare = 0.0f;
    grDb = 0.0f;
    sustainDb = 0.0f;
}

void BandCompressor::setSettings (const Settings& newSettings) noexcept
{
    settings = newSettings;
    detectorCoef = coefficient (settings.detectorMs);
    attackCoef = coefficient (settings.attackMs);
    releaseFastCoef = coefficient (settings.releaseFastMs);
    releaseSlowCoef = coefficient (settings.releaseSlowMs);
}

float BandCompressor::coefficient (float timeMs) const noexcept
{
    return (float) std::exp (-1.0 / (0.001 * (double) timeMs * sampleRate));
}

float BandCompressor::process (float power) noexcept
{
    meanSquare = power + detectorCoef * (meanSquare - power);

    // a sine of peak amplitude A has mean square A^2 / 2
    const float levelDb = 10.0f * std::log10 (meanSquare * 2.0f + 1.0e-12f);
    const float over = levelDb - settings.thresholdDb;
    const float slope = 1.0f - 1.0f / settings.ratio;

    float target = 0.0f;

    if (2.0f * over > settings.kneeDb)
        target = slope * over;
    else if (2.0f * over > -settings.kneeDb)
        target = slope * (over + settings.kneeDb * 0.5f) * (over + settings.kneeDb * 0.5f) / (2.0f * settings.kneeDb);

    if (target > grDb)
    {
        grDb = attackCoef * grDb + (1.0f - attackCoef) * target;
    }
    else
    {
        const auto mix = std::clamp (sustainDb / sustainFullDb, 0.0f, 1.0f);
        const auto releaseCoef = releaseFastCoef + (releaseSlowCoef - releaseFastCoef) * mix;
        grDb = releaseCoef * grDb + (1.0f - releaseCoef) * target;

        if (target == 0.0f && grDb < 1.0e-5f)
            grDb = 0.0f;
    }

    sustainDb = sustainCoef * sustainDb + (1.0f - sustainCoef) * grDb;

    return std::exp (-grDb * dbToNeper);
}
}
