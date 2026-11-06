#include "EqComponent.h"

#include "../LookAndFeel.h"

using namespace theme;

EqComponent::EqComponent()
{
    constexpr std::array<const char*, bandCount> names {{ "LOW", "MID", "HIGH" }};
    for (std::size_t band = 0; band < names.size(); ++band)
    {
        auto& label = labels[band];
        label.setText(names[band], juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, textSecondary);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        addAndMakeVisible(label);

        auto& slider = sliders[band];
        configureRotarySlider(slider, -12.0, 12.0, 0.5, " dB");
        slider.setValue(0.0, juce::dontSendNotification);
        slider.onValueChange = [this, band]
        {
            if (onGainChanged)
                onGainChanged(band, static_cast<float>(sliders[band].getValue()));
        };
        addAndMakeVisible(slider);
    }
}

void EqComponent::setFromState(const audio_core::EqualizerState& state)
{
    sliders[0].setValue(state.lowGainDecibels, juce::dontSendNotification);
    sliders[1].setValue(state.midGainDecibels, juce::dontSendNotification);
    sliders[2].setValue(state.highGainDecibels, juce::dontSendNotification);
}

void EqComponent::resized()
{
    for (std::size_t band = 0; band < bandCount; ++band)
    {
        auto bounds = juce::Rectangle<int>(static_cast<int>(band) * bandWidth, 0,
                                           bandWidth, height);
        labels[band].setBounds(bounds.removeFromTop(18));
        sliders[band].setBounds(bounds);
    }
}
