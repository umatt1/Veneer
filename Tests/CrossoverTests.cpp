#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "TestUtils.h"
#include "dsp/Crossover.h"
#include "dsp/Voicing.h"

namespace
{
struct Responses
{
    std::vector<double> low, mid, high, sum;
};

Responses impulseResponses (double sampleRate, int length)
{
    veneer::Crossover crossover;
    crossover.prepare (sampleRate, 1);
    crossover.setFrequencies (veneer::voicing::lowCrossoverHz, veneer::voicing::highCrossoverHz);

    Responses r;

    for (int n = 0; n < length; ++n)
    {
        float low = 0.0f, mid = 0.0f, high = 0.0f;
        crossover.process (0, n == 0 ? 1.0f : 0.0f, low, mid, high);
        r.low.push_back (low);
        r.mid.push_back (mid);
        r.high.push_back (high);
        r.sum.push_back ((double) low + mid + high);
    }

    return r;
}

double magnitudeDb (const std::vector<double>& h, double freqHz, double sampleRate)
{
    std::complex<double> acc;

    for (size_t n = 0; n < h.size(); ++n)
        acc += h[n] * std::polar (1.0, -2.0 * testutil::pi * freqHz * (double) n / sampleRate);

    return testutil::linearToDb (std::abs (acc));
}
}

TEST_CASE ("crossover bands sum to a flat magnitude", "[crossover]")
{
    for (const double fs : { 44100.0, 48000.0, 96000.0 })
    {
        const auto r = impulseResponses (fs, 16384);
        double worst = 0.0;

        for (double f = 20.0; f < 20000.0 && f < 0.45 * fs; f *= 1.1)
            worst = std::max (worst, std::abs (magnitudeDb (r.sum, f, fs)));

        INFO ("fs " << fs << " worst deviation " << worst << " dB");
        REQUIRE (worst < 0.1);
    }
}

TEST_CASE ("each band is 6 dB down at its crossover frequency", "[crossover]")
{
    constexpr double fs = 48000.0;
    const auto r = impulseResponses (fs, 16384);

    REQUIRE (std::abs (magnitudeDb (r.low, veneer::voicing::lowCrossoverHz, fs) + 6.02) < 0.2);
    REQUIRE (std::abs (magnitudeDb (r.mid, veneer::voicing::lowCrossoverHz, fs) + 6.02) < 0.2);
    REQUIRE (std::abs (magnitudeDb (r.mid, veneer::voicing::highCrossoverHz, fs) + 6.02) < 0.2);
    REQUIRE (std::abs (magnitudeDb (r.high, veneer::voicing::highCrossoverHz, fs) + 6.02) < 0.2);
}

TEST_CASE ("bands reject content far from their range", "[crossover]")
{
    constexpr double fs = 48000.0;
    const auto r = impulseResponses (fs, 16384);

    REQUIRE (magnitudeDb (r.low, 5000.0, fs) < -60.0);
    REQUIRE (magnitudeDb (r.high, 200.0, fs) < -60.0);
    REQUIRE (magnitudeDb (r.mid, 30.0, fs) < -30.0);
    REQUIRE (magnitudeDb (r.mid, 15000.0, fs) < -30.0);
}
