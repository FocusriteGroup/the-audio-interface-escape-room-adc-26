#pragma once

#include "../components/LevelMeterComponent.h"

#include <JuceHeader.h>

#include <app-state/AppState.h>

#include <functional>

enum class SourceType
{
    bundledWav = 1,
    sine,
    impulse,
    noise,
    silence
};

// One virtual input source plus its input strip (gain, mute, phase, meter).
class InputPanel final : public juce::Component
{
public:
    static constexpr int width = 540;
    static constexpr int height = 172;
    static constexpr int stripX = 170;

    InputPanel(std::size_t channelIndex, float defaultFrequency, float defaultAmplitude);

    void resized() override;

    void resetSource();
    void setFromState(const audio_core::InputState& state);
    void setMeter(float level, float peakHold, const juce::String& text);
    double getFrequency() const { return frequencySlider.getValue(); }
    double getAmplitude() const { return amplitudeSlider.getValue(); }

    std::function<void(SourceType)> onSourceTypeChanged;
    std::function<void(double)> onFrequencyChanged;
    std::function<void(double)> onAmplitudeChanged;
    std::function<void(float)> onGainChanged;
    std::function<void(bool)> onMuteChanged;
    std::function<void(bool)> onPhaseChanged;

private:
    friend class WorkshopComponentTests;

    float defaultFrequency;
    float defaultAmplitude;

    juce::Label sourceLabel;
    juce::ComboBox sourceSelector;
    juce::Label frequencyLabel;
    juce::Label amplitudeLabel;
    juce::Slider frequencySlider;
    juce::Slider amplitudeSlider;

    juce::Label inputLabel;
    juce::Slider gainSlider;
    juce::ToggleButton muteButton;
    juce::ToggleButton phaseButton;
    LevelMeterComponent meter;
    juce::Label peakLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputPanel)
};
