#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "Parameters.h"
#include "ui/MeterMath.h"

namespace
{
using veneer::ui::fallen;
using veneer::ui::levelPosition;
using veneer::ui::levelText;
}

TEST_CASE ("level position runs from 0 at the floor to 1 at the ceiling and clamps", "[ui]")
{
    REQUIRE (std::abs (levelPosition (-48.0f, -48.0f, 0.0f)) < 1.0e-6f);
    REQUIRE (std::abs (levelPosition (0.0f, -48.0f, 0.0f) - 1.0f) < 1.0e-6f);
    REQUIRE (std::abs (levelPosition (-24.0f, -48.0f, 0.0f) - 0.5f) < 1.0e-6f);
    REQUIRE (std::abs (levelPosition (-100.0f, -48.0f, 0.0f)) < 1.0e-6f);
    REQUIRE (std::abs (levelPosition (6.0f, -48.0f, 0.0f) - 1.0f) < 1.0e-6f);

    auto previous = -1.0f;

    for (auto db = -48.0f; db <= 0.0f; db += 0.5f)
    {
        const auto position = levelPosition (db, -48.0f, 0.0f);
        REQUIRE (position > previous);
        previous = position;
    }
}

TEST_CASE ("a meter rises at once and falls at a fixed rate", "[ui]")
{
    REQUIRE (std::abs (fallen (2.0f, 9.0f, 0.033f, 40.0f) - 9.0f) < 1.0e-6f);
    REQUIRE (std::abs (fallen (9.0f, 0.0f, 0.1f, 40.0f) - 5.0f) < 1.0e-5f);
    REQUIRE (std::abs (fallen (9.0f, 8.0f, 0.1f, 40.0f) - 8.0f) < 1.0e-6f);
    REQUIRE (std::abs (fallen (3.0f, 3.0f, 0.1f, 40.0f) - 3.0f) < 1.0e-6f);

    auto shown = 12.0f;
    auto seconds = 0.0f;

    while (shown > 0.0f && seconds < 5.0f)
    {
        shown = fallen (shown, 0.0f, 1.0f / 30.0f, 40.0f);
        seconds += 1.0f / 30.0f;
    }

    REQUIRE (std::abs (seconds - 12.0f / 40.0f) < 0.05f);
}

TEST_CASE ("readout text is one decimal, never negative zero, and dashes when silent", "[ui]")
{
    REQUIRE (levelText (-14.24f, -70.0f) == "-14.2");
    REQUIRE (levelText (-0.04f, -70.0f) == "0.0");
    REQUIRE (levelText (0.0f, -70.0f) == "0.0");
    REQUIRE (levelText (-69.9f, -70.0f) == "-69.9");
    REQUIRE (levelText (-70.0f, -70.0f) == "--");
    REQUIRE (levelText (-100.0f, -70.0f) == "--");
}

TEST_CASE ("parameter text is signed for bipolar controls and whole for percent", "[ui]")
{
    using veneer::params::formatValue;
    using veneer::params::Text;

    REQUIRE (formatValue (4.5f, Text::signedDecimal) == "+4.5");
    REQUIRE (formatValue (-2.5f, Text::signedDecimal) == "-2.5");
    REQUIRE (formatValue (0.0f, Text::signedDecimal) == "0.0");
    REQUIRE (formatValue (-0.02f, Text::signedDecimal) == "0.0");
    REQUIRE (formatValue (3.0f, Text::decimal) == "3.0");
    REQUIRE (formatValue (-4.0f, Text::decimal) == "-4.0");
    REQUIRE (formatValue (137.4f, Text::whole) == "137");
    REQUIRE (formatValue (0.0f, Text::whole) == "0");
}
