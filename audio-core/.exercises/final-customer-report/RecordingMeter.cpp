#include <LoopbackCapture.h>

#include <Analysis.h>

#include <algorithm>
#include <cmath>

namespace audio_core
{
float recordingMeterPeak(std::span<const float> source,
                         std::span<const float> recordingBus) noexcept
{
    return analyze(source).peak;
}

std::size_t updateCaptureClipCount(std::size_t clipsSoFar, const LoopbackCapture& capture,
                                   std::size_t newFrames) noexcept
{
    // Recount the whole take, so the readout can never drift from what was recorded.
    std::size_t clips = 0;
    for (std::size_t output = 0; output < outputCount; ++output)
        for (const auto sample : capture.channel(output))
            if (std::abs(sample) >= 1.0f)
                ++clips;
    return clips;
}
}
