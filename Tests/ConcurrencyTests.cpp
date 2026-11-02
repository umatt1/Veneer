#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

#include "Parameters.h"
#include "PluginProcessor.h"
#include "TestUtils.h"

namespace
{
void setParameter (VeneerProcessor& processor, const char* id, float value)
{
    auto* p = processor.getState().getParameter (id);
    REQUIRE (p != nullptr);
    p->setValueNotifyingHost (p->convertTo0to1 (value));
}
}

// A host calls processBlock on one thread while the interface, the host's own parameter handling and its state
// saving run on others. Run under a thread sanitizer this is what finds a data race; run normally it is a smoke test.
TEST_CASE ("the audio thread and the interface thread work at the same time", "[concurrency]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;
    processor.setPlayConfigDetails (2, 2, 48000.0, 64);
    processor.prepareToPlay (48000.0, 64);

    constexpr int blockSize = 64, blocks = 3000;
    std::atomic<bool> finished { false }, sane { true };

    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        const auto source = testutil::pinkNoise (0.7, blockSize * 64, 4);

        for (int block = 0; block < blocks; ++block)
        {
            for (int ch = 0; ch < 2; ++ch)
                buffer.copyFrom (ch, 0, source.data() + (block % 64) * blockSize, blockSize);

            processor.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    if (! std::isfinite (buffer.getSample (ch, i)))
                        sane = false;
        }

        finished = true;
    });

    auto& meters = processor.getMeters();
    int rounds = 0;

    while (! finished)
    {
        const auto held = meters.takeHeld();

        for (const auto v : { held.limiterGrDb, held.truePeakDb, held.bandGrDb[0], held.bandGrDb[1], held.bandGrDb[2],
                              meters.inputRmsDb.load(), meters.momentaryLufs.load(), meters.shortTermLufs.load(),
                              meters.integratedLufs.load() })
            if (! std::isfinite (v))
                sane = false;

        if (rounds % 7 == 0)
            processor.resetLoudness();

        processor.setEditorWidth (570 + rounds % 900);

        juce::MemoryBlock state;
        processor.getStateInformation (state);

        setParameter (processor, veneer::params::amount, (float) (rounds * 37 % 201));
        setParameter (processor, veneer::params::push, (float) (rounds % 13));
        setParameter (processor, veneer::params::input, (float) (rounds % 25) - 12.0f);
        setParameter (processor, veneer::params::bypass, rounds % 5 == 0 ? 1.0f : 0.0f);
        setParameter (processor, veneer::params::ceiling, (float) (rounds % 2));

        ++rounds;
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }

    audio.join();

    REQUIRE (sane.load());
    REQUIRE (rounds > 10);
}
