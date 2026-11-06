#pragma once

#include <Analysis.h>
#include <AudioProcessor.h>

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace audio_core
{
enum class ReferenceScenario
{
    cleanPassthrough,
    asymmetricRouting,
    matchedInputs,
    steadySignals,
    exerciseRightChannel,
    exerciseMono,
    exercisePreset,
    exerciseRecording
};

struct ReferenceScenarioMetrics
{
    std::array<SignalMetrics, inputCount> input;
    std::array<SignalMetrics, outputCount> output;
    std::array<std::array<float, inputCount>, outputCount> outputInputCorrelation {};
    float outputCorrelation = 0.0f;
    SignalMetrics monoOutput;
};

struct ReferenceScenarioResult
{
    ReferenceScenario scenario = ReferenceScenario::cleanPassthrough;
    double sampleRate = 0.0;
    std::size_t blockSize = 0;
    std::size_t processedBlocks = 0;
    float recordingBusPeak = 0.0f;
    float displayedRecordingPeak = 0.0f;
    std::array<std::vector<float>, inputCount> inputs;
    std::array<std::vector<float>, outputCount> outputs;
    ReferenceScenarioMetrics metrics;
};

std::span<const std::string_view> referenceScenarioNames() noexcept;
std::string_view referenceScenarioName(ReferenceScenario scenario) noexcept;
ReferenceScenario referenceScenarioFromName(std::string_view name);
ReferenceScenarioResult runReferenceScenario(ReferenceScenario scenario,
                                             double sampleRate,
                                             std::size_t blockSize);
}
