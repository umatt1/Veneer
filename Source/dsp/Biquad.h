#pragma once

#include <algorithm>

namespace veneer
{
// a filter designed at a fixed frequency is held below this fraction of the sample rate, so at a low rate the
// tone changes but the filter stays stable
inline double usableFrequency (double sampleRate, double frequencyHz) noexcept
{
    return std::min (frequencyHz, 0.45 * sampleRate);
}

struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    double process (double x) noexcept
    {
        const auto y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0.0; }

    void setCoefficients (const Biquad& other) noexcept
    {
        b0 = other.b0;
        b1 = other.b1;
        b2 = other.b2;
        a1 = other.a1;
        a2 = other.a2;
    }
};

// RBJ audio EQ cookbook filters. At 0 dB each one has b == a, so it passes the input through unchanged.
Biquad makeLowShelf (double sampleRate, double frequencyHz, double q, double gainDb);
Biquad makeHighShelf (double sampleRate, double frequencyHz, double q, double gainDb);
Biquad makePeak (double sampleRate, double frequencyHz, double q, double gainDb);
}
