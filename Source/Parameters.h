#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace veneer::params
{
inline constexpr const char* input   = "input";
inline constexpr const char* amount  = "amount";
inline constexpr const char* weight  = "weight";
inline constexpr const char* body    = "body";
inline constexpr const char* air     = "air";
inline constexpr const char* push    = "push";
inline constexpr const char* output  = "output";
inline constexpr const char* ceiling = "ceiling";
inline constexpr const char* bypass  = "bypass";

inline constexpr float ceilingDbChoices[] = { -1.0f, -0.3f };

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using Float = juce::AudioParameterFloat;
    using Attributes = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const char* id, const char* name, float min, float max, float step, float def, const char* unit)
    {
        layout.add (std::make_unique<Float> (juce::ParameterID { id, 1 }, name,
                                             juce::NormalisableRange<float> (min, max, step), def,
                                             Attributes().withLabel (unit)));
    };

    addFloat (input,  "Input",  -12.0f,  12.0f, 0.1f,   0.0f, "dB");
    addFloat (amount, "Amount",   0.0f, 200.0f, 0.1f, 100.0f, "%");
    addFloat (weight, "Weight",  -6.0f,   6.0f, 0.1f,   0.0f, "dB");
    addFloat (body,   "Body",    -6.0f,   6.0f, 0.1f,   0.0f, "dB");
    addFloat (air,    "Air",     -6.0f,   6.0f, 0.1f,   0.0f, "dB");
    addFloat (push,   "Push",     0.0f,  12.0f, 0.1f,   3.0f, "dB");
    addFloat (output, "Output",  -12.0f,   0.0f, 0.1f,   0.0f, "dB");

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ceiling, 1 }, "Ceiling",
        juce::StringArray { "-1.0 dBTP", "-0.3 dBTP" }, 0,
        juce::AudioParameterChoiceAttributes().withAutomatable (false)));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { bypass, 1 }, "Bypass", false));

    return layout;
}
}
