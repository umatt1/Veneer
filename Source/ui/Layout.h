#pragma once

#include <juce_graphics/juce_graphics.h>

// Logical pixels at 100%. The faceplate is laid out once in this space and scaled as a whole.
namespace veneer::ui::layout
{
inline constexpr int width = 760, height = 460;
inline constexpr float minScale = 0.75f, maxScale = 2.0f;
inline constexpr int bands = 3;

inline juce::Rectangle<int> header()       { return { 0, 0, width, 60 }; }
inline juce::Rectangle<int> wordmark()     { return { 24, 8, 220, 34 }; }
inline juce::Rectangle<int> tagline()      { return { 26, 40, 220, 14 }; }
inline juce::Rectangle<int> bypassButton() { return { 628, 16, 84, 28 }; }
inline juce::Rectangle<int> settingsButton() { return { 720, 16, 28, 28 }; }

inline juce::Rectangle<int> inputPanel()  { return { 16, 76, 152, 368 }; }
inline juce::Rectangle<int> tonePanel()   { return { 180, 76, 392, 368 }; }
inline juce::Rectangle<int> finishPanel() { return { 584, 76, 160, 368 }; }

inline juce::Rectangle<int> inputLabel()  { return { 16, 88, 152, 14 }; }
inline juce::Rectangle<int> inputMeter()  { return { 30, 112, 54, 316 }; }
inline juce::Rectangle<int> inputKnob()   { return { 94, 126, 64, 88 }; }
inline juce::Rectangle<int> inputHint()   { return { 92, 226, 68, 60 }; }

inline juce::Rectangle<int> amountLabel() { return { 180, 88, 392, 14 }; }
inline juce::Rectangle<int> amountKnob()  { return { 304, 108, 144, 156 }; }

inline int bandLeft (int band) { return 190 + band * 124; }
inline juce::Rectangle<int> bandLabel (int band)   { return { bandLeft (band), 288, 124, 14 }; }
inline juce::Rectangle<int> bandCaption (int band) { return { bandLeft (band), 303, 124, 12 }; }
inline juce::Rectangle<int> bandMeter (int band)   { return { bandLeft (band) + 14, 322, 26, 108 }; }
inline juce::Rectangle<int> bandKnob (int band)    { return { bandLeft (band) + 44, 320, 72, 88 }; }

inline juce::Rectangle<int> pushLabel()    { return { 596, 88, 72, 14 }; }
inline juce::Rectangle<int> pushKnob()     { return { 596, 108, 72, 88 }; }
inline juce::Rectangle<int> limiterLabel() { return { 684, 88, 32, 14 }; }
inline juce::Rectangle<int> limiterMeter() { return { 684, 108, 32, 100 }; }
inline juce::Rectangle<int> outputLabel()  { return { 596, 214, 72, 14 }; }
inline juce::Rectangle<int> outputKnob()   { return { 596, 234, 72, 88 }; }
inline juce::Rectangle<int> loudness()     { return { 598, 340, 134, 92 }; }
}
