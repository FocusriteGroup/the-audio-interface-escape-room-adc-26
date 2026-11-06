#pragma once

#include <JuceHeader.h>

class DiagnosticConsoleComponent final : public juce::Component,
                                         private juce::Timer
{
public:
    DiagnosticConsoleComponent();

    void resized() override;

private:
    void timerCallback() override;
    void clear();

    static constexpr int maximumVisibleLines = 200;

    juce::StringArray visibleLines;
    juce::TextEditor terminal;
    juce::TextButton clearButton { "CLEAR" };
};
