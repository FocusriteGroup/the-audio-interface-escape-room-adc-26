#include <audio-processing/SignalGenerator.h>

#include <cmath>
#include <numbers>

namespace audio_core
{
std::vector<float> generateSine(float frequencyHz,
                                float amplitude,
                                double sampleRate,
                                std::size_t sampleCount)
{
    std::vector<float> samples(sampleCount);
    const auto phaseIncrement = 2.0 * std::numbers::pi * frequencyHz / sampleRate;

    for (std::size_t index = 0; index < samples.size(); ++index)
        samples[index] = amplitude * static_cast<float>(std::sin(phaseIncrement * index));

    return samples;
}
}
