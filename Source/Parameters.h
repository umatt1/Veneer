#pragma once

#include <cmath>

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

enum class Text { decimal, signedDecimal, whole };

inline juce::String formatValue (float value, Text text)
{
    if (text == Text::whole)
        return juce::String (juce::roundToInt (value));

    const auto rounded = std::round (value * 10.0f) / 10.0f;
    const auto shown = std::abs (rounded) < 0.05f ? 0.0f : rounded;
    return (text == Text::signedDecimal && shown > 0.0f ? "+" : "") + juce::String (shown, 1);
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using Float = juce::AudioParameterFloat;
    using Attributes = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const char* id, const char* name, float min, float max, float step, float def, const char* unit, Text text)
    {
        layout.add (std::make_unique<Float> (juce::ParameterID { id, 1 }, name,
                                             juce::NormalisableRange<float> (min, max, step), def,
                                             Attributes().withLabel (unit)
                                                         .withStringFromValueFunction ([text] (float v, int) { return formatValue (v, text); })));
    };

    addFloat (input,  "Input",  -12.0f,  12.0f, 0.1f,   0.0f, "dB", Text::signedDecimal);
    addFloat (amount, "Amount",   0.0f, 200.0f, 0.1f, 100.0f, "%",  Text::whole);
    addFloat (weight, "Weight",  -6.0f,   6.0f, 0.1f,   0.0f, "dB", Text::signedDecimal);
    addFloat (body,   "Body",    -6.0f,   6.0f, 0.1f,   0.0f, "dB", Text::signedDecimal);
    addFloat (air,    "Air",     -6.0f,   6.0f, 0.1f,   0.0f, "dB", Text::signedDecimal);
    addFloat (push,   "Push",     0.0f,  12.0f, 0.1f,   3.0f, "dB", Text::decimal);
    addFloat (output, "Output",  -12.0f,   0.0f, 0.1f,   0.0f, "dB", Text::decimal);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ceiling, 1 }, "Ceiling",
        juce::StringArray { "-1.0 dBTP", "-0.3 dBTP" }, 0,
        juce::AudioParameterChoiceAttributes().withAutomatable (false)));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { bypass, 1 }, "Bypass", false));

    return layout;
}
}
