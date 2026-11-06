#pragma once

#include <array>
#include <cstdint>

namespace audio_core
{
struct DiagnosticLogEntry
{
    std::array<char, 64> text {};
    double value = 0.0;
    std::uint64_t sequence = 0;
    bool hasValue = false;
};

void logDiagnostic(const char* message) noexcept;
void logDiagnosticValue(const char* label, double value) noexcept;
bool tryPopDiagnosticLog(DiagnosticLogEntry& entry) noexcept;
std::uint64_t takeDroppedDiagnosticLogCount() noexcept;
void clearDiagnosticLog() noexcept;
}