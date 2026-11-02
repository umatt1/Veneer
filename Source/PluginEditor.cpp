#include "PluginEditor.h"

#include <cmath>
#include <iterator>

#include "Parameters.h"
#include "ui/Layout.h"

namespace
{
namespace layout = veneer::ui::layout;

int widthForScale (float scale)
{
    return juce::roundToInt ((float) layout::width * scale);
}

juce::String percentText (float scale)
{
    return juce::String (juce::roundToInt (scale * 100.0f)) + "%";
}
}

VeneerEditor::VeneerEditor (VeneerProcessor& p)
    : juce::AudioProcessorEditor (&p), owner (p), faceplate (p.getState())
{
    const auto saved = owner.getEditorWidth();

    addAndMakeVisible (faceplate);

    faceplate.onSettingsClicked = [this] { showSettingsMenu(); };
    faceplate.onResetLoudness = [this] { owner.resetLoudness(); };

    setResizeLimits (widthForScale (layout::minScale), widthForScale (layout::minScale) * layout::height / layout::width,
                     widthForScale (layout::maxScale), widthForScale (layout::maxScale) * layout::height / layout::width);
    getConstrainer()->setFixedAspectRatio ((double) layout::width / (double) layout::height);
    setResizable (true, true);

    setInterfaceScale (saved > 0 ? (float) saved / (float) layout::width : 1.0f);

    // after the children exist: setting it recurses, and a slider builds its text box from the look-and-feel it finds
    setLookAndFeel (&lookAndFeel);

    owner.getMeters().takeHeld();
    startTimerHz (refreshHz);
}

VeneerEditor::~VeneerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VeneerEditor::resized()
{
    const auto scale = (float) getWidth() / (float) layout::width;

    if (scale > 0.0f)
    {
        faceplate.setTransform (juce::AffineTransform::scale (scale));
        faceplate.setBounds (0, 0, layout::width, layout::height);
        owner.setEditorWidth (getWidth());
    }
}

void VeneerEditor::updateMeters (float seconds)
{
    faceplate.updateMeters (owner.getMeters(), seconds);
}

void VeneerEditor::setInterfaceScale (float scale)
{
    const auto clamped = juce::jlimit (layout::minScale, layout::maxScale, scale);
    setSize (widthForScale (clamped), juce::roundToInt ((float) layout::height * clamped));
}

float VeneerEditor::getInterfaceScale() const noexcept
{
    return (float) getWidth() / (float) layout::width;
}

void VeneerEditor::showSettingsMenu()
{
    juce::PopupMenu menu;

    menu.addSectionHeader ("Interface size");

    for (size_t i = 0; i < std::size (scaleSteps); ++i)
        menu.addItem (scaleBase + (int) i, percentText (scaleSteps[i]), true, std::abs (getInterfaceScale() - scaleSteps[i]) < 0.01f);

    menu.addSectionHeader ("True-peak ceiling");
    const auto current = juce::jlimit (0, 1, juce::roundToInt (owner.getState().getRawParameterValue (veneer::params::ceiling)->load()));

    for (int i = 0; i < 2; ++i)
        menu.addItem (ceilingBase + i, juce::String (veneer::params::ceilingDbChoices[i], 1) + " dBTP", true, i == current);

    menu.addSeparator();
    menu.addItem (resetReadouts, "Reset loudness and peak readouts");
    menu.addSeparator();
    menu.addItem (about, juce::String ("Veneer ") + JucePlugin_VersionString + ", free software (AGPL-3.0)", false);

    juce::Component::SafePointer<VeneerEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (faceplate.getSettingsButton()),
                        [safe] (int id) { if (safe != nullptr && id != 0) safe->applyMenuChoice (id); });
}

void VeneerEditor::applyMenuChoice (int id)
{
    if (id >= scaleBase && id < scaleBase + (int) std::size (scaleSteps))
    {
        setInterfaceScale (scaleSteps[(size_t) (id - scaleBase)]);
    }
    else if (id == ceilingBase || id == ceilingBase + 1)
    {
        if (auto* ceiling = owner.getState().getParameter (veneer::params::ceiling))
        {
            ceiling->beginChangeGesture();
            ceiling->setValueNotifyingHost (ceiling->convertTo0to1 ((float) (id - ceilingBase)));
            ceiling->endChangeGesture();
        }
    }
    else if (id == resetReadouts)
    {
        faceplate.resetReadouts();
    }
}
