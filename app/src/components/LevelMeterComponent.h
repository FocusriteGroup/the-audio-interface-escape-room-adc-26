#pragma once

#include <JuceHeader.h>

class LevelMeterComponent final : public juce::Component
{
public:
    void setLevels(float newLevel, float newPeakHold);
    void paint(juce::Graphics& graphics) override;

private:
    float level = 0.0f;
    float peakHold = 0.0f;
};