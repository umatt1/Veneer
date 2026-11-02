#pragma once

#include <array>
#include <atomic>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "BandCompressor.h"
#include "Crossover.h"
#include "Limiter.h"
#include "Loudness.h"
#include "Meters.h"
#include "Saturator.h"
#include "ShelfBellEq.h"
#include "TruePeak.h"
#include "Widener.h"

namespace veneer
{
struct EngineParams
{
    float inputDb = 0.0f;
    float amountPercent = 100.0f;
    float weightDb = 0.0f;
    float bodyDb = 0.0f;
    float airDb = 0.0f;
    float pushDb = 0.0f;
    float outputDb = 0.0f;
    float ceilingDb = -1.0f;
    bool bypass = false;
};

class Engine
{
public:
    static constexpr int numBands = 3;
    static constexpr int maxChannels = 2;

    // input beyond this is clipped to it (+60 dBFS, far above any real signal) and NaN or infinity becomes silence
    static constexpr float maxInputMagnitude = 1000.0f;

    // seconds after the input stops until the output is below -100 dBFS; the longest measured was 0.26 s
    static constexpr double tailSeconds = 0.3;

    void prepare (double sampleRate, int numChannels);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const EngineParams& params);

    int getLatencySamples() const noexcept { return latency; }
    Meters& getMeters() noexcept { return meters; }
    // safe from any thread: the audio thread clears the histogram at the start of its next block
    void requestIntegratedReset() noexcept;

private:
    using LinearSmoothed = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    using GainSmoothed = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>;

    struct BlockStats
    {
        std::array<float, numBands> bandGrDb {};
        float limiterGrDb = 0.0f;
        float truePeak = 0.0f;
    };

    void setTargets (const EngineParams& params);
    void updateBandSettings (int chunkLength);
    void processFrame (float* const* data, int index, BlockStats& stats);

    double sampleRate = 44100.0;
    int numChannels = 2, latency = 0;
    bool initialised = false;
    std::atomic<bool> integratedResetRequested { false };

    ShelfBellEq eq;
    Crossover crossover;
    std::array<BandCompressor, numBands> compressors;
    std::array<Saturator, numBands> saturators;
    Widener widener;
    LookaheadLimiter limiter;
    LoudnessMeter loudness;
    TruePeakDetector outputPeak;
    Meters meters;

    GainSmoothed inputGain, pushGain, outputGain;
    std::array<GainSmoothed, numBands> focusGain;
    LinearSmoothed amount, bypassMix;

    std::array<std::vector<float>, maxChannels> dryDelay;
    size_t dryPosition = 0;

    float inputMeanSquare = 0.0f, inputMeterCoef = 0.0f;
};
}
