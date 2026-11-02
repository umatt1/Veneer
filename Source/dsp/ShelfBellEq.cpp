#include "ShelfBellEq.h"

namespace veneer
{
void ShelfBellEq::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void ShelfBellEq::reset() noexcept
{
    for (auto& channel : filters)
        for (auto& f : channel)
            f.reset();
}

void ShelfBellEq::setSections (const std::array<Section, 3>& sections) noexcept
{
    const auto& [low, bell, high] = sections;

    const std::array<Biquad, 3> coefficients {
        makeLowShelf (sampleRate, (double) low.frequencyHz, (double) low.q, (double) low.gainDb),
        makePeak (sampleRate, (double) bell.frequencyHz, (double) bell.q, (double) bell.gainDb),
        makeHighShelf (sampleRate, (double) high.frequencyHz, (double) high.q, (double) high.gainDb),
    };

    for (auto& channel : filters)
        for (size_t i = 0; i < coefficients.size(); ++i)
            channel[i].setCoefficients (coefficients[i]);
}

float ShelfBellEq::process (int channel, float in) noexcept
{
    auto& f = filters[(size_t) channel];
    return (float) f[2].process (f[1].process (f[0].process ((double) in)));
}
}
