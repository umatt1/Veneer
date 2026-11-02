#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdio>

#include "TestUtils.h"
#include "dsp/Engine.h"

TEST_CASE ("engine speed", "[.perf]")
{
    for (const double fs : { 48000.0, 96000.0, 192000.0 })
    {
        veneer::Engine engine;
        engine.prepare (fs, 2);

        const auto seconds = 20;
        const auto n = (int) (seconds * fs);
        const testutil::Stereo signal { testutil::pinkNoise (0.5, n, 1), testutil::pinkNoise (0.5, n, 2) };

        veneer::EngineParams params;
        params.pushDb = 6.0f;

        const auto start = std::chrono::steady_clock::now();
        testutil::render (engine, params, signal, 512);
        const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();

        std::printf ("%6.0f Hz stereo: %.1fx realtime, %.2f%% of one core\n", fs, seconds / elapsed, 100.0 * elapsed / seconds);
    }
}

TEST_CASE ("engine speed by block size", "[.perf]")
{
    constexpr double fs = 48000.0;

    for (const int blockSize : { 1, 8, 16, 32, 64, 128, 512, 4096 })
    {
        veneer::Engine engine;
        engine.prepare (fs, 2);

        const auto seconds = blockSize < 8 ? 4 : 10;
        const auto n = (int) (seconds * fs);
        const testutil::Stereo signal { testutil::pinkNoise (0.5, n, 1), testutil::pinkNoise (0.5, n, 2) };

        veneer::EngineParams params;
        params.pushDb = 6.0f;

        const auto start = std::chrono::steady_clock::now();
        testutil::render (engine, params, signal, blockSize);
        const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();

        std::printf ("block %5d at 48 kHz: %.1fx realtime, %.2f%% of one core\n", blockSize, seconds / elapsed, 100.0 * elapsed / seconds);
    }
}
