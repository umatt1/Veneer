#pragma once

#include <array>

namespace veneer
{
struct SamplePair
{
    float first = 0.0f, second = 0.0f;
};

// Linear-phase halfband filters for 2x oversampling, in polyphase form so zero-stuffing and decimation
// cost nothing: one of the two phases is a plain delayed copy and the other a (halfLength + 1)-tap FIR.
// An interpolator followed by a decimator delays the signal by latencySamples input samples.
class HalfbandInterpolator
{
public:
    static constexpr int halfLength = 15;
    static constexpr int taps = halfLength + 1;
    static constexpr int latencySamples = halfLength;
    static constexpr int historySize = 16;

    void reset() noexcept;

    // the two samples at twice the input rate, in time order
    SamplePair process (float in) noexcept;

    // the input from latencySamples ago
    float delayed() const noexcept;

private:
    static_assert (historySize >= taps && (historySize & (historySize - 1)) == 0);

    // each sample is stored twice, historySize apart, so the window is always contiguous
    std::array<float, 2 * historySize> history {};
    int position = 0;
};

class HalfbandDecimator
{
public:
    void reset() noexcept;

    float process (float first, float second) noexcept;

private:
    static constexpr int historySize = HalfbandInterpolator::historySize;

    std::array<float, 2 * historySize> even {}, odd {};
    int position = 0;
};
}
