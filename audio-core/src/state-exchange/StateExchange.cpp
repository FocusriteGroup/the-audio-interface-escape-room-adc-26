#include <state-exchange/StateExchange.h>

namespace audio_core
{
bool StateExchange::publish(const ProcessorState& state) noexcept
{
    const auto write = writeIndex.load(std::memory_order_relaxed);
    const auto nextWrite = (write + 1) % capacity;
    if (nextWrite == readIndex.load(std::memory_order_acquire))
        return false;

    states[write] = state;
    writeIndex.store(nextWrite, std::memory_order_release);
    return true;
}

bool StateExchange::consumeLatest(ProcessorState& state) noexcept
{
    auto read = readIndex.load(std::memory_order_relaxed);
    const auto write = writeIndex.load(std::memory_order_acquire);
    if (read == write)
        return false;

    while (read != write)
    {
        state = states[read];
        read = (read + 1) % capacity;
    }

    readIndex.store(read, std::memory_order_release);
    return true;
}
}
