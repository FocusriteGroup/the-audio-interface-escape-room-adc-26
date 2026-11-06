#include "AnalysisPanel.h"

#include "../LookAndFeel.h"

#include <algorithm>
#include <cmath>

using namespace theme;

namespace
{
// The first three taps are before the mixer, so the scope's channels are the
// two inputs; from Post mix onwards they're the two outputs.
void nameScopeChannels(ScopeComponent& scope, int tapIndex)
{
    constexpr auto firstOutputTap = 3;
    if (tapIndex >= firstOutputTap)
        scope.setChannelNames("OUTPUT L", "OUTPUT R");
    else
        scope.setChannelNames("INPUT 1", "INPUT 2");
}
}

AnalysisPanel::AnalysisPanel()
{
    peakTitleLabel.setText("TAP PEAK", juce::dontSendNotification);
    peakTitleLabel.setColour(juce::Label::textColourId, textSecondary);
    peakTitleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(peakTitleLabel);

    peakValueLabel.setText("-- dBFS", juce::dontSendNotification);
    peakValueLabel.setColour(juce::Label::textColourId, textPrimary);
    peakValueLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                             16.0f, juce::Font::bold));
    addAndMakeVisible(peakValueLabel);

    scopeLabel.setText("STEREO SCOPE", juce::dontSendNotification);
    scopeLabel.setColour(juce::Label::textColourId, textPrimary);
    scopeLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(scopeLabel);

    analysisTapSelector.addItemList({ "Input", "Post gain", "Post EQ", "Post mix", "Output" }, 1);
    analysisTapSelector.setSelectedId(5);
    nameScopeChannels(oscilloscope, analysisTapSelector.getSelectedItemIndex());
    analysisTapSelector.setTooltip("Choose where the analyzer taps the signal graph");
    analysisTapSelector.onChange = [this]
    {
        selectedAnalysisTap.store(
            static_cast<std::size_t>(analysisTapSelector.getSelectedItemIndex()));
        nameScopeChannels(oscilloscope, analysisTapSelector.getSelectedItemIndex());
        oscilloscope.clear();
        resetMeasurements();
    };
    addAndMakeVisible(analysisTapSelector);

    analysisChannelSelector.addItemList({ "Channel 1", "Channel 2" }, 1);
    analysisChannelSelector.setSelectedId(1);
    analysisChannelSelector.setTooltip("Channel for peak, RMS, spectrum and scope triggering");
    analysisChannelSelector.onChange = [this]
    {
        selectedAnalysisChannel.store(
            static_cast<std::size_t>(analysisChannelSelector.getSelectedItemIndex()));
        oscilloscope.setTriggerChannel(selectedChannelIndex());
        resetMeasurements();
    };
    addAndMakeVisible(analysisChannelSelector);

    rmsValueLabel.setText("RMS  --.- dBFS", juce::dontSendNotification);
    rmsValueLabel.setColour(juce::Label::textColourId, textSecondary);
    rmsValueLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                            11.0f, juce::Font::bold));
    addAndMakeVisible(rmsValueLabel);

    correlationValueLabel.setText("CORR  +0.00", juce::dontSendNotification);
    correlationValueLabel.setColour(juce::Label::textColourId, textSecondary);
    correlationValueLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                                    11.0f, juce::Font::bold));
    addAndMakeVisible(correlationValueLabel);

    scopeTriggerButton.setTooltip("Hold the oscilloscope display on a rising zero crossing");
    scopeTriggerButton.onClick = [this]
    {
        oscilloscope.setTriggerEnabled(scopeTriggerButton.getToggleState());
    };
    addAndMakeVisible(scopeTriggerButton);

    scopePauseButton.setButtonText("PAUSE SCOPE");
    scopePauseButton.setClickingTogglesState(true);
    scopePauseButton.setTooltip("Freeze or resume oscilloscope capture without stopping audio");
    scopePauseButton.onClick = [this]
    {
        const auto paused = scopePauseButton.getToggleState();
        oscilloscope.setCapturePaused(paused);
        scopePauseButton.setButtonText(paused ? "RESUME SCOPE" : "PAUSE SCOPE");
    };
    addAndMakeVisible(scopePauseButton);

    scopeClearButton.setButtonText("CLEAR");
    scopeClearButton.setTooltip("Clear captured oscilloscope history");
    scopeClearButton.onClick = [this] { oscilloscope.clear(); };
    addAndMakeVisible(scopeClearButton);

    timeScaleLabel.setText("TIME SCALE", juce::dontSendNotification);
    timeScaleLabel.setColour(juce::Label::textColourId, textSecondary);
    timeScaleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    addAndMakeVisible(timeScaleLabel);

    timeScaleSlider.setRange(2.0, 2'000.0, 1.0);
    timeScaleSlider.setSkewFactorFromMidPoint(50.0);
    timeScaleSlider.setValue(20.0);
    timeScaleSlider.setTextValueSuffix(" ms");
    timeScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    timeScaleSlider.setTooltip("Lower values zoom in on time; higher values show a longer interval");
    styleSliderTextBox(timeScaleSlider);
    timeScaleSlider.onValueChange = [this]
    {
        oscilloscope.setTimeWindowMilliseconds(timeScaleSlider.getValue());
    };
    addAndMakeVisible(timeScaleSlider);

    amplitudeZoomLabel.setText("AMPLITUDE ZOOM", juce::dontSendNotification);
    amplitudeZoomLabel.setColour(juce::Label::textColourId, textSecondary);
    amplitudeZoomLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    addAndMakeVisible(amplitudeZoomLabel);

    amplitudeZoomSlider.setRange(0.5, 8.0, 0.5);
    amplitudeZoomSlider.setSkewFactorFromMidPoint(2.0);
    amplitudeZoomSlider.setValue(1.0);
    amplitudeZoomSlider.setTextValueSuffix("x");
    amplitudeZoomSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    amplitudeZoomSlider.setTooltip("Magnify the display vertically without changing the audio");
    styleSliderTextBox(amplitudeZoomSlider);
    amplitudeZoomSlider.onValueChange = [this]
    {
        oscilloscope.setVerticalZoom(static_cast<float>(amplitudeZoomSlider.getValue()));
    };
    addAndMakeVisible(amplitudeZoomSlider);

    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(spectrum);
}

std::size_t AnalysisPanel::selectedTapIndex() const noexcept
{
    return selectedAnalysisTap.load(std::memory_order_relaxed);
}

std::size_t AnalysisPanel::selectedChannelIndex() const noexcept
{
    return selectedAnalysisChannel.load(std::memory_order_relaxed);
}

void AnalysisPanel::setSampleRate(double sampleRate) noexcept
{
    oscilloscope.setSampleRate(sampleRate);
    spectrum.setSampleRate(sampleRate);
}

void AnalysisPanel::publishRms(float rootMeanSquare) noexcept
{
    accumulatePeak(pendingAnalysisRms, rootMeanSquare);
}

void AnalysisPanel::publishPeak(float peak) noexcept
{
    accumulatePeak(pendingAnalysisPeak, peak);
}

void AnalysisPanel::resetMeasurements()
{
    pendingAnalysisPeak.store(0.0f);
    pendingAnalysisRms.store(0.0f);
    analysisRmsBallistics = {};
    spectrum.clear();
}

void AnalysisPanel::publishCorrelation(float correlation) noexcept
{
    pendingCorrelation.store(correlation, std::memory_order_relaxed);
}

void AnalysisPanel::pushOscilloscopeBuffer(const juce::AudioBuffer<float>& buffer,
                                                     int startSample,
                                                     int numSamples) noexcept
{
    oscilloscope.pushBuffer(buffer, startSample, numSamples);
}

void AnalysisPanel::pushSpectrumSamples(std::span<const float> samples) noexcept
{
    spectrum.pushSamples(samples);
}

void AnalysisPanel::refresh(bool playing, std::uint32_t now)
{
    spectrum.setActive(playing);
    const auto measuredPeak = pendingAnalysisPeak.exchange(0.0f, std::memory_order_acq_rel);
    const auto measuredRms = pendingAnalysisRms.exchange(0.0f, std::memory_order_acq_rel);
    const auto measuredCorrelation = pendingCorrelation.load(std::memory_order_relaxed);
    updateMeter(analysisRmsBallistics, measuredRms, now, playing);
    smoothedCorrelation = playing
        ? 0.85f * smoothedCorrelation + 0.15f * measuredCorrelation
        : 0.0f;
    if (!playing || std::abs(smoothedCorrelation - displayedCorrelation) >= 0.02f)
        displayedCorrelation = smoothedCorrelation;

    rmsValueLabel.setText(
        playing ? "RMS " + formatDecibels(analysisRmsBallistics.heldPeak) + "FS"
                : "RMS  --.- dBFS",
        juce::dontSendNotification);
    correlationValueLabel.setText(
        juce::String::formatted("CORR %+0.2f", static_cast<double>(displayedCorrelation)),
        juce::dontSendNotification);
    correlationValueLabel.setColour(juce::Label::textColourId,
                                    displayedCorrelation < -0.25f ? accent : textSecondary);
    peakValueLabel.setText(
        playing ? formatDecibels(measuredPeak) + "FS" : "-- dBFS",
        juce::dontSendNotification);
}

void AnalysisPanel::resized()
{
    auto analyzer = getLocalBounds();
    auto scopeHeader = analyzer.removeFromTop(30);
    scopeClearButton.setBounds(scopeHeader.removeFromRight(80));
    scopeHeader.removeFromRight(8);
    scopePauseButton.setBounds(scopeHeader.removeFromRight(132));
    analysisChannelSelector.setBounds(scopeHeader.removeFromRight(112).reduced(2));
    analysisTapSelector.setBounds(scopeHeader.removeFromRight(118).reduced(2));
    scopeLabel.setBounds(scopeHeader.removeFromLeft(155));
    peakTitleLabel.setBounds(scopeHeader.removeFromLeft(82));
    peakValueLabel.setBounds(scopeHeader);

    auto metricsRow = analyzer.removeFromTop(26);
    rmsValueLabel.setBounds(metricsRow.removeFromLeft(130));
    correlationValueLabel.setBounds(metricsRow.removeFromLeft(120));
    scopeTriggerButton.setBounds(metricsRow.removeFromLeft(128).reduced(1));

    auto displays = analyzer.removeFromTop(std::max(100, analyzer.getHeight() - 58));
    oscilloscope.setBounds(displays.removeFromLeft(displays.getWidth() / 2).reduced(0, 2));
    displays.removeFromLeft(8);
    spectrum.setBounds(displays.reduced(0, 2));
    auto analysisControls = analyzer;
    auto timeControls = analysisControls.removeFromLeft(analysisControls.getWidth() / 2);
    timeScaleLabel.setBounds(timeControls.removeFromTop(18));
    timeScaleSlider.setBounds(timeControls);
    analysisControls.removeFromLeft(16);
    amplitudeZoomLabel.setBounds(analysisControls.removeFromTop(18));
    amplitudeZoomSlider.setBounds(analysisControls);
}

void AnalysisPanel::accumulatePeak(std::atomic<float>& destination, float peak) noexcept
{
    auto current = destination.load(std::memory_order_relaxed);
    while (peak > current
           && !destination.compare_exchange_weak(current, peak,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed))
    {
    }
}

void AnalysisPanel::updateMeter(MeterBallistics& ballistics,
                                         float measuredPeak,
                                         std::uint32_t now,
                                         bool playing)
{
    if (!playing)
    {
        ballistics = {};
        return;
    }

    constexpr auto release = 0.88f;
    constexpr auto holdDecay = 0.96f;
    constexpr std::uint32_t holdDurationMs = 1'500;
    ballistics.smoothedPeak = std::max(measuredPeak, ballistics.smoothedPeak * release);

    if (measuredPeak >= ballistics.heldPeak)
    {
        ballistics.heldPeak = measuredPeak;
        ballistics.holdUntilMs = now + holdDurationMs;
    }
    else if (static_cast<std::int32_t>(now - ballistics.holdUntilMs) >= 0)
    {
        ballistics.heldPeak = std::max(ballistics.smoothedPeak,
                                       ballistics.heldPeak * holdDecay);
    }
}

juce::String AnalysisPanel::formatDecibels(float linearLevel)
{
    if (linearLevel <= 0.0f)
        return "  -inf dB";
    const auto decibels = juce::Decibels::gainToDecibels(linearLevel, -60.0f);
    return juce::String::formatted("%6.1f dB", static_cast<double>(decibels));
}