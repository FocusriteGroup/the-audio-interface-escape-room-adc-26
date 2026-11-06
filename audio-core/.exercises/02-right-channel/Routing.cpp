#include <audio-processing/Routing.h>

namespace audio_core
{
float mixOutput(std::array<float, inputCount> processedInputs,
                const AppState& state, std::size_t outputIndex) noexcept
{
    auto mixed = 0.0f;
    for (std::size_t inputIndex = 0; inputIndex < inputCount; ++inputIndex)
        mixed += processedInputs[inputIndex] * state.routing[inputIndex][outputIndex];
    return mixed;
}
}