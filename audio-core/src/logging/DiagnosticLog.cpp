#include <logging/DiagnosticLog.h>

#include <atomic>
#include <cstring>

namespace audio_core
{
namespace
{
constexpr std::uint64_t capacity = 256;

struct DiagnosticLogState
{
    void push(const char* text, double value, bool hasValue) noexcept
    {
        const auto write = writePosition.load(std::memory_order_relaxed);
        const auto read = readPosition.load(std::memory_order_acquire);
        if (write - read >= capacity)
        {
            droppedEntries.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        auto& entry = entries[static_cast<std::size_t>(write % capacity)];
        entry.text.fill('\0');
        if (text != nullptr)
            std::strncpy(entry.text.data(), text, entry.text.size() - 1);
        entry.value = value;
        entry.sequence = nextSequence++;
        entry.hasValue = hasValue;
        writePosition.store(write + 1, std::memory_order_release);
    }

    std::array<DiagnosticLogEntry, capacity> entries;
    std::atomic<std::uint64_t> writePosition { 0 };
    std::atomic<std::uint64_t> readPosition { 0 };
    std::atomic<std::uint64_t> droppedEntries { 0 };
    std::uint64_t nextSequence = 1;
};

DiagnosticLogState state;
}

void logDiagnostic(const char* message) noexcept
{
    state.push(message, 0.0, false);
}

void logDiagnosticValue(const char* label, double value) noexcept
{
    state.push(label, value, true);
}

bool tryPopDiagnosticLog(DiagnosticLogEntry& entry) noexcept
{
    const auto read = state.readPosition.load(std::memory_order_relaxed);
    if (read == state.writePosition.load(std::memory_order_acquire))
        return false;

    entry = state.entries[static_cast<std::size_t>(read % capacity)];
    state.readPosition.store(read + 1, std::memory_order_release);
    return true;
}

std::uint64_t takeDroppedDiagnosticLogCount() noexcept
{
    return state.droppedEntries.exchange(0, std::memory_order_acq_rel);
}

void clearDiagnosticLog() noexcept
{
    state.readPosition.store(state.writePosition.load(std::memory_order_acquire),
                             std::memory_order_release);
    state.droppedEntries.store(0, std::memory_order_relaxed);
}
}