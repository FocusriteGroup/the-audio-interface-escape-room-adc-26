#include "RoutingPanel.h"

#include "../LookAndFeel.h"

using namespace theme;

RoutingPanel::RoutingPanel()
{
    routingLabel.setText("ROUTING MATRIX", juce::dontSendNotification);
    routingLabel.setColour(juce::Label::textColourId, textPrimary);
    routingLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(routingLabel);

    for (std::size_t index = 0; index < audio_core::inputCount; ++index)
    {
        auto& column = columnLabels[index];
        column.setText("IN " + juce::String(index + 1), juce::dontSendNotification);
        column.setColour(juce::Label::textColourId, textSecondary);
        column.setJustificationType(juce::Justification::centred);
        column.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(column);
    }

    for (std::size_t index = 0; index < audio_core::outputCount; ++index)
    {
        auto& row = rowLabels[index];
        row.setText(index == 0 ? "OUT L" : "OUT R", juce::dontSendNotification);
        row.setColour(juce::Label::textColourId, textSecondary);
        row.setJustificationType(juce::Justification::centredRight);
        row.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(row);
    }

    for (std::size_t index = 0; index < buttons.size(); ++index)
    {
        const auto outputIndex = index / audio_core::inputCount;
        const auto inputIndex = index % audio_core::inputCount;
        auto& button = buttons[index];
        button.getProperties().set("matrixCell", true);
        button.setTitle("Input " + juce::String(inputIndex + 1)
                + (outputIndex == 0 ? " to Output L" : " to Output R"));
        button.setTooltip(button.getTitle());
        button.onClick = [this, index, outputIndex, inputIndex]
        {
            if (onRouteChanged)
                onRouteChanged(outputIndex, inputIndex, buttons[index].getToggleState());
        };
        addAndMakeVisible(button);
    }
}

void RoutingPanel::setFromState(const audio_core::AppState& state)
{
    for (std::size_t index = 0; index < buttons.size(); ++index)
    {
        const auto outputIndex = index / audio_core::inputCount;
        const auto inputIndex = index % audio_core::inputCount;
        buttons[index].setToggleState(state.routing[outputIndex][inputIndex] != 0.0f,
                                      juce::dontSendNotification);
    }
}

void RoutingPanel::resized()
{
    auto matrix = getLocalBounds().reduced(14, 12);
    routingLabel.setBounds(matrix.removeFromTop(26));
    matrix.removeFromTop(10);
    auto columnHeader = matrix.removeFromTop(24);
    columnHeader.removeFromLeft(54);
    const auto columnWidth = columnHeader.getWidth() / 2;
    columnLabels[0].setBounds(columnHeader.removeFromLeft(columnWidth));
    columnLabels[1].setBounds(columnHeader);
    matrix.removeFromTop(12);
    const auto routeHeight = 72;
    for (std::size_t outputIndex = 0; outputIndex < audio_core::outputCount; ++outputIndex)
    {
        auto row = matrix.removeFromTop(routeHeight);
        rowLabels[outputIndex].setBounds(row.removeFromLeft(54));
        const auto cellWidth = row.getWidth() / 2;
        buttons[outputIndex * 2].setBounds(row.removeFromLeft(cellWidth).reduced(10));
        buttons[outputIndex * 2 + 1].setBounds(row.reduced(10));
        matrix.removeFromTop(34);
    }
}
