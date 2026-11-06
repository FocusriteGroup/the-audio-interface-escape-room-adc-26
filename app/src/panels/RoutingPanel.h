#pragma once

#include <JuceHeader.h>

#include <app-state/AppState.h>

#include <array>
#include <functional>

class RoutingPanel final : public juce::Component
{
public:
    static constexpr int width = 240;
    static constexpr int height = 338;

    RoutingPanel();

    void resized() override;
    void setFromState(const audio_core::AppState& state);

    std::function<void(std::size_t output, std::size_t input, bool enabled)> onRouteChanged;

private:
    juce::Label routingLabel;
    std::array<juce::Label, audio_core::inputCount> columnLabels;
    std::array<juce::Label, audio_core::outputCount> rowLabels;
    std::array<juce::ToggleButton, audio_core::inputCount * audio_core::outputCount> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoutingPanel)
};
