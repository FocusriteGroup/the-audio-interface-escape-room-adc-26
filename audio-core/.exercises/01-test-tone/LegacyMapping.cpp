#include <app-state/LegacyMapping.h>

#include <AudioProcessor.h>

#include <stdexcept>

namespace audio_core
{
std::size_t legacyInputIndex(int oneBasedInput)
{
    if (oneBasedInput < 1 || oneBasedInput > static_cast<int>(inputCount))
        throw std::invalid_argument("Version 1 channel mapping is out of range");
    return static_cast<std::size_t>(oneBasedInput - 1);
}
}
