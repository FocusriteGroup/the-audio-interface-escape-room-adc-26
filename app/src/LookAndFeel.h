#pragma once

#include <JuceHeader.h>

namespace theme
{
inline const auto accent = juce::Colour(0xffd6292f);
inline const auto ink = juce::Colour(0xff151618);
inline const auto panel = juce::Colour(0xff242629);
inline const auto panelRaised = juce::Colour(0xff303236);
inline const auto panelOutline = juce::Colour(0xff4b4e53);
inline const auto textPrimary = juce::Colour(0xfff4f2ed);
inline const auto textSecondary = juce::Colour(0xffa9adb3);
inline const auto signalGreen = juce::Colour(0xff57c785);
inline const auto signalRed = juce::Colour(0xfff05b4f);
// Marks signal above full scale, which the output device clips.
inline const auto overFullScale = juce::Colour(0xfff2b84b);
inline const auto inset = juce::Colour(0xff0c0d0e);

inline void styleSliderTextBox(juce::Slider& slider)
{
    slider.setColour(juce::Slider::textBoxTextColourId, textPrimary);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, panelRaised);
    slider.setColour(juce::Slider::textBoxOutlineColourId, panelOutline);
}

inline void configureRotarySlider(juce::Slider& slider,
                                  double minimum,
                                  double maximum,
                                  double interval,
                                  const juce::String& suffix)
{
    slider.setRange(minimum, maximum, interval);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 20);
    slider.setTextValueSuffix(suffix);
    styleSliderTextBox(slider);
}
}

class AppLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    AppLookAndFeel();

    void drawRotarySlider(juce::Graphics& graphics,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosition,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;
    void drawToggleButton(juce::Graphics& graphics,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
};
