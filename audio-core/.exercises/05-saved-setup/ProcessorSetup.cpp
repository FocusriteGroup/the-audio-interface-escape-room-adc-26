#include <ProcessorSetup.h>

namespace audio_core
{
ProcessorState prepareForSampleRate(const AppState& state, double sampleRate)
{
    auto prepared = AudioProcessor::prepareState(state, 48'000.0);
    prepared.sampleRate = sampleRate;
    return prepared;
}
}
