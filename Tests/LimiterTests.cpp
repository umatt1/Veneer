#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include "TestUtils.h"
#include "dsp/Limiter.h"

namespace
{
constexpr double fs = 48000.0;

std::vector<float> run (const std::vector<float>& in, float ceilingDb, int* latencyOut = nullptr)
{
    veneer::LookaheadLimiter limiter;
    limiter.prepare (fs, 1);
    limiter.setCeilingDb (ceilingDb);

    if (latencyOut != nullptr)
        *latencyOut = limiter.getLatencySamples();

    std::vector<float> out (in.size());

    for (size_t n = 0; n < in.size(); ++n)
        limiter.process (&in[n], &out[n]);

    return out;
}

struct Signal
{
    const char* name;
    std::vector<float> samples;
};

std::vector<Signal> hostileSignals()
{
    std::vector<Signal> signals;
    const auto n = (int) (2.0 * fs);

    signals.push_back ({ "pink noise 3.0", testutil::pinkNoise (3.0, n, 5) });
    signals.push_back ({ "fs/4 at 45 degrees 1.41", testutil::sine (fs / 4.0, 1.41, fs, n, testutil::pi / 4.0) });

    {
        std::vector<float> sweep ((size_t) n);
        double phase = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const auto f = 20.0 * std::pow (1000.0, (double) i / n);
            phase += 2.0 * testutil::pi * f / fs;
            sweep[(size_t) i] = (float) (1.5 * std::sin (phase));
        }

        signals.push_back ({ "sweep 20 Hz to 20 kHz 1.5", sweep });
    }

    {
        auto x = testutil::sine (1000.0, 1.0, fs, n);
        for (auto& v : x)
            v = v >= 0.0f ? 2.0f : -2.0f;
        signals.push_back ({ "hard square 2.0", x });
    }

    signals.push_back ({ "white noise 4.0", testutil::noise (4.0, n, 3) });

    {
        std::vector<float> pulses ((size_t) n, 0.0f);
        std::mt19937 rng (11);

        for (int i = 0; i < 40; ++i)
            pulses[(size_t) (1000 + (int) (rng() % (unsigned) (n - 2000)))] = (i % 2 == 0 ? 3.0f : -3.0f);

        signals.push_back ({ "pulses 3.0", pulses });
    }

    return signals;
}
}

TEST_CASE ("an impulse comes out exactly latency samples later", "[limiter]")
{
    std::vector<float> in (4000, 0.0f);
    in[100] = 0.5f;

    int latency = 0;
    const auto out = run (in, -1.0f, &latency);

    for (size_t n = 0; n < out.size(); ++n)
    {
        if ((int) n == 100 + latency)
            REQUIRE (testutil::bitEqual (out[n], 0.5f));
        else
            REQUIRE (testutil::bitEqual (out[n], 0.0f));
    }
}

TEST_CASE ("signals under the ceiling pass bit-exact", "[limiter]")
{
    const auto in = testutil::noise (0.25, 48000, 9);

    int latency = 0;
    const auto out = run (in, -1.0f, &latency);

    for (size_t n = (size_t) latency; n < in.size(); ++n)
        REQUIRE (testutil::bitEqual (out[n], in[n - (size_t) latency]));
}

TEST_CASE ("true peak never exceeds the ceiling on hostile input", "[limiter]")
{
    for (const float ceilingDb : { -1.0f, -0.3f })
    {
        const auto ceilingLinear = testutil::dbToLinear (ceilingDb);

        for (const auto& signal : hostileSignals())
        {
            int latency = 0;
            const auto out = run (signal.samples, ceilingDb, &latency);
            const auto n = (int) out.size();

            const auto truePeakDb = testutil::linearToDb (testutil::referenceTruePeak (out, latency + 64, n - 64));
            const auto overshoot = truePeakDb - ceilingDb;

            std::printf ("limiter %5.1f dBTP  %-28s overshoot %+.3f dB\n", (double) ceilingDb, signal.name, overshoot);

            INFO (signal.name << " at ceiling " << ceilingDb << " dBTP: true peak " << truePeakDb << " dBTP");
            CHECK (overshoot <= testutil::overshootToleranceDb);
            CHECK (testutil::samplePeak (out, 0, n) <= ceilingLinear + 1.0e-6);
        }
    }
}

TEST_CASE ("gain recovers after a burst and never exceeds unity", "[limiter]")
{
    std::vector<float> in;
    double phase = 0.0;
    testutil::appendTone (in, phase, fs, testutil::linearToDb (3.0), 0.3);
    testutil::appendTone (in, phase, fs, testutil::linearToDb (0.1), 4.0);

    int latency = 0;
    const auto out = run (in, -1.0f, &latency);
    const auto n = (int) in.size();

    for (int i = latency; i < n; ++i)
        REQUIRE (std::abs (out[(size_t) i]) <= std::abs (in[(size_t) (i - latency)]) * 1.000001f + 1.0e-9f);

    std::vector<float> delayed (in.begin() + (n - 24000 - latency), in.end() - latency);
    std::vector<float> tail (out.begin() + (n - 24000), out.end());

    REQUIRE (testutil::rms (tail, 0, 24000) / testutil::rms (delayed, 0, 24000) > 0.999);
}

TEST_CASE ("silence, DC and denormals stay finite", "[limiter]")
{
    for (const float value : { 0.0f, 0.5f, 1.0e-40f })
    {
        const auto out = run (std::vector<float> (48000, value), -1.0f);

        for (const auto v : out)
            REQUIRE (std::isfinite (v));
    }
}

TEST_CASE ("stereo channels share one gain", "[limiter]")
{
    const auto n = (int) (3.0 * fs);
    const auto left = testutil::sine (1000.0, 3.0, fs, n);
    const auto right = testutil::sine (1000.0, 0.1, fs, n);

    veneer::LookaheadLimiter limiter;
    limiter.prepare (fs, 2);
    limiter.setCeilingDb (-1.0f);

    std::vector<float> outL ((size_t) n), outR ((size_t) n);

    for (int i = 0; i < n; ++i)
    {
        const float in[2] = { left[(size_t) i], right[(size_t) i] };
        float out[2] = {};
        limiter.process (in, out);
        outL[(size_t) i] = out[0];
        outR[(size_t) i] = out[1];
    }

    const auto inRatioDb = testutil::linearToDb (testutil::rms (right, n / 2, n) / testutil::rms (left, n / 2, n));
    const auto outRatioDb = testutil::linearToDb (testutil::rms (outR, n / 2, n) / testutil::rms (outL, n / 2, n));

    REQUIRE (std::abs (inRatioDb - outRatioDb) < 0.1);
}
