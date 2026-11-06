#include <Analysis.h>
#include <logging/DiagnosticLog.h>
#include <audio-processing/Gain.h>
#include <audio-processing/OutputBuffer.h>
#include <AudioProcessor.h>
#include <app-state/AppState.h>
#include <state-exchange/StateExchange.h>
#include <audio-processing/SignalGenerator.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "TestSupport.h"

namespace
{
void testOutputBuffer()
{
    for (const auto blockSize : { 0u, 1u, 2u, 3u, 127u, 128u, 129u, 256u, 257u })
    {
        std::vector<float> source(blockSize);
        for (std::size_t sampleIndex = 0; sampleIndex < source.size(); ++sampleIndex)
            source[sampleIndex] = sampleIndex % 2 == 0 ? 0.25f : -0.5f;

        std::vector<float> destination(blockSize + 2, 7.0f);
        audio_core::copyOutputBlock(source,
            std::span<float>(destination).subspan(1, blockSize));
        require(destination.front() == 7.0f && destination.back() == 7.0f,
                "output must not change samples outside the audio it was given");
        for (std::size_t sampleIndex = 0; sampleIndex < source.size(); ++sampleIndex)
            requireNear(destination[sampleIndex + 1], source[sampleIndex], 0.0f,
                        "output must match the input sample for sample");
    }

    std::vector<float> source(997);
    for (std::size_t sampleIndex = 0; sampleIndex < source.size(); ++sampleIndex)
        source[sampleIndex] = static_cast<float>(sampleIndex % 31 + 1) / 32.0f;
    std::vector<float> whole(source.size(), 7.0f);
    audio_core::copyOutputBlock(source, whole);
    require(whole == source, "whole-buffer output must match the independent reference");

    for (const auto blockSize : { 1u, 3u, 127u, 128u, 129u, 256u, 257u })
    {
        std::vector<float> partitioned(source.size(), 7.0f);
        for (std::size_t offset = 0; offset < source.size(); offset += blockSize)
        {
            const auto count = std::min<std::size_t>(blockSize, source.size() - offset);
            audio_core::copyOutputBlock(std::span<const float>(source).subspan(offset, count),
                std::span<float>(partitioned).subspan(offset, count));
        }
        require(partitioned == whole, "output must not depend on block partitioning");
    }

    const std::array<float, 2> shortSource { 0.25f, -0.5f };
    std::array<float, 5> destination { 7.0f, 7.0f, 7.0f, 7.0f, 7.0f };
    audio_core::copyOutputBlock(shortSource, destination);
    require(destination == std::array<float, 5> { 0.25f, -0.5f, 0.0f, 0.0f, 0.0f },
            "short input must leave the rest of the output silent");
    audio_core::copyOutputBlock({}, destination);
    require(std::all_of(destination.begin(), destination.end(),
                       [](float sample) { return sample == 0.0f; }),
            "empty input must silence the destination");
    std::array<float, 1> shortDestination { 7.0f };
    audio_core::copyOutputBlock(shortSource, shortDestination);
    require(shortDestination[0] == shortSource[0],
            "long input must be bounded by the output capacity");
}

void testAudioProcessor(double sampleRate, std::size_t blockSize)
{
    std::array<std::vector<float>, audio_core::inputCount> inputs {
        std::vector<float>(blockSize), std::vector<float>(blockSize)
    };
    std::array<std::vector<float>, audio_core::outputCount> outputs {
        std::vector<float>(blockSize), std::vector<float>(blockSize)
    };
    inputs[0][0] = 0.25f;
    inputs[1][0] = 0.5f;

    audio_core::AppState state;
    state.inputs[0].gainDecibels = 6.0206f;
    state.inputs[1].phaseInverted = true;
    state.routing = {{ {{ 1.0f, 1.0f }}, {{ 1.0f, 0.0f }} }};
    state.outputs[1].gainDecibels = -6.0206f;

    audio_core::AudioProcessor processor;
    processor.prepare(sampleRate);
    processor.setState(state);

    const std::array<std::span<const float>, audio_core::inputCount> inputSpans {{
        inputs[0], inputs[1]
    }};
    const std::array<std::span<float>, audio_core::outputCount> outputSpans {{
        outputs[0], outputs[1]
    }};
    audio_core::AnalysisTaps analysis;
    processor.process(inputSpans, outputSpans, analysis);

    requireNear(outputs[0][0], 0.0f, 0.0002f,
                "output L must be silent when Input 1 at +6 dB meets Input 2 with PHASE 2 on");
    requireNear(outputs[1][0], 0.25f, 0.0002f,
                "output gain should be applied after routing");
    requireNear(analysis.inputPeak[1], 0.5f, 0.0001f,
                "input analysis tap is incorrect");
    requireNear(analysis.postGainPeak[0], 0.5f, 0.0001f,
                "post-gain analysis tap is incorrect");
    requireNear(analysis.postEqualizerPeak[0], 0.5f, 0.0002f,
                "flat equalizer should preserve the signal");
    requireNear(analysis.postMixPeak[1], 0.5f, 0.0002f,
                "post-mix analysis tap is incorrect");
    requireNear(analysis.outputPeak[1], 0.25f, 0.0002f,
                "output analysis tap is incorrect");

    state.inputs[0].muted = true;
    state.outputs[0].muted = true;
    processor.setState(state);
    processor.reset();
    processor.process(inputSpans, outputSpans, analysis);
    requireNear(analysis.postGainPeak[0], 0.0f, 0.0001f,
                "input mute should silence the post-gain signal");
    requireNear(analysis.outputPeak[0], 0.0f, 0.0001f,
                "output mute should silence the output");
}

    void testStateExchange()
    {
        audio_core::AppState first;
        first.inputs[0].gainDecibels = 3.0f;
        audio_core::AppState latest;
        latest.inputs[0].gainDecibels = 9.0f;

        audio_core::StateExchange exchange;
        require(exchange.publish(audio_core::AudioProcessor::prepareState(first, 48'000.0)),
            "first prepared state should publish");
        require(exchange.publish(audio_core::AudioProcessor::prepareState(latest, 48'000.0)),
            "latest prepared state should publish");

        audio_core::ProcessorState consumed;
        require(exchange.consumeLatest(consumed), "prepared state should be available");
        requireNear(consumed.appState.inputs[0].gainDecibels, 9.0f, 0.0001f,
            "consumer should receive the latest prepared state");
        require(!exchange.consumeLatest(consumed), "consumed state queue should be empty");
    }

    void testEqualizerAtBandCentre(double sampleRate)
    {
        const auto sampleCount = static_cast<std::size_t>(sampleRate);
        auto tone = audio_core::generateSine(120.0f, 0.1f, sampleRate, sampleCount);
        std::vector<float> silence(sampleCount);
        std::array<std::vector<float>, audio_core::outputCount> outputs {
            std::vector<float>(sampleCount), std::vector<float>(sampleCount)
        };

        audio_core::AppState state;
        state.inputs[0].equalizer.lowGainDecibels = 6.0206f;
        audio_core::AudioProcessor processor;
        processor.prepare(sampleRate);
        processor.setState(state);

        const std::array<std::span<const float>, audio_core::inputCount> inputs {{
            tone, silence
        }};
        const std::array<std::span<float>, audio_core::outputCount> outputSpans {{
            outputs[0], outputs[1]
        }};
        audio_core::AnalysisTaps analysis;
        processor.process(inputs, outputSpans, analysis);

        const auto settledSignal = std::span<const float>(outputs[0]).last(sampleCount / 2);
        const auto metrics = audio_core::analyze(settledSignal);
        requireNear(metrics.peak, 0.2f, 0.002f,
                    "low EQ band should apply its requested centre-frequency gain");
    }

void testAppStatePersistence()
{
    audio_core::AppState state;
    state.inputs[0].gainDecibels = 7.5f;
    state.inputs[1].phaseInverted = true;
    state.inputs[1].equalizer.highGainDecibels = -3.0f;
    state.outputs[0].muted = true;
    state.routing = {{ {{ 0.0f, 1.0f }}, {{ 1.0f, 1.0f }} }};

    const auto json = state.serialize();
    const auto restored = audio_core::AppState::deserialize(json);
    requireNear(restored.inputs[0].gainDecibels, 7.5f, 0.0001f,
                "input gain should survive state round trip");
    require(restored.inputs[1].phaseInverted,
            "phase should survive state round trip");
    requireNear(restored.inputs[1].equalizer.highGainDecibels, -3.0f, 0.0001f,
                "EQ should survive state round trip");
    require(restored.outputs[0].muted, "output mute should survive state round trip");
    requireNear(restored.routing[0][1], 1.0f, 0.0001f,
                "routing should survive state round trip");

    std::ifstream fixture(std::string(ESCAPE_ROOM_FIXTURES_DIR) + "/presets/legacy-v1.json");
    require(static_cast<bool>(fixture), "legacy preset fixture should open");
    std::ostringstream contents;
    contents << fixture.rdbuf();
    const auto migrated = audio_core::AppState::deserialize(contents.str());
    requireNear(migrated.routing[0][1], 1.0f, 0.0001f,
                "legacy-v1.json must route Input 2 to Output L");
    requireNear(migrated.routing[1][0], 1.0f, 0.0001f,
                "legacy-v1.json must route Input 1 to Output R");
}

void testDiagnosticLog()
{
    audio_core::clearDiagnosticLog();
    audio_core::logDiagnostic("state published");
    audio_core::logDiagnosticValue("input peak", 0.5);

    audio_core::DiagnosticLogEntry entry;
    require(audio_core::tryPopDiagnosticLog(entry), "diagnostic text entry should be available");
    require(std::string(entry.text.data()) == "state published",
        "diagnostic text entry should preserve its message");
    require(!entry.hasValue, "diagnostic text entry should not contain a value");
    require(audio_core::tryPopDiagnosticLog(entry), "diagnostic value entry should be available");
    require(std::string(entry.text.data()) == "input peak",
        "diagnostic value entry should preserve its label");
    require(entry.hasValue, "diagnostic value entry should contain a value");
    requireNear(static_cast<float>(entry.value), 0.5f, 0.0001f,
        "diagnostic value entry should preserve its value");
    require(!audio_core::tryPopDiagnosticLog(entry), "diagnostic queue should be empty");

    for (int index = 0; index < 300; ++index)
    audio_core::logDiagnostic("overflow test");
    require(audio_core::takeDroppedDiagnosticLogCount() > 0,
        "diagnostic queue should report dropped entries when full");
    audio_core::clearDiagnosticLog();
}
}

// escape_room_focused_check compiles this file again under another name; see FocusedCheck.cpp.
#ifndef ESCAPE_ROOM_TEST_MAIN
#define ESCAPE_ROOM_TEST_MAIN main
#endif

int ESCAPE_ROOM_TEST_MAIN(int argumentCount, char** arguments)
{
    require(argumentCount == 1
                || (argumentCount == 2 && std::string_view(arguments[1]) == "--exercise-04"),
            "Usage: escape_room_core_tests [--exercise-04]");
    testOutputBuffer();
    if (argumentCount == 2)
    {
        std::cout << "All output-buffer tests passed\n";
        return EXIT_SUCCESS;
    }
    requireNear(audio_core::decibelsToLinear(0.0f), 1.0f, 0.0001f,
                "0 dB should be unity gain");
    requireNear(audio_core::decibelsToLinear(6.0206f), 2.0f, 0.0001f,
                "+6.0206 dB should double amplitude");
    requireNear(audio_core::decibelsToLinear(-6.0206f), 0.5f, 0.0001f,
                "-6.0206 dB should halve amplitude");

    auto samples = audio_core::generateSine(1'000.0f, 0.25f, 48'000.0, 4'800);
    audio_core::GainStage gain;
    gain.setGainDecibels(6.0206f);
    gain.process(samples);

    const auto metrics = audio_core::analyze(samples);
    requireNear(metrics.peak, 0.5f, 0.0001f, "processed signal peak is incorrect");
    requireNear(metrics.rootMeanSquare, 0.353553f, 0.0002f,
                "processed signal RMS is incorrect");
    require(metrics.clippedSamples == 0, "processed signal should not clip");
    requireNear(audio_core::correlation(samples, samples), 1.0f, 0.0001f,
                "identical signals should have positive unity correlation");
    auto invertedSamples = samples;
    for (auto& sample : invertedSamples)
        sample = -sample;
    requireNear(audio_core::correlation(samples, invertedSamples), -1.0f, 0.0001f,
                "inverted signals should have negative unity correlation");

    for (const auto sampleRate : { 44'100.0, 48'000.0, 96'000.0 })
    {
        for (const auto blockSize : { 32U, 128U, 512U, 2'048U })
            testAudioProcessor(sampleRate, blockSize);
        testEqualizerAtBandCentre(sampleRate);
    }
    testStateExchange();
    testAppStatePersistence();
    testDiagnosticLog();

    std::cout << "All core tests passed\n";
    return EXIT_SUCCESS;
}
