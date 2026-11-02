#pragma once

#include <juce_graphics/juce_graphics.h>

namespace veneer::ui
{
namespace palette
{
inline constexpr juce::uint32 panel       = 0xff15171b;
inline constexpr juce::uint32 panelRaised = 0xff1d2025;
inline constexpr juce::uint32 edge        = 0xff2e3239;
inline constexpr juce::uint32 well        = 0xff0d0e11;
inline constexpr juce::uint32 text        = 0xffece6da;
inline constexpr juce::uint32 textDim     = 0xff9a948a;
inline constexpr juce::uint32 ink         = 0xff1a1511;
inline constexpr juce::uint32 brass       = 0xffd9a65a;
inline constexpr juce::uint32 teal        = 0xff56c2b0;
inline constexpr juce::uint32 tealDim     = 0xff2c6f65;
inline constexpr juce::uint32 amber       = 0xffe0a458;
inline constexpr juce::uint32 coral       = 0xffe0675a;
inline constexpr juce::uint32 walnutLight = 0xff80563a;
inline constexpr juce::uint32 walnutDark  = 0xff35210f;
}

inline juce::Colour colour (juce::uint32 argb)
{
    return juce::Colour (argb);
}

inline juce::Font font (float height, int style = juce::Font::plain)
{
    return juce::Font (juce::FontOptions (height, style));
}
}
