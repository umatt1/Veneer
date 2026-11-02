#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdio>
#include <vector>

#include "TestUtils.h"
#include "dsp/TruePeak.h"

namespace
{
double detectedPeak (const std::vector<float>& x, int from = 0, int to = -1)
{
    veneer::TruePeakDetector detector;
    float best = 0.0f;

    for (int n = 0; n < (int) x.size(); ++n)
    {
        const auto out = detector.process (0, x[(size_t) n]);

        if (n >= from && (to < 0 || n < to))
            best = std::max (best, out);
    }

    return best;
}

// EBU Tech 3341 test signals 15-19: tapered with a 10 ms fade at each end
std::vector<float> ebuTruePeakSignal (double fs, double freqHz, double amplitude, double phaseDegrees)
{
    const auto n = (int) (0.5 * fs);
    const auto fade = (int) (0.01 * fs);
    auto x = testutil::sine (freqHz, amplitude, fs, n, phaseDegrees * testutil::pi / 180.0);

    for (int i = 0; i < fade; ++i)
    {
        const auto w = 0.5 - 0.5 * std::cos (testutil::pi * i / fade);
        x[(size_t) i] *= (float) w;
        x[(size_t) (n - 1 - i)] *= (float) w;
    }

    return x;
}
}

TEST_CASE ("impulse appears after the stated group delay at full amplitude", "[truepeak]")
{
    veneer::TruePeakDetector detector;

    for (int n = 0; n < 64; ++n)
    {
        const auto out = detector.process (0, n == 0 ? 1.0f : 0.0f);

        if (n == veneer::TruePeakDetector::groupDelaySamples)
            REQUIRE (std::abs (out - 1.0f) < 1.0e-6f);
        else if (n < veneer::TruePeakDetector::groupDelaySamples - 1)
            REQUIRE (out < 0.7f);
    }
}

TEST_CASE ("EBU Tech 3341 true-peak test signals 15 to 19 read within tolerance", "[truepeak]")
{
    struct Case { const char* name; double divisor; double amplitude; double phase; double expectedDb; };

    const Case cases[] = {
        { "15", 4.0, 0.50, 0.0,  -6.0 },
        { "16", 4.0, 0.50, 45.0, -6.0 },
        { "17", 6.0, 0.50, 60.0, -6.0 },
        { "18", 8.0, 0.50, 67.5, -6.0 },
        { "19", 4.0, 1.41, 45.0,  3.0 },
    };

    for (const double fs : { 44100.0, 48000.0, 96000.0 })
    {
        for (const auto& c : cases)
        {
            const auto measuredDb = testutil::linearToDb (detectedPeak (ebuTruePeakSignal (fs, fs / c.divisor, c.amplitude, c.phase)));
            const auto error = measuredDb - c.expectedDb;

            INFO ("case " << c.name << " at " << fs << " Hz measured " << measuredDb << " dBTP");
            REQUIRE (error <= 0.2);
            REQUIRE (error >= -0.4);
        }
    }
}

TEST_CASE ("detector tracks an independent reference across the band", "[truepeak]")
{
    constexpr double fs = 48000.0;
    double worstUnder = 0.0, worstOver = 0.0;

    for (double f = 500.0; f <= 0.45 * fs; f += 1237.0)
    {
        const auto x = testutil::sine (f, 0.8, fs, 4800, 0.7);
        const auto delay = veneer::TruePeakDetector::groupDelaySamples;
        const auto reference = testutil::linearToDb (testutil::referenceTruePeak (x, 60, 4700));
        const auto detected = testutil::linearToDb (detectedPeak (x, 60 + delay, 4700 + delay));
        const auto error = detected - reference;

        worstUnder = std::min (worstUnder, error);
        worstOver = std::max (worstOver, error);

        if (f <= 0.40 * fs)
            REQUIRE (error > -0.4);
    }

    std::printf ("true-peak detector error vs reference: worst under %.3f dB, worst over %.3f dB\n", worstUnder, worstOver);
    REQUIRE (worstOver < 0.1);
}

TEST_CASE ("detector never under-reads noise confined to the top of the band", "[truepeak]")
{
    // noise between 21 kHz and Nyquist sits in the interpolator's transition band. Reading high there is
    // the safe side for a limiter; the design trades a little of that for a narrower transition.
    constexpr int n = 65536;
    const auto delay = veneer::TruePeakDetector::groupDelaySamples;
    const auto end = n - std::max (delay, 48);
    double worst = 100.0, best = -100.0;

    for (const unsigned seed : { 1u, 2u, 3u })
    {
        auto x = testutil::noise (1.0, n, seed);
        float stages[4] = {};

        for (int i = 0; i < n; ++i)
        {
            float v = x[(size_t) i];

            for (auto& s : stages)
            {
                s += 0.35f * (v - s);
                v = s;
            }

            x[(size_t) i] = (i % 2 == 0 ? v : -v);
        }

        const auto scale = 0.9f / (float) testutil::samplePeak (x, 0, n);

        for (auto& v : x)
            v *= scale;

        const auto reference = testutil::linearToDb (testutil::referenceTruePeak (x, 64, end));
        const auto detected = testutil::linearToDb (detectedPeak (x, 64 + delay, n));

        std::printf ("true-peak detector on noise above 21 kHz, seed %u: %+.3f dB against the reference\n", seed, detected - reference);
        worst = std::min (worst, detected - reference);
        best = std::max (best, detected - reference);
    }

    REQUIRE (worst > -0.1);
    REQUIRE (best < 0.6);
}

TEST_CASE ("detector never reads below the sample peak", "[truepeak]")
{
    const auto x = testutil::noise (0.9, 48000, 5);
    veneer::TruePeakDetector detector;
    float best = 0.0f;

    for (const auto v : x)
        best = std::max (best, detector.process (0, v));

    REQUIRE (best >= (float) testutil::samplePeak (x, 0, 48000 - veneer::TruePeakDetector::groupDelaySamples) - 1.0e-4f);
}
