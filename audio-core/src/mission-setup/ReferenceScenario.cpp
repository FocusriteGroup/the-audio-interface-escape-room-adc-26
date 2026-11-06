#include <mission-setup/ReferenceScenario.h>

#include <audio-processing/OutputBuffer.h>
#include <LoopbackCapture.h>
#include <app-state/AppState.h>
#include <audio-processing/SignalGenerator.h>
#include <mission-setup/MissionSetup.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
constexpr std::array<std::string_view, 8> names {{
    "clean-passthrough",
    "asymmetric-routing",
    "matched-inputs",
    "steady-signals",
    "exercise-right-channel",
    "exercise-mono",
    "exercise-preset",
    "exercise-recording"
}};

std::vector<float> makeTone(double sampleRate, double divisor, float amplitude,
                            std::size_t sampleCount)
{
    return audio_core::generateSine(static_cast<float>(sampleRate / divisor), amplitude,
                                     sampleRate, sampleCount);
}

std::size_t scenarioSampleCount(double sampleRate, std::size_t blockSize)
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0)
        throw std::invalid_argument("Sample rate must be finite and positive");
    if (blockSize == 0)
        throw std::invalid_argument("Block size must be positive");
    if (blockSize > (std::numeric_limits<std::size_t>::max() - 1) / 3)
        throw std::invalid_argument("Block size is too large");

    const auto durationSamples = std::ceil(sampleRate / 8.0);
    if (durationSamples > static_cast<double>(std::numeric_limits<std::size_t>::max()))
        throw std::invalid_argument("Sample rate is too large");

    return std::max(static_cast<std::size_t>(durationSamples), blockSize * 3 + 1);
}

void configureScenario(audio_core::ReferenceScenarioResult& result,
                       audio_core::AppState& state)
{
    const auto sampleCount = result.outputs[0].size();
    switch (result.scenario)
    {
        case audio_core::ReferenceScenario::cleanPassthrough:
            result.inputs[0] = makeTone(result.sampleRate, 64.0, 0.25f, sampleCount);
            result.inputs[1] = makeTone(result.sampleRate, 37.0, 0.125f, sampleCount);
            break;

        case audio_core::ReferenceScenario::asymmetricRouting:
            result.inputs[0] = makeTone(result.sampleRate, 64.0, 0.2f, sampleCount);
            result.inputs[1] = makeTone(result.sampleRate, 31.0, 0.35f, sampleCount);
            state.routing = {{ {{ 0.0f, 1.0f }}, {{ 0.0f, 0.0f }} }};
            break;

        case audio_core::ReferenceScenario::matchedInputs:
            result.inputs[0] = makeTone(result.sampleRate, 64.0, 0.4f, sampleCount);
            result.inputs[1] = result.inputs[0];
            state.inputs[1].phaseInverted = true;
            break;

        case audio_core::ReferenceScenario::steadySignals:
            result.inputs[0].assign(sampleCount, 0.25f);
            result.inputs[1] = audio_core::generateSine(
                1'000.0f, 0.2f, result.sampleRate, sampleCount);
            break;

        case audio_core::ReferenceScenario::exerciseRightChannel:
        {
            const auto setup = audio_core::setupForMission("02-right-channel");
            state = setup.product;
            for (std::size_t input = 0; input < audio_core::inputCount; ++input)
                result.inputs[input] = audio_core::generateSine(setup.frequencies[input],
                    setup.amplitudes[input], result.sampleRate, sampleCount);
            break;
        }

        case audio_core::ReferenceScenario::exerciseMono:
            result.inputs[0] = audio_core::generateSine(120.0f, 0.4f,
                result.sampleRate, sampleCount);
            result.inputs[1] = result.inputs[0];
            break;

        case audio_core::ReferenceScenario::exercisePreset:
            state = audio_core::AppState::deserialize(
                R"({"schemaVersion":1,"outputChannelMappings":[2,1]})");
            result.inputs[0].assign(sampleCount, 0.0f);
            result.inputs[1].assign(sampleCount, 0.0f);
            result.inputs[0][0] = 0.25f;
            result.inputs[1][0] = 0.5f;
            break;

        case audio_core::ReferenceScenario::exerciseRecording:
            state = audio_core::setupForMission("final-customer-report").product;
            result.inputs[0] = audio_core::generateSine(240.0f, 0.3f,
                result.sampleRate, sampleCount);
            result.inputs[1].assign(sampleCount, 0.0f);
            break;

        default:
            throw std::invalid_argument("Invalid reference scenario");
    }
}

audio_core::ReferenceScenarioMetrics calculateMetrics(
    const audio_core::ReferenceScenarioResult& result)
{
    audio_core::ReferenceScenarioMetrics metrics;
    std::vector<float> mono(result.outputs[0].size());

    for (std::size_t input = 0; input < audio_core::inputCount; ++input)
    {
        metrics.input[input] = audio_core::analyze(result.inputs[input]);
    }
    for (std::size_t output = 0; output < audio_core::outputCount; ++output)
    {
        metrics.output[output] = audio_core::analyze(result.outputs[output]);
        for (std::size_t input = 0; input < audio_core::inputCount; ++input)
            metrics.outputInputCorrelation[output][input] =
                audio_core::correlation(result.outputs[output], result.inputs[input]);
    }

    metrics.outputCorrelation = audio_core::correlation(result.outputs[0], result.outputs[1]);
    for (std::size_t sample = 0; sample < mono.size(); ++sample)
        mono[sample] = (result.outputs[0][sample] + result.outputs[1][sample]) * 0.5f;
    metrics.monoOutput = audio_core::analyze(mono);
    return metrics;
}
}

namespace audio_core
{
std::span<const std::string_view> referenceScenarioNames() noexcept
{
    return names;
}

std::string_view referenceScenarioName(ReferenceScenario scenario) noexcept
{
    const auto index = static_cast<std::size_t>(scenario);
    return index < names.size() ? names[index] : "unknown";
}

ReferenceScenario referenceScenarioFromName(std::string_view name)
{
    const auto match = std::find(names.begin(), names.end(), name);
    if (match == names.end())
        throw std::invalid_argument("Unknown reference scenario: " + std::string(name));
    return static_cast<ReferenceScenario>(std::distance(names.begin(), match));
}

ReferenceScenarioResult runReferenceScenario(ReferenceScenario scenario,
                                             double sampleRate,
                                             std::size_t blockSize)
{
    ReferenceScenarioResult result;
    result.scenario = scenario;
    result.sampleRate = sampleRate;
    result.blockSize = blockSize;

    const auto sampleCount = scenarioSampleCount(sampleRate, blockSize);
    for (auto& output : result.outputs)
        output.resize(sampleCount);

    AppState state;
    configureScenario(result, state);

    AudioProcessor processor;
    const auto recording = scenario == ReferenceScenario::exerciseRecording;
    processor.prepare(recording ? 48'000.0 : sampleRate);
    processor.setState(state);
    if (recording)
        processor.prepare(sampleRate);
    LoopbackCapture capture(recording ? sampleCount : 0);

    std::array<std::vector<float>, outputCount> processed;
    for (auto& channel : processed)
        channel.resize(std::min(blockSize, sampleCount));

    for (std::size_t offset = 0; offset < sampleCount; offset += blockSize)
    {
        const auto samplesInBlock = std::min(blockSize, sampleCount - offset);
        const std::array<std::span<const float>, inputCount> inputs {{
            std::span<const float>(result.inputs[0]).subspan(offset, samplesInBlock),
            std::span<const float>(result.inputs[1]).subspan(offset, samplesInBlock)
        }};
        const std::array<std::span<float>, outputCount> outputs {{
            std::span<float>(processed[0]).first(samplesInBlock),
            std::span<float>(processed[1]).first(samplesInBlock)
        }};
        AnalysisTaps analysis;
        processor.process(inputs, outputs, analysis);
        for (std::size_t output = 0; output < outputCount; ++output)
            copyOutputBlock(outputs[output],
                std::span<float>(result.outputs[output]).subspan(offset, samplesInBlock));
        if (recording)
        {
            const std::array<std::span<const float>, outputCount> bus {{
                std::span<const float>(result.outputs[0]).subspan(offset, samplesInBlock),
                std::span<const float>(result.outputs[1]).subspan(offset, samplesInBlock)
            }};
            for (std::size_t output = 0; output < outputCount; ++output)
            {
                result.recordingBusPeak = std::max(result.recordingBusPeak,
                    analyze(bus[output]).peak);
            }
            for (std::size_t input = 0; input < inputCount && input < outputCount; ++input)
                result.displayedRecordingPeak = std::max(result.displayedRecordingPeak,
                    recordingMeterPeak(inputs[input], bus[input]));
            capture.append(bus);
        }
        ++result.processedBlocks;
    }

    if (recording)
        for (std::size_t output = 0; output < outputCount; ++output)
            std::copy(capture.channel(output).begin(), capture.channel(output).end(),
                      result.outputs[output].begin());
    result.metrics = calculateMetrics(result);
    return result;
}
}
