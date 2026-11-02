#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "TestUtils.h"
#include "dsp/HalfbandOversampler.h"

namespace
{
using veneer::HalfbandDecimator;
using veneer::HalfbandInterpolator;

constexpr int latency = HalfbandInterpolator::latencySamples;

std::vector<float> roundTrip (const std::vector<float>& in)
{
    HalfbandInterpolator up;
    HalfbandDecimator down;
    std::vector<float> out (in.size());

    for (size_t n = 0; n < in.size(); ++n)
    {
        const auto pair = up.process (in[n]);
        out[n] = down.process (pair.first, pair.second);
    }

    return out;
}
}

TEST_CASE ("an impulse comes back symmetric around the stated latency", "[halfband]")
{
    std::vector<float> impulse (64, 0.0f);
    impulse[0] = 1.0f;
    const auto out = roundTrip (impulse);

    REQUIRE (std::distance (out.begin(), std::max_element (out.begin(), out.end())) == latency);

    for (int j = 1; j <= latency; ++j)
        REQUIRE (std::abs (out[(size_t) (latency + j)] - out[(size_t) (latency - j)]) < 1.0e-6f);
}

TEST_CASE ("the interpolator's delayed tap is the input from latency samples ago", "[halfband]")
{
    const auto in = testutil::noise (0.5, 256, 3);
    HalfbandInterpolator up;

    for (size_t n = 0; n < in.size(); ++n)
    {
        up.process (in[n]);

        if (n >= (size_t) latency)
            REQUIRE (testutil::bitEqual (up.delayed(), in[n - (size_t) latency]));
    }
}

TEST_CASE ("the round trip is a flat delay across the audio band", "[halfband]")
{
    constexpr double fs = 48000.0;

    for (const double f : { 100.0, 1000.0, 5000.0, 10000.0, 15000.0 })
    {
        const auto in = testutil::sine (f, 0.5, fs, 8192);
        const auto out = roundTrip (in);

        double worst = 0.0;

        for (size_t n = 1024; n < in.size(); ++n)
            worst = std::max (worst, (double) std::abs (out[n] - in[n - (size_t) latency]));

        INFO ("frequency " << f << " Hz, largest difference from the delayed input " << worst);
        REQUIRE (worst < 1.0e-3);
    }
}

TEST_CASE ("the decimator rejects what lies above half the base rate", "[halfband]")
{
    constexpr double doubledRate = 96000.0;

    for (const double f : { 32000.0, 36000.0, 40000.0, 46000.0 })
    {
        HalfbandDecimator down;
        double energy = 0.0;
        int counted = 0;

        for (int n = 0; n < 8192; ++n)
        {
            const auto a = (float) std::sin (2.0 * testutil::pi * f * (2 * n) / doubledRate);
            const auto b = (float) std::sin (2.0 * testutil::pi * f * (2 * n + 1) / doubledRate);
            const auto y = down.process (a, b);

            if (n > 256)
            {
                energy += (double) y * y;
                ++counted;
            }
        }

        const auto attenuation = 10.0 * std::log10 (energy / counted / 0.5);

        INFO ("frequency " << f << " Hz, " << attenuation << " dB");
        REQUIRE (attenuation < -65.0);
    }
}
