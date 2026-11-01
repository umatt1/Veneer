#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "TestUtils.h"
#include "dsp/BandCompressor.h"
#include "dsp/Voicing.h"

namespace
{
constexpr double fs = 48000.0;

veneer::BandCompressor::Settings testSettings()
{
    veneer::BandCompressor::Settings s;
    s.thresholdDb = -20.0f;
    s.ratio = 3.0f;
    s.kneeDb = 6.0f;
    s.detectorMs = 10.0f;
    s.attackMs = 5.0f;
    s.releaseFastMs = 50.0f;
    s.releaseSlowMs = 400.0f;
    return s;
}

double steadyStateReduction (double peakLevelDb, double seconds = 3.0)
{
    veneer::BandCompressor compressor;
    compressor.prepare (fs);
    compressor.setSettings (testSettings());

    const auto amplitude = testutil::dbToLinear (peakLevelDb);
    const auto n = (int) (seconds * fs);
    double sum = 0.0;
    int counted = 0;

    for (int i = 0; i < n; ++i)
    {
        const auto x = amplitude * std::sin (2.0 * testutil::pi * 997.0 * i / fs);
        compressor.process ((float) (x * x));

        if (i >= n - (int) (0.5 * fs))
        {
            sum += compressor.getGainReductionDb();
            ++counted;
        }
    }

    return sum / counted;
}
}

TEST_CASE ("ratio of one never changes the gain", "[compressor]")
{
    veneer::BandCompressor compressor;
    compressor.prepare (fs);

    auto s = testSettings();
    s.ratio = 1.0f;
    compressor.setSettings (s);

    const auto x = testutil::noise (1.0, 48000);

    for (const auto v : x)
        REQUIRE (testutil::bitEqual (compressor.process (v * v), 1.0f));
}

TEST_CASE ("static curve follows threshold, knee and ratio", "[compressor]")
{
    // below the knee: untouched
    REQUIRE (std::abs (steadyStateReduction (-35.0)) < 0.05);

    // centre of the knee: (1 - 1/R) * W / 8 = 0.5 dB
    REQUIRE (std::abs (steadyStateReduction (-20.0) - 0.5) < 0.1);

    // above the knee: (1 - 1/R) * (level - threshold)
    REQUIRE (std::abs (steadyStateReduction (-8.0) - 8.0) < 0.15);
    REQUIRE (std::abs (steadyStateReduction (0.0) - 13.333) < 0.2);
}

TEST_CASE ("attack settles without overshoot", "[compressor]")
{
    veneer::BandCompressor compressor;
    compressor.prepare (fs);
    compressor.setSettings (testSettings());

    const auto amplitude = testutil::dbToLinear (-8.0);
    double previous = 0.0, maximum = 0.0;

    for (int i = 0; i < (int) (0.5 * fs); ++i)
    {
        const auto x = amplitude * std::sin (2.0 * testutil::pi * 997.0 * i / fs);
        compressor.process ((float) (x * x));
        maximum = std::max (maximum, (double) compressor.getGainReductionDb());
        previous = compressor.getGainReductionDb();
    }

    REQUIRE (previous > 7.6);
    REQUIRE (maximum < 8.3);
}

TEST_CASE ("release is slower after sustained compression than after a short burst", "[compressor]")
{
    auto timeToHalf = [] (double burstSeconds)
    {
        veneer::BandCompressor compressor;
        compressor.prepare (fs);
        compressor.setSettings (testSettings());

        const auto amplitude = testutil::dbToLinear (-8.0);

        for (int i = 0; i < (int) (burstSeconds * fs); ++i)
        {
            const auto x = amplitude * std::sin (2.0 * testutil::pi * 997.0 * i / fs);
            compressor.process ((float) (x * x));
        }

        const auto start = compressor.getGainReductionDb();
        int samples = 0;

        while (compressor.getGainReductionDb() > 0.5 * start && samples < (int) (5.0 * fs))
        {
            compressor.process (0.0f);
            ++samples;
        }

        return (double) samples / fs;
    };

    const auto shortRelease = timeToHalf (0.05);
    const auto sustainedRelease = timeToHalf (3.0);

    INFO ("short " << shortRelease << " s, sustained " << sustainedRelease << " s");
    REQUIRE (sustainedRelease > 2.0 * shortRelease);
}

TEST_CASE ("silence and tiny values stay finite and at unity", "[compressor]")
{
    veneer::BandCompressor compressor;
    compressor.prepare (fs);
    compressor.setSettings (testSettings());

    for (int i = 0; i < 48000; ++i)
        REQUIRE (testutil::bitEqual (compressor.process (i % 2 == 0 ? 0.0f : 1.0e-30f), 1.0f));
}

TEST_CASE ("amount zero gives ratio one for every band", "[voicing]")
{
    for (int band = 0; band < 3; ++band)
        REQUIRE (testutil::bitEqual (veneer::voicing::compressorSettings (band, 0.0f).ratio, 1.0f));

    REQUIRE (veneer::voicing::compressorSettings (0, 200.0f).ratio > veneer::voicing::compressorSettings (0, 100.0f).ratio);
}
