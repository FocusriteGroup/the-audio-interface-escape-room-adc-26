#include "OutputPanel.h"

#include "../LookAndFeel.h"

using namespace theme;

OutputPanel::OutputPanel(std::size_t channelIndex)
{
    label.setText(channelIndex == 0 ? "OUTPUT L" : "OUTPUT R", juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, textPrimary);
    label.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(label);

    configureRotarySlider(gainSlider, -24.0, 12.0, 0.5, " dB");
    gainSlider.setValue(0.0, juce::dontSendNotification);
    gainSlider.onValueChange = [this]
    {
        if (onGainChanged)
            onGainChanged(static_cast<float>(gainSlider.getValue()));
    };
    addAndMakeVisible(gainSlider);

    muteButton.onClick = [this]
    {
        if (onMuteChanged)
            onMuteChanged(muteButton.getToggleState());
    };
    addAndMakeVisible(muteButton);

    peakLabel.setText("-- dBFS", juce::dontSendNotification);
    peakLabel.setColour(juce::Label::textColourId, signalGreen);
    peakLabel.setJustificationType(juce::Justification::centred);
    peakLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                        11.0f, juce::Font::bold));
    addAndMakeVisible(peakLabel);
    addAndMakeVisible(meter);
}

void OutputPanel::setFromState(const audio_core::OutputState& state)
{
    gainSlider.setValue(state.gainDecibels, juce::dontSendNotification);
    muteButton.setToggleState(state.muted, juce::dontSendNotification);
}

void OutputPanel::setMeter(float level, float peakHold, const juce::String& text)
{
    meter.setLevels(level, peakHold);
    peakLabel.setText(text, juce::dontSendNotification);
}

void OutputPanel::resized()
{
    auto node = getLocalBounds().reduced(12, 8);
    label.setBounds(node.removeFromTop(22));
    auto footer = node.removeFromBottom(28);
    muteButton.setBounds(footer.removeFromLeft(76));
    peakLabel.setBounds(footer);
    meter.setBounds(node.removeFromRight(18).reduced(2, 4));
    gainSlider.setBounds(node);
}
