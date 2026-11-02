#pragma once

#include <array>
#include <atomic>
#include <vector>

#include "Biquad.h"

namespace veneer
{
struct KWeightingCoefficients
{
    Biquad shelf, highpass;
};

// ITU-R BS.1770-4 K-weighting for an arbitrary sample rate
KWeightingCoefficients makeKWeighting (double sampleRate);

class LoudnessMeter
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, int numChannels);
    void reset();
    void resetIntegrated();

    void processFrame (const float* frame) noexcept;
    void process (const float* const* channels, int numSamples) noexcept;

    float getMomentary() const noexcept { return momentary.load (std::memory_order_relaxed); }
    float getShortTerm() const noexcept { return shortTerm.load (std::memory_order_relaxed); }
    float getIntegrated() const noexcept { return integrated.load (std::memory_order_relaxed); }

private:
    static constexpr int momentarySubblocks = 40;
    static constexpr int shortTermSubblocks = 300;
    static constexpr int gatingHopSubblocks = 10;
    static constexpr int histogramBins = 8000;

    void finishSubblock() noexcept;
    void addGatingBlock (double meanSquare) noexcept;
    float computeIntegrated() const noexcept;

    struct Channel
    {
        Biquad shelf, highpass;
        double subblockEnergy = 0.0;
        std::array<double, shortTermSubblocks> history {};
    };

    std::array<Channel, maxChannels> channels;
    int numChannels = 2;
    int subblockLength = 480, subblockFill = 0, subblocksSeen = 0, integratedSubblocks = 0, historyPosition = 0;

    std::vector<double> binEnergy;
    std::vector<int> binCount;

    std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, integrated { -100.0f };
};

float energyToLufs (double meanSquareSum) noexcept;
}
