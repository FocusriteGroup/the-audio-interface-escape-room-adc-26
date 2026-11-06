#include <LoopbackCapture.h>

#include <algorithm>

namespace audio_core
{
LoopbackCapture::LoopbackCapture(std::size_t capacity)
{
    for (auto& channel : audio)
        channel.resize(capacity);
}

void LoopbackCapture::reset() noexcept
{
    recorded = 0;
    replayPosition = 0;
}

void LoopbackCapture::append(std::array<std::span<const float>, outputCount> outputs) noexcept
{
    const auto count = std::min({ outputs[0].size(), outputs[1].size(),
                                 audio[0].size() - recorded });
    for (std::size_t outputIndex = 0; outputIndex < outputCount; ++outputIndex)
        for (std::size_t sample = 0; sample < count; ++sample)
            audio[outputIndex][recorded + sample] = std::clamp(outputs[outputIndex][sample],
                                                                -1.0f, 1.0f);
    recorded += count;
}

void LoopbackCapture::rewind() noexcept
{
    replayPosition = 0;
}

void LoopbackCapture::replay(std::array<std::span<float>, outputCount> outputs) noexcept
{
    for (auto output : outputs)
        std::fill(output.begin(), output.end(), 0.0f);
    const auto count = std::min(outputs[0].size(), outputs[1].size());
    if (recorded == 0)
        return;
    for (std::size_t sample = 0; sample < count; ++sample)
    {
        for (std::size_t outputIndex = 0; outputIndex < outputCount; ++outputIndex)
            outputs[outputIndex][sample] = audio[outputIndex][replayPosition];
        replayPosition = (replayPosition + 1) % recorded;
    }
}

std::size_t LoopbackCapture::frameCount() const noexcept
{
    return recorded;
}

std::span<const float> LoopbackCapture::channel(std::size_t channelIndex) const noexcept
{
    return channelIndex < outputCount
        ? std::span<const float>(audio[channelIndex]).first(recorded)
        : std::span<const float>();
}
}