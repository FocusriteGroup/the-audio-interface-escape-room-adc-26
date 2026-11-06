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
    // Scales only what reaches the speakers, after recording, meters and analysis, to
    // protect ears in exercises whose fault drives the output to full scale.
    float listeningGain = 1.0f;
};

MissionSetup setupForMission(std::string_view name);
}
