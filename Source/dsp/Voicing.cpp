#include "Voicing.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace veneer::voicing
{
namespace
{
struct BandVoicing
{
    float thresholdDb, ratioAtHundred, kneeDb, detectorMs, attackMs, releaseFastMs, releaseSlowMs;
};

constexpr std::array<BandVoicing, 3> bands { {
    { -20.0f, 2.0f, 8.0f, 40.0f, 30.0f, 80.0f, 600.0f },
    { -22.0f, 2.2f, 8.0f, 15.0f, 15.0f, 60.0f, 400.0f },
    { -26.0f, 1.8f, 8.0f,  5.0f,  5.0f, 40.0f, 250.0f },
} };

struct EqVoicing
{
    float frequencyHz, q, gainDbAtHundred;
};

constexpr std::array<EqVoicing, 3> eq { {
    {   90.0f, 0.70f, 1.0f },
    {  900.0f, 0.45f, 1.2f },
    { 9000.0f, 0.60f, 0.8f },
} };

// drive is the gain into tanh at 100%; a band peak of 0.3 (-10 dBFS) at a drive of 1 gives 0.7% third harmonic
struct SaturationVoicing
{
    float driveAtHundred, bias;
};

constexpr std::array<SaturationVoicing, 3> saturation { {
    { 1.0f, 0.08f },
    { 0.9f, 0.12f },
    { 0.7f, 0.00f },
} };

// extra dB of drive per dB a band is boosted
constexpr float focusDriveRatio = 0.5f;

constexpr float widenDbAtHundred = 2.0f;

float depthOf (float amountPercent)
{
    return std::max (amountPercent, 0.0f) * 0.01f;
}
}

BandCompressor::Settings compressorSettings (int band, float amountPercent)
{
    const auto& v = bands[(size_t) std::clamp (band, 0, 2)];
    const auto depth = depthOf (amountPercent);

    BandCompressor::Settings s;
    s.thresholdDb = v.thresholdDb - 3.0f * (depth - 1.0f);
    s.ratio = 1.0f + (v.ratioAtHundred - 1.0f) * depth;
    s.kneeDb = v.kneeDb;
    s.detectorMs = v.detectorMs;
    s.attackMs = v.attackMs;
    s.releaseFastMs = v.releaseFastMs;
    s.releaseSlowMs = v.releaseSlowMs;
    return s;
}

std::array<ShelfBellEq::Section, 3> eqSections (float amountPercent)
{
    const auto depth = depthOf (amountPercent);
    std::array<ShelfBellEq::Section, 3> sections;

    for (size_t i = 0; i < sections.size(); ++i)
        sections[i] = { eq[i].frequencyHz, eq[i].q, eq[i].gainDbAtHundred * depth };

    return sections;
}

float saturatorDrive (int band, float amountPercent, float focusDb)
{
    const auto& v = saturation[(size_t) std::clamp (band, 0, 2)];
    const auto extraDb = focusDriveRatio * std::max (focusDb, 0.0f);

    return v.driveAtHundred * depthOf (amountPercent) * std::pow (10.0f, extraDb / 20.0f);
}

float saturatorBias (int band)
{
    return saturation[(size_t) std::clamp (band, 0, 2)].bias;
}

float widenerBoostDb (float amountPercent)
{
    return widenDbAtHundred * depthOf (amountPercent);
}
}
