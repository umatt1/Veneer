#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdio>
#include <vector>

#include <juce_dsp/juce_dsp.h>

#include "TestUtils.h"
#include "dsp/Saturator.h"

namespace
{
constexpr double fs = 48000.0;
constexpr int latency = veneer::Saturator::latencySamples;

std::vector<float> saturate (float drive, float bias, const std::vector<float>& in)
{
    veneer::Saturator saturator;
    saturator.prepare (fs);
    saturator.setShape (drive, bias);

    std::vector<float> out (in.size());

    for (size_t n = 0; n < in.size(); ++n)
        out[n] = saturator.process (0, in[n]);

    return out;
}

// the curve the saturator implements, normalised to a slope of 1 at the origin
double shaped (double x, double drive, double bias)
{
    const auto t = std::tanh (bias);
    return (std::tanh (drive * x + bias) - t) / (1.0 - t * t) / drive;
}

// amplitude of harmonic h of the curve driven by a sine, integrated over one period
double curveHarmonic (int h, double amplitude, double drive, double bias)
{
    constexpr int steps = 20000;
    double re = 0.0, im = 0.0;

    for (int i = 0; i < steps; ++i)
    {
        const auto phase = 2.0 * testutil::pi * i / steps;
        const auto y = shaped (amplitude * std::sin (phase), drive, bias);
        re += y * std::cos (h * phase);
        im += y * std::sin (h * phase);
    }

    return 2.0 * std::hypot (re, im) / steps;
}

// largest component away from the fundamental and from the harmonics that fit below Nyquist,
// in dB relative to the fundamental
double worstSpurDb (const std::vector<float>& y, int from, int order, double fundamentalHz)
{
    const auto n = 1 << order;
    const auto binHz = fs / n;
    std::vector<float> data (2 * (size_t) n, 0.0f);

    for (int i = 0; i < n; ++i)
    {
        const auto r = 2.0 * testutil::pi * i / (n - 1);
        const auto window = 0.35875 - 0.48829 * std::cos (r) + 0.14128 * std::cos (2.0 * r) - 0.01168 * std::cos (3.0 * r);
        data[(size_t) i] = (float) ((double) y[(size_t) (from + i)] * window);
    }

    juce::dsp::FFT fft (order);
    fft.performFrequencyOnlyForwardTransform (data.data());

    const auto fundamentalBin = (int) std::lround (fundamentalHz / binHz);
    float reference = 0.0f;

    for (int b = fundamentalBin - 3; b <= fundamentalBin + 3; ++b)
        reference = std::max (reference, data[(size_t) b]);

    double worst = -300.0;

    for (int b = 0; b < n / 2; ++b)
    {
        bool excluded = false;

        for (int h = 1; h * fundamentalHz < fs / 2.0; ++h)
            excluded = excluded || std::abs (b * binHz - h * fundamentalHz) < 8.0 * binHz;

        if (! excluded)
            worst = std::max (worst, testutil::linearToDb ((double) data[(size_t) b] / (double) reference));
    }

    return worst;
}
}

TEST_CASE ("drive zero is an exact delay", "[saturator]")
{
    const auto in = testutil::noise (0.9, 4096, 11);
    const auto out = saturate (0.0f, 0.0f, in);

    for (size_t n = (size_t) latency; n < in.size(); ++n)
        REQUIRE (testutil::bitEqual (out[n], in[n - (size_t) latency]));
}

TEST_CASE ("small signals pass at unit gain whatever the drive", "[saturator]")
{
    const auto in = testutil::sine (1000.0, 0.001, fs, 60000);

    for (const float drive : { 0.5f, 3.0f, 10.0f })
    {
        const auto out = saturate (drive, 0.0f, in);
        const auto gain = testutil::linearToDb (testutil::amplitudeAt (out, 12000, 48000, 1000.0, fs) / 0.001);

        INFO ("drive " << drive << " gain " << gain << " dB");
        REQUIRE (std::abs (gain) < 0.01);
    }
}

TEST_CASE ("harmonics match the static curve", "[saturator]")
{
    struct Case { float drive, bias; double amplitude; };

    for (const auto c : { Case { 1.0f, 0.0f, 0.3 }, Case { 3.0f, 0.0f, 0.5 }, Case { 2.0f, 0.15f, 0.4 }, Case { 4.0f, 0.0f, 1.0 } })
    {
        const auto out = saturate (c.drive, c.bias, testutil::sine (1000.0, c.amplitude, fs, 60000));

        for (int h = 1; h <= 5; ++h)
        {
            const auto expected = curveHarmonic (h, c.amplitude, (double) c.drive, (double) c.bias);

            if (expected < 3.0e-5)
                continue;

            const auto measured = testutil::amplitudeAt (out, 12000, 48000, 1000.0 * h, fs);
            const auto difference = testutil::linearToDb (measured / expected);

            INFO ("drive " << c.drive << " bias " << c.bias << " amplitude " << c.amplitude << " harmonic " << h << ": " << difference << " dB off");
            REQUIRE (std::abs (difference) < 0.1);
        }
    }
}

TEST_CASE ("only a biased curve adds even harmonics", "[saturator]")
{
    const auto in = testutil::sine (1000.0, 0.5, fs, 60000);
    const auto symmetric = saturate (3.0f, 0.0f, in);
    const auto biased = saturate (3.0f, 0.15f, in);

    REQUIRE (testutil::amplitudeAt (symmetric, 12000, 48000, 2000.0, fs) < 1.0e-5);
    REQUIRE (testutil::amplitudeAt (symmetric, 12000, 48000, 4000.0, fs) < 1.0e-5);
    REQUIRE (testutil::amplitudeAt (biased, 12000, 48000, 2000.0, fs) > 1.0e-3);
}

TEST_CASE ("the DC an asymmetric curve produces is removed", "[saturator]")
{
    constexpr double amplitude = 0.4;
    constexpr float drive = 3.0f, bias = 0.3f;

    double curveMean = 0.0;

    for (int i = 0; i < 1000; ++i)
        curveMean += shaped (amplitude * std::sin (2.0 * testutil::pi * i / 1000), (double) drive, (double) bias) / 1000;

    REQUIRE (std::abs (curveMean) > 1.0e-2);

    const auto out = saturate (drive, bias, testutil::sine (1000.0, amplitude, fs, 96000));
    double mean = 0.0;

    for (size_t n = 48000; n < out.size(); ++n)
        mean += (double) out[n] / 48000.0;

    INFO ("mean of the output " << mean);
    REQUIRE (std::abs (mean) < 1.0e-5);
}

TEST_CASE ("aliasing stays far below a plain tanh", "[saturator]")
{
    constexpr int order = 15, n = 1 << order, settle = 2048;
    constexpr double amplitude = 0.5, drive = 3.0;

    for (const double target : { 7000.0, 11000.0, 13000.0 })
    {
        const auto bin = std::lround (target / fs * n);
        const auto f = (double) bin * fs / n;
        const auto in = testutil::sine (f, amplitude, fs, n + settle);

        auto plain = in;

        for (auto& v : plain)
            v = (float) (std::tanh (drive * (double) v) / drive);

        const auto plainSpur = worstSpurDb (plain, settle, order, f);
        const auto spur = worstSpurDb (saturate ((float) drive, 0.0f, in), settle, order, f);

        std::printf ("saturator aliasing, %5.0f Hz at 1.5 into tanh: plain %+6.1f dBc, here %+6.1f dBc\n", f, plainSpur, spur);

        CHECK (spur < -60.0);
        CHECK (spur < plainSpur - 35.0);
    }
}

TEST_CASE ("output stays finite and bounded for hostile input at any drive", "[saturator]")
{
    std::vector<std::vector<float>> inputs;
    inputs.push_back (std::vector<float> (4800, 0.0f));
    inputs.push_back (std::vector<float> (4800, 1.0f));
    inputs.push_back (std::vector<float> (4800, 1.0e-40f));
    inputs.push_back (testutil::noise (1.0, 4800, 5));

    {
        std::vector<float> impulses (4800, 0.0f);

        for (size_t i = 0; i < impulses.size(); i += 97)
            impulses[i] = i % 2 == 0 ? 1.0f : -1.0f;

        inputs.push_back (impulses);
    }

    for (const float drive : { 0.5f, 5.0f, 20.0f })
    {
        for (const float bias : { 0.0f, 0.3f })
        {
            for (const auto& in : inputs)
            {
                for (const auto v : saturate (drive, bias, in))
                {
                    REQUIRE (std::isfinite (v));
                    REQUIRE (std::abs (v) < 3.0f);
                }
            }
        }
    }
}
