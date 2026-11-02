#include "Limiter.h"

#include <algorithm>
#include <cmath>

namespace veneer
{
namespace
{
constexpr float lookaheadMs = 2.0f;
constexpr float releaseFastMs = 30.0f;
constexpr float releaseSlowMs = 400.0f;
constexpr float sustainTimeMs = 200.0f;
constexpr float sustainFullDb = 4.0f;

// -0.3 dB: the detector read up to 0.27 dB low on heavily limited programme material (see docs/DSP.md)
constexpr float detectionMargin = 0.9660f;

float coefficient (float timeMs, double sampleRate)
{
    return (float) std::exp (-1.0 / (0.001 * (double) timeMs * sampleRate));
}
}

void LookaheadLimiter::prepare (double sampleRate, int channels)
{
    numChannels = std::clamp (channels, 1, maxChannels);

    const auto window = std::max (8, (int) std::lround (0.001 * (double) lookaheadMs * sampleRate));
    latency = (window - 1) + TruePeakDetector::groupDelaySamples;

    slidingMin.prepare (window);
    smoother.prepare (window);

    for (auto& d : delay)
        d.assign ((size_t) latency, 0.0f);

    releaseFastCoef = coefficient (releaseFastMs, sampleRate);
    releaseSlowCoef = coefficient (releaseSlowMs, sampleRate);
    sustainCoef = coefficient (sustainTimeMs, sampleRate);

    reset();
}

void LookaheadLimiter::reset() noexcept
{
    detector.reset();
    slidingMin.reset();
    smoother.reset (1.0f);

    for (auto& d : delay)
        std::fill (d.begin(), d.end(), 0.0f);

    delayPosition = 0;
    previousRequired = 1.0f;
    envelope = 1.0f;
    sustainDb = 0.0f;
}

void LookaheadLimiter::setCeilingDb (float db) noexcept
{
    ceilingLinear = std::pow (10.0f, db / 20.0f);
}

float LookaheadLimiter::process (const float* in, float* out) noexcept
{
    float peak = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
        peak = std::max (peak, detector.process (ch, in[ch]));

    const auto limit = ceilingLinear * detectionMargin;
    const auto required = peak > limit ? limit / peak : 1.0f;
    const auto guarded = std::min (required, previousRequired);
    previousRequired = required;

    const auto windowMin = slidingMin.push (guarded);

    if (windowMin < envelope)
    {
        envelope = windowMin;
    }
    else
    {
        const auto mix = std::clamp (sustainDb / sustainFullDb, 0.0f, 1.0f);
        const auto releaseCoef = releaseFastCoef + (releaseSlowCoef - releaseFastCoef) * mix;
        envelope = windowMin + (envelope - windowMin) * releaseCoef;
    }

    const auto envelopeDb = envelope < 1.0f ? -20.0f * std::log10 (envelope) : 0.0f;
    sustainDb = sustainCoef * sustainDb + (1.0f - sustainCoef) * envelopeDb;

    const auto gain = std::min (1.0f, smoother.push (envelope));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& d = delay[(size_t) ch];
        const auto delayed = d[delayPosition];
        d[delayPosition] = in[ch];
        out[ch] = std::clamp (delayed * gain, -ceilingLinear, ceilingLinear);
    }

    delayPosition = (delayPosition + 1) % (size_t) latency;

    return gain < 1.0f ? -20.0f * std::log10 (gain) : 0.0f;
}
}
