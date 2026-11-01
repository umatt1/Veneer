#pragma once

#include "BandCompressor.h"

namespace veneer::voicing
{
inline constexpr float lowCrossoverHz = 200.0f;
inline constexpr float highCrossoverHz = 5000.0f;

BandCompressor::Settings compressorSettings (int band, float amountPercent);
}
