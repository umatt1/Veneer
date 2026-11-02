#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdlib>
#include <new>
#include <vector>

#include "Parameters.h"
#include "PluginProcessor.h"
#include "TestUtils.h"

// Counts C++ allocations made on the calling thread while `counting` is set. Direct calls to malloc are not seen.
namespace
{
thread_local bool counting = false;
std::atomic<long> allocations { 0 };

void* allocate (std::size_t size)
{
    if (counting)
        ++allocations;

    if (auto* p = std::malloc (size > 0 ? size : 1))
        return p;

    throw std::bad_alloc();
}

void setParameter (VeneerProcessor& processor, const char* id, float value)
{
    auto* p = processor.getState().getParameter (id);
    REQUIRE (p != nullptr);
    p->setValueNotifyingHost (p->convertTo0to1 (value));
}
}

void* operator new (std::size_t size)                         { return allocate (size); }
void* operator new[] (std::size_t size)                       { return allocate (size); }
void operator delete (void* p) noexcept                       { std::free (p); }
void operator delete[] (void* p) noexcept                     { std::free (p); }
void operator delete (void* p, std::size_t) noexcept          { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept        { std::free (p); }

TEST_CASE ("processing a block does not allocate", "[realtime]")
{
    juce::ScopedJuceInitialiser_GUI init;

    {
        const auto before = allocations.load();
        counting = true;
        std::vector<float> probe (100);
        counting = false;
        REQUIRE (allocations.load() > before);
    }

    VeneerProcessor processor;
    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 4096);
    juce::MidiBuffer midi;
    const auto noise = testutil::noise (0.5, 4096, 12);

    for (int ch = 0; ch < 2; ++ch)
        buffer.copyFrom (ch, 0, noise.data(), 4096);

    processor.processBlock (buffer, midi);

    struct Setting { float amount, push, input; bool bypass; };

    for (const auto& setting : { Setting { 100.0f, 3.0f, 0.0f, false }, Setting { 0.0f, 0.0f, 0.0f, false },
                                 Setting { 200.0f, 12.0f, 12.0f, false }, Setting { 100.0f, 3.0f, 0.0f, true },
                                 Setting { 150.0f, 6.0f, -12.0f, false } })
    {
        setParameter (processor, veneer::params::amount, setting.amount);
        setParameter (processor, veneer::params::push, setting.push);
        setParameter (processor, veneer::params::input, setting.input);
        setParameter (processor, veneer::params::bypass, setting.bypass ? 1.0f : 0.0f);

        const auto before = allocations.load();
        counting = true;

        for (const int size : { 1, 7, 32, 100, 512, 4096 })
        {
            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, 0, size);
            processor.processBlock (view, midi);
        }

        counting = false;
        INFO ("amount " << setting.amount << " push " << setting.push << " bypass " << setting.bypass);
        REQUIRE (allocations.load() == before);
    }
}
