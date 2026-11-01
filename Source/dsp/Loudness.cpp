#include "Loudness.h"

#include <algorithm>
#include <cmath>

namespace veneer
{
namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr double floorLufs = -100.0;
constexpr double absoluteGateLufs = -70.0;
constexpr double binsPerLu = 100.0;
}

KWeightingCoefficients makeKWeighting (double sampleRate)
{
    KWeightingCoefficients c;

    {
        const double f0 = 1681.974450955533, gainDb = 3.999843853973347, q = 0.7071752369554196;
        const auto k = std::tan (pi * f0 / sampleRate);
        const auto vh = std::pow (10.0, gainDb / 20.0);
        const auto vb = std::pow (vh, 0.4996667741545416);
        const auto a0 = 1.0 + k / q + k * k;

        c.shelf.b0 = (vh + vb * k / q + k * k) / a0;
        c.shelf.b1 = 2.0 * (k * k - vh) / a0;
        c.shelf.b2 = (vh - vb * k / q + k * k) / a0;
        c.shelf.a1 = 2.0 * (k * k - 1.0) / a0;
        c.shelf.a2 = (1.0 - k / q + k * k) / a0;
    }

    {
        const double f0 = 38.13547087602444, q = 0.5003270373238773;
        const auto k = std::tan (pi * f0 / sampleRate);
        const auto a0 = 1.0 + k / q + k * k;

        c.highpass.b0 = 1.0;
        c.highpass.b1 = -2.0;
        c.highpass.b2 = 1.0;
        c.highpass.a1 = 2.0 * (k * k - 1.0) / a0;
        c.highpass.a2 = (1.0 - k / q + k * k) / a0;
    }

    return c;
}

float energyToLufs (double meanSquareSum) noexcept
{
    if (meanSquareSum <= 0.0)
        return (float) floorLufs;

    return (float) std::max (floorLufs, -0.691 + 10.0 * std::log10 (meanSquareSum));
}

void LoudnessMeter::prepare (double sampleRate, int channelCount)
{
    numChannels = std::clamp (channelCount, 1, maxChannels);
    subblockLength = std::max (1, (int) std::lround (0.01 * sampleRate));

    const auto k = makeKWeighting (sampleRate);

    for (auto& ch : channels)
    {
        ch.shelf = k.shelf;
        ch.highpass = k.highpass;
    }

    binEnergy.assign ((size_t) histogramBins, 0.0);
    binCount.assign ((size_t) histogramBins, 0);

    reset();
}

void LoudnessMeter::reset()
{
    for (auto& ch : channels)
    {
        ch.shelf.reset();
        ch.highpass.reset();
        ch.subblockEnergy = 0.0;
        ch.history.fill (0.0);
    }

    subblockFill = 0;
    subblocksSeen = 0;
    historyPosition = 0;

    momentary.store ((float) floorLufs);
    shortTerm.store ((float) floorLufs);
    resetIntegrated();
}

void LoudnessMeter::resetIntegrated()
{
    std::fill (binEnergy.begin(), binEnergy.end(), 0.0);
    std::fill (binCount.begin(), binCount.end(), 0);
    integrated.store ((float) floorLufs);
}

void LoudnessMeter::processFrame (const float* frame) noexcept
{
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& c = channels[(size_t) ch];
        const auto weighted = c.highpass.process (c.shelf.process ((double) frame[ch]));
        c.subblockEnergy += weighted * weighted;
    }

    if (++subblockFill >= subblockLength)
        finishSubblock();
}

void LoudnessMeter::process (const float* const* data, int numSamples) noexcept
{
    float frame[maxChannels] {};

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            frame[ch] = data[ch][i];

        processFrame (frame);
    }
}

void LoudnessMeter::finishSubblock() noexcept
{
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& c = channels[(size_t) ch];
        c.history[(size_t) historyPosition] = c.subblockEnergy;
        c.subblockEnergy = 0.0;
    }

    subblockFill = 0;
    ++subblocksSeen;

    auto windowMeanSquare = [this] (int subblocks)
    {
        double total = 0.0;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto& c = channels[(size_t) ch];
            double sum = 0.0;

            for (int i = 0; i < subblocks; ++i)
                sum += c.history[(size_t) ((historyPosition - i + shortTermSubblocks) % shortTermSubblocks)];

            total += sum / ((double) subblocks * (double) subblockLength);
        }

        return total;
    };

    const auto momentaryMs = windowMeanSquare (momentarySubblocks);
    momentary.store (energyToLufs (momentaryMs), std::memory_order_relaxed);
    shortTerm.store (energyToLufs (windowMeanSquare (shortTermSubblocks)), std::memory_order_relaxed);

    if (subblocksSeen >= momentarySubblocks && subblocksSeen % gatingHopSubblocks == 0)
        addGatingBlock (momentaryMs);

    historyPosition = (historyPosition + 1) % shortTermSubblocks;
}

void LoudnessMeter::addGatingBlock (double meanSquare) noexcept
{
    const auto lufs = (double) energyToLufs (meanSquare);

    if (lufs <= absoluteGateLufs)
        return;

    const auto bin = std::clamp ((int) std::floor ((lufs - absoluteGateLufs) * binsPerLu), 0, histogramBins - 1);
    binEnergy[(size_t) bin] += meanSquare;
    ++binCount[(size_t) bin];

    integrated.store (computeIntegrated(), std::memory_order_relaxed);
}

float LoudnessMeter::computeIntegrated() const noexcept
{
    double energy = 0.0;
    long long count = 0;

    for (int i = 0; i < histogramBins; ++i)
    {
        energy += binEnergy[(size_t) i];
        count += binCount[(size_t) i];
    }

    if (count == 0)
        return (float) floorLufs;

    const auto relativeGate = (double) energyToLufs (energy / (double) count) - 10.0;
    const auto firstBin = std::clamp ((int) std::ceil ((relativeGate - absoluteGateLufs) * binsPerLu), 0, histogramBins);

    energy = 0.0;
    count = 0;

    for (int i = firstBin; i < histogramBins; ++i)
    {
        energy += binEnergy[(size_t) i];
        count += binCount[(size_t) i];
    }

    return count == 0 ? (float) floorLufs : energyToLufs (energy / (double) count);
}
}
