#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "dsp/Meters.h"

namespace veneer::ui
{
// vertical RMS meter with the target band lit behind it
class LevelMeter final : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr float floorDb = -48.0f, ceilingDb = 0.0f;

    LevelMeter();

    void setLevelDb (float db);
    float getLevelDb() const noexcept { return shownDb; }
    void paint (juce::Graphics&) override;

private:
    juce::Rectangle<float> barArea() const;

    float shownDb = floorDb;
};

// bar that fills downwards from the top as reduction deepens, with the value underneath
class GainReductionMeter final : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Kind { band, limiter };
    static constexpr float rangeDb = 12.0f;

    explicit GainReductionMeter (Kind = Kind::band);

    void setReductionDb (float db, float seconds);
    float getReductionDb() const noexcept { return shownDb; }
    void paint (juce::Graphics&) override;

private:
    Kind kind;
    float shownDb = 0.0f;
};

class LoudnessReadout final : public juce::Component, public juce::SettableTooltipClient
{
public:
    struct Values
    {
        float momentary = Meters::silenceDb, shortTerm = Meters::silenceDb, integrated = Meters::silenceDb,
              truePeak = Meters::silenceDb;
    };

    LoudnessReadout();

    void setValues (const Values&);
    void setCeilingDb (float db);
    const Values& getValues() const noexcept { return values; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void()> onClick;

private:
    Values values;
    float ceilingDb = -1.0f;
};
}
