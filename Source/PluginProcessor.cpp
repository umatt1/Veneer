#include "PluginProcessor.h"

#include "Parameters.h"
#include "PluginEditor.h"

namespace
{
constexpr const char* editorWidthProperty = "editorWidth";
}

VeneerProcessor::VeneerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VeneerState", veneer::params::createLayout())
{
    namespace p = veneer::params;

    inputParam = apvts.getRawParameterValue (p::input);
    amountParam = apvts.getRawParameterValue (p::amount);
    weightParam = apvts.getRawParameterValue (p::weight);
    bodyParam = apvts.getRawParameterValue (p::body);
    airParam = apvts.getRawParameterValue (p::air);
    pushParam = apvts.getRawParameterValue (p::push);
    outputParam = apvts.getRawParameterValue (p::output);
    ceilingParam = apvts.getRawParameterValue (p::ceiling);
    bypassRaw = apvts.getRawParameterValue (p::bypass);
    bypassParameter = apvts.getParameter (p::bypass);
}

void VeneerProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, getTotalNumOutputChannels());
    setLatencySamples (engine.getLatencySamples());
}

void VeneerProcessor::releaseResources() {}

void VeneerProcessor::reset()
{
    engine.reset();
}

bool VeneerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

veneer::EngineParams VeneerProcessor::readParams() const
{
    veneer::EngineParams p;
    p.inputDb = inputParam->load();
    p.amountPercent = amountParam->load();
    p.weightDb = weightParam->load();
    p.bodyDb = bodyParam->load();
    p.airDb = airParam->load();
    p.pushDb = pushParam->load();
    p.outputDb = outputParam->load();
    p.ceilingDb = veneer::params::ceilingDbChoices[juce::jlimit (0, 1, (int) ceilingParam->load())];
    p.bypass = bypassRaw->load() > 0.5f;
    return p;
}

void VeneerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.process (buffer, readParams());
}

juce::AudioProcessorEditor* VeneerProcessor::createEditor()
{
    return new VeneerEditor (*this);
}

void VeneerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (editorWidthProperty, editorWidth.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VeneerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            editorWidth.store (state.getProperty (editorWidthProperty, 0));
            state.removeProperty (editorWidthProperty, nullptr);
            apvts.replaceState (state);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VeneerProcessor();
}
