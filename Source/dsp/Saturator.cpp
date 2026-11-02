#include "Saturator.h"

#include <cmath>

namespace veneer
{
namespace
{
constexpr double blockerHz = 5.0;
constexpr double minimumDrive = 1.0e-4;
constexpr double smallStep = 1.0e-9;
constexpr double ln2 = 0.69314718055994531;
constexpr double pi = 3.14159265358979323846;

// log (cosh (a)) without overflow
double logCosh (double a) noexcept
{
    const auto m = std::abs (a);
    return m + std::log1p (std::exp (-2.0 * m)) - ln2;
}
}

void Saturator::prepare (double sampleRate)
{
    blockerCoefficient = std::exp (-2.0 * pi * blockerHz / sampleRate);
    drive = bias = 0.0;
    active = false;
    reset();
}

void Saturator::reset() noexcept
{
    for (auto& s : state)
    {
        s.interpolator.reset();
        s.decimator.reset();
        s.previousInput = 0.0;
        s.previousIntegral = active ? integral (0.0) : 0.0;
        s.blockerInput = s.blockerOutput = 0.0;
    }
}

void Saturator::setShape (float newDrive, float newBias) noexcept
{
    drive = (double) newDrive;
    bias = (double) newBias;
    active = drive > minimumDrive;

    if (! active)
        return;

    inverseDrive = 1.0 / drive;
    inverseDriveSquared = inverseDrive * inverseDrive;
    biasTanh = std::tanh (bias);
    inverseSlope = 1.0 / (1.0 - biasTanh * biasTanh);

    for (auto& s : state)
        s.previousIntegral = integral (s.previousInput);
}

// shaped - input, with the shaper normalised to a slope of 1 at the origin
double Saturator::curve (double u) const noexcept
{
    return (std::tanh (drive * u + bias) - biasTanh) * inverseSlope * inverseDrive - u;
}

// antiderivative of curve
double Saturator::integral (double u) const noexcept
{
    const auto z = drive * u;
    return (logCosh (z + bias) - z * biasTanh) * inverseSlope * inverseDriveSquared - 0.5 * u * u;
}

float Saturator::distortion (ChannelState& s, double u) const noexcept
{
    const auto current = integral (u);
    const auto step = u - s.previousInput;
    const auto result = std::abs (step) > smallStep ? (current - s.previousIntegral) / step
                                                     : curve (0.5 * (u + s.previousInput));

    s.previousInput = u;
    s.previousIntegral = current;
    return (float) result;
}

float Saturator::process (int channel, float in) noexcept
{
    auto& s = state[(size_t) channel];

    const auto up = s.interpolator.process (in);
    SamplePair shaped;

    if (active)
    {
        shaped.first = distortion (s, (double) up.first);
        shaped.second = distortion (s, (double) up.second);
    }
    else
    {
        s.previousInput = (double) up.second;
    }

    const auto wet = (double) s.decimator.process (shaped.first, shaped.second);

    // removes the DC that an asymmetric curve adds; the input itself never passes through it
    s.blockerOutput = wet - s.blockerInput + blockerCoefficient * s.blockerOutput;
    s.blockerInput = wet;

    return s.interpolator.delayed() + (float) s.blockerOutput;
}
}
