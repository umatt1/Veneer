#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdio>
#include <vector>

#include "TestUtils.h"
#include "dsp/Engine.h"

namespace
{
constexpr double fs = 48000.0;

veneer::EngineParams neutral()
{
    veneer::EngineParams p;
    p.amountPercent = 0.0f;
    p.pushDb = 0.0f;
    return p;
}

testutil::Stereo process (const veneer::EngineParams& params, const testutil::Stereo& in, veneer::Engine* engineOut = nullptr)
{
    veneer::Engine local;
    auto& engine = engineOut != nullptr ? *engineOut : local;
    engine.prepare (fs, 2);
    return testutil::render (engine, params, in);
}

bool allFinite (const testutil::Stereo& s)
{
    for (const auto v : s.left)
        if (! std::isfinite (v))
            return false;

    for (const auto v : s.right)
        if (! std::isfinite (v))
            return false;

    return true;
}
}

TEST_CASE ("bypass is the input delayed by the reported latency, bit-exact", "[engine]")
{
    auto params = neutral();
    params.bypass = true;

    veneer::Engine engine;
    const auto in = testutil::dualMono (testutil::noise (0.8, 48000, 21));
    const auto out = process (params, in, &engine);
    const auto latency = (size_t) engine.getLatencySamples();

    for (size_t n = latency; n < (size_t) in.size(); ++n)
    {
        REQUIRE (testutil::bitEqual (out.left[n], in.left[n - latency]));
        REQUIRE (testutil::bitEqual (out.right[n], in.right[n - latency]));
    }
}

TEST_CASE ("neutral settings keep the magnitude response flat", "[engine]")
{
    for (const double f : { 40.0, 200.0, 1000.0, 5000.0, 12000.0 })
    {
        const auto in = testutil::dualMono (testutil::sine (f, 0.1, fs, 48000));
        const auto out = process (neutral(), in);

        const auto deviation = testutil::linearToDb (testutil::rms (out.left, 24000, 48000) / testutil::rms (in.left, 24000, 48000));

        INFO ("frequency " << f << " Hz deviation " << deviation << " dB");
        REQUIRE (std::abs (deviation) < 0.1);
    }
}

TEST_CASE ("push adds gain ahead of the limiter", "[engine]")
{
    auto params = neutral();
    params.pushDb = 6.0f;

    const auto in = testutil::dualMono (testutil::sine (1000.0, 0.1, fs, 48000));
    const auto out = process (params, in);

    const auto gain = testutil::linearToDb (testutil::rms (out.left, 24000, 48000) / testutil::rms (in.left, 24000, 48000));
    REQUIRE (std::abs (gain - 6.0) < 0.1);
}

TEST_CASE ("output trim lowers the level", "[engine]")
{
    auto params = neutral();
    params.outputDb = -6.0f;

    const auto in = testutil::dualMono (testutil::sine (1000.0, 0.1, fs, 48000));
    const auto out = process (params, in);

    const auto gain = testutil::linearToDb (testutil::rms (out.left, 24000, 48000) / testutil::rms (in.left, 24000, 48000));
    REQUIRE (std::abs (gain + 6.0) < 0.1);
}

TEST_CASE ("band focus moves only its own band", "[engine]")
{
    auto params = neutral();
    params.bodyDb = 6.0f;

    const auto midIn = testutil::dualMono (testutil::sine (1000.0, 0.1, fs, 48000));
    const auto lowIn = testutil::dualMono (testutil::sine (40.0, 0.1, fs, 48000));

    const auto mid = process (params, midIn);
    const auto low = process (params, lowIn);

    REQUIRE (std::abs (testutil::linearToDb (testutil::rms (mid.left, 24000, 48000) / testutil::rms (midIn.left, 24000, 48000)) - 6.0) < 0.2);
    REQUIRE (std::abs (testutil::linearToDb (testutil::rms (low.left, 24000, 48000) / testutil::rms (lowIn.left, 24000, 48000))) < 0.2);
}

TEST_CASE ("more amount means more gain reduction", "[engine]")
{
    auto reduction = [] (float amount)
    {
        auto params = neutral();
        params.amountPercent = amount;

        veneer::Engine engine;
        process (params, testutil::dualMono (testutil::sine (1000.0, 0.5, fs, 96000)), &engine);
        return engine.getMeters().bandGrDb[1].load();
    };

    const auto none = reduction (0.0f);
    const auto normal = reduction (100.0f);
    const auto heavy = reduction (200.0f);

    INFO ("gain reduction " << none << " / " << normal << " / " << heavy << " dB");
    REQUIRE (testutil::bitEqual (none, 0.0f));
    REQUIRE (normal > 5.0f);
    REQUIRE (heavy > normal + 3.0f);
}

TEST_CASE ("the ceiling holds at extreme settings", "[engine]")
{
    veneer::EngineParams params;
    params.inputDb = 12.0f;
    params.amountPercent = 200.0f;
    params.weightDb = 6.0f;
    params.bodyDb = 6.0f;
    params.airDb = 6.0f;
    params.pushDb = 12.0f;
    params.ceilingDb = -1.0f;

    const auto ceiling = testutil::dbToLinear (-1.0);

    veneer::Engine latencyProbe;
    latencyProbe.prepare (fs, 2);
    const auto from = latencyProbe.getLatencySamples() + 64;

    struct Case { const char* name; testutil::Stereo signal; };

    const Case cases[] = {
        { "pink noise", { testutil::pinkNoise (0.9, 96000, 31), testutil::pinkNoise (0.9, 96000, 32) } },
        { "white noise", { testutil::noise (0.9, 96000, 33), testutil::noise (0.9, 96000, 34) } },
    };

    for (const auto& c : cases)
    {
        const auto out = process (params, c.signal);

        const auto left = testutil::linearToDb (testutil::referenceTruePeak (out.left, from, 96000 - 64)) + 1.0;
        const auto right = testutil::linearToDb (testutil::referenceTruePeak (out.right, from, 96000 - 64)) + 1.0;

        std::printf ("engine, extreme settings, %-12s overshoot L %+.3f dB, R %+.3f dB\n", c.name, left, right);

        CHECK (testutil::samplePeak (out.left, 0, 96000) <= ceiling + 1.0e-6);
        CHECK (left <= testutil::overshootToleranceDb);
        CHECK (right <= testutil::overshootToleranceDb);
    }
}

TEST_CASE ("stereo image is preserved", "[engine]")
{
    auto params = neutral();
    params.amountPercent = 150.0f;
    params.pushDb = 6.0f;

    const testutil::Stereo in { testutil::sine (1000.0, 0.3, fs, 96000), testutil::sine (1000.0, 0.15, fs, 96000) };
    const auto out = process (params, in);

    const auto inRatio = testutil::linearToDb (testutil::rms (in.left, 48000, 96000) / testutil::rms (in.right, 48000, 96000));
    const auto outRatio = testutil::linearToDb (testutil::rms (out.left, 48000, 96000) / testutil::rms (out.right, 48000, 96000));

    REQUIRE (std::abs (inRatio - outRatio) < 0.05);
}

TEST_CASE ("toggling bypass does not click", "[engine]")
{
    veneer::Engine engine;
    engine.prepare (fs, 2);

    auto params = neutral();
    auto in = testutil::dualMono (testutil::sine (1000.0, 0.5, fs, 48000));
    testutil::Stereo out;

    auto first = testutil::Stereo { { in.left.begin(), in.left.begin() + 24000 }, { in.right.begin(), in.right.begin() + 24000 } };
    auto second = testutil::Stereo { { in.left.begin() + 24000, in.left.end() }, { in.right.begin() + 24000, in.right.end() } };

    auto a = testutil::render (engine, params, first);
    params.bypass = true;
    auto b = testutil::render (engine, params, second);

    out.left = a.left;
    out.left.insert (out.left.end(), b.left.begin(), b.left.end());

    double largestStep = 0.0;

    for (size_t n = 1; n < out.left.size(); ++n)
        largestStep = std::max (largestStep, (double) std::abs (out.left[n] - out.left[n - 1]));

    INFO ("largest step " << largestStep);
    REQUIRE (largestStep < 0.15);
}

TEST_CASE ("an output change ramps instead of stepping", "[engine]")
{
    veneer::Engine engine;
    engine.prepare (fs, 2);

    auto params = neutral();
    const auto in = testutil::dualMono (testutil::sine (1000.0, 0.3, fs, 48000));
    const testutil::Stereo first { { in.left.begin(), in.left.begin() + 24000 }, { in.right.begin(), in.right.begin() + 24000 } };
    const testutil::Stereo second { { in.left.begin() + 24000, in.left.end() }, { in.right.begin() + 24000, in.right.end() } };

    auto a = testutil::render (engine, params, first);
    params.outputDb = -12.0f;
    auto b = testutil::render (engine, params, second);

    auto joined = a.left;
    joined.insert (joined.end(), b.left.begin(), b.left.end());

    double largestStep = 0.0;

    for (size_t n = 1; n < joined.size(); ++n)
        largestStep = std::max (largestStep, (double) std::abs (joined[n] - joined[n - 1]));

    REQUIRE (largestStep < 0.1);
}

TEST_CASE ("output stays finite and under the ceiling for hostile input at any setting", "[engine]")
{
    std::vector<testutil::Stereo> inputs;
    inputs.push_back (testutil::dualMono (std::vector<float> (48000, 0.0f)));
    inputs.push_back (testutil::dualMono (std::vector<float> (48000, 0.5f)));
    inputs.push_back (testutil::dualMono (std::vector<float> (48000, 1.0e-40f)));
    inputs.push_back (testutil::Stereo { testutil::noise (1.0, 48000, 41), testutil::noise (1.0, 48000, 42) });

    {
        auto square = testutil::sine (100.0, 1.0, fs, 48000);

        for (auto& v : square)
            v = v >= 0.0f ? 1.0f : -1.0f;

        inputs.push_back (testutil::dualMono (square));
    }

    const auto ceiling = testutil::dbToLinear (-1.0);

    for (const float amount : { 0.0f, 100.0f, 200.0f })
    {
        for (const float push : { 0.0f, 12.0f })
        {
            for (const float input : { -12.0f, 12.0f })
            {
                veneer::EngineParams params;
                params.amountPercent = amount;
                params.pushDb = push;
                params.inputDb = input;

                for (const auto& in : inputs)
                {
                    const auto out = process (params, in);
                    REQUIRE (allFinite (out));
                    REQUIRE (testutil::samplePeak (out.left, 0, out.size()) <= ceiling + 1.0e-6);
                }
            }
        }
    }
}

TEST_CASE ("a mono bus is processed", "[engine]")
{
    veneer::Engine engine;
    engine.prepare (fs, 1);

    auto signal = testutil::sine (1000.0, 0.1, fs, 48000);
    float* pointers[1] = { signal.data() };
    juce::AudioBuffer<float> buffer (pointers, 1, 48000);

    engine.process (buffer, neutral());

    REQUIRE (std::abs (testutil::linearToDb (testutil::rms (signal, 24000, 48000) / (0.1 / std::sqrt (2.0)))) < 0.1);
}

TEST_CASE ("meters read sensible values", "[engine]")
{
    veneer::Engine engine;
    std::vector<float> tone;
    double phase = 0.0;
    testutil::appendTone (tone, phase, fs, -23.0, 6.0);

    process (neutral(), testutil::dualMono (tone), &engine);
    const auto& m = engine.getMeters();

    REQUIRE (std::abs (m.shortTermLufs.load() + 23.0f) < 0.3f);
    REQUIRE (std::abs (m.inputRmsDb.load() + 26.02f) < 0.3f);
    REQUIRE (testutil::bitEqual (m.limiterGrDb.load(), 0.0f));
    REQUIRE (m.truePeakDb.load() < -20.0f);
}
