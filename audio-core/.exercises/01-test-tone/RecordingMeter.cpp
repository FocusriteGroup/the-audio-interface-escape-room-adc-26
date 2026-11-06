#include <LoopbackCapture.h>

namespace audio_core
{
// Loopback recording isn't part of this build: the app doesn't show its controls, so
// nothing calls these yet.
float recordingMeterPeak(std::span<const float>, std::span<const float>) noexcept
{
    return 0.0f;
}

std::size_t updateCaptureClipCount(std::size_t clipsSoFar, const LoopbackCapture&,
                                   std::size_t) noexcept
{
    return clipsSoFar;
}
}
