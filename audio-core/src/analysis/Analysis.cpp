#include <Analysis.h>

#include <algorithm>
#include <cmath>

namespace audio_core
{
SignalMetrics analyze(std::span<const float> samples)
{
    SignalMetrics metrics;
    double sumOfSquares = 0.0;

    for (const auto sample : samples)
    {
        metrics.peak = std::max(metrics.peak, std::abs(sample));
        sumOfSquares += static_cast<double>(sample) * sample;
        if (std::abs(sample) > 1.0f)
            ++metrics.clippedSamples;
    }

    if (!samples.empty())
        metrics.rootMeanSquare = static_cast<float>(
            std::sqrt(sumOfSquares / static_cast<double>(samples.size())));

    return metrics;
}

float correlation(std::span<const float> first, std::span<const float> second)
{
    const auto sampleCount = std::min(first.size(), second.size());
    if (sampleCount == 0)
        return 0.0f;

    double crossProduct = 0.0;
    double firstEnergy = 0.0;
    double secondEnergy = 0.0;
    for (std::size_t index = 0; index < sampleCount; ++index)
    {
        crossProduct += static_cast<double>(first[index]) * second[index];
        firstEnergy += static_cast<double>(first[index]) * first[index];
        secondEnergy += static_cast<double>(second[index]) * second[index];
    }

    const auto denominator = std::sqrt(firstEnergy * secondEnergy);
    return denominator > 0.0 ? static_cast<float>(crossProduct / denominator) : 0.0f;
}
}
