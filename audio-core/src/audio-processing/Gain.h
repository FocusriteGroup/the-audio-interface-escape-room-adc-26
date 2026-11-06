#pragma once

#include <span>

namespace audio_core
{
float decibelsToLinear(float decibels);

class GainStage
{
public:
    void setGainDecibels(float decibels);
    void process(std::span<float> samples) const;

private:
    float linearGain = 1.0f;
};
}
