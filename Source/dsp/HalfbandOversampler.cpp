#include "HalfbandOversampler.h"

#include <cmath>

#include "Kaiser.h"

namespace veneer
{
namespace
{
constexpr int halfLength = HalfbandInterpolator::halfLength;
constexpr int taps = HalfbandInterpolator::taps;
constexpr double kaiserBeta = 7.0;
constexpr double pi = 3.14159265358979323846;

static_assert (halfLength % 2 == 1 && taps % 4 == 0);

// Windowed-sinc halfband prototype with taps at odd offsets -halfLength..halfLength plus a centre tap of
// exactly 0.5. The odd taps are scaled so the filter has unity gain at DC.
struct Tables
{
    Tables()
    {
        std::array<double, taps> odd {};
        double sum = 0.0;

        for (int i = 0; i < taps; ++i)
        {
            const auto k = 2 * i - halfLength;
            odd[(size_t) i] = std::sin (pi * k / 2.0) / (pi * k) * kaiser ((double) k / halfLength, kaiserBeta);
            sum += odd[(size_t) i];
        }

        for (size_t i = 0; i < (size_t) taps; ++i)
        {
            down[i] = (float) (odd[i] * 0.5 / sum);
            up[i] = 2.0f * down[i];
        }
    }

    std::array<float, taps> up {}, down {};
};

const Tables& tables()
{
    static const Tables instance;
    return instance;
}

float dot (const float* a, const float* b) noexcept
{
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;

    for (int j = 0; j < taps; j += 4)
    {
        s0 += a[j] * b[j];
        s1 += a[j + 1] * b[j + 1];
        s2 += a[j + 2] * b[j + 2];
        s3 += a[j + 3] * b[j + 3];
    }

    return (s0 + s1) + (s2 + s3);
}
}

void HalfbandInterpolator::reset() noexcept
{
    history.fill (0.0f);
    position = 0;
}

SamplePair HalfbandInterpolator::process (float in) noexcept
{
    position = (position + 1) & (historySize - 1);
    history[(size_t) position] = in;
    history[(size_t) (position + historySize)] = in;

    // window[halfLength] is the newest sample
    const float* window = &history[(size_t) (position + historySize - halfLength)];

    return { dot (tables().up.data(), window), window[(halfLength + 1) / 2] };
}

float HalfbandInterpolator::delayed() const noexcept
{
    return history[(size_t) (position + historySize - halfLength)];
}

void HalfbandDecimator::reset() noexcept
{
    even.fill (0.0f);
    odd.fill (0.0f);
    position = 0;
}

float HalfbandDecimator::process (float first, float second) noexcept
{
    position = (position + 1) & (historySize - 1);
    even[(size_t) position] = first;
    even[(size_t) (position + historySize)] = first;
    odd[(size_t) position] = second;
    odd[(size_t) (position + historySize)] = second;

    const float* evenWindow = &even[(size_t) (position + historySize - halfLength)];
    const float* oddWindow = &odd[(size_t) (position + historySize - halfLength)];

    return dot (tables().down.data(), evenWindow) + 0.5f * oddWindow[(halfLength - 1) / 2];
}
}
