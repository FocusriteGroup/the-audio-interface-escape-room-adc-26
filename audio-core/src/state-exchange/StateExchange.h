#pragma once

#include <AudioProcessor.h>

#include <array>
#include <atomic>
#include <cstddef>

namespace audio_core
{
class StateExchange
{
public:
    bool publish(const ProcessorState& state) noexcept;
    bool consumeLatest(ProcessorState& state) noexcept;

private:
    static constexpr std::size_t capacity = 8;
    std::array<ProcessorState, capacity> states;
    std::atomic<std::size_t> writeIndex { 0 };
    std::atomic<std::size_t> readIndex { 0 };
};
}
