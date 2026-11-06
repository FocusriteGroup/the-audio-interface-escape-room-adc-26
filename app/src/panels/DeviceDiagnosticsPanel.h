#pragma once

#include "../components/DiagnosticConsoleComponent.h"

#include <JuceHeader.h>

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

class DeviceDiagnosticsPanel final : public juce::Component,
                                     private juce::ListBoxModel
{
public:
    explicit DeviceDiagnosticsPanel(juce::AudioDeviceManager& deviceManager);

    void publishAudioCallback(double durationMilliseconds,
                              int bufferSamples,
                              double sampleRate) noexcept;
    void publishClipCount(std::size_t clipCount) noexcept;
    void setAppStateView(std::string_view state, std::string_view difference);
    std::function<void()> onMarkState;
    void refresh(bool playing);

    void resized() override;

private:
    friend class WorkshopComponentTests;
    struct TraceEvent
    {
        juce::int64 ticks = 0;
        double durationMilliseconds = 0.0;
        double budgetMilliseconds = 0.0;
        std::uint64_t count = 0;
    };

    void enqueueTrace(TraceEvent event) noexcept;
    void appendTraceLine(const juce::String& line);
    void refreshTrace(std::uint32_t now, bool showing);
    void updateTraceDisplay();
    int traceContentWidth() const;
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& graphics, int width, int height,
                          bool selected) override;
    juce::String traceTimestamp(juce::int64 ticks) const;
    void setTracePaused(bool paused);
    void layOutDevicePage();
    void layOutTracePage();

    // A tab page that hands its layout back to the panel.
    template <typename Base>
    struct Page final : Base
    {
        std::function<void()> onResize;
        void resized() override
        {
            Base::resized();
            if (onResize)
                onResize();
        }
    };

    juce::AudioDeviceManager& deviceManager;
    juce::Label audioOutputLabel;
    juce::Label audioStatusLabel;
    juce::Label diagnosticsLabel;
    juce::Label timingLabel;
    juce::Label clipValueLabel;
    juce::TextButton resetTimingButton { "RESET TIMING" };
    juce::TextButton markStateButton { "MARK STATE" };
    juce::AudioDeviceSelectorComponent outputSelector;
    // The device settings scroll inside their tab rather than spilling past it.
    Page<juce::Viewport> devicePage;
    DiagnosticConsoleComponent diagnosticConsole;
    juce::StringArray traceLines;
    juce::StringArray displayedTraceLines;
    const juce::Font traceFont { juce::FontOptions(
        juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain) };
    juce::ListBox traceView { "Trace", this };
    Page<juce::Component> tracePage;
    juce::TextButton pauseTraceButton { "PAUSE TRACE" };
    juce::Label traceStatusLabel;
    bool tracePaused = false;
    int traceLinesSincePause = 0;
    juce::TextEditor stateView;
    juce::TextEditor differenceView;
    juce::TextEditor creditsView;
    bool traceChanged = false;
    bool traceWasShowing = false;
    std::uint32_t lastTraceRefreshAtMs = 0;
    juce::AbstractFifo traceFifo { 256 };
    std::array<TraceEvent, 256> traceEvents;
    std::atomic<unsigned int> droppedTraceEvents { 0 };
    const juce::int64 traceStartedAt = juce::Time::getHighResolutionTicks();
    juce::TabbedComponent diagnosticsTabs { juce::TabbedButtonBar::TabsAtTop };
    std::atomic<std::size_t> latestClipCount { 0 };
    std::atomic<std::uint64_t> callbackCount { 0 };
    std::atomic<double> peakCallbackMilliseconds { 0.0 };
    std::atomic<double> callbackBudgetMilliseconds { 0.0 };
    std::atomic<std::uint64_t> deadlineMissCount { 0 };
    std::uint64_t previousCallbackCount = 0;
    std::uint32_t lastCallbackSeenAtMs = 0;
    const std::uint32_t audioStartupBeganAtMs = juce::Time::getMillisecondCounter();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceDiagnosticsPanel)
};
