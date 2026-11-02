#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <vector>

#include "TestUtils.h"
#include "dsp/ShelfBellEq.h"
#include "dsp/Voicing.h"
#include "dsp/Widener.h"

namespace
{
constexpr double fs = 48000.0;

std::array<veneer::ShelfBellEq::Section, 3> sections (float lowDb, float bellDb, float highDb)
{
    return { { { 200.0f, 0.7f, lowDb }, { 1000.0f, 1.0f, bellDb }, { 5000.0f, 0.7f, highDb } } };
}

std::vector<double> impulseResponse (const std::array<veneer::ShelfBellEq::Section, 3>& s)
{
    veneer::ShelfBellEq eq;
    eq.prepare (fs);
    eq.setSections (s);

    std::vector<double> h (16384);

    for (size_t n = 0; n < h.size(); ++n)
        h[n] = (double) eq.process (0, n == 0 ? 1.0f : 0.0f);

    return h;
}

// response of the side signal alone: left = +impulse, right = -impulse
std::vector<double> sideImpulseResponse (float gainDb)
{
    veneer::Widener widener;
    widener.prepare (fs);
    widener.setShelf (6000.0f, 0.7f, gainDb);

    std::vector<double> h (16384);

    for (size_t n = 0; n < h.size(); ++n)
    {
        auto left = n == 0 ? 0.5f : 0.0f;
        auto right = -left;
        widener.process (left, right);
        h[n] = (double) left - (double) right;
    }

    return h;
}
}

TEST_CASE ("at 0 dB the equaliser passes the signal bit-exact", "[voicing]")
{
    veneer::ShelfBellEq eq;
    eq.prepare (fs);
    eq.setSections (sections (0.0f, 0.0f, 0.0f));

    const auto in = testutil::noise (0.9, 8192, 17);

    for (const auto v : in)
        REQUIRE (testutil::bitEqual (eq.process (0, v), v));
}

TEST_CASE ("each section reaches its gain where its shape says it must", "[voicing]")
{
    for (const float gain : { 6.0f, -4.0f })
    {
        const auto g = (double) gain;

        const auto low = impulseResponse (sections (gain, 0.0f, 0.0f));
        REQUIRE (std::abs (testutil::magnitudeDb (low, 0.0, fs) - g) < 0.02);
        REQUIRE (std::abs (testutil::magnitudeDb (low, fs / 2.0, fs)) < 0.02);

        const auto bell = impulseResponse (sections (0.0f, gain, 0.0f));
        REQUIRE (std::abs (testutil::magnitudeDb (bell, 1000.0, fs) - g) < 0.02);
        REQUIRE (std::abs (testutil::magnitudeDb (bell, 0.0, fs)) < 0.02);
        REQUIRE (std::abs (testutil::magnitudeDb (bell, fs / 2.0, fs)) < 0.02);

        const auto high = impulseResponse (sections (0.0f, 0.0f, gain));
        REQUIRE (std::abs (testutil::magnitudeDb (high, fs / 2.0, fs) - g) < 0.02);
        REQUIRE (std::abs (testutil::magnitudeDb (high, 0.0, fs)) < 0.02);
    }
}

TEST_CASE ("the tone curve is flat at zero and scales in dB with Amount", "[voicing]")
{
    const auto flat = veneer::voicing::eqSections (0.0f);
    const auto normal = veneer::voicing::eqSections (100.0f);
    const auto heavy = veneer::voicing::eqSections (200.0f);

    for (size_t i = 0; i < flat.size(); ++i)
    {
        REQUIRE (testutil::bitEqual (flat[i].gainDb, 0.0f));
        REQUIRE (std::abs (normal[i].gainDb) > 0.1f);
        REQUIRE (std::abs (heavy[i].gainDb - 2.0f * normal[i].gainDb) < 1.0e-5f);
    }
}

TEST_CASE ("saturator drive is zero at zero Amount, doubles at 200 and rises with a boost", "[voicing]")
{
    for (int band = 0; band < 3; ++band)
    {
        const auto base = veneer::voicing::saturatorDrive (band, 100.0f, 0.0f);

        REQUIRE (testutil::bitEqual (veneer::voicing::saturatorDrive (band, 0.0f, 6.0f), 0.0f));
        REQUIRE (base > 0.1f);
        REQUIRE (std::abs (veneer::voicing::saturatorDrive (band, 200.0f, 0.0f) - 2.0f * base) < 1.0e-5f);
        REQUIRE (veneer::voicing::saturatorDrive (band, 100.0f, 6.0f) > base * 1.3f);
        REQUIRE (testutil::bitEqual (veneer::voicing::saturatorDrive (band, 100.0f, -6.0f), base));
    }
}

TEST_CASE ("the widening boost is zero at zero Amount and grows with it", "[voicing]")
{
    REQUIRE (testutil::bitEqual (veneer::voicing::widenerBoostDb (0.0f), 0.0f));
    REQUIRE (veneer::voicing::widenerBoostDb (100.0f) > 0.5f);
    REQUIRE (std::abs (veneer::voicing::widenerBoostDb (200.0f) - 2.0f * veneer::voicing::widenerBoostDb (100.0f)) < 1.0e-5f);
}

TEST_CASE ("widening never changes the mono sum", "[widener]")
{
    veneer::Widener widener;
    widener.prepare (fs);
    widener.setShelf (6000.0f, 0.7f, 4.0f);

    const auto left = testutil::noise (0.5, 8192, 1);
    const auto right = testutil::noise (0.5, 8192, 2);

    for (size_t n = 0; n < left.size(); ++n)
    {
        auto l = left[n], r = right[n];
        const auto before = (double) l + (double) r;
        widener.process (l, r);

        REQUIRE (std::abs ((double) l + (double) r - before) < 1.0e-6);
    }
}

TEST_CASE ("identical channels are not changed by widening", "[widener]")
{
    veneer::Widener widener;
    widener.prepare (fs);
    widener.setShelf (6000.0f, 0.7f, 6.0f);

    for (const auto v : testutil::noise (0.8, 4096, 4))
    {
        auto l = v, r = v;
        widener.process (l, r);

        REQUIRE (testutil::bitEqual (l, v));
        REQUIRE (testutil::bitEqual (r, v));
    }
}

TEST_CASE ("widening raises the side signal by the set gain above the shelf and not below it", "[widener]")
{
    const auto h = sideImpulseResponse (4.0f);

    REQUIRE (std::abs (testutil::magnitudeDb (h, fs / 2.0, fs) - 4.0) < 0.02);
    REQUIRE (std::abs (testutil::magnitudeDb (h, 0.0, fs)) < 0.02);
    REQUIRE (std::abs (testutil::magnitudeDb (h, 100.0, fs)) < 0.05);
}
