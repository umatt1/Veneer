#pragma once

#include <array>

#include "HalfbandOversampler.h"

namespace veneer
{
// Oversampled tanh with first-order antiderivative anti-aliasing (Parker et al., DAFx 2016). Only the
// distortion goes through the oversampler: output = delayed input + decimated (shaped - input), so small
// signals pass at unit gain and drive 0 is an exact delay.
class Saturator
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int latencySamples = HalfbandInterpolator::latencySamples;

    void prepare (double sampleRate);
    void reset() noexcept;

    // drive is the gain into tanh (and back out), bias moves the operating point to add even harmonics
    void setShape (float drive, float bias) noexcept;

    float process (int channel, float in) noexcept;

private:
    struct ChannelState
    {
        HalfbandInterpolator interpolator;
        HalfbandDecimator decimator;
        double previousInput = 0.0, previousIntegral = 0.0;
        double blockerInput = 0.0, blockerOutput = 0.0;
    };

    double curve (double u) const noexcept;
    double integral (double u) const noexcept;
    float distortion (ChannelState& s, double u) const noexcept;

    std::array<ChannelState, maxChannels> state;

    bool active = false;
    double drive = 0.0, bias = 0.0;
    double inverseDrive = 0.0, inverseDriveSquared = 0.0, biasTanh = 0.0, inverseSlope = 1.0;
    double blockerCoefficient = 0.0;
};
}
