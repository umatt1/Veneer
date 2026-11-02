#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "PluginProcessor.h"
#include "Parameters.h"
#include "TestUtils.h"

namespace
{
constexpr double fs = 48000.0;

juce::RangedAudioParameter* find (VeneerProcessor& processor, const char* id)
{
    for (auto* p : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (ranged->getParameterID() == id)
                return ranged;

    return nullptr;
}

void set (VeneerProcessor& processor, const char* id, float value)
{
    auto* p = find (processor, id);
    REQUIRE (p != nullptr);
    p->setValueNotifyingHost (p->convertTo0to1 (value));
}

float get (VeneerProcessor& processor, const char* id)
{
    auto* p = find (processor, id);
    REQUIRE (p != nullptr);
    return p->convertFrom0to1 (p->getValue());
}

void prepare (VeneerProcessor& processor, double sampleRate = fs, int block = 512)
{
    processor.setPlayConfigDetails (2, 2, sampleRate, block);
    processor.prepareToPlay (sampleRate, block);
}

std::vector<float> run (VeneerProcessor& processor, const std::vector<float>& in, int block = 512)
{
    std::vector<float> out;
    juce::MidiBuffer midi;

    for (size_t start = 0; start < in.size(); start += (size_t) block)
    {
        const auto length = (int) std::min ((size_t) block, in.size() - start);
        juce::AudioBuffer<float> buffer (2, length);

        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (ch, 0, in.data() + start, length);

        processor.processBlock (buffer, midi);
        out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + length);
    }

    return out;
}
}

TEST_CASE ("every parameter exists with the documented range and default", "[processor]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;

    struct Expected { const char* id; float min, max, def; };

    const Expected expected[] = {
        { veneer::params::input,  -12.0f,  12.0f,   0.0f },
        { veneer::params::amount,   0.0f, 200.0f, 100.0f },
        { veneer::params::weight,  -6.0f,   6.0f,   0.0f },
        { veneer::params::body,    -6.0f,   6.0f,   0.0f },
        { veneer::params::air,     -6.0f,   6.0f,   0.0f },
        { veneer::params::push,     0.0f,  12.0f,   3.0f },
        { veneer::params::output,  -12.0f,   0.0f,   0.0f },
    };

    for (const auto& e : expected)
    {
        auto* p = find (processor, e.id);
        INFO (e.id);
        REQUIRE (p != nullptr);
        REQUIRE (std::abs (p->convertFrom0to1 (0.0f) - e.min) < 1.0e-4f);
        REQUIRE (std::abs (p->convertFrom0to1 (1.0f) - e.max) < 1.0e-4f);
        REQUIRE (std::abs (p->convertFrom0to1 (p->getDefaultValue()) - e.def) < 1.0e-4f);
        REQUIRE (p->isAutomatable());
    }

    auto* ceiling = find (processor, veneer::params::ceiling);
    REQUIRE (ceiling != nullptr);
    REQUIRE_FALSE (ceiling->isAutomatable());

    REQUIRE (processor.getBypassParameter() == find (processor, veneer::params::bypass));
}

TEST_CASE ("latency is reported after prepare, and does not depend on settings", "[processor]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;
    prepare (processor);

    veneer::Engine reference;
    reference.prepare (fs, 2);

    REQUIRE (processor.getLatencySamples() == reference.getLatencySamples());
    REQUIRE (processor.getLatencySamples() > 0);

    set (processor, veneer::params::amount, 200.0f);
    set (processor, veneer::params::push, 12.0f);
    set (processor, veneer::params::bypass, 1.0f);
    run (processor, testutil::noise (0.3, 4800));
    REQUIRE (processor.getLatencySamples() == reference.getLatencySamples());

    prepare (processor, 96000.0, 256);
    REQUIRE (processor.getLatencySamples() > reference.getLatencySamples());
}

TEST_CASE ("the bypass parameter gives the input back delayed by the latency", "[processor]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;
    prepare (processor);
    set (processor, veneer::params::bypass, 1.0f);

    const auto in = testutil::noise (0.5, 24000, 3);
    const auto out = run (processor, in);
    const auto latency = (size_t) processor.getLatencySamples();

    for (size_t n = latency; n < in.size(); ++n)
        REQUIRE (testutil::bitEqual (out[n], in[n - latency]));
}

TEST_CASE ("parameters reach the engine", "[processor]")
{
    juce::ScopedJuceInitialiser_GUI init;

    auto gainReduction = [] (float amount)
    {
        VeneerProcessor processor;
        prepare (processor);
        set (processor, veneer::params::amount, amount);
        run (processor, testutil::sine (1000.0, 0.5, fs, 48000));
        return processor.getMeters().bandGrDb[1].load();
    };

    REQUIRE (gainReduction (0.0f) < 0.01f);
    REQUIRE (gainReduction (100.0f) > 3.0f);

    auto level = [] (float outputDb)
    {
        VeneerProcessor processor;
        prepare (processor);
        set (processor, veneer::params::amount, 0.0f);
        set (processor, veneer::params::push, 0.0f);
        set (processor, veneer::params::output, outputDb);
        const auto out = run (processor, testutil::sine (1000.0, 0.1, fs, 48000));
        return testutil::linearToDb (testutil::rms (out, 24000, 48000));
    };

    REQUIRE (std::abs ((level (-6.0f) - level (0.0f)) + 6.0) < 0.1);
}

TEST_CASE ("ceiling choice moves the limiter ceiling", "[processor]")
{
    juce::ScopedJuceInitialiser_GUI init;

    auto peak = [] (float choiceIndex)
    {
        VeneerProcessor processor;
        prepare (processor);
        set (processor, veneer::params::push, 12.0f);
        set (processor, veneer::params::ceiling, choiceIndex);
        const auto out = run (processor, testutil::sine (1000.0, 0.9, fs, 48000));
        return testutil::linearToDb (testutil::samplePeak (out, 24000, 48000));
    };

    // the ceiling is a guarantee: peaks may sit up to the detection margin below it, never above
    for (const auto& [choice, ceilingDb] : { std::pair { 0.0f, -1.0 }, std::pair { 1.0f, -0.3 } })
    {
        const auto measured = peak (choice);
        INFO ("choice " << choice << " measured " << measured << " dBFS");
        REQUIRE (measured <= ceilingDb + 0.001);
        REQUIRE (measured >= ceilingDb - 0.35);
    }
}

TEST_CASE ("state survives a save and restore", "[processor]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor original;

    set (original, veneer::params::input, 4.5f);
    set (original, veneer::params::amount, 137.0f);
    set (original, veneer::params::weight, -2.5f);
    set (original, veneer::params::body, 1.5f);
    set (original, veneer::params::air, 3.0f);
    set (original, veneer::params::push, 7.0f);
    set (original, veneer::params::output, -4.0f);
    set (original, veneer::params::ceiling, 1.0f);

    juce::MemoryBlock state;
    original.getStateInformation (state);

    VeneerProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());

    for (const auto* id : { veneer::params::input, veneer::params::amount, veneer::params::weight, veneer::params::body,
                            veneer::params::air, veneer::params::push, veneer::params::output, veneer::params::ceiling })
    {
        INFO (id);
        REQUIRE (std::abs (get (restored, id) - get (original, id)) < 1.0e-4f);
    }
}

TEST_CASE ("parameter text is what the host and the interface both show", "[processor]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;

    auto text = [&] (const char* id, float value)
    {
        auto* p = find (processor, id);
        REQUIRE (p != nullptr);
        return p->getText (p->convertTo0to1 (value), 32);
    };

    REQUIRE (text (veneer::params::input, 4.5f) == "+4.5");
    REQUIRE (text (veneer::params::weight, -2.5f) == "-2.5");
    REQUIRE (text (veneer::params::air, 0.0f) == "0.0");
    REQUIRE (text (veneer::params::amount, 137.0f) == "137");
    REQUIRE (text (veneer::params::push, 3.0f) == "3.0");
    REQUIRE (text (veneer::params::output, -4.0f) == "-4.0");

    auto* input = find (processor, veneer::params::input);
    REQUIRE (std::abs (input->convertFrom0to1 (input->getValueForText ("+4.5")) - 4.5f) < 0.06f);
    REQUIRE (std::abs (input->convertFrom0to1 (input->getValueForText ("-3 dB")) + 3.0f) < 0.06f);
}

TEST_CASE ("the editor width is saved with the session and an old session has none", "[processor]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor original;
    REQUIRE (original.getEditorWidth() == 0);

    juce::MemoryBlock fresh;
    original.getStateInformation (fresh);

    original.setEditorWidth (1140);
    juce::MemoryBlock saved;
    original.getStateInformation (saved);

    VeneerProcessor restored;
    restored.setStateInformation (saved.getData(), (int) saved.getSize());
    REQUIRE (restored.getEditorWidth() == 1140);
    REQUIRE_FALSE (restored.getState().state.hasProperty ("editorWidth"));

    restored.setStateInformation (fresh.getData(), (int) fresh.getSize());
    REQUIRE (restored.getEditorWidth() == 0);
}

TEST_CASE ("only matching mono and stereo layouts are accepted", "[processor]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;

    auto layout = [] (const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        l.outputBuses.add (out);
        return l;
    };

    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();

    REQUIRE (processor.checkBusesLayoutSupported (layout (stereo, stereo)));
    REQUIRE (processor.checkBusesLayoutSupported (layout (mono, mono)));
    REQUIRE_FALSE (processor.checkBusesLayoutSupported (layout (mono, stereo)));
    REQUIRE_FALSE (processor.checkBusesLayoutSupported (layout (juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1())));
}
