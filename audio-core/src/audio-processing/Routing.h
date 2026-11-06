#pragma once

#include <AudioProcessor.h>

namespace audio_core
{
float mixOutput(std::array<float, inputCount> processedInputs,
                const AppState& state, std::size_t outputIndex) noexcept;
}
