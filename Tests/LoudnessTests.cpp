#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "TestUtils.h"
#include "dsp/Loudness.h"

namespace
{
using testutil::appendSilence;
using testutil::appendTone;

struct Trace
{
    std::vector<float> momentary, shortTerm;
    float integrated = -100.0f;
};

Trace measure (const std::vector<float>& mono, double fs, int channels = 2)
{
    veneer::LoudnessMeter meter;
    meter.prepare (fs, channels);

    const auto hop = (int) std::lround (0.01 * fs);
    Trace trace;
    float frameData[2] = {};

    for (int n = 0; n < (int) mono.size(); ++n)
    {
        frameData[0] = frameData[1] = mono[(size_t) n];
        meter.processFrame (frameData);

        if ((n + 1) % hop == 0)
        {
            trace.momentary.push_back (meter.getMomentary());
            trace.shortTerm.push_back (meter.getShortTerm());
        }
    }

    trace.integrated = meter.getIntegrated();
    return trace;
}

float maxOf (const std::vector<float>& v, size_t from, size_t to)
{
    return *std::max_element (v.begin() + (long) from, v.begin() + (long) std::min (to, v.size()));
}

constexpr double fs = 48000.0;
}

TEST_CASE ("K-weighting at 48 kHz matches the ITU-R BS.1770-4 coefficients", "[loudness]")
{
    const auto k = veneer::makeKWeighting (48000.0);

    REQUIRE (std::abs (k.shelf.b0 - 1.53512485958697) < 1.0e-6);
    REQUIRE (std::abs (k.shelf.b1 + 2.69169618940638) < 1.0e-6);
    REQUIRE (std::abs (k.shelf.b2 - 1.19839281085285) < 1.0e-6);
    REQUIRE (std::abs (k.shelf.a1 + 1.69065929318241) < 1.0e-6);
    REQUIRE (std::abs (k.shelf.a2 - 0.73248077421585) < 1.0e-6);

    REQUIRE (std::abs (k.highpass.a1 + 1.99004745483398) < 1.0e-6);
    REQUIRE (std::abs (k.highpass.a2 - 0.99007225036621) < 1.0e-6);
}

TEST_CASE ("EBU Tech 3341 case 1 and 2: steady tones read M, S and I", "[loudness][ebu]")
{
    for (const double levelDb : { -23.0, -33.0 })
    {
        std::vector<float> x;
        double phase = 0.0;
        appendTone (x, phase, fs, levelDb, 20.0);

        const auto t = measure (x, fs);

        INFO ("level " << levelDb);
        REQUIRE (std::abs (t.momentary.back() - levelDb) <= 0.1);
        REQUIRE (std::abs (t.shortTerm.back() - levelDb) <= 0.1);
        REQUIRE (std::abs (t.integrated - levelDb) <= 0.1);
    }
}

TEST_CASE ("EBU Tech 3341 cases 3, 4 and 5: gated integrated loudness", "[loudness][ebu]")
{
    struct Step { double db, seconds; };

    const std::vector<std::vector<Step>> cases = {
        { { -36.0, 10.0 }, { -23.0, 60.0 }, { -36.0, 10.0 } },
        { { -72.0, 10.0 }, { -36.0, 10.0 }, { -23.0, 60.0 }, { -36.0, 10.0 }, { -72.0, 10.0 } },
        { { -26.0, 20.0 }, { -20.0, 20.1 }, { -26.0, 20.0 } },
    };

    int index = 3;

    for (const auto& steps : cases)
    {
        std::vector<float> x;
        double phase = 0.0;

        for (const auto& s : steps)
            appendTone (x, phase, fs, s.db, s.seconds);

        const auto t = measure (x, fs);

        INFO ("case " << index++ << " integrated " << t.integrated);
        REQUIRE (std::abs (t.integrated + 23.0f) <= 0.1f);
    }
}

TEST_CASE ("EBU Tech 3341 case 9: short-term is constant after 3 s", "[loudness][ebu]")
{
    std::vector<float> x;
    double phase = 0.0;

    for (int i = 0; i < 5; ++i)
    {
        appendTone (x, phase, fs, -20.0, 1.34);
        appendTone (x, phase, fs, -30.0, 1.66);
    }

    const auto t = measure (x, fs);

    for (size_t i = 300; i < t.shortTerm.size(); ++i)
        REQUIRE (std::abs (t.shortTerm[i] + 23.0f) <= 0.1f);
}

TEST_CASE ("EBU Tech 3341 case 12: momentary is constant after 1 s", "[loudness][ebu]")
{
    std::vector<float> x;
    double phase = 0.0;

    for (int i = 0; i < 25; ++i)
    {
        appendTone (x, phase, fs, -20.0, 0.18);
        appendTone (x, phase, fs, -30.0, 0.22);
    }

    const auto t = measure (x, fs);

    for (size_t i = 100; i < t.momentary.size(); ++i)
        REQUIRE (std::abs (t.momentary[i] + 23.0f) <= 0.1f);
}

TEST_CASE ("EBU Tech 3341 cases 10 and 13: file-based maximum readings", "[loudness][ebu]")
{
    for (int i = 0; i < 20; ++i)
    {
        std::vector<float> shortTermCase, momentaryCase;
        double phase = 0.0;

        appendSilence (shortTermCase, fs, i * 0.15);
        appendTone (shortTermCase, phase, fs, -23.0, 3.0);
        appendSilence (shortTermCase, fs, 1.0);

        appendSilence (momentaryCase, fs, i * 0.02);
        appendTone (momentaryCase, phase, fs, -23.0, 0.4);
        appendSilence (momentaryCase, fs, 1.0);

        INFO ("segment " << i);
        REQUIRE (std::abs (maxOf (measure (shortTermCase, fs).shortTerm, 0, 100000) + 23.0f) <= 0.1f);
        REQUIRE (std::abs (maxOf (measure (momentaryCase, fs).momentary, 0, 100000) + 23.0f) <= 0.1f);
    }
}

TEST_CASE ("EBU Tech 3341 cases 11 and 14: live maximum readings per segment", "[loudness][ebu]")
{
    std::vector<float> shortTermCase, momentaryCase;
    double phase = 0.0;

    for (int i = 0; i < 20; ++i)
    {
        appendSilence (shortTermCase, fs, i * 0.15);
        appendTone (shortTermCase, phase, fs, -38.0 + i, 3.0);
        appendSilence (shortTermCase, fs, 3.0 - i * 0.15);

        appendSilence (momentaryCase, fs, i * 0.02);
        appendTone (momentaryCase, phase, fs, -38.0 + i, 0.4);
        appendSilence (momentaryCase, fs, 0.4 - i * 0.02);
    }

    const auto s = measure (shortTermCase, fs);
    const auto m = measure (momentaryCase, fs);

    for (int i = 0; i < 20; ++i)
    {
        INFO ("segment " << i);
        REQUIRE (std::abs (maxOf (s.shortTerm, (size_t) i * 600, (size_t) (i + 1) * 600) - (-38.0f + (float) i)) <= 0.1f);
        REQUIRE (std::abs (maxOf (m.momentary, (size_t) i * 80, (size_t) (i + 1) * 80) - (-38.0f + (float) i)) <= 0.1f);
    }
}

TEST_CASE ("a 1 kHz tone reads the same at other sample rates", "[loudness]")
{
    for (const double rate : { 44100.0, 88200.0, 96000.0, 192000.0 })
    {
        std::vector<float> x;
        double phase = 0.0;
        appendTone (x, phase, rate, -23.0, 6.0);

        const auto t = measure (x, rate);

        INFO ("rate " << rate);
        REQUIRE (std::abs (t.shortTerm.back() + 23.0f) <= 0.1f);
        REQUIRE (std::abs (t.integrated + 23.0f) <= 0.1f);
    }
}

TEST_CASE ("a mono input counts as one channel", "[loudness]")
{
    std::vector<float> x;
    double phase = 0.0;
    appendTone (x, phase, fs, -23.0, 6.0);

    const auto t = measure (x, fs, 1);
    REQUIRE (std::abs (t.shortTerm.back() - (-23.0f - 3.01f)) <= 0.1f);
}

TEST_CASE ("reset clears the integrated measurement", "[loudness]")
{
    veneer::LoudnessMeter meter;
    meter.prepare (fs, 2);

    std::vector<float> x;
    double phase = 0.0;
    appendTone (x, phase, fs, -23.0, 5.0);

    for (const auto v : x)
    {
        const float frame[2] = { v, v };
        meter.processFrame (frame);
    }

    REQUIRE (meter.getIntegrated() > -30.0f);
    meter.resetIntegrated();
    REQUIRE (meter.getIntegrated() <= -99.0f);
}
