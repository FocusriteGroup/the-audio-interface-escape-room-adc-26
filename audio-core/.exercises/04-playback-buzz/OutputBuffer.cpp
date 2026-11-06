#include <audio-processing/OutputBuffer.h>

#include <algorithm>

namespace audio_core
{
void copyOutputBlock(std::span<const float> source, std::span<float> destination) noexcept
{
    std::fill(destination.begin(), destination.end(), 0.0f);
    const auto sampleCount = std::min(source.size(), destination.size());
    if (sampleCount > 0)
        std::copy_n(source.begin(), sampleCount - 1, destination.begin());
}
}