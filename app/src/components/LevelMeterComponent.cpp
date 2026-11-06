#include "LevelMeterComponent.h"

#include "../LookAndFeel.h"

#include <algorithm>

using namespace theme;

void LevelMeterComponent::setLevels(float newLevel, float newPeakHold)
{
    level = std::clamp(newLevel, 0.0f, 2.0f);
    peakHold = std::clamp(newPeakHold, 0.0f, 2.0f);
    repaint();
}

void LevelMeterComponent::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    graphics.setColour(inset);
    graphics.fillRoundedRectangle(bounds, 2.0f);
    graphics.setColour(panelOutline);
    graphics.drawRoundedRectangle(bounds, 2.0f, 1.0f);

    const auto levelToPosition = [bounds](float linearLevel)
    {
        const auto decibels = juce::Decibels::gainToDecibels(linearLevel, -60.0f);
        return juce::jmap(std::clamp(decibels, -60.0f, 6.0f),
                          -60.0f, 6.0f, bounds.getBottom(), bounds.getY());
    };

    const auto levelY = levelToPosition(level);
    const auto meterBounds = juce::Rectangle<float>(bounds.getX() + 2.0f,
                                                     levelY,
                                                     bounds.getWidth() - 4.0f,
                                                     bounds.getBottom() - levelY - 2.0f);
    juce::ColourGradient gradient(signalGreen, 0.0f, bounds.getBottom(),
                                  juce::Colour(0xffffc857), 0.0f, bounds.getY(), false);
    gradient.addColour(0.83, juce::Colour(0xffffc857));
    gradient.addColour(0.93, accent);
    graphics.setGradientFill(gradient);
    graphics.fillRect(meterBounds);

    const auto holdY = levelToPosition(peakHold);
    graphics.setColour(peakHold >= 1.0f ? accent : textPrimary);
    graphics.fillRect(bounds.getX() + 1.0f, holdY - 1.0f, bounds.getWidth() - 2.0f, 2.0f);
}