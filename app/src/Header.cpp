#include "Header.h"

#include "LookAndFeel.h"

#include <EscapeRoomMission.h>

using namespace theme;

Header::Header()
{
    #if defined(ESCAPE_ROOM_MISSION_01)
     missionLabel.setText("EXERCISE 01  /  PLAYBACK REPORT", juce::dontSendNotification);
     titleLabel.setText("THE TEST TONE SOUNDS DISTORTED", juce::dontSendNotification);
    #elif defined(ESCAPE_ROOM_MISSION_02)
     missionLabel.setText("EXERCISE 02  /  PLAYBACK REPORT", juce::dontSendNotification);
     titleLabel.setText("NO SOUND IN THE RIGHT CHANNEL", juce::dontSendNotification);
    #elif defined(ESCAPE_ROOM_MISSION_03)
     missionLabel.setText("EXERCISE 03  /  PLAYBACK REPORT", juce::dontSendNotification);
     titleLabel.setText("NO SOUND WHEN IN MONO", juce::dontSendNotification);
    #elif defined(ESCAPE_ROOM_MISSION_04)
     missionLabel.setText("EXERCISE 04  /  PLAYBACK REPORT", juce::dontSendNotification);
     titleLabel.setText("A STEADY TONE HAS A FAINT BUZZ", juce::dontSendNotification);
    #elif defined(ESCAPE_ROOM_MISSION_05)
     missionLabel.setText("EXERCISE 05  /  PRESET REPORT", juce::dontSendNotification);
     titleLabel.setText("MY SAVED SETUP SOUNDS DIFFERENT", juce::dontSendNotification);
    #elif defined(ESCAPE_ROOM_MISSION_FINAL)
     missionLabel.setText("COMBINED EXERCISE  /  RECORDING REPORT", juce::dontSendNotification);
     titleLabel.setText("MY RECORDING SOUNDS WRONG", juce::dontSendNotification);
    #else
     missionLabel.setText("PRODUCT BASELINE  /  STAGE 02", juce::dontSendNotification);
     titleLabel.setText("VIRTUAL 2-IN / 2-OUT INTERFACE", juce::dontSendNotification);
    #endif

    #if defined(ESCAPE_ROOM_SOLUTION)
     missionLabel.setText("SOLUTION  /  " + juce::String(ESCAPE_ROOM_MISSION_NAME),
                          juce::dontSendNotification);
    #endif

    missionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    missionLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(missionLabel);

    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont(juce::FontOptions(25.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    for (auto* button : { &savePresetButton, &loadPresetButton })
    {
        button->setColour(juce::TextButton::buttonColourId, ink);
        button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible(*button);
    }
    savePresetButton.setTooltip("Save the current product controls as JSON");
    savePresetButton.onClick = [this] { if (onSavePreset) onSavePreset(); };
    loadPresetButton.setTooltip("Load product controls from JSON");
    loadPresetButton.onClick = [this] { if (onLoadPreset) onLoadPreset(); };

    playButton.setButtonText("PLAY SIGNAL");
    playButton.setColour(juce::TextButton::buttonColourId, ink);
    playButton.setColour(juce::TextButton::buttonOnColourId, accent);
    playButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    playButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    playButton.setClickingTogglesState(true);
    playButton.setTooltip("Play or stop the virtual input signal");
    playButton.onClick = [this]
    {
        const auto shouldPlay = playButton.getToggleState();
        playButton.setButtonText(shouldPlay ? "STOP SIGNAL" : "PLAY SIGNAL");
        if (onPlayToggled)
            onPlayToggled(shouldPlay);
    };
    addAndMakeVisible(playButton);

    captureButton.setTooltip("Capture a bounded loopback segment; rate changes begin a new segment");
    captureButton.onClick = [this]
    {
        if (onCaptureToggled)
            onCaptureToggled(captureButton.getToggleState());
    };
    replayButton.setTooltip("Loop the latest captured segment at the current device rate");
    replayButton.onClick = [this]
    {
        if (onReplayToggled)
            onReplayToggled(replayButton.getToggleState());
    };
    captureStatusLabel.setColour(juce::Label::textColourId, textPrimary);
    captureStatusLabel.setFont(juce::FontOptions(12.0f));
    #if defined(ESCAPE_ROOM_MISSION_FINAL)
     addAndMakeVisible(captureButton);
     addAndMakeVisible(replayButton);
     addAndMakeVisible(captureMeter);
     addAndMakeVisible(captureStatusLabel);
    #endif
}

void Header::setCaptureMeter(float level, float peakHold)
{
    captureMeter.setLevels(level, peakHold);
}

void Header::setCaptureStatus(const juce::String& text)
{
    captureStatusLabel.setText(text, juce::dontSendNotification);
}

void Header::setReplayToggled(bool shouldBeOn)
{
    replayButton.setToggleState(shouldBeOn, juce::dontSendNotification);
}

void Header::setCaptureToggled(bool shouldBeOn)
{
    captureButton.setToggleState(shouldBeOn, juce::dontSendNotification);
}

void Header::paint(juce::Graphics& graphics)
{
    graphics.setColour(accent);
    graphics.fillRect(getLocalBounds());
}

void Header::resized()
{
    auto header = getLocalBounds().reduced(24, 12);
    playButton.setBounds(header.removeFromRight(170).reduced(0, 8));
    header.removeFromRight(8);
    loadPresetButton.setBounds(header.removeFromRight(132).reduced(0, 8));
    header.removeFromRight(8);
    savePresetButton.setBounds(header.removeFromRight(132).reduced(0, 8));
    header.removeFromRight(20);
    if (captureButton.isVisible())
    {
        captureStatusLabel.setBounds(header.removeFromRight(190));
        header.removeFromRight(6);
        captureMeter.setBounds(header.removeFromRight(18).reduced(2, 10));
        header.removeFromRight(8);
        replayButton.setBounds(header.removeFromRight(145));
        captureButton.setBounds(header.removeFromRight(145));
        header.removeFromRight(8);
    }
    missionLabel.setBounds(header.removeFromTop(22));
    titleLabel.setBounds(header);
}
