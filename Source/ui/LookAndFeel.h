#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace veneer::ui
{
class VeneerLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VeneerLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float position,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawCornerResizer (juce::Graphics&, int width, int height, bool mouseOver, bool dragging) override;
};
}
