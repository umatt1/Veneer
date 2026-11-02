#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace veneer::ui
{
// everything on the faceplate that never changes: panels, veneer header, wordmark and captions
class Backdrop final : public juce::Component
{
public:
    Backdrop();

    void paint (juce::Graphics&) override;
};
}
