#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Artwork.h"
#include "Layout.h"
#include "MeterViews.h"
#include "dsp/Meters.h"

namespace veneer::ui
{
// All controls and meters, laid out in the logical space of layout.h. The editor scales it as a whole.
class Faceplate final : public juce::Component
{
public:
    explicit Faceplate (juce::AudioProcessorValueTreeState&);

    void resized() override;

    // takes the held peaks out of the meters, so call it once per display refresh
    void updateMeters (Meters&, float seconds);
    void resetReadouts();

    juce::Slider* findSlider (const juce::String& parameterId);
    juce::ToggleButton& getBypassButton() noexcept { return bypassButton; }
    juce::Button& getSettingsButton() noexcept { return settingsButton; }
    LevelMeter& getInputMeter() noexcept { return inputMeter; }
    GainReductionMeter& getBandMeter (int band) noexcept { return bandMeters[(size_t) band]; }
    GainReductionMeter& getLimiterMeter() noexcept { return limiterMeter; }
    LoudnessReadout& getReadout() noexcept { return readout; }

    std::function<void()> onSettingsClicked, onResetLoudness;

private:
    struct Knob
    {
        Knob (juce::AudioProcessorValueTreeState&, const char* parameterId, juce::Rectangle<int> bounds,
              const juce::String& tooltip, const juce::String& suffix, bool hero);

        juce::String id;
        juce::Rectangle<int> bounds;
        juce::Slider slider;
        juce::SliderParameterAttachment attachment;
    };

    void addKnob (juce::AudioProcessorValueTreeState&, const char* parameterId, juce::Rectangle<int> bounds,
                  const juce::String& tooltip, const juce::String& suffix, bool hero = false);

    Backdrop backdrop;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::ToggleButton bypassButton { "Bypass" };
    juce::ShapeButton settingsButton { "Settings", juce::Colour(), juce::Colour(), juce::Colour() };
    LevelMeter inputMeter;
    std::array<GainReductionMeter, (size_t) layout::bands> bandMeters;
    GainReductionMeter limiterMeter { GainReductionMeter::Kind::limiter };
    LoudnessReadout readout;
    juce::ButtonParameterAttachment bypassAttachment;
    std::atomic<float>* ceilingChoice = nullptr;
    float peakHoldDb = Meters::silenceDb;
};
}
