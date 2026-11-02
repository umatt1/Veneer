#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/Faceplate.h"
#include "ui/LookAndFeel.h"

class VeneerEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int refreshHz = 30;
    static constexpr float scaleSteps[] = { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };

    explicit VeneerEditor (VeneerProcessor&);
    ~VeneerEditor() override;

    void resized() override;

    // what the timer does, callable directly so a test or a snapshot can drive the meters
    void updateMeters (float seconds);

    void setInterfaceScale (float scale);
    float getInterfaceScale() const noexcept;

    // the ids used by the settings menu, shared with tests
    enum MenuId { scaleBase = 100, ceilingBase = 200, resetReadouts = 300, about = 400 };
    void applyMenuChoice (int id);

    veneer::ui::Faceplate& getFaceplate() noexcept { return faceplate; }

private:
    void timerCallback() override { updateMeters (1.0f / (float) refreshHz); }
    void showSettingsMenu();

    VeneerProcessor& owner;
    veneer::ui::VeneerLookAndFeel lookAndFeel;
    veneer::ui::Faceplate faceplate;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VeneerEditor)
};
