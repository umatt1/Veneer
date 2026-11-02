#pragma once

#include <array>

namespace veneer
{
class TruePeakDetector
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int oversampling = 8;
    static constexpr int tapsPerPhase = 96;
    static constexpr int groupDelaySamples = (oversampling * tapsPerPhase) / 2 / oversampling;

    void reset() noexcept;

    // returns the largest |value| among the interpolated points that
    // follow the sample from groupDelaySamples ago
    float process (int channel, float sample) noexcept;

private:
    static constexpr int historySize = 128;

    // each sample is stored twice, historySize apart, so the last tapsPerPhase
    // samples are always contiguous
    struct ChannelState
    {
        std::array<float, 2 * historySize> history {};
        int position = 0;
    };

    std::array<ChannelState, maxChannels> state;
};
}
