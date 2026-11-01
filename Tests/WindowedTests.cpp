#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <random>
#include <vector>

#include "TestUtils.h"
#include "dsp/Windowed.h"

TEST_CASE ("SlidingMin matches a brute-force minimum", "[windowed]")
{
    for (const int window : { 1, 2, 7, 96, 384 })
    {
        veneer::SlidingMin filter;
        filter.prepare (window);

        std::mt19937 rng (42);
        std::uniform_real_distribution<float> dist (0.0f, 1.0f);
        std::vector<float> history;

        for (int n = 0; n < 5000; ++n)
        {
            const auto value = dist (rng);
            history.push_back (value);

            const auto first = std::max (0, n - window + 1);
            const auto expected = *std::min_element (history.begin() + first, history.end());

            REQUIRE (testutil::bitEqual (filter.push (value), expected));
        }
    }
}

TEST_CASE ("MovingAverage matches a brute-force mean and starts at its fill value", "[windowed]")
{
    constexpr int window = 96;

    veneer::MovingAverage average;
    average.prepare (window, 1.0f);

    std::mt19937 rng (7);
    std::uniform_real_distribution<float> dist (0.0f, 1.0f);
    std::vector<float> history (window, 1.0f);

    for (int n = 0; n < 5000; ++n)
    {
        const auto value = dist (rng);
        history.push_back (value);

        double sum = 0.0;

        for (int k = 0; k < window; ++k)
            sum += history[history.size() - 1 - (size_t) k];

        REQUIRE (std::abs (average.push (value) - (float) (sum / window)) < 1.0e-5f);
    }

    average.reset (1.0f);
    REQUIRE (testutil::bitEqual (average.push (1.0f), 1.0f));
}
