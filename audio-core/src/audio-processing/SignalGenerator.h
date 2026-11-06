#pragma once

#include <cstddef>
#include <vector>

namespace audio_core
{
std::vector<float> generateSine(float frequencyHz,
                                float amplitude,
                                double sampleRate,
                                std::size_t sampleCount);
}
