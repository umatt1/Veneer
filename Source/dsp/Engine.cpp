#include "Engine.h"

#include <algorithm>
#include <cmath>

#include "Voicing.h"

namespace veneer
{
namespace
{
constexpr int chunkLength = 32;
constexpr double gainRampSeconds = 0.02;
constexpr double amountRampSeconds = 0.05;
constexpr double bypassRampSeconds = 0.01;
constexpr double inputMeterSeconds = 0.3;
constexpr float silenceDb = Meters::silenceDb;

float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db / 20.0f);
}

float gainToDb (float gain) noexcept
{
    return gain > 1.0e-5f ? 20.0f * std::log10 (gain) : silenceDb;
}

// NaN or infinity would sit in the filters until the next reset, and a huge value would overflow their squares
float sanitised (float x) noexcept
{
    return std::isfinite (x) ? std::clamp (x, -Engine::maxInputMagnitude, Engine::maxInputMagnitude) : 0.0f;
}
}

void Engine::prepare (double newSampleRate, int channels)
{
    sampleRate = newSampleRate;
    numChannels = std::clamp (channels, 1, maxChannels);

    eq.prepare (sampleRate);
    crossover.prepare (sampleRate, numChannels);
    crossover.setFrequencies (voicing::lowCrossoverHz, voicing::highCrossoverHz);

    for (auto& c : compressors)
        c.prepare (sampleRate);

    for (auto& s : saturators)
        s.prepare (sampleRate);

    widener.prepare (sampleRate);
    limiter.prepare (sampleRate, numChannels);
    loudness.prepare (sampleRate, numChannels);

    latency = Saturator::latencySamples + limiter.getLatencySamples();

    for (auto& d : dryDelay)
        d.assign ((size_t) latency, 0.0f);

    for (auto* s : { &inputGain, &pushGain, &outputGain })
        s->reset (sampleRate, gainRampSeconds);

    for (auto& s : focusGain)
        s.reset (sampleRate, gainRampSeconds);

    amount.reset (sampleRate, amountRampSeconds);
    bypassMix.reset (sampleRate, bypassRampSeconds);

    inputMeterCoef = (float) std::exp (-1.0 / (inputMeterSeconds * sampleRate));

    reset();
}

void Engine::reset()
{
    eq.reset();
    crossover.reset();

    for (auto& c : compressors)
        c.reset();

    for (auto& s : saturators)
        s.reset();

    widener.reset();
    limiter.reset();
    loudness.reset();
    outputPeak.reset();

    for (auto& d : dryDelay)
        std::fill (d.begin(), d.end(), 0.0f);

    dryPosition = 0;
    inputMeanSquare = 0.0f;
    initialised = false;
}

void Engine::requestIntegratedReset() noexcept
{
    integratedResetRequested.store (true, std::memory_order_relaxed);
    meters.integratedLufs.store (silenceDb, std::memory_order_relaxed);
}

void Engine::setTargets (const EngineParams& p)
{
    const std::array<float, numBands> focusDb { p.weightDb, p.bodyDb, p.airDb };

    if (! initialised)
    {
        inputGain.setCurrentAndTargetValue (dbToGain (p.inputDb));
        pushGain.setCurrentAndTargetValue (dbToGain (p.pushDb));
        outputGain.setCurrentAndTargetValue (dbToGain (p.outputDb));
        amount.setCurrentAndTargetValue (p.amountPercent);
        bypassMix.setCurrentAndTargetValue (p.bypass ? 1.0f : 0.0f);

        for (size_t b = 0; b < (size_t) numBands; ++b)
            focusGain[b].setCurrentAndTargetValue (dbToGain (focusDb[b]));

        initialised = true;
    }

    inputGain.setTargetValue (dbToGain (p.inputDb));
    pushGain.setTargetValue (dbToGain (p.pushDb));
    outputGain.setTargetValue (dbToGain (p.outputDb));
    amount.setTargetValue (p.amountPercent);
    bypassMix.setTargetValue (p.bypass ? 1.0f : 0.0f);

    for (size_t b = 0; b < (size_t) numBands; ++b)
        focusGain[b].setTargetValue (dbToGain (focusDb[b]));

    limiter.setCeilingDb (p.ceilingDb);
}

void Engine::updateBandSettings (int length)
{
    const auto currentAmount = amount.getCurrentValue();

    for (size_t b = 0; b < (size_t) numBands; ++b)
    {
        const auto band = (int) b;
        const auto focusDb = gainToDb (focusGain[b].getCurrentValue());

        compressors[b].setSettings (voicing::compressorSettings (band, currentAmount));
        saturators[b].setShape (voicing::saturatorDrive (band, currentAmount, focusDb), voicing::saturatorBias (band));
    }

    eq.setSections (voicing::eqSections (currentAmount));
    widener.setShelf (voicing::widenerHz, voicing::widenerQ, voicing::widenerBoostDb (currentAmount));

    amount.skip (length);
}

void Engine::process (juce::AudioBuffer<float>& buffer, const EngineParams& params)
{
    const auto numSamples = buffer.getNumSamples();
    const auto activeChannels = std::min (buffer.getNumChannels(), numChannels);

    if (activeChannels == 0 || numSamples == 0)
        return;

    if (integratedResetRequested.exchange (false, std::memory_order_relaxed))
        loudness.resetIntegrated();

    setTargets (params);

    std::array<float*, maxChannels> data {};

    for (int ch = 0; ch < activeChannels; ++ch)
        data[(size_t) ch] = buffer.getWritePointer (ch);

    BlockStats stats;

    for (int start = 0; start < numSamples; start += chunkLength)
    {
        const auto length = std::min (chunkLength, numSamples - start);
        updateBandSettings (length);

        for (int i = 0; i < length; ++i)
            processFrame (data.data(), start + i, stats);
    }

    for (size_t b = 0; b < (size_t) numBands; ++b)
        meters.bandGrDb[b].store (stats.bandGrDb[b], std::memory_order_relaxed);

    meters.limiterGrDb.store (stats.limiterGrDb, std::memory_order_relaxed);
    meters.truePeakDb.store (gainToDb (stats.truePeak), std::memory_order_relaxed);
    meters.inputRmsDb.store (10.0f * std::log10 (inputMeanSquare + 1.0e-10f), std::memory_order_relaxed);
    meters.momentaryLufs.store (loudness.getMomentary(), std::memory_order_relaxed);
    meters.shortTermLufs.store (loudness.getShortTerm(), std::memory_order_relaxed);
    meters.integratedLufs.store (loudness.getIntegrated(), std::memory_order_relaxed);
    meters.hold ({ stats.bandGrDb, stats.limiterGrDb, gainToDb (stats.truePeak) });
}

void Engine::processFrame (float* const* data, int index, BlockStats& stats)
{
    std::array<float, maxChannels> dry {}, input {}, wet {}, limiterIn {}, limiterOut {};

    const auto inGain = inputGain.getNextValue();
    float power = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        dry[(size_t) ch] = data[ch] != nullptr ? sanitised (data[ch][index]) : 0.0f;
        input[(size_t) ch] = dry[(size_t) ch] * inGain;
        power += input[(size_t) ch] * input[(size_t) ch];
    }

    inputMeanSquare = power / (float) numChannels + inputMeterCoef * (inputMeanSquare - power / (float) numChannels);

    std::array<std::array<float, maxChannels>, numBands> bands {};

    for (int ch = 0; ch < numChannels; ++ch)
        crossover.process (ch, eq.process (ch, input[(size_t) ch]), bands[0][(size_t) ch], bands[1][(size_t) ch], bands[2][(size_t) ch]);

    for (size_t b = 0; b < (size_t) numBands; ++b)
    {
        float linked = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
            linked = std::max (linked, bands[b][(size_t) ch] * bands[b][(size_t) ch]);

        const auto compression = compressors[b].process (linked);
        const auto focus = focusGain[b].getNextValue();
        stats.bandGrDb[b] = std::max (stats.bandGrDb[b], compressors[b].getGainReductionDb());

        for (int ch = 0; ch < numChannels; ++ch)
            wet[(size_t) ch] += saturators[b].process (ch, bands[b][(size_t) ch] * compression) * focus;
    }

    if (numChannels == 2)
        widener.process (wet[0], wet[1]);

    const auto push = pushGain.getNextValue();

    for (int ch = 0; ch < numChannels; ++ch)
        limiterIn[(size_t) ch] = wet[(size_t) ch] * push;

    const auto limiterGr = limiter.process (limiterIn.data(), limiterOut.data());
    stats.limiterGrDb = std::max (stats.limiterGrDb, limiterGr);

    const auto outGain = outputGain.getNextValue();
    const auto mix = bypassMix.getNextValue();
    float framePeak = 0.0f;

    std::array<float, maxChannels> out {};

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& delay = dryDelay[(size_t) ch];
        const auto delayedDry = delay[dryPosition];
        delay[dryPosition] = dry[(size_t) ch];

        const auto processed = limiterOut[(size_t) ch] * outGain;
        out[(size_t) ch] = processed * (1.0f - mix) + delayedDry * mix;

        if (data[ch] != nullptr)
            data[ch][index] = out[(size_t) ch];

        framePeak = std::max (framePeak, outputPeak.process (ch, out[(size_t) ch]));
    }

    dryPosition = (dryPosition + 1) % (size_t) latency;
    stats.truePeak = std::max (stats.truePeak, framePeak);

    loudness.processFrame (out.data());
}
}
