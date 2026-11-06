#include "DiagnosticConsoleComponent.h"

#include "../LookAndFeel.h"

#include <logging/DiagnosticLog.h>

using namespace theme;

DiagnosticConsoleComponent::DiagnosticConsoleComponent()
{
    terminal.setMultiLine(true);
    terminal.setReadOnly(true);
    terminal.setScrollbarsShown(true);
    terminal.setCaretVisible(false);
    terminal.setPopupMenuEnabled(true);
    terminal.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f,
                                       juce::Font::plain));
    terminal.setColour(juce::TextEditor::backgroundColourId, inset);
    terminal.setColour(juce::TextEditor::textColourId, signalGreen);
    terminal.setColour(juce::TextEditor::outlineColourId, panelOutline);
    terminal.setColour(juce::TextEditor::focusedOutlineColourId, panelOutline);
    addAndMakeVisible(terminal);

    clearButton.setTooltip("Clear diagnostic log output");
    clearButton.onClick = [this] { clear(); };
    addAndMakeVisible(clearButton);

    audio_core::logDiagnostic("Diagnostic log ready");
    startTimerHz(20);
}

void DiagnosticConsoleComponent::timerCallback()
{
    auto changed = false;
    const auto dropped = audio_core::takeDroppedDiagnosticLogCount();
    if (dropped > 0)
    {
        visibleLines.add("[log] dropped " + juce::String(dropped) + " entries");
        changed = true;
    }

    audio_core::DiagnosticLogEntry entry;
    while (audio_core::tryPopDiagnosticLog(entry))
    {
        auto line = "[" + juce::String(entry.sequence).paddedLeft('0', 5) + "] "
            + juce::String(entry.text.data());
        if (entry.hasValue)
            line += " = " + juce::String(entry.value, 6);
        visibleLines.add(line);
        changed = true;
    }

    if (!changed)
        return;

    while (visibleLines.size() > maximumVisibleLines)
        visibleLines.remove(0);
    terminal.setText(visibleLines.joinIntoString("\n"), false);
    terminal.moveCaretToEnd();
}

void DiagnosticConsoleComponent::clear()
{
    audio_core::clearDiagnosticLog();
    visibleLines.clear();
    terminal.clear();
}

void DiagnosticConsoleComponent::resized()
{
    auto bounds = getLocalBounds().reduced(4);
    clearButton.setBounds(bounds.removeFromBottom(24).removeFromRight(72));
    bounds.removeFromBottom(4);
    terminal.setBounds(bounds);
}
