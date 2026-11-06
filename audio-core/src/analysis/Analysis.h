#pragma once

#include <cstddef>
#include <span>

namespace audio_core
{
struct SignalMetrics
{
    float peak = 0.0f;
    float rootMeanSquare = 0.0f;
    std::size_t clippedSamples = 0;
};

SignalMetrics analyze(std::span<const float> samples);
float correlation(std::span<const float> first, std::span<const float> second);
}
