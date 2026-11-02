#pragma once

#include <cmath>

namespace veneer
{
inline double besselI0 (double x) noexcept
{
    double sum = 1.0, term = 1.0;

    for (int k = 1; k < 60; ++k)
    {
        const auto half = x / (2.0 * k);
        term *= half * half;
        sum += term;
    }

    return sum;
}

// Kaiser window evaluated at r in [-1, 1], where 0 is the centre and +-1 the ends
inline double kaiser (double r, double beta) noexcept
{
    return besselI0 (beta * std::sqrt (1.0 - r * r)) / besselI0 (beta);
}
}
