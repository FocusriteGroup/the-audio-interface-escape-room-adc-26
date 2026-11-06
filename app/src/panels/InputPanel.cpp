#include "InputPanel.h"

#include "../LookAndFeel.h"

using namespace theme;

InputPanel::InputPanel(std::size_t channelIndex, float frequency, float amplitude)
    : defaultFrequency(frequency), defaultAmplitude(amplitude)
{
    const auto number = juce::String(channelIndex + 1);

    sourceLabel.setText("VIRTUAL INPUT " + number, juce::dontSendNotification);
    sourceLabel.setColour(juce::Label::textColourId, textSecondary);
    sourceLabel.setJustificationType(juce::Justification::centred);
    sourceLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(sourceLabel);

    sourceSelector.addItem("Drum loop", static_cast<int>(SourceType::bundledWav));
    sourceSelector.addItem("Sine", static_cast<int>(SourceType::sine));
    sourceSelector.addItem("Impulse", static_cast<int>(SourceType::impulse));
    sourceSelector.addItem("Noise", static_cast<int>(SourceType::noise));
    sourceSelector.addItem("Silence", static_cast<int>(SourceType::silence));
    sourceSelector.setSelectedId(static_cast<int>(SourceType::sine), juce::dontSendNotification);
    sourceSelector.setTooltip("Select virtual input " + number + " source");
    sourceSelector.onChange = [this]
    {
        const auto type = static_cast<SourceType>(sourceSelector.getSelectedId());
        frequencySlider.setEnabled(type == SourceType::sine);
        amplitudeSlider.setEnabled(type != SourceType::silence);
        if (onSourceTypeChanged)
            onSourceTypeChanged(type);
    };
    addAndMakeVisible(sourceSelector);

    for (auto* label : { &frequencyLabel, &amplitudeLabel })
    {
        label->setColour(juce::Label::textColourId, textSecondary);
        label->setFont(juce::FontOptions(10.0f, juce::Font::bold));
        addAndMakeVisible(*label);
    }
    frequencyLabel.setText("FREQUENCY", juce::dontSendNotification);
    amplitudeLabel.setText("PEAK (LINEAR)", juce::dontSendNotification);

    frequencySlider.setRange(20.0, 20'000.0, 1.0);
    frequencySlider.setSliderStyle(juce::Slider::IncDecButtons);
    frequencySlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 70, 24);
    frequencySlider.setTextValueSuffix(" Hz");
    frequencySlider.setTooltip("Sine frequency; other sources keep their original timing");
    styleSliderTextBox(frequencySlider);
    frequencySlider.setValue(defaultFrequency, juce::dontSendNotification);
    frequencySlider.onValueChange = [this]
    {
        if (onFrequencyChanged)
            onFrequencyChanged(frequencySlider.getValue());
    };
    addAndMakeVisible(frequencySlider);

    amplitudeSlider.setRange(0.0, 1.0, 0.01);
    amplitudeSlider.setSliderStyle(juce::Slider::IncDecButtons);
    amplitudeSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 70, 24);
    amplitudeSlider.setTooltip("Source peak amplitude before the input strip");
    styleSliderTextBox(amplitudeSlider);
    amplitudeSlider.setValue(defaultAmplitude, juce::dontSendNotification);
    amplitudeSlider.onValueChange = [this]
    {
        if (onAmplitudeChanged)
            onAmplitudeChanged(amplitudeSlider.getValue());
    };
    addAndMakeVisible(amplitudeSlider);

    inputLabel.setText("INPUT " + number, juce::dontSendNotification);
    inputLabel.setColour(juce::Label::textColourId, textPrimary);
    inputLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(inputLabel);

    configureRotarySlider(gainSlider, -24.0, 24.0, 0.1, " dB");
    gainSlider.setValue(0.0, juce::dontSendNotification);
    gainSlider.onValueChange = [this]
    {
        if (onGainChanged)
            onGainChanged(static_cast<float>(gainSlider.getValue()));
    };
    addAndMakeVisible(gainSlider);

    muteButton.setButtonText("MUTE " + number);
    muteButton.onClick = [this]
    {
        if (onMuteChanged)
            onMuteChanged(muteButton.getToggleState());
    };
    addAndMakeVisible(muteButton);

    phaseButton.setButtonText("PHASE " + number);
    phaseButton.onClick = [this]
    {
        if (onPhaseChanged)
            onPhaseChanged(phaseButton.getToggleState());
    };
    addAndMakeVisible(phaseButton);

    addAndMakeVisible(meter);

    peakLabel.setText("PRE  --.- dB", juce::dontSendNotification);
    peakLabel.setColour(juce::Label::textColourId, signalGreen);
    peakLabel.setJustificationType(juce::Justification::centred);
    peakLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                        11.0f, juce::Font::bold));
    addAndMakeVisible(peakLabel);
}

void InputPanel::resetSource()
{
    sourceSelector.setSelectedId(static_cast<int>(SourceType::sine), juce::sendNotificationSync);
    frequencySlider.setValue(defaultFrequency, juce::sendNotificationSync);
    amplitudeSlider.setValue(defaultAmplitude, juce::sendNotificationSync);
}

void InputPanel::setFromState(const audio_core::InputState& state)
{
    gainSlider.setValue(state.gainDecibels, juce::dontSendNotification);
    muteButton.setToggleState(state.muted, juce::dontSendNotification);
    phaseButton.setToggleState(state.phaseInverted, juce::dontSendNotification);
}

void InputPanel::setMeter(float level, float peakHold, const juce::String& text)
{
    meter.setLevels(level, peakHold);
    peakLabel.setText(text, juce::dontSendNotification);
}

void InputPanel::resized()
{
    auto source = juce::Rectangle<int>(0, 0, 140, height).reduced(7, 5);
    sourceLabel.setBounds(source.removeFromTop(26));
    sourceSelector.setBounds(source.removeFromTop(26));
    source.removeFromTop(5);
    frequencyLabel.setBounds(source.removeFromTop(18));
    frequencySlider.setBounds(source.removeFromTop(28));
    source.removeFromTop(5);
    amplitudeLabel.setBounds(source.removeFromTop(18));
    amplitudeSlider.setBounds(source.removeFromTop(28));

    auto node = juce::Rectangle<int>(stripX, 0, 370, height).reduced(12, 8);
    inputLabel.setBounds(node.removeFromTop(22));
    auto footer = node.removeFromBottom(26);
    muteButton.setBounds(footer.removeFromLeft(62).reduced(1));
    phaseButton.setBounds(footer.removeFromLeft(82).reduced(1));
    peakLabel.setBounds(footer);

    auto gainColumn = node.removeFromLeft(100);
    meter.setBounds(gainColumn.removeFromLeft(14).reduced(1, 5));
    gainColumn.removeFromLeft(4);
    gainSlider.setBounds(gainColumn);
}
