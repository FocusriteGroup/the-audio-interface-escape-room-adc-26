#pragma once

#include <AudioProcessor.h>

#include <vector>

namespace audio_core
{
class LoopbackCapture
{
public:
    explicit LoopbackCapture(std::size_t capacity);
    void reset() noexcept;
    void append(std::array<std::span<const float>, outputCount> outputs) noexcept;
    void rewind() noexcept;
    void replay(std::array<std::span<float>, outputCount> outputs) noexcept;
    std::size_t frameCount() const noexcept;
    std::span<const float> channel(std::size_t channelIndex) const noexcept;

private:
    std::array<std::vector<float>, outputCount> audio;
    std::size_t recorded = 0;
    std::size_t replayPosition = 0;
};

float recordingMeterPeak(std::span<const float> source,
                         std::span<const float> recordingBus) noexcept;

// Clipped samples (at full scale) in the whole take so far, for the CAPTURE readout.
// Called on the audio thread once per block while recording, after the block has been
// appended to the capture. newFrames is how many frames that block added; it is 0 once
// the capture is full.
std::size_t updateCaptureClipCount(std::size_t clipsSoFar, const LoopbackCapture& capture,
                                   std::size_t newFrames) noexcept;
}
