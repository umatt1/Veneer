#pragma once

#include <juce_dsp/juce_dsp.h>

namespace veneer
{
class Crossover
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();
    void setFrequencies (float lowMidHz, float midHighHz);

    void process (int channel, float in, float& low, float& mid, float& high) noexcept;

private:
    juce::dsp::LinkwitzRileyFilter<float> splitHigh, splitLow, alignHigh;
};
}
