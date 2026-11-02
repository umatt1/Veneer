#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/Engine.h"

namespace testutil
{
constexpr double pi = 3.14159265358979323846;

// Overshoot above the ceiling allowed when measured against an ideal 16x sinc reconstruction
constexpr double overshootToleranceDb = 0.0;

inline bool bitEqual (float a, float b)
{
    return std::memcmp (&a, &b, sizeof (float)) == 0;
}

inline double dbToLinear (double db) { return std::pow (10.0, db / 20.0); }
inline double linearToDb (double v) { return 20.0 * std::log10 (std::max (v, 1.0e-12)); }

inline std::vector<float> sine (double freqHz, double amplitude, double sampleRate, int numSamples, double phase = 0.0)
{
    std::vector<float> x ((size_t) numSamples);

    for (int n = 0; n < numSamples; ++n)
        x[(size_t) n] = (float) (amplitude * std::sin (2.0 * pi * freqHz * n / sampleRate + phase));

    return x;
}

inline std::vector<float> noise (double amplitude, int numSamples, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
    std::vector<float> x ((size_t) numSamples);

    for (auto& v : x)
        v = (float) amplitude * dist (rng);

    return x;
}

// Paul Kellet's economy pink filter, scaled so the largest sample equals `amplitude`
inline std::vector<float> pinkNoise (double amplitude, int numSamples, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<double> dist (-1.0, 1.0);
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, largest = 0.0;
    std::vector<double> y ((size_t) numSamples);

    for (auto& v : y)
    {
        const auto white = dist (rng);
        b0 = 0.99886 * b0 + white * 0.0555179;
        b1 = 0.99332 * b1 + white * 0.0750759;
        b2 = 0.96900 * b2 + white * 0.1538520;
        b3 = 0.86650 * b3 + white * 0.3104856;
        b4 = 0.55000 * b4 + white * 0.5329522;
        b5 = -0.7616 * b5 - white * 0.0168980;
        v = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362;
        b6 = white * 0.115926;
        largest = std::max (largest, std::abs (v));
    }

    std::vector<float> x ((size_t) numSamples);

    for (size_t i = 0; i < x.size(); ++i)
        x[i] = (float) (y[i] / largest * amplitude);

    return x;
}

// magnitude of the DFT of an impulse response at one frequency
inline double magnitudeDb (const std::vector<double>& h, double freqHz, double sampleRate)
{
    std::complex<double> acc;

    for (size_t n = 0; n < h.size(); ++n)
        acc += h[n] * std::polar (1.0, -2.0 * pi * freqHz * (double) n / sampleRate);

    return linearToDb (std::abs (acc));
}

// amplitude of the component at freqHz; exact when the window holds a whole number of cycles
inline double amplitudeAt (const std::vector<float>& x, int from, int count, double freqHz, double sampleRate)
{
    std::complex<double> acc;

    for (int n = from; n < from + count; ++n)
        acc += (double) x[(size_t) n] * std::polar (1.0, -2.0 * pi * freqHz * (double) n / sampleRate);

    return 2.0 * std::abs (acc) / (double) count;
}

inline double rms (const std::vector<float>& x, int from, int to)
{
    double sum = 0.0;

    for (int n = from; n < to; ++n)
        sum += (double) x[(size_t) n] * (double) x[(size_t) n];

    return std::sqrt (sum / (double) (to - from));
}

inline double samplePeak (const std::vector<float>& x, int from, int to)
{
    double peak = 0.0;

    for (int n = from; n < to; ++n)
        peak = std::max (peak, (double) std::abs (x[(size_t) n]));

    return peak;
}

// Independent of the plugin's detector: 16x interpolation with a long Kaiser-windowed sinc.
// Needs 48 samples of context on both sides of [from, to).
inline double referenceTruePeak (const std::vector<float>& x, int from, int to)
{
    static constexpr int halfWidth = 48, steps = 16, taps = 2 * halfWidth;
    static constexpr double beta = 9.0;

    static const std::vector<double> kernel = []
    {
        auto besselI0 = [] (double v)
        {
            double sum = 1.0, term = 1.0;

            for (int k = 1; k < 80; ++k)
            {
                const auto h = v / (2.0 * k);
                term *= h * h;
                sum += term;
            }

            return sum;
        };

        std::vector<double> k ((size_t) (steps * taps));

        for (int j = 0; j < steps; ++j)
        {
            for (int tap = 0; tap < taps; ++tap)
            {
                const auto t = (double) j / steps - (double) (tap - halfWidth + 1);
                const auto sinc = t == 0.0 ? 1.0 : std::sin (pi * t) / (pi * t);
                const auto r = std::min (1.0, std::abs (t) / halfWidth);
                k[(size_t) (j * taps + tap)] = sinc * besselI0 (beta * std::sqrt (1.0 - r * r)) / besselI0 (beta);
            }
        }

        return k;
    }();

    double best = 0.0;

    for (int n = from; n < to; ++n)
    {
        for (int j = 0; j < steps; ++j)
        {
            double acc = 0.0;

            for (int tap = 0; tap < taps; ++tap)
                acc += kernel[(size_t) (j * taps + tap)] * (double) x[(size_t) (n + tap - halfWidth + 1)];

            best = std::max (best, std::abs (acc));
        }
    }

    return best;
}

struct Stereo
{
    std::vector<float> left, right;
    int size() const { return (int) left.size(); }
};

inline Stereo dualMono (const std::vector<float>& x)
{
    return { x, x };
}

inline Stereo render (veneer::Engine& engine, const veneer::EngineParams& params, Stereo signal, int blockSize = 512)
{
    const auto n = signal.size();

    for (int start = 0; start < n; start += blockSize)
    {
        const auto length = std::min (blockSize, n - start);
        float* pointers[2] = { signal.left.data() + start, signal.right.data() + start };
        juce::AudioBuffer<float> view (pointers, 2, length);
        engine.process (view, params);
    }

    return signal;
}

inline void appendTone (std::vector<float>& x, double& phase, double sampleRate, double levelDb, double seconds, double freqHz = 1000.0)
{
    const auto count = (int) std::lround (seconds * sampleRate);
    const auto amplitude = dbToLinear (levelDb);
    const auto step = 2.0 * pi * freqHz / sampleRate;

    for (int i = 0; i < count; ++i)
    {
        x.push_back ((float) (amplitude * std::sin (phase)));
        phase += step;
    }
}

inline void appendSilence (std::vector<float>& x, double sampleRate, double seconds)
{
    x.insert (x.end(), (size_t) std::lround (seconds * sampleRate), 0.0f);
}
}
