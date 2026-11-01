#pragma once

namespace veneer
{
class BandCompressor
{
public:
    struct Settings
    {
        float thresholdDb = 0.0f;
        float ratio = 1.0f;
        float kneeDb = 6.0f;
        float detectorMs = 10.0f;
        float attackMs = 10.0f;
        float releaseFastMs = 50.0f;
        float releaseSlowMs = 400.0f;
    };

    void prepare (double sampleRate);
    void reset() noexcept;
    void setSettings (const Settings& newSettings) noexcept;

    // takes the linked squared level of the band, returns the linear gain to apply
    float process (float power) noexcept;

    float getGainReductionDb() const noexcept { return grDb; }

private:
    float coefficient (float timeMs) const noexcept;

    double sampleRate = 44100.0;
    Settings settings;
    float detectorCoef = 0.0f, attackCoef = 0.0f, releaseFastCoef = 0.0f, releaseSlowCoef = 0.0f, sustainCoef = 0.0f;
    float meanSquare = 0.0f, grDb = 0.0f, sustainDb = 0.0f;
};
}
