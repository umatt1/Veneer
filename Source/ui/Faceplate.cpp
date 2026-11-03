#include "Faceplate.h"

#include <algorithm>
#include <cmath>

#include "Parameters.h"
#include "Style.h"

namespace veneer::ui
{
namespace
{
juce::Path gearShape()
{
    constexpr int teeth = 8;
    constexpr float outer = 10.0f, root = 7.6f;
    constexpr float step = juce::MathConstants<float>::twoPi / (float) teeth;
    constexpr float offsets[] = { -0.30f, -0.17f, 0.17f, 0.30f };
    constexpr float radii[] = { root, outer, outer, root };

    juce::Path outline;

    for (int tooth = 0; tooth < teeth; ++tooth)
    {
        for (size_t corner = 0; corner < 4; ++corner)
        {
            const auto angle = ((float) tooth + offsets[corner]) * step;
            const juce::Point<float> p (std::sin (angle) * radii[corner], -std::cos (angle) * radii[corner]);

            if (tooth == 0 && corner == 0)
                outline.startNewSubPath (p);
            else
                outline.lineTo (p);
        }
    }

    outline.closeSubPath();

    auto gear = outline.createPathWithRoundedCorners (1.2f);
    gear.addEllipse (-3.2f, -3.2f, 6.4f, 6.4f);
    gear.setUsingNonZeroWinding (false);
    return gear;
}
}

Faceplate::Knob::Knob (juce::AudioProcessorValueTreeState& state, const char* parameterId, juce::Rectangle<int> area,
                       const juce::String& tooltip, const juce::String& suffix, bool hero)
    : id (parameterId),
      bounds (area),
      attachment (*state.getParameter (parameterId), slider)
{
    auto& parameter = *state.getParameter (parameterId);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, hero ? 120 : 72, hero ? 26 : 18);
    slider.setWantsKeyboardFocus (true);
    slider.setTitle (parameter.getName (64));
    slider.setTooltip (tooltip);
    slider.getProperties().set ("hero", hero);

    slider.textFromValueFunction = [&parameter, suffix] (double value)
    {
        return parameter.getText (parameter.convertTo0to1 ((float) value), 0) + suffix;
    };
    slider.updateText();
}

Faceplate::Faceplate (juce::AudioProcessorValueTreeState& state)
    : bypassAttachment (*state.getParameter (params::bypass), bypassButton),
      ceilingChoice (state.getRawParameterValue (params::ceiling))
{
    addAndMakeVisible (backdrop);

    addKnob (state, params::input, layout::inputKnob(), "Trim into the processor. Aim the level meter into the lit band.", " dB");
    addKnob (state, params::amount, layout::amountKnob(),
             "How much of everything: tone, compression, saturation and width together. 0% is neutral.", "%", true);
    addKnob (state, params::weight, layout::bandKnob (0), "Level and drive of the low band, below 200 Hz.", " dB");
    addKnob (state, params::body, layout::bandKnob (1), "Level and drive of the mid band, 200 Hz to 5 kHz.", " dB");
    addKnob (state, params::air, layout::bandKnob (2), "Level and drive of the high band, above 5 kHz.", " dB");
    addKnob (state, params::push, layout::pushKnob(), "How hard the limiter is driven.", " dB");
    addKnob (state, params::output, layout::outputKnob(), "Final level trim after the limiter.", " dB");

    bypassButton.setTitle ("Bypass");
    bypassButton.setTooltip ("Hear the input, delayed to match the processed signal.");
    bypassButton.setWantsKeyboardFocus (true);
    addAndMakeVisible (bypassButton);

    settingsButton.setShape (gearShape(), false, true, false);
    settingsButton.setColours (colour (palette::text).withAlpha (0.7f), colour (palette::text), colour (palette::brass));
    settingsButton.setTitle ("Settings");
    settingsButton.setTooltip ("Interface size, true-peak ceiling and readout reset.");
    settingsButton.setBorderSize (juce::BorderSize<int> (6));
    settingsButton.setWantsKeyboardFocus (true);
    settingsButton.onClick = [this] { if (onSettingsClicked) onSettingsClicked(); };
    addAndMakeVisible (settingsButton);

    inputMeter.setTooltip ("Input level (RMS). The lit band is the level the dynamics are voiced for.");
    addAndMakeVisible (inputMeter);

    static constexpr const char* bandNames[] = { "low", "mid", "high" };

    for (size_t band = 0; band < bandMeters.size(); ++band)
    {
        bandMeters[band].setTooltip (juce::String ("Gain reduction in the ") + bandNames[band] + " band.");
        addAndMakeVisible (bandMeters[band]);
    }

    limiterMeter.setTooltip ("Gain reduction in the limiter.");
    addAndMakeVisible (limiterMeter);

    readout.setTooltip ("Momentary, short-term and integrated loudness, and true peak. Click to reset the integrated value and the peak.");
    readout.onClick = [this] { resetReadouts(); };
    addAndMakeVisible (readout);

    setSize (layout::width, layout::height);
}

void Faceplate::addKnob (juce::AudioProcessorValueTreeState& state, const char* parameterId, juce::Rectangle<int> bounds,
                         const juce::String& tooltip, const juce::String& suffix, bool hero)
{
    knobs.push_back (std::make_unique<Knob> (state, parameterId, bounds, tooltip, suffix, hero));
    addAndMakeVisible (knobs.back()->slider);
}

void Faceplate::resized()
{
    backdrop.setBounds (getLocalBounds());
    bypassButton.setBounds (layout::bypassButton());
    settingsButton.setBounds (layout::settingsButton());
    inputMeter.setBounds (layout::inputMeter());

    for (auto& knob : knobs)
        knob->slider.setBounds (knob->bounds);

    for (size_t band = 0; band < bandMeters.size(); ++band)
        bandMeters[band].setBounds (layout::bandMeter ((int) band));

    limiterMeter.setBounds (layout::limiterMeter());
    readout.setBounds (layout::loudness());
}

juce::Slider* Faceplate::findSlider (const juce::String& parameterId)
{
    for (auto& knob : knobs)
        if (knob->id == parameterId)
            return &knob->slider;

    return nullptr;
}

void Faceplate::updateMeters (Meters& meters, float seconds)
{
    const auto held = meters.takeHeld();
    inputMeter.setLevelDb (meters.inputRmsDb.load (std::memory_order_relaxed));

    for (size_t band = 0; band < bandMeters.size(); ++band)
        bandMeters[band].setReductionDb (held.bandGrDb[band], seconds);

    limiterMeter.setReductionDb (held.limiterGrDb, seconds);

    peakHoldDb = std::max (peakHoldDb, held.truePeakDb);
    readout.setCeilingDb (params::ceilingDbChoices[juce::jlimit (0, 1, (int) ceilingChoice->load())]);
    readout.setValues ({ meters.momentaryLufs.load (std::memory_order_relaxed),
                         meters.shortTermLufs.load (std::memory_order_relaxed),
                         meters.integratedLufs.load (std::memory_order_relaxed),
                         peakHoldDb });
}

void Faceplate::resetReadouts()
{
    peakHoldDb = Meters::silenceDb;
    auto values = readout.getValues();
    values.integrated = values.truePeak = Meters::silenceDb;
    readout.setValues (values);

    if (onResetLoudness)
        onResetLoudness();
}
}
