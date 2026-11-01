#pragma once

#include <array>
#include <vector>

#include "TruePeak.h"
#include "Windowed.h"

namespace veneer
{
class LookaheadLimiter
{
public:
    static constexpr int maxChannels = TruePeakDetector::maxChannels;

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setCeilingDb (float db) noexcept;

    int getLatencySamples() const noexcept { return latency; }

    // reads one frame, writes the delayed and gain-controlled frame, returns gain reduction in dB
    float process (const float* in, float* out) noexcept;

private:
    TruePeakDetector detector;
    SlidingMin slidingMin;
    MovingAverage smoother;
    std::array<std::vector<float>, maxChannels> delay;

    size_t delayPosition = 0;
    int numChannels = 2, latency = 0;
    float ceilingLinear = 0.891f;
    float previousRequired = 1.0f, envelope = 1.0f, sustainDb = 0.0f;
    float releaseFastCoef = 0.0f, releaseSlowCoef = 0.0f, sustainCoef = 0.0f;
};
}
