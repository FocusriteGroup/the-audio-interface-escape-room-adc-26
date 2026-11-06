#pragma once

#include <AudioProcessor.h>

#include <string_view>

namespace audio_core
{
struct MissionSetup
{
    AppState product;
    std::array<float, inputCount> frequencies {{ 1'000.0f, 250.0f }};
    std::array<float, inputCount> amplitudes {{ 0.4f, 0.2f }};
};

MissionSetup setupForMission(std::string_view name);
}
