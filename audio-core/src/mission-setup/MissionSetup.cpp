#include <mission-setup/MissionSetup.h>

namespace audio_core
{
MissionSetup setupForMission(std::string_view name)
{
    MissionSetup setup;
    if (name == "01-test-tone")
    {
        setup.product.inputs[1].muted = true;
    }
    else if (name == "02-right-channel")
    {
        // Both inputs play their default tones (1 kHz and 250 Hz), so you can tell
        // which input reaches which output by ear, meter and spectrum.
        setup.product.routing = {{ {{ 0.0f, 0.0f }}, {{ 1.0f, 0.0f }} }};
    }
    else if (name == "03-mono-playback")
    {
        setup.frequencies = {{ 120.0f, 120.0f }};
        setup.amplitudes = {{ 0.4f, 0.4f }};
    }
    else if (name == "05-saved-setup")
    {
        // Start with nothing routed so participants can see the effect of the legacy preset
        setup.product.routing = {{ {{ 0.0f, 0.0f }}, {{ 0.0f, 0.0f }} }};
    }
    else if (name == "final-customer-report")
    {
        setup.frequencies[0] = 240.0f;
        setup.amplitudes[0] = 0.3f;
        setup.product.inputs[0].equalizer.lowGainDecibels = 12.0f;
        setup.product.inputs[1].muted = true;
        // About -8 dB. The fault clips the output at full scale, and the slow clip count
        // can break it into dropouts; this keeps both near the other exercises' level.
        setup.listeningGain = 0.4f;
    }
    return setup;
}
}