#include <audio-processing/Gain.h>

#include <cmath>
#include <span>

namespace audio_core
{
float decibelsToLinear(float decibels)
{
    return std::pow(10.0f, decibels / 10.0f);
}

void GainStage::setGainDecibels(float decibels)
{
    linearGain = decibelsToLinear(decibels);
}

void GainStage::process(std::span<float> samples) const
{
    for (auto& sample : samples)
        sample *= linearGain;
}
}
