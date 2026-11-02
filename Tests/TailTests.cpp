#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <utility>
#include <vector>

#include "PluginProcessor.h"
#include "TestUtils.h"
#include "dsp/Engine.h"

namespace
{
veneer::EngineParams extreme()
{
    veneer::EngineParams p;
    p.inputDb = 12.0f;
    p.amountPercent = 200.0f;
    p.weightDb = p.bodyDb = p.airDb = 6.0f;
    p.pushDb = 12.0f;
    return p;
}

// seconds from the end of the burst to the last sample at or above the threshold
double lastAudibleSeconds (double fs, const veneer::EngineParams& params, const std::vector<float>& burst, double thresholdDb)
{
    veneer::Engine engine;
    engine.prepare (fs, 2);

    const auto burstLength = (int) burst.size();
    const auto total = burstLength + (int) ((veneer::Engine::tailSeconds + 0.25) * fs);
    std::vector<float> padded (burst);
    padded.resize ((size_t) total, 0.0f);

    const auto out = testutil::render (engine, params, testutil::dualMono (padded));
    const auto threshold = (float) testutil::dbToLinear (thresholdDb);
    int last = burstLength;

    for (int n = burstLength; n < total; ++n)
        if (std::abs (out.left[(size_t) n]) >= threshold || std::abs (out.right[(size_t) n]) >= threshold)
            last = n;

    return (double) (last - burstLength) / fs;
}
}

TEST_CASE ("nothing above -100 dBFS is left once the reported tail has passed", "[engine]")
{
    for (const double fs : { 44100.0, 48000.0, 192000.0 })
    {
        const auto n = (int) (0.5 * fs);

        std::vector<float> impulse ((size_t) n, 0.0f);
        impulse.back() = 1.0f;

        const std::vector<std::pair<const char*, std::vector<float>>> bursts {
            { "pink noise", testutil::pinkNoise (0.5, n, 21) },
            { "45 Hz tone", testutil::sine (45.0, 0.5, fs, n) },
            { "8 kHz tone", testutil::sine (8000.0, 0.5, fs, n) },
            { "impulse", impulse },
        };

        for (const auto& [name, burst] : bursts)
        {
            for (const auto& params : { veneer::EngineParams(), extreme() })
            {
                INFO (name << " at " << fs << " Hz, push " << params.pushDb);
                REQUIRE (lastAudibleSeconds (fs, params, burst, -100.0) <= veneer::Engine::tailSeconds);
            }
        }
    }
}

TEST_CASE ("the processor reports the tail", "[processor]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;

    REQUIRE (processor.getTailLengthSeconds() >= 0.25);
    REQUIRE (processor.getTailLengthSeconds() == veneer::Engine::tailSeconds);
}
