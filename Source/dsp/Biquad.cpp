#include "Biquad.h"

#include <cmath>

namespace veneer
{
namespace
{
constexpr double pi = 3.14159265358979323846;

Biquad normalised (double b0, double b1, double b2, double a0, double a1, double a2)
{
    Biquad f;
    f.b0 = b0 / a0;
    f.b1 = b1 / a0;
    f.b2 = b2 / a0;
    f.a1 = a1 / a0;
    f.a2 = a2 / a0;
    return f;
}
}

Biquad makeLowShelf (double sampleRate, double frequencyHz, double q, double gainDb)
{
    const auto A = std::pow (10.0, gainDb / 40.0);
    const auto w0 = 2.0 * pi * usableFrequency (sampleRate, frequencyHz) / sampleRate;
    const auto cosW = std::cos (w0);
    const auto beta = std::sin (w0) / q * std::sqrt (A);

    return normalised (A * ((A + 1.0) - (A - 1.0) * cosW + beta),
                       2.0 * A * ((A - 1.0) - (A + 1.0) * cosW),
                       A * ((A + 1.0) - (A - 1.0) * cosW - beta),
                       (A + 1.0) + (A - 1.0) * cosW + beta,
                       -2.0 * ((A - 1.0) + (A + 1.0) * cosW),
                       (A + 1.0) + (A - 1.0) * cosW - beta);
}

Biquad makeHighShelf (double sampleRate, double frequencyHz, double q, double gainDb)
{
    const auto A = std::pow (10.0, gainDb / 40.0);
    const auto w0 = 2.0 * pi * usableFrequency (sampleRate, frequencyHz) / sampleRate;
    const auto cosW = std::cos (w0);
    const auto beta = std::sin (w0) / q * std::sqrt (A);

    return normalised (A * ((A + 1.0) + (A - 1.0) * cosW + beta),
                       -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW),
                       A * ((A + 1.0) + (A - 1.0) * cosW - beta),
                       (A + 1.0) - (A - 1.0) * cosW + beta,
                       2.0 * ((A - 1.0) - (A + 1.0) * cosW),
                       (A + 1.0) - (A - 1.0) * cosW - beta);
}

Biquad makePeak (double sampleRate, double frequencyHz, double q, double gainDb)
{
    const auto A = std::pow (10.0, gainDb / 40.0);
    const auto w0 = 2.0 * pi * usableFrequency (sampleRate, frequencyHz) / sampleRate;
    const auto cosW = std::cos (w0);
    const auto alpha = std::sin (w0) / (2.0 * q);

    return normalised (1.0 + alpha * A, -2.0 * cosW, 1.0 - alpha * A,
                       1.0 + alpha / A, -2.0 * cosW, 1.0 - alpha / A);
}
}
