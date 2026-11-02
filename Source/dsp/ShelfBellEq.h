#pragma once

#include <array>

#include "Biquad.h"

namespace veneer
{
// Low shelf, bell and high shelf in series. With every gain at 0 dB the filters have b == a and the
// signal passes through unchanged.
class ShelfBellEq
{
public:
    static constexpr int maxChannels = 2;

    struct Section
    {
        float frequencyHz = 1000.0f, q = 0.7f, gainDb = 0.0f;
    };

    void prepare (double sampleRate);
    void reset() noexcept;

    // [0] low shelf, [1] bell, [2] high shelf
    void setSections (const std::array<Section, 3>& sections) noexcept;

    float process (int channel, float in) noexcept;

private:
    double sampleRate = 44100.0;
    std::array<std::array<Biquad, 3>, maxChannels> filters;
};
}
