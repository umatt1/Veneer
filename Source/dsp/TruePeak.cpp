#include "TruePeak.h"

#include <algorithm>
#include <cmath>

namespace veneer
{
namespace
{
constexpr int oversampling = TruePeakDetector::oversampling;
constexpr int tapsPerPhase = TruePeakDetector::tapsPerPhase;
constexpr int numTaps = oversampling * tapsPerPhase + 1;
constexpr double kaiserBeta = 6.0;
constexpr double pi = 3.14159265358979323846;

double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;

    for (int k = 1; k < 60; ++k)
    {
        const auto half = x / (2.0 * k);
        term *= half * half;
        sum += term;
    }

    return sum;
}

// Phase zero of this filter is exactly the delayed input, so only phases 1 to 7 are stored.
// Coefficients are ordered oldest sample first to match the contiguous history window.
struct Coefficients
{
    Coefficients()
    {
        const auto centre = (numTaps - 1) / 2;

        for (int m = 0; m < numTaps; ++m)
        {
            const auto phase = m % oversampling;
            const auto k = m / oversampling;

            if (phase == 0 || k >= tapsPerPhase)
                continue;

            const auto offset = m - centre;
            const auto t = (double) offset / oversampling;
            const auto r = (double) offset / centre;
            const auto window = besselI0 (kaiserBeta * std::sqrt (1.0 - r * r)) / besselI0 (kaiserBeta);

            table[(size_t) phase][(size_t) (tapsPerPhase - 1 - k)] = (float) (std::sin (pi * t) / (pi * t) * window);
        }
    }

    std::array<std::array<float, tapsPerPhase>, oversampling> table {};
};

const Coefficients& coefficients()
{
    static const Coefficients instance;
    return instance;
}

float dot (const float* a, const float* b) noexcept
{
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;

    for (int j = 0; j < tapsPerPhase; j += 4)
    {
        s0 += a[j] * b[j];
        s1 += a[j + 1] * b[j + 1];
        s2 += a[j + 2] * b[j + 2];
        s3 += a[j + 3] * b[j + 3];
    }

    return (s0 + s1) + (s2 + s3);
}
}

void TruePeakDetector::reset() noexcept
{
    for (auto& s : state)
        s = {};
}

float TruePeakDetector::process (int channel, float sample) noexcept
{
    auto& s = state[(size_t) channel];
    s.position = (s.position + 1) & (historySize - 1);
    s.history[(size_t) s.position] = sample;
    s.history[(size_t) (s.position + historySize)] = sample;

    // window[tapsPerPhase - 1] is the newest sample
    const float* window = &s.history[(size_t) (s.position + historySize - (tapsPerPhase - 1))];
    const auto& table = coefficients().table;

    auto peak = std::abs (window[tapsPerPhase - 1 - groupDelaySamples]);

    for (int phase = 1; phase < oversampling; ++phase)
        peak = std::max (peak, std::abs (dot (table[(size_t) phase].data(), window)));

    return peak;
}
}
