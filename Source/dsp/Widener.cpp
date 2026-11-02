#include "Widener.h"

namespace veneer
{
void Widener::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void Widener::reset() noexcept
{
    side.reset();
}

void Widener::setShelf (float frequencyHz, float q, float gainDb) noexcept
{
    side.setCoefficients (makeHighShelf (sampleRate, (double) frequencyHz, (double) q, (double) gainDb));
}

void Widener::process (float& left, float& right) noexcept
{
    const auto mid = 0.5 * ((double) left + (double) right);
    const auto widened = side.process (0.5 * ((double) left - (double) right));

    left = (float) (mid + widened);
    right = (float) (mid - widened);
}
}
