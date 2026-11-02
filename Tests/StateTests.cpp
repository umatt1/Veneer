#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>

#include "Parameters.h"
#include "PluginProcessor.h"

namespace
{
namespace params = veneer::params;

float get (VeneerProcessor& processor, const char* id)
{
    auto* p = processor.getState().getParameter (id);
    REQUIRE (p != nullptr);
    return p->convertFrom0to1 (p->getValue());
}

void set (VeneerProcessor& processor, const char* id, float value)
{
    auto* p = processor.getState().getParameter (id);
    REQUIRE (p != nullptr);
    p->setValueNotifyingHost (p->convertTo0to1 (value));
}

juce::MemoryBlock saved (VeneerProcessor& processor)
{
    juce::MemoryBlock block;
    processor.getStateInformation (block);
    return block;
}

void load (VeneerProcessor& processor, const juce::XmlElement& xml)
{
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary (xml, block);
    processor.setStateInformation (block.getData(), (int) block.getSize());
}

void setKnown (VeneerProcessor& processor)
{
    set (processor, params::amount, 150.0f);
    set (processor, params::input, 4.5f);
}

void requireKnown (VeneerProcessor& processor)
{
    REQUIRE (std::abs (get (processor, params::amount) - 150.0f) < 0.06f);
    REQUIRE (std::abs (get (processor, params::input) - 4.5f) < 0.06f);
}
}

TEST_CASE ("saved state carries a version", "[state]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;

    const auto block = saved (processor);
    const auto xml = juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize());

    REQUIRE (xml != nullptr);
    REQUIRE (xml->getIntAttribute ("stateVersion") == 1);
}

TEST_CASE ("data that is not a Veneer state is ignored", "[state]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;
    setKnown (processor);

    const char garbage[] = "this is not a state at all, only some text of a reasonable length";
    processor.setStateInformation (garbage, (int) sizeof (garbage));
    processor.setStateInformation (garbage, 3);
    processor.setStateInformation (garbage, 0);
    processor.setStateInformation (nullptr, 0);

    juce::XmlElement other ("SomethingElse");
    other.setAttribute ("amount", 12.0);
    load (processor, other);

    requireKnown (processor);
}

// JUCE's XML reader tolerates a missing closing tag and loads everything before it, so the cuts here are inside
TEST_CASE ("a state cut off inside its content is ignored", "[state]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor source, target;
    set (source, params::amount, 20.0f);
    setKnown (target);

    const auto block = saved (source);

    for (const size_t keep : { block.getSize() / 2, block.getSize() - 40, (size_t) 9 })
    {
        target.setStateInformation (block.getData(), (int) keep);
        requireKnown (target);
    }
}

TEST_CASE ("a newer state with unknown content and out-of-range values loads what it understands", "[state]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor source, target;
    setKnown (target);

    const auto block = saved (source);
    auto xml = juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize());
    REQUIRE (xml != nullptr);

    xml->setAttribute ("stateVersion", 99);
    auto* input = xml->getChildByAttribute ("id", params::input);
    REQUIRE (input != nullptr);
    input->setAttribute ("value", 999.0);

    auto* future = xml->createNewChildElement ("FUTURE");
    future->setAttribute ("id", "somethingNew");
    future->setAttribute ("value", 3.0);

    load (target, *xml);

    REQUIRE (std::abs (get (target, params::input) - 12.0f) < 0.001f);
    REQUIRE (std::abs (get (target, params::amount) - 100.0f) < 0.06f);
}
