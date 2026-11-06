#include "LookAndFeel.h"

#include <algorithm>

using namespace theme;

AppLookAndFeel::AppLookAndFeel()
    : LookAndFeel_V4(getDarkColourScheme())
{
    setColour(juce::Label::textColourId, textPrimary);
    setColour(juce::Slider::textBoxTextColourId, textPrimary);
    setColour(juce::Slider::textBoxBackgroundColourId, panelRaised);
    setColour(juce::Slider::textBoxOutlineColourId, panelOutline);
    setColour(juce::TextButton::textColourOffId, textPrimary);
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    setColour(juce::TextButton::buttonColourId, panelRaised);
    setColour(juce::TextButton::buttonOnColourId, accent);
}

void AppLookAndFeel::drawRotarySlider(juce::Graphics& graphics,
                                           int x,
                                           int y,
                                           int width,
                                           int height,
                                           float sliderPosition,
                                           float rotaryStartAngle,
                                           float rotaryEndAngle,
                                           juce::Slider&)
{
    const auto diameter = static_cast<float>(std::min(width, height)) - 14.0f;
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                static_cast<float>(width), static_cast<float>(height))
                            .withSizeKeepingCentre(diameter, diameter);
    const auto angle = rotaryStartAngle + sliderPosition * (rotaryEndAngle - rotaryStartAngle);
    const auto radius = bounds.getWidth() * 0.5f;
    const auto centre = bounds.getCentre();

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                        rotaryStartAngle, rotaryEndAngle, true);
    graphics.setColour(panelOutline);
    graphics.strokePath(track, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved));

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                        rotaryStartAngle, angle, true);
    graphics.setColour(accent);
    graphics.strokePath(value, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved));

    graphics.setColour(inset);
    graphics.fillEllipse(bounds.reduced(7.0f));
    graphics.setColour(panelOutline);
    graphics.drawEllipse(bounds.reduced(7.0f), 1.0f);

    juce::Path marker;
    marker.addRoundedRectangle(-1.5f, -radius + 12.0f, 3.0f, radius * 0.35f, 1.5f);
    graphics.setColour(textPrimary);
    graphics.fillPath(marker, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void AppLookAndFeel::drawToggleButton(juce::Graphics& graphics,
                                           juce::ToggleButton& button,
                                           bool highlighted,
                                           bool down)
{
    if (static_cast<bool>(button.getProperties()["matrixCell"]))
    {
        const auto size = static_cast<float>(std::min(button.getWidth(), button.getHeight()) - 8);
        const auto box = juce::Rectangle<float>(size, size)
                             .withCentre(button.getLocalBounds().toFloat().getCentre());
        graphics.setColour(button.getToggleState() ? accent : juce::Colour(0xff111214));
        graphics.fillRoundedRectangle(box, 3.0f);
        graphics.setColour(button.getToggleState() ? accent.brighter(0.25f) : panelOutline);
        graphics.drawRoundedRectangle(box, 3.0f, 1.5f);

        if (button.getToggleState())
        {
            juce::Path tick;
            tick.startNewSubPath(box.getX() + box.getWidth() * 0.22f,
                                 box.getY() + box.getHeight() * 0.52f);
            tick.lineTo(box.getX() + box.getWidth() * 0.43f,
                        box.getY() + box.getHeight() * 0.72f);
            tick.lineTo(box.getX() + box.getWidth() * 0.79f,
                        box.getY() + box.getHeight() * 0.29f);
            graphics.setColour(juce::Colours::white);
            graphics.strokePath(tick, juce::PathStrokeType(2.2f,
                                                            juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));
        }
        return;
    }

    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const auto active = button.getToggleState();
    auto background = active ? accent : panelRaised;
    if (highlighted || down)
        background = background.brighter(0.12f);

    graphics.setColour(background);
    graphics.fillRoundedRectangle(bounds, 4.0f);
    graphics.setColour(active ? accent.brighter(0.25f) : panelOutline);
    graphics.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    graphics.setColour(textPrimary);
    graphics.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    graphics.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(5, 2),
                            juce::Justification::centred, 1);
}
