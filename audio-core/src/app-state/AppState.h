#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace audio_core
{
constexpr std::size_t inputCount = 2;
constexpr std::size_t outputCount = 2;

struct EqualizerState
{
    float lowGainDecibels = 0.0f;
    float midGainDecibels = 0.0f;
    float highGainDecibels = 0.0f;
};

struct InputState
{
    float gainDecibels = 0.0f;
    bool muted = false;
    bool phaseInverted = false;
    EqualizerState equalizer;
};

struct OutputState
{
    float gainDecibels = 0.0f;
    bool muted = false;
};

struct AppState
{
    std::array<InputState, inputCount> inputs;
    std::array<OutputState, outputCount> outputs;
    std::array<std::array<float, inputCount>, outputCount> routing {{
        {{ 1.0f, 0.0f }},
        {{ 0.0f, 1.0f }}
    }};

    std::string serialize() const;
    static AppState deserialize(std::string_view jsonText);
    static std::string diff(const AppState& before, const AppState& after);
};
}
