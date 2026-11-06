#pragma once

#include <JuceHeader.h>

#include <app-state/AppState.h>

#include <array>
#include <functional>

// Three-band equaliser controls for a single input.
class EqComponent final : public juce::Component
{
public:
    static constexpr int bandCount = 3;
    static constexpr int bandWidth = 78;
    static constexpr int width = bandCount * bandWidth;
    static constexpr int height = 108;

    EqComponent();

    void resized() override;
    void setFromState(const audio_core::EqualizerState& state);

    std::function<void(std::size_t band, float gainDecibels)> onGainChanged;

private:
    std::array<juce::Label, bandCount> labels;
    std::array<juce::Slider, bandCount> sliders;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqComponent)
};
