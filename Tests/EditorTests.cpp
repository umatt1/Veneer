#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "TestUtils.h"
#include "ui/Layout.h"

namespace
{
namespace layout = veneer::ui::layout;
namespace params = veneer::params;

constexpr double fs = 48000.0;
constexpr int refreshSamples = 1600;

const char* const floatIds[] = { params::input, params::amount, params::weight, params::body,
                                 params::air, params::push, params::output };

juce::RangedAudioParameter& parameter (VeneerProcessor& processor, const char* id)
{
    auto* p = processor.getState().getParameter (id);
    REQUIRE (p != nullptr);
    return *p;
}

void setParameter (VeneerProcessor& processor, const char* id, float value)
{
    auto& p = parameter (processor, id);
    p.setValueNotifyingHost (p.convertTo0to1 (value));
}

float getParameter (VeneerProcessor& processor, const char* id)
{
    auto& p = parameter (processor, id);
    return p.convertFrom0to1 (p.getValue());
}

void prepare (VeneerProcessor& processor)
{
    processor.setPlayConfigDetails (2, 2, fs, 512);
    processor.prepareToPlay (fs, 512);
}

// processes in host-sized blocks and refreshes the editor at roughly its timer rate
void play (VeneerProcessor& processor, VeneerEditor& editor, const std::vector<float>& signal)
{
    juce::MidiBuffer midi;
    int sinceRefresh = 0;

    for (size_t start = 0; start < signal.size(); start += 512)
    {
        const auto length = (int) std::min<size_t> (512, signal.size() - start);
        juce::AudioBuffer<float> buffer (2, length);

        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (ch, 0, signal.data() + start, length);

        processor.processBlock (buffer, midi);
        sinceRefresh += length;

        if (sinceRefresh >= refreshSamples)
        {
            editor.updateMeters ((float) refreshSamples / (float) fs);
            sinceRefresh = 0;
        }
    }
}

std::vector<juce::Component*> controlsOf (veneer::ui::Faceplate& faceplate)
{
    std::vector<juce::Component*> controls;

    for (int i = 0; i < faceplate.getNumChildComponents(); ++i)
        if (faceplate.getChildComponent (i)->getBounds() != faceplate.getLocalBounds())
            controls.push_back (faceplate.getChildComponent (i));

    return controls;
}
}

TEST_CASE ("every control sits inside the faceplate and none overlap", "[ui]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;
    VeneerEditor editor (processor);
    auto& faceplate = editor.getFaceplate();

    const auto controls = controlsOf (faceplate);
    REQUIRE (controls.size() == 15u);

    for (size_t a = 0; a < controls.size(); ++a)
    {
        INFO ("control " << a);
        REQUIRE (faceplate.getLocalBounds().contains (controls[a]->getBounds()));

        for (size_t b = a + 1; b < controls.size(); ++b)
            REQUIRE_FALSE (controls[a]->getBounds().intersects (controls[b]->getBounds()));
    }
}

TEST_CASE ("moving a control moves its parameter, and the other way round", "[ui]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;
    VeneerEditor editor (processor);
    auto& faceplate = editor.getFaceplate();

    for (const auto* id : floatIds)
    {
        INFO (id);
        auto* slider = faceplate.findSlider (id);
        REQUIRE (slider != nullptr);

        const auto target = (float) (slider->getMinimum() + 0.7 * (slider->getMaximum() - slider->getMinimum()));
        slider->setValue (target, juce::sendNotificationSync);
        REQUIRE (std::abs (getParameter (processor, id) - target) < 0.06f);

        setParameter (processor, id, (float) slider->getMinimum());
        REQUIRE (std::abs (slider->getValue() - slider->getMinimum()) < 1.0e-3);
    }

    auto& bypass = faceplate.getBypassButton();
    bypass.setToggleState (true, juce::sendNotificationSync);
    REQUIRE (getParameter (processor, params::bypass) > 0.5f);
    setParameter (processor, params::bypass, 0.0f);
    REQUIRE_FALSE (bypass.getToggleState());
}

TEST_CASE ("every knob has the parameter's name, a tooltip and keyboard focus", "[ui]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;
    VeneerEditor editor (processor);
    auto& faceplate = editor.getFaceplate();

    for (const auto* id : floatIds)
    {
        INFO (id);
        auto* slider = faceplate.findSlider (id);
        REQUIRE (slider != nullptr);
        REQUIRE (slider->getTitle() == parameter (processor, id).getName (64));
        REQUIRE (slider->getTooltip().isNotEmpty());
        REQUIRE (slider->getWantsKeyboardFocus());

        const auto& p = parameter (processor, id);
        REQUIRE (std::abs ((float) slider->getDoubleClickReturnValue() - p.convertFrom0to1 (p.getDefaultValue())) < 0.06f);
    }

    REQUIRE (faceplate.getBypassButton().getTitle() == "Bypass");
    REQUIRE (faceplate.getSettingsButton().getTitle() == "Settings");
}

TEST_CASE ("a knob shows its value with the unit, and typed values reach the parameter", "[ui]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;
    VeneerEditor editor (processor);
    auto& faceplate = editor.getFaceplate();

    auto text = [&] (const char* id, double value) { return faceplate.findSlider (id)->getTextFromValue (value); };

    REQUIRE (text (params::input, 4.5) == "+4.5 dB");
    REQUIRE (text (params::input, -2.5) == "-2.5 dB");
    REQUIRE (text (params::amount, 137.0) == "137%");
    REQUIRE (text (params::weight, 0.0) == "0.0 dB");
    REQUIRE (text (params::push, 3.0) == "3.0 dB");
    REQUIRE (text (params::output, -4.0) == "-4.0 dB");

    auto* output = faceplate.findSlider (params::output);
    output->setValue (output->getValueFromText ("-3 dB"), juce::sendNotificationSync);
    REQUIRE (std::abs (getParameter (processor, params::output) + 3.0f) < 0.06f);

    auto* amount = faceplate.findSlider (params::amount);
    amount->setValue (amount->getValueFromText ("150%"), juce::sendNotificationSync);
    REQUIRE (std::abs (getParameter (processor, params::amount) - 150.0f) < 0.06f);
}

TEST_CASE ("the editor scales in proportion and stays inside its limits", "[ui]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;
    VeneerEditor editor (processor);
    auto& faceplate = editor.getFaceplate();

    for (const auto scale : VeneerEditor::scaleSteps)
    {
        INFO ("scale " << scale);
        editor.setInterfaceScale (scale);

        REQUIRE (editor.getWidth() == juce::roundToInt ((float) layout::width * scale));
        REQUIRE (editor.getHeight() == juce::roundToInt ((float) layout::height * scale));
        REQUIRE (std::abs ((double) editor.getWidth() / editor.getHeight() - (double) layout::width / layout::height) < 0.005);
        REQUIRE (faceplate.getWidth() == layout::width);
        REQUIRE (faceplate.getHeight() == layout::height);
        REQUIRE (std::abs (faceplate.getTransform().mat00 - scale) < 0.002f);
        REQUIRE (processor.getEditorWidth() == editor.getWidth());
    }

    editor.setInterfaceScale (0.1f);
    REQUIRE (editor.getWidth() == juce::roundToInt ((float) layout::width * layout::minScale));

    editor.setInterfaceScale (5.0f);
    REQUIRE (editor.getWidth() == juce::roundToInt ((float) layout::width * layout::maxScale));
}

TEST_CASE ("a reopened editor comes back at the size it was left at", "[ui]")
{
    juce::ScopedJuceInitialiser_GUI init;

    VeneerProcessor first;

    {
        VeneerEditor editor (first);
        REQUIRE (editor.getWidth() == layout::width);
        editor.setInterfaceScale (1.5f);
    }

    juce::MemoryBlock state;
    first.getStateInformation (state);

    VeneerProcessor second;
    second.setStateInformation (state.getData(), (int) state.getSize());
    REQUIRE_FALSE (second.getState().state.hasProperty ("editorWidth"));

    VeneerEditor reopened (second);
    REQUIRE (reopened.getWidth() == 1140);
}

TEST_CASE ("the meters follow the engine and the peak hold lasts until it is reset", "[ui]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;
    prepare (processor);
    setParameter (processor, params::amount, 0.0f);
    setParameter (processor, params::push, 12.0f);

    VeneerEditor editor (processor);
    auto& faceplate = editor.getFaceplate();

    play (processor, editor, testutil::sine (1000.0, 0.9, fs, 48000));

    REQUIRE (std::abs (faceplate.getInputMeter().getLevelDb() + 3.9f) < 0.6f);
    REQUIRE (faceplate.getLimiterMeter().getReductionDb() > 10.0f);

    const auto peak = faceplate.getReadout().getValues().truePeak;
    REQUIRE (std::abs (peak + 1.3f) < 0.5f);

    play (processor, editor, std::vector<float> (4 * 48000, 0.0f));
    REQUIRE (faceplate.getLimiterMeter().getReductionDb() < 0.5f);
    REQUIRE (faceplate.getInputMeter().getLevelDb() < -40.0f);
    REQUIRE (std::abs (faceplate.getReadout().getValues().truePeak - peak) < 0.01f);

    faceplate.resetReadouts();
    editor.updateMeters (0.033f);
    REQUIRE (faceplate.getReadout().getValues().truePeak <= -99.0f);
    REQUIRE (faceplate.getReadout().getValues().integrated <= -99.0f);

    setParameter (processor, params::amount, 100.0f);
    setParameter (processor, params::push, 0.0f);
    play (processor, editor, testutil::sine (1000.0, 0.5, fs, 48000));
    REQUIRE (faceplate.getBandMeter (1).getReductionDb() > 3.0f);
}

TEST_CASE ("the readout turns red only above the ceiling", "[ui]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;
    VeneerEditor editor (processor);
    auto& readout = editor.getFaceplate().getReadout();

    auto render = [&] (float truePeakDb)
    {
        readout.setCeilingDb (-1.0f);
        readout.setValues ({ -20.0f, -20.0f, -20.0f, truePeakDb });
        return readout.createComponentSnapshot (readout.getLocalBounds());
    };

    auto redPixels = [] (const juce::Image& image)
    {
        int count = 0;

        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
            {
                const auto pixel = image.getPixelAt (x, y);

                if (pixel.getAlpha() > 128 && pixel.getRed() > 190 && pixel.getGreen() < 150 && pixel.getBlue() < 120)
                    ++count;
            }

        return count;
    };

    REQUIRE (redPixels (render (-1.3f)) == 0);
    REQUIRE (redPixels (render (-0.97f)) == 0);
    REQUIRE (redPixels (render (0.4f)) > 20);
}

TEST_CASE ("the settings menu changes the size, the ceiling and the readouts", "[ui]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;
    VeneerEditor editor (processor);

    editor.applyMenuChoice (VeneerEditor::scaleBase + 3);
    REQUIRE (std::abs (editor.getInterfaceScale() - 1.5f) < 0.001f);

    editor.applyMenuChoice (VeneerEditor::ceilingBase + 1);
    REQUIRE (std::abs (getParameter (processor, params::ceiling) - 1.0f) < 0.001f);

    editor.applyMenuChoice (VeneerEditor::ceilingBase);
    REQUIRE (std::abs (getParameter (processor, params::ceiling)) < 0.001f);

    editor.applyMenuChoice (VeneerEditor::about);
    editor.applyMenuChoice (0);
    REQUIRE (std::abs (editor.getInterfaceScale() - 1.5f) < 0.001f);

    processor.getMeters().integratedLufs.store (-14.0f);
    editor.updateMeters (0.033f);
    REQUIRE (std::abs (editor.getFaceplate().getReadout().getValues().integrated + 14.0f) < 0.001f);

    editor.applyMenuChoice (VeneerEditor::resetReadouts);
    REQUIRE (editor.getFaceplate().getReadout().getValues().integrated <= -99.0f);
}

TEST_CASE ("the interface draws, and a knob shows its value", "[ui]")
{
    juce::ScopedJuceInitialiser_GUI init;
    VeneerProcessor processor;
    VeneerEditor editor (processor);

    auto snapshot = [&] { return editor.createComponentSnapshot (editor.getLocalBounds()); };

    const auto full = snapshot();
    REQUIRE (full.getWidth() == layout::width);
    REQUIRE (full.getHeight() == layout::height);

    std::set<juce::uint32> colours;

    for (int y = 0; y < full.getHeight(); y += 3)
        for (int x = 0; x < full.getWidth(); x += 3)
            colours.insert (full.getPixelAt (x, y).getARGB());

    REQUIRE (colours.size() > 200);

    setParameter (processor, params::amount, 0.0f);
    const auto lowered = snapshot();

    const auto knob = layout::amountKnob();
    int insideDifferences = 0, outsideDifferences = 0;

    for (int y = 0; y < full.getHeight(); ++y)
    {
        for (int x = 0; x < full.getWidth(); ++x)
        {
            if (full.getPixelAt (x, y) != lowered.getPixelAt (x, y))
                (knob.contains (x, y) ? insideDifferences : outsideDifferences)++;
        }
    }

    REQUIRE (insideDifferences > 50);
    REQUIRE (outsideDifferences == 0);
}
