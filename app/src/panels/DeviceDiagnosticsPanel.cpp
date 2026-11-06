#include "DeviceDiagnosticsPanel.h"

#include "../LookAndFeel.h"

#include <algorithm>

using namespace theme;

DeviceDiagnosticsPanel::DeviceDiagnosticsPanel(juce::AudioDeviceManager& manager)
    : deviceManager(manager),
      outputSelector(deviceManager, 0, 0, 2, 2, false, false, true, false)
{
    audioOutputLabel.setText("AUDIO OUTPUT", juce::dontSendNotification);
    audioOutputLabel.setColour(juce::Label::textColourId, textPrimary);
    audioOutputLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(audioOutputLabel);

    audioStatusLabel.setColour(juce::Label::textColourId, textSecondary);
    audioStatusLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(audioStatusLabel);

    diagnosticsLabel.setColour(juce::Label::textColourId, textSecondary);
    diagnosticsLabel.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(diagnosticsLabel);

    timingLabel.setColour(juce::Label::textColourId, textSecondary);
    timingLabel.setFont(juce::FontOptions(12.0f));
    timingLabel.setTooltip("CPU is JUCE's smoothed average callback load. MAX and MISSES "
                           "track the worst single callback against its deadline.");
    addAndMakeVisible(timingLabel);

    clipValueLabel.setJustificationType(juce::Justification::centred);
    clipValueLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(clipValueLabel);

    resetTimingButton.setTooltip("Reset maximum durations and deadline misses");
    resetTimingButton.onClick = [this]
    {
        peakCallbackMilliseconds.store(0.0);
        deadlineMissCount.store(0);
        appendTraceLine(traceTimestamp(juce::Time::getHighResolutionTicks()) + " [UI] timing reset");
    };
    addAndMakeVisible(resetTimingButton);
    markStateButton.setTooltip("Compare subsequent control changes and loads against this state");
    markStateButton.onClick = [this] { if (onMarkState) onMarkState(); };
    addAndMakeVisible(markStateButton);
    traceView.setRowHeight(16);
    traceView.setColour(juce::ListBox::backgroundColourId, inset);
    for (auto* view : { &stateView, &differenceView, &creditsView })
    {
        view->setMultiLine(true);
        view->setReadOnly(true);
        view->setScrollbarsShown(true);
        view->setCaretVisible(false);
        view->setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f,
                                       juce::Font::plain));
        view->setColour(juce::TextEditor::backgroundColourId, inset);
        view->setColour(juce::TextEditor::textColourId, textPrimary);
    }
    // Sample rate and buffer size are always shown; the page scrolls if the
    // window is too short for them. At 21 px a row, the device type, output,
    // sample rate and buffer size all fit at the smallest window size. Windows
    // always shows the device type, as it has several.
    outputSelector.setItemHeight(21);
    devicePage.setViewedComponent(&outputSelector, false);
    devicePage.setScrollBarsShown(true, false);
    devicePage.onResize = [this] { layOutDevicePage(); };

    pauseTraceButton.setClickingTogglesState(true);
    pauseTraceButton.setTooltip("Hold the trace still to read it; events keep being collected");
    pauseTraceButton.onClick = [this] { setTracePaused(pauseTraceButton.getToggleState()); };
    traceStatusLabel.setColour(juce::Label::textColourId, textSecondary);
    traceStatusLabel.setFont(juce::FontOptions(11.0f));
    for (auto* child : std::initializer_list<juce::Component*> { &pauseTraceButton,
                                                                 &traceStatusLabel, &traceView })
        tracePage.addAndMakeVisible(child);
    tracePage.onResize = [this] { layOutTracePage(); };
    setTracePaused(false);

    diagnosticsTabs.setTabBarDepth(26);
    diagnosticsTabs.addTab("DEVICE", panelRaised, &devicePage, false);
    diagnosticsTabs.addTab("LOG", panelRaised, &diagnosticConsole, false);
    diagnosticsTabs.addTab("TRACE", panelRaised, &tracePage, false);
    diagnosticsTabs.addTab("STATE", panelRaised, &stateView, false);
    diagnosticsTabs.addTab("DIFF", panelRaised, &differenceView, false);
    creditsView.setText(juce::String::fromUTF8(BinaryData::README_md, BinaryData::README_mdSize), false);
    diagnosticsTabs.addTab("CREDITS", panelRaised, &creditsView, false);
    addAndMakeVisible(diagnosticsTabs);
}

void DeviceDiagnosticsPanel::publishAudioCallback(double durationMilliseconds,
                                                       int bufferSamples,
                                                       double sampleRate) noexcept
{
    const auto count = callbackCount.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto budget = sampleRate > 0.0
        ? 1'000.0 * static_cast<double>(bufferSamples) / sampleRate
        : 0.0;
    callbackBudgetMilliseconds.store(budget, std::memory_order_relaxed);

    auto peak = peakCallbackMilliseconds.load(std::memory_order_relaxed);
    while (durationMilliseconds > peak
           && !peakCallbackMilliseconds.compare_exchange_weak(
               peak, durationMilliseconds,
               std::memory_order_release, std::memory_order_relaxed))
    {
    }

    if (budget > 0.0 && durationMilliseconds > budget)
        deadlineMissCount.fetch_add(1, std::memory_order_relaxed);
    if (count % 256 == 1 || (budget > 0.0 && durationMilliseconds > budget))
        enqueueTrace({ juce::Time::getHighResolutionTicks(), durationMilliseconds, budget,
                       count });
}

void DeviceDiagnosticsPanel::enqueueTrace(TraceEvent event) noexcept
{
    if (traceFifo.getFreeSpace() == 0)
    {
        droppedTraceEvents.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    traceFifo.write(1).forEach([this, event](int index)
    {
        traceEvents[static_cast<std::size_t>(index)] = event;
    });
}

juce::String DeviceDiagnosticsPanel::traceTimestamp(juce::int64 ticks) const
{
    return juce::String(1'000.0 * juce::Time::highResolutionTicksToSeconds(ticks - traceStartedAt), 2)
        + " ms";
}

void DeviceDiagnosticsPanel::appendTraceLine(const juce::String& line)
{
    if (tracePaused)
        ++traceLinesSincePause;
    traceLines.add(line);
    while (traceLines.size() > 256)
        traceLines.remove(0);
    traceChanged = true;
}

void DeviceDiagnosticsPanel::refreshTrace(std::uint32_t now, bool showing)
{
    const auto justShown = showing && !traceWasShowing;
    traceWasShowing = showing;
    if (showing && tracePaused)
    {
        traceStatusLabel.setText("PAUSED  /  " + juce::String(traceLinesSincePause)
                                     + " NEW EVENTS SINCE",
                                 juce::dontSendNotification);
        return;
    }
    if (!showing || !traceChanged)
        return;

    constexpr std::uint32_t refreshIntervalMs = 200;
    if (!justShown && now - lastTraceRefreshAtMs < refreshIntervalMs)
        return;

    updateTraceDisplay();
    lastTraceRefreshAtMs = now;
    traceChanged = false;
}

void DeviceDiagnosticsPanel::setTracePaused(bool paused)
{
    tracePaused = paused;
    traceLinesSincePause = 0;
    pauseTraceButton.setToggleState(paused, juce::dontSendNotification);
    pauseTraceButton.setButtonText(paused ? "RESUME TRACE" : "PAUSE TRACE");
    if (paused)
    {
        traceStatusLabel.setText("PAUSED  /  0 NEW EVENTS SINCE", juce::dontSendNotification);
        return;
    }
    traceStatusLabel.setText("LIVE  /  LATEST 256 EVENTS", juce::dontSendNotification);
    updateTraceDisplay();
    traceChanged = false;
}

void DeviceDiagnosticsPanel::layOutDevicePage()
{
    outputSelector.setSize(devicePage.getWidth() - devicePage.getScrollBarThickness(),
                           outputSelector.getHeight());
}

void DeviceDiagnosticsPanel::layOutTracePage()
{
    auto bounds = tracePage.getLocalBounds();
    auto toolbar = bounds.removeFromTop(26).reduced(0, 2);
    pauseTraceButton.setBounds(toolbar.removeFromLeft(118));
    toolbar.removeFromLeft(8);
    traceStatusLabel.setBounds(toolbar);
    bounds.removeFromTop(2);
    traceView.setBounds(bounds);
}

void DeviceDiagnosticsPanel::updateTraceDisplay()
{
    displayedTraceLines = traceLines;
    traceView.setMinimumContentWidth(traceContentWidth());
    traceView.updateContent();
    traceView.repaint();
    if (!displayedTraceLines.isEmpty())
        traceView.scrollToEnsureRowIsOnscreen(displayedTraceLines.size() - 1);
}

int DeviceDiagnosticsPanel::traceContentWidth() const
{
    int longestLine = 0;
    for (const auto& line : displayedTraceLines)
        longestLine = std::max(longestLine, line.length());

    const auto characterWidth = juce::GlyphArrangement::getStringWidth(traceFont, "M");
    constexpr int horizontalPadding = 8;
    return horizontalPadding + juce::roundToInt(static_cast<float>(longestLine) * characterWidth);
}

int DeviceDiagnosticsPanel::getNumRows()
{
    return displayedTraceLines.size();
}

void DeviceDiagnosticsPanel::paintListBoxItem(int row, juce::Graphics& graphics,
                                              int width, int height, bool selected)
{
    if (!juce::isPositiveAndBelow(row, displayedTraceLines.size()))
        return;

    if (selected)
        graphics.fillAll(panelRaised);

    graphics.setFont(traceFont);
    graphics.setColour(textPrimary);
    graphics.drawText(displayedTraceLines[row], 4, 0, juce::jmax(0, width - 8), height,
                      juce::Justification::centredLeft, false);
}

void DeviceDiagnosticsPanel::setAppStateView(std::string_view state,
                                                     std::string_view difference)
{
    const auto update = [](juce::TextEditor& editor, std::string_view text)
    {
        const auto value = juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
        if (editor.getText() != value)
            editor.setText(value, false);
    };
    update(stateView, state);
    update(differenceView, difference);
}

void DeviceDiagnosticsPanel::publishClipCount(std::size_t clipCount) noexcept
{
    latestClipCount.store(clipCount, std::memory_order_relaxed);
}

void DeviceDiagnosticsPanel::refresh(bool playing)
{
    const auto currentCallbackCount = callbackCount.load(std::memory_order_relaxed);
    const auto callbackMilliseconds = peakCallbackMilliseconds.load(std::memory_order_relaxed);
    const auto budgetMilliseconds = callbackBudgetMilliseconds.load(std::memory_order_relaxed);
    const auto deadlineMisses = deadlineMissCount.load(std::memory_order_relaxed);
    const auto now = juce::Time::getMillisecondCounter();

    traceFifo.read(traceFifo.getNumReady()).forEach([this](int index)
    {
        const auto& event = traceEvents[static_cast<std::size_t>(index)];
        appendTraceLine(traceTimestamp(event.ticks) + " [AUDIO] callback "
            + juce::String(event.durationMilliseconds, 3) + " ms / budget "
            + juce::String(event.budgetMilliseconds, 3) + " ms / #"
            + juce::String(event.count));
    });
    const auto dropped = droppedTraceEvents.exchange(0);
    if (dropped > 0)
        appendTraceLine("[TRACE] dropped " + juce::String(dropped) + " events");
    refreshTrace(now, traceView.isShowing());

    if (currentCallbackCount != previousCallbackCount)
        lastCallbackSeenAtMs = now;

    previousCallbackCount = currentCallbackCount;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto callbackIsActive = currentCallbackCount > 0
            && device->isPlaying()
            && now - lastCallbackSeenAtMs < 1'000;
        const auto deviceError = device->getLastError();

        audioStatusLabel.setText("SELECTED  /  " + device->getName(),
                                 juce::dontSendNotification);
        audioStatusLabel.setColour(juce::Label::textColourId,
                                   device->isPlaying() ? signalGreen : accent);

        if (deviceError.isNotEmpty())
        {
            diagnosticsLabel.setText("DEVICE ERROR  /  " + deviceError,
                                     juce::dontSendNotification);
            timingLabel.setText({}, juce::dontSendNotification);
        }
        else
        {
            juce::String callbackState;
            if (callbackIsActive)
                callbackState = "CALLBACK ACTIVE";
            else if (currentCallbackCount == 0 && now - audioStartupBeganAtMs >= 5'000)
                callbackState = "NO CALLBACKS  /  CHECK SYSTEM AUDIO";
            else
                callbackState = "CALLBACK STARTING";

            diagnosticsLabel.setText(
                callbackState + "  /  "
                    + juce::String(device->getCurrentSampleRate(), 0) + " HZ  /  "
                    + juce::String(device->getCurrentBufferSizeSamples()) + " SAMPLES  /  #"
                    + juce::String(currentCallbackCount),
                juce::dontSendNotification);
            const auto cpuPercent = juce::roundToInt(100.0 * deviceManager.getCpuUsage());
            timingLabel.setText(
                "CPU " + juce::String(cpuPercent) + "%  /  MAX "
                    + juce::String(callbackMilliseconds, 2) + " OF "
                    + juce::String(budgetMilliseconds, 2) + " MS  /  MISSES "
                    + juce::String(deadlineMisses),
                juce::dontSendNotification);
        }
    }
    else
    {
        audioStatusLabel.setText("NO OUTPUT OPEN  /  CHOOSE A DEVICE BELOW",
                                 juce::dontSendNotification);
        audioStatusLabel.setColour(juce::Label::textColourId, accent);
        diagnosticsLabel.setText("AUDIO CALLBACK WAITING", juce::dontSendNotification);
        timingLabel.setText({}, juce::dontSendNotification);
    }

    if (!playing)
    {
        clipValueLabel.setText("SIGNAL PAUSED", juce::dontSendNotification);
        clipValueLabel.setColour(juce::Label::backgroundColourId, panelRaised);
        clipValueLabel.setColour(juce::Label::textColourId, textSecondary);
        return;
    }

    const auto clips = latestClipCount.load(std::memory_order_relaxed);
    if (clips > 0)
    {
        clipValueLabel.setText("CLIPPING  /  " + juce::String(clips) + " SAMPLES",
                               juce::dontSendNotification);
        clipValueLabel.setColour(juce::Label::backgroundColourId, accent);
        clipValueLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    }
    else
    {
        clipValueLabel.setText("SIGNAL WITHIN RANGE", juce::dontSendNotification);
        clipValueLabel.setColour(juce::Label::backgroundColourId, signalGreen.darker(0.65f));
        clipValueLabel.setColour(juce::Label::textColourId, signalGreen.brighter(0.35f));
    }
}

void DeviceDiagnosticsPanel::resized()
{
    auto device = getLocalBounds();
    auto header = device.removeFromTop(24);
    resetTimingButton.setBounds(header.removeFromRight(108));
    header.removeFromRight(4);
    markStateButton.setBounds(header.removeFromRight(100));
    audioOutputLabel.setBounds(header);
    // Compact status rows leave the tabs room for the device settings.
    audioStatusLabel.setBounds(device.removeFromTop(20));
    diagnosticsLabel.setBounds(device.removeFromTop(18));
    timingLabel.setBounds(device.removeFromTop(18));
    clipValueLabel.setBounds(device.removeFromTop(24));
    device.removeFromTop(4);
    diagnosticsTabs.setBounds(device);
}
