#pragma once

#include <AudioProcessor.h>

namespace audio_core
{
ProcessorState prepareForSampleRate(const AppState& state, double sampleRate);
}
