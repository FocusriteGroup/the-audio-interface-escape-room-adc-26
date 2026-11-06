#pragma once

#include <JuceHeader.h>

#include <functional>

class Header final : public juce::Component
{
public:
    static constexpr int height = 92;

    Header();

    void paint(juce::Graphics& graphics) override;
    void resized() override;

    bool captureControlsVisible() const { return captureButton.isVisible(); }
    void setCaptureStatus(const juce::String& text);
    void setReplayToggled(bool shouldBeOn);
    void setCaptureToggled(bool shouldBeOn);

    std::function<void()> onSavePreset;
    std::function<void()> onLoadPreset;
    std::function<void(bool)> onPlayToggled;
    std::function<void(bool)> onCaptureToggled;
    std::function<void(bool)> onReplayToggled;

private:
    juce::Label missionLabel;
    juce::Label titleLabel;
    juce::TextButton savePresetButton { "SAVE PRESET" };
    juce::TextButton loadPresetButton { "LOAD PRESET" };
    juce::TextButton playButton;
    juce::ToggleButton captureButton { "LOOPBACK RECORD" };
    juce::ToggleButton replayButton { "REPLAY CAPTURE" };
    juce::Label captureStatusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Header)
};
