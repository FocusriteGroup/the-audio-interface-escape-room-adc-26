#pragma once

#include "../components/LevelMeterComponent.h"

#include <JuceHeader.h>

#include <app-state/AppState.h>

#include <functional>

// One output strip: gain, mute and meter.
class OutputPanel final : public juce::Component
{
public:
    static constexpr int width = 250;
    static constexpr int height = 172;

    explicit OutputPanel(std::size_t channelIndex);

    void resized() override;
    void setFromState(const audio_core::OutputState& state);
    void setMeter(float level, float peakHold, const juce::String& text);

    std::function<void(float)> onGainChanged;
    std::function<void(bool)> onMuteChanged;

private:
    juce::Label label;
    juce::Slider gainSlider;
    juce::ToggleButton muteButton { "MUTE" };
    juce::Label peakLabel;
    LevelMeterComponent meter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputPanel)
};
