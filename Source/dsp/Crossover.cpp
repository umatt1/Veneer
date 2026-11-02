#include "Crossover.h"

#include "Biquad.h"

namespace veneer
{
void Crossover::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    const juce::dsp::ProcessSpec spec { sampleRate, 512, (juce::uint32) numChannels };

    splitHigh.prepare (spec);
    splitLow.prepare (spec);
    alignHigh.prepare (spec);
    alignHigh.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
}

void Crossover::reset()
{
    splitHigh.reset();
    splitLow.reset();
    alignHigh.reset();
}

void Crossover::setFrequencies (float lowMidHz, float midHighHz)
{
    const auto lowMid = (float) usableFrequency (sampleRate, (double) lowMidHz);
    const auto midHigh = (float) usableFrequency (sampleRate, (double) midHighHz);

    splitLow.setCutoffFrequency (lowMid);
    alignHigh.setCutoffFrequency (lowMid);
    splitHigh.setCutoffFrequency (midHigh);
}

void Crossover::process (int channel, float in, float& low, float& mid, float& high) noexcept
{
    float lowMid = 0.0f, highRaw = 0.0f;

    splitHigh.processSample (channel, in, lowMid, highRaw);
    splitLow.processSample (channel, lowMid, low, mid);
    high = alignHigh.processSample (channel, highRaw);
}
}
