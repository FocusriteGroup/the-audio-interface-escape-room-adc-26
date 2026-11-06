#include <ProcessorSetup.h>

namespace audio_core
{
ProcessorState prepareForSampleRate(const AppState& state, double sampleRate)
{
    return AudioProcessor::prepareState(state, sampleRate);
}
}
