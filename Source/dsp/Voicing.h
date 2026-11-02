#pragma once

#include <array>

#include "BandCompressor.h"
#include "ShelfBellEq.h"

namespace veneer::voicing
{
inline constexpr float lowCrossoverHz = 200.0f;
inline constexpr float highCrossoverHz = 5000.0f;

inline constexpr float widenerHz = 6000.0f;
inline constexpr float widenerQ = 0.7f;

BandCompressor::Settings compressorSettings (int band, float amountPercent);

// static tone curve ahead of the band split; flat at 0%, gains scale linearly in dB with Amount
std::array<ShelfBellEq::Section, 3> eqSections (float amountPercent);

// gain into the band's tanh stage; 0 at 0%, and a boosted band is driven harder
float saturatorDrive (int band, float amountPercent, float focusDb);
float saturatorBias (int band);

float widenerBoostDb (float amountPercent);
}
