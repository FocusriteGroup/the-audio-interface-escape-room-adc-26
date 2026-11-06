#include <Analysis.h>
#include <LoopbackCapture.h>
#include <AudioProcessor.h>
#include <app-state/AppState.h>
#include <audio-processing/SignalGenerator.h>

#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "TestSupport.h"

namespace
{
using Inputs = std::array<std::vector<float>, audio_core::inputCount>;
using Outputs = std::array<std::vector<float>, audio_core::outputCount>;

Outputs render(const Inputs& inputs, const audio_core::AppState& state)
{
    Outputs outputs { std::vector<float>(inputs[0].size()),
                      std::vector<float>(inputs[1].size()) };
    audio_core::AudioProcessor processor;
    processor.prepare(48'000.0);
    processor.setState(state);
    audio_core::AnalysisTaps analysis;
    processor.process({ inputs[0], inputs[1] }, { outputs[0], outputs[1] }, analysis);
    return outputs;
}

void testRouting()
{
    for (std::size_t inputIndex = 0; inputIndex < audio_core::inputCount; ++inputIndex)
    {
        for (std::size_t outputIndex = 0; outputIndex < audio_core::outputCount; ++outputIndex)
        {
            Inputs inputs { std::vector<float>(32), std::vector<float>(32) };
            inputs[inputIndex][0] = 0.25f;
            audio_core::AppState state;
            state.routing = {};
            state.routing[outputIndex][inputIndex] = 1.0f;
            const auto outputs = render(inputs, state);
            const char* outputNames[] { "Output L", "Output R" };
            const auto route = "route Input " + std::to_string(inputIndex + 1) + " -> "
                + outputNames[outputIndex] + ": ";
            for (std::size_t channel = 0; channel < audio_core::outputCount; ++channel)
            {
                const auto wanted = channel == outputIndex;
                const auto message = route + outputNames[channel]
                    + (wanted ? " must carry the impulse, and only at sample 0"
                              : " must stay silent");
                for (std::size_t sample = 0; sample < outputs[channel].size(); ++sample)
                    requireNear(outputs[channel][sample], wanted && sample == 0 ? 0.25f : 0.0f,
                                0.00002f, message.c_str());
            }
        }
    }
}

void requirePresetRejected(std::string_view json,
                           const test_support::Location& where = test_support::Location::current())
{
    try
    {
        audio_core::AppState::deserialize(json);
    }
    catch (const std::invalid_argument&)
    {
        return;
    }
    test_support::fail("invalid or unsupported preset must be rejected", where);
}

void testMigration()
{
    audio_core::AppState current;
    current.inputs[0].gainDecibels = 3.0f;
    current.inputs[1].phaseInverted = true;
    current.outputs[1].muted = true;
    current.routing = {{ {{ 0.0f, 1.0f }}, {{ 1.0f, 0.0f }} }};
    const auto json = current.serialize();
    require(audio_core::AppState::deserialize(json).serialize() == json,
            "current-state roundtrip must preserve all controls");

    const auto valid = nlohmann::json::parse(json);
        const audio_core::AppState original;
        const auto patch = nlohmann::json::parse(audio_core::AppState::diff(original, current));
        require(nlohmann::json::parse(original.serialize()).patch(patch) == valid,
            "state diff must independently reconstruct the changed controls");
        require(audio_core::AppState::diff(current, current) == "[]",
            "identical states must produce an empty diff");
    for (const auto* field : { "/inputs/0/gainDb", "/outputs/0/gainDb",
                              "/inputs/0/equalizer/lowGainDb", "/routing/0/1" })
    {
        for (const auto& invalid : { nlohmann::json(900.0), nlohmann::json(-900.0),
                                     nlohmann::json(nullptr), nlohmann::json("1") })
        {
            auto malformed = valid;
            malformed[nlohmann::json::json_pointer(field)] = invalid;
            requirePresetRejected(malformed.dump());
        }
    }
    for (const auto& invalid : { nlohmann::json(1.9), nlohmann::json(4294967298ULL),
                                 nlohmann::json("2"), nlohmann::json(true) })
    {
        auto malformed = valid;
        malformed["schemaVersion"] = invalid;
        requirePresetRejected(malformed.dump());
    }
    requirePresetRejected(R"({"schemaVersion":1,"outputChannelMappings":[1.9,2]})");
    requirePresetRejected(R"({"schemaVersion":1,"outputChannelMappings":[4294967297,2]})");
    requirePresetRejected(R"({"schemaVersion":1,"outputChannelMappings":{"0":1,"1":2}})");
    for (const auto* field : { "/inputs", "/outputs", "/routing", "/routing/0" })
    {
        auto malformed = valid;
        malformed[nlohmann::json::json_pointer(field)].push_back(0);
        requirePresetRejected(malformed.dump());
    }

    std::ifstream file(std::string(ESCAPE_ROOM_FIXTURES_DIR) + "/presets/legacy-v1.json");
    require(file.good(), "legacy golden preset must be readable");
    std::ostringstream contents;
    contents << file.rdbuf();
    const auto restored = audio_core::AppState::deserialize(contents.str());
    require(restored.routing == current.routing, "legacy preset must restore its declared routes");
    Inputs inputs { std::vector<float>(32), std::vector<float>(32) };
    inputs[0][0] = 0.25f;
    inputs[1][0] = 0.5f;
    const auto outputs = render(inputs, restored);
    requireNear(outputs[0][0], 0.5f, 0.00002f, "legacy input 2 must reach output L");
    requireNear(outputs[1][0], 0.25f, 0.00002f, "legacy input 1 must reach output R");
    requirePresetRejected(R"({"schemaVersion":99})");
    requirePresetRejected(R"({"schemaVersion":1,"outputChannelMappings":[0,1]})");
    requirePresetRejected(R"({"schemaVersion":1,"outputChannelMappings":[3,1]})");
    requirePresetRejected(R"({"schemaVersion":1,"outputChannelMappings":[1]})");
}

void testRateLifecycle()
{
    audio_core::AppState state;
    state.inputs[0].equalizer.lowGainDecibels = 6.0206f;
    audio_core::AudioProcessor processor;
    processor.prepare(48'000.0);
    processor.setState(state);
    const auto stale = audio_core::AudioProcessor::prepareState(state, 48'000.0);
    // 96 kHz comes first because a wrong response differs most there. At 44.1 kHz
    // the same error can be small enough to look like a tolerance issue.
    for (const auto rate : { 96'000.0, 48'000.0, 44'100.0, 96'000.0, 48'000.0 })
    {
        processor.prepare(rate);
        if (rate != 48'000.0)
            processor.setProcessorState(stale);
        const auto count = static_cast<std::size_t>(rate / 4.0);
        Inputs inputs { audio_core::generateSine(120.0f, 0.1f, rate, count),
                std::vector<float>(count) };
        Outputs outputs { std::vector<float>(count), std::vector<float>(count) };
        audio_core::AnalysisTaps analysis;
        processor.process({ inputs[0], inputs[1] }, { outputs[0], outputs[1] }, analysis);
        const auto settled = std::span<const float>(outputs[0]).last(count / 2);
        const auto peak = audio_core::analyze(settled).peak;
        std::cout << "rate=" << rate << " settled_peak=" << peak << '\n';
        const auto message = "after restarting at " + std::to_string(static_cast<int>(rate))
            + " Hz, the output level must match the saved settings";
        requireNear(peak, 0.2f, 0.002f, message.c_str());
    }
}

void testRecording()
{
    const std::array<float, 4> left { 0.25f, 1.2f, -1.3f, 0.5f };
    const std::array<float, 4> right { -0.25f, -0.5f, 0.75f, 0.2f };
    audio_core::LoopbackCapture capture(3);
    capture.append({ left, right });
    require(capture.frameCount() == 3, "capture must stop at its bounded capacity");
    requireNear(capture.channel(0)[1], 1.0f, 0.0f, "recording must preserve positive PCM clipping");
    requireNear(capture.channel(0)[2], -1.0f, 0.0f, "recording must preserve negative PCM clipping");
    require(capture.channel(2).empty(), "invalid channel view must be empty");
    Outputs replay { std::vector<float>(7), std::vector<float>(7) };
    capture.replay({ replay[0], replay[1] });
    for (std::size_t sample = 0; sample < replay[0].size(); ++sample)
        for (std::size_t channel = 0; channel < audio_core::outputCount; ++channel)
            requireNear(replay[channel][sample], capture.channel(channel)[sample % 3], 0.0f,
                        "replay must preserve the captured bus and loop safely");
    capture.reset();
    capture.replay({ replay[0], replay[1] });
    require(audio_core::analyze(replay[0]).peak == 0.0f,
            "empty capture must replay silence");
    audio_core::LoopbackCapture empty(0);
    empty.append({ left, right });
    require(empty.frameCount() == 0, "zero-capacity capture must remain empty");
    const std::array<float, 2> source { 0.25f, -0.25f };
    const std::array<float, 2> recording { 1.2f, -1.3f };
    requireNear(audio_core::recordingMeterPeak(source, recording), 1.3f, 0.0f,
                "capture level must match the recorded signal");
}

void testCaptureClipCount()
{
    // The running count must match the clips that went into the capture, block by block.
    audio_core::LoopbackCapture capture(4'000);
    std::size_t clips = 0;
    std::size_t expected = 0;
    for (int block = 0; block < 40; ++block)
    {
        std::array<float, 128> left {};
        std::array<float, 128> right {};
        for (std::size_t sample = 0; sample < left.size(); ++sample)
        {
            left[sample] = sample % 16 == 0 ? 1.5f : 0.25f;
            right[sample] = sample % 32 == 0 ? -1.2f : -0.25f;
        }
        const auto before = capture.frameCount();
        capture.append({ left, right });
        const auto added = capture.frameCount() - before;
        expected += added / 16 + added / 32;
        clips = audio_core::updateCaptureClipCount(clips, capture, added);
        require(clips == expected, "the capture clip count must match the clipped samples recorded so far: expected "
            + std::to_string(expected) + ", got " + std::to_string(clips));
    }
    require(capture.frameCount() == 4'000, "the test capture should now be full");

    // Recording continues once the capture is full, so updates keep coming. Each one runs
    // on the audio thread and must cost the same however long the take is: at 96 kHz with
    // 128-sample blocks, the whole callback has 1.33 ms.
    audio_core::LoopbackCapture take(192'000);
    std::array<float, 128> quiet {};
    quiet.fill(0.5f);
    while (take.frameCount() < 192'000)
        take.append({ quiet, quiet });
    constexpr auto updates = 500;
    const auto start = std::chrono::steady_clock::now();
    std::size_t takeClips = 0;
    for (int update = 0; update < updates; ++update)
        takeClips = audio_core::updateCaptureClipCount(takeClips, take, 0);
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    require(takeClips == 0, "a capture with no clipped samples must count none");
    require(elapsed < 25.0, "updating the clip count must only look at the new block: with a full "
        "192,000-frame capture each update took " + std::to_string(elapsed / updates)
        + " ms of the 1.33 ms callback budget at 96 kHz");
}

void testMono()
{
    const auto tone = audio_core::generateSine(120.0f, 0.4f, 48'000.0, 1'024);
    const Inputs inputs { tone, tone };
    // Both buttons off first: that is the state the bug report describes.
    for (const auto [phase1, phase2] : { std::array { false, false }, std::array { false, true },
                                         std::array { true, false }, std::array { true, true } })
    {
        audio_core::AppState state;
        state.inputs[0].phaseInverted = phase1;
        state.inputs[1].phaseInverted = phase2;
        const auto outputs = render(inputs, state);
        const std::string buttons = std::string(" with PHASE 1 ") + (phase1 ? "on" : "off")
            + " and PHASE 2 " + (phase2 ? "on" : "off");
        const auto cancels = phase1 != phase2;

        std::vector<float> mono(tone.size());
        for (std::size_t sample = 0; sample < tone.size(); ++sample)
            mono[sample] = 0.5f * (outputs[0][sample] + outputs[1][sample]);
        requireNear(audio_core::analyze(mono).peak, cancels ? 0.0f : 0.4f, 0.00002f,
                    ((cancels ? "mono sum must cancel" : "mono sum must keep the input level")
                        + buttons).c_str());
        for (std::size_t sample = 0; sample < tone.size(); ++sample)
        {
            requireNear(outputs[0][sample], phase1 ? -tone[sample] : tone[sample], 0.00002f,
                        ((phase1 ? "output L must be input 1 inverted" : "output L must match input 1")
                            + buttons).c_str());
            requireNear(outputs[1][sample], phase2 ? -tone[sample] : tone[sample], 0.00002f,
                        ((phase2 ? "output R must be input 2 inverted" : "output R must match input 2")
                            + buttons).c_str());
        }
        requireNear(audio_core::correlation(outputs[0], outputs[1]),
                    cancels ? -1.0f : 1.0f, 0.00001f,
                    ((cancels ? "output correlation must be -1" : "output correlation must be +1")
                        + buttons).c_str());
    }
}
}

// escape_room_focused_check compiles this file again under another name; see FocusedCheck.cpp.
#ifndef ESCAPE_ROOM_TEST_MAIN
#define ESCAPE_ROOM_TEST_MAIN main
#endif

int ESCAPE_ROOM_TEST_MAIN(int argumentCount, char** arguments)
{
        require(argumentCount == 2,
            "Usage: escape_room_mission_tests 02|03|05|final|final-recording|final-capture");
    const std::string_view name(arguments[1]);
    if (name == "02")
        testRouting();
    else if (name == "03")
        testMono();
    else if (name == "05")
        testMigration();
    else if (name == "final")
        testRateLifecycle();
    else if (name == "final-recording")
        testRecording();
    else if (name == "final-capture")
        testCaptureClipCount();
    else
        require(false, "Unknown mission test");
    std::cout << "check " << name << " passed\n";
    return EXIT_SUCCESS;
}
