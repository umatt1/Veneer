#include "Voicing.h"

#include <algorithm>
#include <array>

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
}

BandCompressor::Settings compressorSettings (int band, float amountPercent)
{
    const auto& v = bands[(size_t) std::clamp (band, 0, 2)];
    const auto depth = std::max (amountPercent, 0.0f) * 0.01f;

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
}
