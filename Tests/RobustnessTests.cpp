#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <vector>

#include "TestUtils.h"
#include "dsp/Engine.h"

namespace
{
using testutil::Stereo;

veneer::EngineParams settings()
{
    veneer::EngineParams p;
    p.pushDb = 6.0f;
    p.weightDb = 2.0f;
    p.airDb = -3.0f;
    return p;
}

veneer::EngineParams extreme()
{
    veneer::EngineParams p;
    p.inputDb = 12.0f;
    p.amountPercent = 200.0f;
    p.weightDb = p.bodyDb = p.airDb = 6.0f;
    p.pushDb = 12.0f;
    return p;
}

// level swells, two different noise floors and a tone, so every stage has something to do
Stereo programme (double fs, double seconds)
{
    const auto n = (int) (seconds * fs);
    const auto noiseLeft = testutil::pinkNoise (0.4, n, 5), noiseRight = testutil::pinkNoise (0.4, n, 6);
    const auto tone = testutil::sine (220.0, 0.3, fs, n);

    Stereo s { std::vector<float> ((size_t) n), std::vector<float> ((size_t) n) };

    for (size_t i = 0; i < (size_t) n; ++i)
    {
        const auto swell = 0.4f + 0.6f * (float) std::abs (std::sin (2.0 * testutil::pi * 0.7 * (double) i / fs));
        s.left[i] = swell * (noiseLeft[i] + tone[i]);
        s.right[i] = swell * (noiseRight[i] + 0.7f * tone[i]);
    }

    return s;
}

Stereo run (double fs, const veneer::EngineParams& params, const Stereo& in, int blockSize = 512)
{
    veneer::Engine engine;
    engine.prepare (fs, 2);
    return testutil::render (engine, params, in, blockSize);
}

bool allFinite (const Stereo& s)
{
    for (const auto* channel : { &s.left, &s.right })
        for (const auto v : *channel)
            if (! std::isfinite (v))
                return false;

    return true;
}

size_t differences (const Stereo& a, const Stereo& b)
{
    size_t count = 0;

    for (size_t i = 0; i < a.left.size(); ++i)
        count += (size_t) (! testutil::bitEqual (a.left[i], b.left[i])) + (size_t) (! testutil::bitEqual (a.right[i], b.right[i]));

    return count;
}

double overshootDb (const Stereo& out, double ceilingDb)
{
    const auto from = 2048, to = out.size() - 64;
    return testutil::linearToDb (std::max (testutil::referenceTruePeak (out.left, from, to),
                                           testutil::referenceTruePeak (out.right, from, to))) - ceilingDb;
}
}

TEST_CASE ("the block size does not change the output", "[robustness]")
{
    const auto in = programme (48000.0, 3.0);
    const auto reference = run (48000.0, settings(), in, 512);

    for (const int blockSize : { 1, 2, 3, 7, 31, 32, 33, 64, 100, 513, 1000, 4096, 1000000 })
    {
        INFO ("block size " << blockSize);
        REQUIRE (differences (run (48000.0, settings(), in, blockSize), reference) == 0);
    }
}

TEST_CASE ("empty blocks between real ones change nothing", "[robustness]")
{
    const auto in = programme (48000.0, 1.0);
    const auto reference = run (48000.0, settings(), in);

    veneer::Engine engine;
    engine.prepare (48000.0, 2);
    auto out = in;
    const int pattern[] = { 0, 100, 0, 1, 511, 0, 0, 37 };
    size_t step = 0;

    for (int start = 0; start < out.size();)
    {
        const auto length = std::min (pattern[step++ % std::size (pattern)], out.size() - start);
        float* pointers[2] = { out.left.data() + start, out.right.data() + start };
        juce::AudioBuffer<float> view (pointers, 2, length);
        engine.process (view, settings());
        start += length;
    }

    REQUIRE (differences (out, reference) == 0);
}

TEST_CASE ("a mono bus gives the same audio as a dual-mono pair", "[robustness]")
{
    const auto in = programme (48000.0, 2.0);
    const auto dual = run (48000.0, settings(), testutil::dualMono (in.left));
    REQUIRE (differences (dual, { dual.left, dual.left }) == 0);

    veneer::Engine engine;
    engine.prepare (48000.0, 1);
    auto mono = in.left;

    for (size_t start = 0; start < mono.size(); start += 512)
    {
        float* pointers[1] = { mono.data() + start };
        juce::AudioBuffer<float> view (pointers, 1, (int) std::min<size_t> (512, mono.size() - start));
        engine.process (view, settings());
    }

    for (size_t i = 0; i < mono.size(); ++i)
        REQUIRE (testutil::bitEqual (mono[i], dual.left[i]));
}

TEST_CASE ("reset puts every stage back to its initial state", "[robustness]")
{
    const auto in = programme (48000.0, 2.0);
    const auto expected = run (48000.0, settings(), in);

    veneer::Engine engine;
    engine.prepare (48000.0, 2);
    testutil::render (engine, extreme(), Stereo { testutil::noise (1.0, 48000, 8), testutil::noise (1.0, 48000, 9) });
    engine.reset();

    REQUIRE (differences (testutil::render (engine, settings(), in), expected) == 0);
}

TEST_CASE ("preparing again at another sample rate gives a clean start", "[robustness]")
{
    const auto in = programme (96000.0, 2.0);

    veneer::Engine engine;
    engine.prepare (48000.0, 2);
    testutil::render (engine, extreme(), programme (48000.0, 1.0));
    engine.prepare (96000.0, 2);

    veneer::Engine fresh;
    fresh.prepare (96000.0, 2);

    REQUIRE (engine.getLatencySamples() == fresh.getLatencySamples());
    REQUIRE (differences (testutil::render (engine, settings(), in), testutil::render (fresh, settings(), in)) == 0);
}

TEST_CASE ("every supported sample rate reports its latency and holds the ceiling", "[robustness]")
{
    for (const double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        INFO ("sample rate " << fs);

        veneer::Engine bypassed;
        bypassed.prepare (fs, 2);
        auto params = settings();
        params.bypass = true;
        const auto noise = Stereo { testutil::noise (0.5, (int) fs, 3), testutil::noise (0.5, (int) fs, 4) };
        const auto delayed = testutil::render (bypassed, params, noise);
        const auto latency = (size_t) bypassed.getLatencySamples();

        for (size_t n = latency; n < noise.left.size(); ++n)
            REQUIRE (testutil::bitEqual (delayed.left[n], noise.left[n - latency]));

        const auto out = run (fs, extreme(), programme (fs, 2.0));
        REQUIRE (allFinite (out));
        REQUIRE (testutil::samplePeak (out.left, 0, out.size()) <= testutil::dbToLinear (-1.0) + 1.0e-6);
        REQUIRE (overshootDb (out, -1.0) < 0.05);
    }
}

TEST_CASE ("the loudness meter reads the same tone at every supported rate", "[robustness]")
{
    for (const double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        veneer::Engine engine;
        engine.prepare (fs, 2);

        std::vector<float> tone;
        double phase = 0.0;
        testutil::appendTone (tone, phase, fs, -23.0, 6.0);

        veneer::EngineParams params;
        params.amountPercent = 0.0f;
        params.pushDb = 0.0f;
        testutil::render (engine, params, testutil::dualMono (tone));

        INFO ("sample rate " << fs);
        REQUIRE (std::abs (engine.getMeters().shortTermLufs.load() + 23.0f) < 0.05f);
        REQUIRE (std::abs (engine.getMeters().integratedLufs.load() + 23.0f) < 0.05f);
    }
}

TEST_CASE ("rates outside the supported range still give finite, limited output", "[robustness]")
{
    for (const double fs : { 8000.0, 11025.0, 16000.0, 22050.0, 32000.0, 352800.0, 384000.0 })
    {
        INFO ("sample rate " << fs);
        const auto out = run (fs, extreme(), programme (fs, 1.0));
        REQUIRE (allFinite (out));
        REQUIRE (testutil::samplePeak (out.left, 0, out.size()) <= testutil::dbToLinear (-1.0) + 1.0e-6);
    }
}

TEST_CASE ("non-finite and enormous input is replaced before it reaches the filters", "[robustness]")
{
    const auto clean = programme (48000.0, 2.0);
    auto hostile = clean, replaced = clean;

    const auto nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
    const auto limit = veneer::Engine::maxInputMagnitude;

    auto inject = [&] (size_t index, float value, float substitute, bool left = true, bool right = true)
    {
        if (left)
        {
            hostile.left[index] = value;
            replaced.left[index] = substitute;
        }

        if (right)
        {
            hostile.right[index] = value;
            replaced.right[index] = substitute;
        }
    };

    for (size_t i = 20000; i < 20004; ++i)
        inject (i, nan, 0.0f);

    inject (30000, inf, 0.0f);
    inject (30001, -inf, 0.0f);
    inject (40000, 1.0e30f, limit);
    inject (40001, -1.0e30f, -limit);
    inject (40002, std::numeric_limits<float>::max(), limit);

    for (size_t i = 50000; i < 50100; ++i)
        inject (i, nan, 0.0f, true, false);

    veneer::Engine engine;
    engine.prepare (48000.0, 2);
    const auto out = testutil::render (engine, settings(), hostile);

    REQUIRE (allFinite (out));
    REQUIRE (testutil::samplePeak (out.left, 0, out.size()) <= testutil::dbToLinear (-1.0) + 1.0e-6);
    REQUIRE (differences (out, run (48000.0, settings(), replaced)) == 0);

    const auto& m = engine.getMeters();

    for (const auto* meter : { &m.inputRmsDb, &m.truePeakDb, &m.momentaryLufs, &m.shortTermLufs, &m.integratedLufs })
        REQUIRE (std::isfinite (meter->load()));
}
