#pragma once

#include "Biquad.h"

namespace veneer
{
// Raises the high-frequency side signal. The mid signal is untouched, so the mono sum (L + R) is exactly
// what it was before.
class Widener
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    void setShelf (float frequencyHz, float q, float gainDb) noexcept;

    void process (float& left, float& right) noexcept;

private:
    double sampleRate = 44100.0;
    Biquad side;
};
}
