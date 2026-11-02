#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <juce_audio_utils/juce_audio_utils.h>

#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
constexpr int blockSize = 512;

struct Options
{
    std::string output, wavPath;
    float scale = 1.0f, pixels = 1.0f, seconds = 8.0f;
    std::vector<std::pair<std::string, float>> settings;
};

void usage()
{
    std::fprintf (stderr,
                  "usage: VeneerSnapshot <out.png> [--wav in.wav] [--seconds n] [--scale 0.75..2] [--pixels n]\n"
                  "                      [--set parameter=value ...]\n"
                  "renders the plugin interface offscreen; with --wav the audio is played through the processor first so\n"
                  "the meters show something. --pixels is the image resolution relative to the interface (2 for Retina)\n");
}

bool parse (int argc, char** argv, Options& o)
{
    if (argc < 2)
        return false;

    o.output = argv[1];

    for (int i = 2; i + 1 < argc; i += 2)
    {
        const std::string flag = argv[i], value = argv[i + 1];

        if (flag == "--wav")          o.wavPath = value;
        else if (flag == "--seconds") o.seconds = (float) std::atof (value.c_str());
        else if (flag == "--scale")   o.scale = (float) std::atof (value.c_str());
        else if (flag == "--pixels")  o.pixels = (float) std::atof (value.c_str());
        else if (flag == "--set")
        {
            const auto equals = value.find ('=');

            if (equals == std::string::npos)
                return false;

            o.settings.emplace_back (value.substr (0, equals), (float) std::atof (value.c_str() + equals + 1));
        }
        else return false;
    }

    return (argc % 2) == 0;
}
}

int main (int argc, char** argv)
{
    Options options;

    if (! parse (argc, argv, options))
    {
        usage();
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI gui;

    VeneerProcessor processor;

    for (const auto& [id, value] : options.settings)
    {
        auto* parameter = processor.getState().getParameter (id.c_str());

        if (parameter == nullptr)
        {
            std::fprintf (stderr, "no parameter called %s\n", id.c_str());
            return 2;
        }

        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    std::unique_ptr<juce::AudioProcessorEditor> base (processor.createEditor());
    auto& editor = static_cast<VeneerEditor&> (*base);
    editor.setInterfaceScale (options.scale);

    if (! options.wavPath.empty())
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (options.wavPath)));

        if (reader == nullptr || reader->numChannels < 1 || reader->numChannels > 2)
        {
            std::fprintf (stderr, "cannot read a mono or stereo file from %s\n", options.wavPath.c_str());
            return 1;
        }

        const auto sampleRate = reader->sampleRate;
        const auto length = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (options.seconds * (float) sampleRate));

        juce::AudioBuffer<float> audio (2, length);
        reader->read (&audio, 0, length, 0, true, true);

        if (reader->numChannels == 1)
            audio.copyFrom (1, 0, audio, 0, 0, length);

        processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::MidiBuffer midi;
        const auto refreshSamples = (int) (sampleRate / (double) VeneerEditor::refreshHz);
        int sinceRefresh = 0;

        for (int start = 0; start < length; start += blockSize)
        {
            const auto count = std::min (blockSize, length - start);
            juce::AudioBuffer<float> block (audio.getArrayOfWritePointers(), 2, start, count);
            processor.processBlock (block, midi);

            sinceRefresh += count;

            if (sinceRefresh >= refreshSamples)
            {
                editor.updateMeters ((float) sinceRefresh / (float) sampleRate);
                sinceRefresh = 0;
            }
        }
    }

    const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, options.pixels);

    juce::File file (options.output);
    file.deleteFile();
    juce::FileOutputStream stream (file);

    if (! stream.openedOk())
    {
        std::fprintf (stderr, "cannot write %s\n", options.output.c_str());
        return 1;
    }

    juce::PNGImageFormat png;
    png.writeImageToStream (image, stream);

    std::printf ("%dx%d px, interface %dx%d at %.0f%%\n", image.getWidth(), image.getHeight(), editor.getWidth(), editor.getHeight(),
                 (double) (editor.getInterfaceScale() * 100.0f));
    return 0;
}
