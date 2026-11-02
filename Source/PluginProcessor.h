#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Engine.h"

class VeneerProcessor final : public juce::AudioProcessor
{
public:
    VeneerProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    veneer::Meters& getMeters() noexcept { return engine.getMeters(); }
    void resetLoudness() noexcept { engine.requestIntegratedReset(); }

    // kept with the session so a reopened editor comes back at the size it was left at; 0 means the default
    int getEditorWidth() const noexcept { return editorWidth.load(); }
    void setEditorWidth (int width) noexcept { editorWidth.store (width); }

private:
    veneer::EngineParams readParams() const;

    juce::AudioProcessorValueTreeState apvts;
    veneer::Engine engine;
    std::atomic<int> editorWidth { 0 };

    std::atomic<float>* inputParam = nullptr;
    std::atomic<float>* amountParam = nullptr;
    std::atomic<float>* weightParam = nullptr;
    std::atomic<float>* bodyParam = nullptr;
    std::atomic<float>* airParam = nullptr;
    std::atomic<float>* pushParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* ceilingParam = nullptr;
    std::atomic<float>* bypassRaw = nullptr;
    juce::AudioProcessorParameter* bypassParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VeneerProcessor)
};
