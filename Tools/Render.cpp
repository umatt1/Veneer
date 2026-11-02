#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include <juce_audio_formats/juce_audio_formats.h>

#include "dsp/Engine.h"
#include "dsp/Loudness.h"
#include "dsp/TruePeak.h"

namespace
{
constexpr int blockSize = 512;

struct Options
{
    std::string input, output, jsonPath;
    veneer::EngineParams params;
    bool matchLoudness = false;
};

void usage()
{
    std::fprintf (stderr,
                  "usage: VeneerRender <in.wav> <out.wav> [--input dB] [--amount %%] [--weight dB] [--body dB]\n"
                  "                    [--air dB] [--push dB] [--output dB] [--ceiling dBTP] [--bypass] [--match] [--json file]\n"
                  "defaults are the plugin's: amount 100, push 3, ceiling -1; --match scales the written file to the\n"
                  "input's integrated loudness so settings can be compared at equal level\n");
}

bool parse (int argc, char** argv, Options& o)
{
    if (argc < 3)
        return false;

    o.input = argv[1];
    o.output = argv[2];

    for (int i = 3; i < argc; ++i)
    {
        const std::string flag = argv[i];

        if (flag == "--bypass")
        {
            o.params.bypass = true;
            continue;
        }

        if (flag == "--match")
        {
            o.matchLoudness = true;
            continue;
        }

        if (i + 1 >= argc)
            return false;

        const std::string value = argv[++i];
        const auto number = (float) std::atof (value.c_str());

        if (flag == "--input")        o.params.inputDb = number;
        else if (flag == "--amount")  o.params.amountPercent = number;
        else if (flag == "--weight")  o.params.weightDb = number;
        else if (flag == "--body")    o.params.bodyDb = number;
        else if (flag == "--air")     o.params.airDb = number;
        else if (flag == "--push")    o.params.pushDb = number;
        else if (flag == "--output")  o.params.outputDb = number;
        else if (flag == "--ceiling") o.params.ceilingDb = number;
        else if (flag == "--json")    o.jsonPath = value;
        else return false;
    }

    return true;
}

struct Measurement
{
    float integratedLufs = -100.0f, maxShortTermLufs = -100.0f, truePeakDb = -100.0f, samplePeakDb = -100.0f;
};

Measurement measure (const juce::AudioBuffer<float>& audio, double sampleRate, int offset)
{
    const auto channels = audio.getNumChannels();
    veneer::LoudnessMeter loudness;
    loudness.prepare (sampleRate, channels);
    veneer::TruePeakDetector detector;

    Measurement m;
    float frame[2] = {};
    float truePeak = 0.0f, samplePeak = 0.0f;

    for (int n = offset; n < audio.getNumSamples(); ++n)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            frame[ch] = audio.getSample (ch, n);
            samplePeak = std::max (samplePeak, std::abs (frame[ch]));
            truePeak = std::max (truePeak, detector.process (ch, frame[ch]));
        }

        loudness.processFrame (frame);

        if (n % 4800 == 0)
            m.maxShortTermLufs = std::max (m.maxShortTermLufs, loudness.getShortTerm());
    }

    auto toDb = [] (float v) { return v > 1.0e-6f ? 20.0f * std::log10 (v) : -120.0f; };

    m.integratedLufs = loudness.getIntegrated();
    m.maxShortTermLufs = std::max (m.maxShortTermLufs, loudness.getShortTerm());
    m.truePeakDb = toDb (truePeak);
    m.samplePeakDb = toDb (samplePeak);
    return m;
}
}

int main (int argc, char** argv)
{
    Options options;
    options.params.pushDb = 3.0f;

    if (! parse (argc, argv, options))
    {
        usage();
        return 2;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (options.input)));

    if (reader == nullptr || reader->numChannels < 1 || reader->numChannels > 2)
    {
        std::fprintf (stderr, "cannot read a mono or stereo file from %s\n", options.input.c_str());
        return 1;
    }

    const auto sampleRate = reader->sampleRate;
    const auto channels = (int) reader->numChannels;
    const auto length = (int) reader->lengthInSamples;

    juce::AudioBuffer<float> audio (channels, length);
    reader->read (&audio, 0, length, 0, true, true);

    veneer::Engine engine;
    engine.prepare (sampleRate, channels);
    const auto latency = engine.getLatencySamples();

    juce::AudioBuffer<float> processed (channels, length + latency);
    processed.clear();

    for (int ch = 0; ch < channels; ++ch)
        processed.copyFrom (ch, 0, audio, ch, 0, length);

    float maxLimiterGr = 0.0f;
    std::array<float, veneer::Engine::numBands> maxBandGr {}, sumBandGr {};
    int blocks = 0;

    for (int start = 0; start < processed.getNumSamples(); start += blockSize)
    {
        const auto count = std::min (blockSize, processed.getNumSamples() - start);
        juce::AudioBuffer<float> view (processed.getArrayOfWritePointers(), channels, start, count);
        engine.process (view, options.params);

        const auto& meters = engine.getMeters();
        maxLimiterGr = std::max (maxLimiterGr, meters.limiterGrDb.load());

        for (size_t b = 0; b < maxBandGr.size(); ++b)
        {
            maxBandGr[b] = std::max (maxBandGr[b], meters.bandGrDb[b].load());
            sumBandGr[b] += meters.bandGrDb[b].load();
        }

        ++blocks;
    }

    juce::AudioBuffer<float> aligned (channels, length);

    for (int ch = 0; ch < channels; ++ch)
        aligned.copyFrom (ch, 0, processed, ch, latency, length);

    const auto before = measure (audio, sampleRate, 0);
    auto after = measure (aligned, sampleRate, 0);
    float matchGainDb = 0.0f;

    if (options.matchLoudness && before.integratedLufs > -70.0f && after.integratedLufs > -70.0f)
    {
        // never match upwards into clipping
        matchGainDb = std::min (before.integratedLufs - after.integratedLufs, -after.samplePeakDb - 0.1f);
        aligned.applyGain (std::pow (10.0f, matchGainDb / 20.0f));
        after = measure (aligned, sampleRate, 0);
    }

    juce::File outFile (options.output);
    outFile.deleteFile();

    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (outFile.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> writer;

        if (stream != nullptr)
            writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                      .withSampleRate (sampleRate)
                                                      .withNumChannels (channels)
                                                      .withBitsPerSample (24));

        if (writer == nullptr)
        {
            std::fprintf (stderr, "cannot write %s\n", options.output.c_str());
            return 1;
        }

        writer->writeFromAudioSampleBuffer (aligned, 0, length);
    }

    char json[1024];
    std::snprintf (json, sizeof (json),
                   "{\n"
                   "  \"sampleRate\": %.0f,\n  \"channels\": %d,\n  \"seconds\": %.3f,\n  \"latencySamples\": %d,\n"
                   "  \"matchGainDb\": %.2f,\n"
                   "  \"input\":  { \"integratedLufs\": %.2f, \"maxShortTermLufs\": %.2f, \"truePeakDb\": %.2f },\n"
                   "  \"output\": { \"integratedLufs\": %.2f, \"maxShortTermLufs\": %.2f, \"truePeakDb\": %.2f, \"samplePeakDb\": %.2f },\n"
                   "  \"maxGainReductionDb\": { \"low\": %.2f, \"mid\": %.2f, \"high\": %.2f, \"limiter\": %.2f },\n"
                   "  \"meanGainReductionDb\": { \"low\": %.2f, \"mid\": %.2f, \"high\": %.2f }\n"
                   "}\n",
                   sampleRate, channels, (double) length / sampleRate, latency, (double) matchGainDb,
                   (double) before.integratedLufs, (double) before.maxShortTermLufs, (double) before.truePeakDb,
                   (double) after.integratedLufs, (double) after.maxShortTermLufs, (double) after.truePeakDb, (double) after.samplePeakDb,
                   (double) maxBandGr[0], (double) maxBandGr[1], (double) maxBandGr[2], (double) maxLimiterGr,
                   (double) (sumBandGr[0] / (float) blocks), (double) (sumBandGr[1] / (float) blocks), (double) (sumBandGr[2] / (float) blocks));

    std::fputs (json, stdout);

    if (! options.jsonPath.empty())
        juce::File (options.jsonPath).replaceWithText (json);

    return 0;
}
