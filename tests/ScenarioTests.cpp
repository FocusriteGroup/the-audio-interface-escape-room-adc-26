#include <mission-setup/ReferenceScenario.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>

#include "TestSupport.h"

namespace
{
float maximumDifference(std::span<const float> actual, std::span<const float> expected,
                        float expectedScale = 1.0f)
{
    require(actual.size() == expected.size(), "scenario channels should have equal lengths");
    float maximum = 0.0f;
    for (std::size_t sample = 0; sample < actual.size(); ++sample)
        maximum = std::max(maximum, std::abs(actual[sample] - expected[sample] * expectedScale));
    return maximum;
}

void verifyCleanPassthrough(const audio_core::ReferenceScenarioResult& result)
{
        for (std::size_t output = 0; output < audio_core::outputCount; ++output)
    {
        require(maximumDifference(result.outputs[output], result.inputs[output]) < 0.00002f,
                "clean passthrough should preserve every input sample");
        require(std::abs(result.metrics.output[output].peak
                 - result.metrics.input[output].peak) < 0.00002f,
                "clean passthrough should preserve peak level");
        require(std::abs(result.metrics.output[output].rootMeanSquare
                 - result.metrics.input[output].rootMeanSquare) < 0.00002f,
                "clean passthrough should preserve RMS level");
        require(result.metrics.outputInputCorrelation[output][output] > 0.99999f,
                "clean output should correlate with its matching input");
        require(result.metrics.output[output].clippedSamples == 0,
                "clean passthrough should not clip");
    }
}

void verifyAsymmetricRouting(const audio_core::ReferenceScenarioResult& result)
{
    require(maximumDifference(result.outputs[0], result.inputs[1]) < 0.00002f,
            "asymmetric route should send input 2 only to output L");
    require(result.metrics.outputInputCorrelation[0][1] > 0.99999f,
            "asymmetric output should correlate with input 2");
    require(result.metrics.output[1].peak < 0.000001f,
            "unrouted output should remain silent");
    require(result.metrics.output[0].clippedSamples == 0,
            "asymmetric routing should not clip");
}

void verifyMatchedInputs(const audio_core::ReferenceScenarioResult& result)
{
    require(maximumDifference(result.outputs[0], result.inputs[0]) < 0.00002f,
            "matched-inputs scenario should preserve output L");
    require(maximumDifference(result.outputs[1], result.inputs[1], -1.0f) < 0.00002f,
            "matched-inputs scenario should invert output R with PHASE 2 on");
    require(result.metrics.outputCorrelation < -0.99999f,
            "matched-inputs outputs should have a correlation of -1");
    require(result.metrics.monoOutput.peak < 0.000001f,
            "matched-inputs outputs should cancel in mono");
    require(result.metrics.monoOutput.rootMeanSquare < 0.000001f,
            "mono cancellation should have negligible RMS energy");
}

void verifyScenario(audio_core::ReferenceScenario scenario,
                    double sampleRate,
                    std::size_t blockSize)
{
    const auto result = audio_core::runReferenceScenario(scenario, sampleRate, blockSize);
    require(result.sampleRate == sampleRate, "scenario should report its sample rate");
    require(result.blockSize == blockSize, "scenario should report its block size");
    require(result.processedBlocks >= 4, "scenario should process multiple blocks");

    switch (scenario)
    {
        case audio_core::ReferenceScenario::cleanPassthrough:
            verifyCleanPassthrough(result);
            break;
        case audio_core::ReferenceScenario::asymmetricRouting:
            verifyAsymmetricRouting(result);
            break;
        case audio_core::ReferenceScenario::matchedInputs:
            verifyMatchedInputs(result);
            break;
        case audio_core::ReferenceScenario::steadySignals:
        case audio_core::ReferenceScenario::exerciseMono:
            verifyCleanPassthrough(result);
            break;
        case audio_core::ReferenceScenario::exerciseRightChannel:
            require(maximumDifference(result.outputs[1], result.inputs[0]) < 0.00002f,
                "exercise route must deliver input 1 to output R");
            require(result.metrics.output[0].peak == 0.0f,
                "exercise route must leave output L silent");
            break;
        case audio_core::ReferenceScenario::exercisePreset:
            require(maximumDifference(result.outputs[0], result.inputs[1]) < 0.00002f
                && maximumDifference(result.outputs[1], result.inputs[0]) < 0.00002f,
                "legacy render must restore both declared routes");
            break;
        case audio_core::ReferenceScenario::exerciseRecording:
            require(result.recordingBusPeak < 1.0f,
                "rate-change recording must preserve the reference headroom");
            require(std::abs(result.displayedRecordingPeak - result.recordingBusPeak) < 0.00002f,
                "displayed capture level must match the recorded signal");
            require(result.metrics.output[0].clippedSamples == 0,
                "rate-change reference recording must not clip");
            break;
    }
}

void verifyBuffering()
{
    for (const auto blockSize : { 1U, 3U, 127U, 128U, 129U, 256U, 257U })
        verifyScenario(audio_core::ReferenceScenario::steadySignals, 48'000.0, blockSize);

    const auto whole = audio_core::runReferenceScenario(
        audio_core::ReferenceScenario::steadySignals, 48'000.0, 6'000);
    const auto partitioned = audio_core::runReferenceScenario(
        audio_core::ReferenceScenario::steadySignals, 48'000.0, 128);
    for (std::size_t output = 0; output < audio_core::outputCount; ++output)
        require(maximumDifference(partitioned.outputs[output],
                    std::span<const float>(whole.outputs[output]).first(
                        partitioned.outputs[output].size())) < 0.00002f,
                "rendered output must not depend on block partitioning");
}
}

// escape_room_focused_check compiles this file again under another name; see FocusedCheck.cpp.
#ifndef ESCAPE_ROOM_TEST_MAIN
#define ESCAPE_ROOM_TEST_MAIN main
#endif

int ESCAPE_ROOM_TEST_MAIN(int argumentCount, char** arguments)
{
    const auto option = argumentCount == 2 ? std::string_view(arguments[1]) : std::string_view();
    require(argumentCount == 1
                || (argumentCount == 2
                    && (option == "--exercise-04" || option == "--without-recording")),
            "Usage: escape_room_scenario_tests [--exercise-04 | --without-recording]");
    if (option == "--exercise-04")
    {
        verifyBuffering();
        std::cout << "All exercise 04 render checks passed\n";
        return EXIT_SUCCESS;
    }
    const auto names = audio_core::referenceScenarioNames();
    require(names.size() == 8, "all reference scenarios should be discoverable");

    for (const auto name : names)
    {
        const auto scenario = audio_core::referenceScenarioFromName(name);
        require(audio_core::referenceScenarioName(scenario) == name,
                "scenario names should round trip");
        // Exercises 1-5 stub out the recording code; see tests/CMakeLists.txt.
        if (option == "--without-recording"
            && scenario == audio_core::ReferenceScenario::exerciseRecording)
            continue;

        for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0 })
            for (const auto blockSize : { 32U, 128U, 512U, 2'048U })
                verifyScenario(scenario, sampleRate, blockSize);
    }

    std::cout << "All reference scenario tests passed\n";
    return EXIT_SUCCESS;
}
