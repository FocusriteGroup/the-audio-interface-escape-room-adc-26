#include <LoopbackCapture.h>

#include <Analysis.h>

#include <algorithm>
#include <cmath>

namespace audio_core
{
float recordingMeterPeak(std::span<const float>,
                         std::span<const float> recordingBus) noexcept
{
    return analyze(recordingBus).peak;
}

std::size_t updateCaptureClipCount(std::size_t clipsSoFar, const LoopbackCapture& capture,
                                   std::size_t newFrames) noexcept
{
    // Only the frames this block added are new; earlier ones are already counted.
    const auto first = capture.frameCount() - std::min(newFrames, capture.frameCount());
    for (std::size_t output = 0; output < outputCount; ++output)
        for (const auto sample : capture.channel(output).subspan(first))
            if (std::abs(sample) >= 1.0f)
                ++clipsSoFar;
    return clipsSoFar;
}
}
