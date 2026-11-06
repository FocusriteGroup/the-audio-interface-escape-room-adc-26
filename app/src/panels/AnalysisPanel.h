#pragma once

#include "../components/ScopeComponent.h"
#include "../components/SpectrumComponent.h"

#include <JuceHeader.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

class AnalysisPanel final : public juce::Component
{
public:
    AnalysisPanel();

    std::size_t selectedTapIndex() const noexcept;
    std::size_t selectedChannelIndex() const noexcept;
    void setSampleRate(double sampleRate) noexcept;
    void publishPeak(float peak) noexcept;
    void publishRms(float rootMeanSquare) noexcept;
    void publishCorrelation(float correlation) noexcept;
    void pushOscilloscopeBuffer(const juce::AudioBuffer<float>& buffer,
                                int startSample,
                                int numSamples) noexcept;
    void pushSpectrumSamples(std::span<const float> samples) noexcept;
    void refresh(bool playing, std::uint32_t now);
    // Clears the meters and scope, e.g. when the signal being measured changes.
    void resetMeasurements();

    void resized() override;

private:
    friend class WorkshopComponentTests;
    struct MeterBallistics
    {
        float smoothedPeak = 0.0f;
        float heldPeak = 0.0f;
        std::uint32_t holdUntilMs = 0;
    };

    static void accumulatePeak(std::atomic<float>& destination, float peak) noexcept;
    static void updateMeter(MeterBallistics& ballistics,
                            float measuredPeak,
                            std::uint32_t now,
                            bool playing);
    static juce::String formatDecibels(float linearLevel);

    juce::Label peakTitleLabel;
    juce::Label peakValueLabel;
    juce::Label scopeLabel;
    juce::ComboBox analysisTapSelector;
    juce::ComboBox analysisChannelSelector;
    juce::Label rmsValueLabel;
    juce::Label correlationValueLabel;
    juce::ToggleButton scopeTriggerButton { "ZERO TRIGGER" };
    juce::TextButton scopePauseButton;
    juce::TextButton scopeClearButton;
    juce::Label timeScaleLabel;
    juce::Slider timeScaleSlider;
    juce::Label amplitudeZoomLabel;
    juce::Slider amplitudeZoomSlider;
    ScopeComponent oscilloscope;
    SpectrumComponent spectrum;

    std::atomic<std::size_t> selectedAnalysisTap { 4 };
    std::atomic<std::size_t> selectedAnalysisChannel { 0 };
    std::atomic<float> pendingAnalysisPeak { 0.0f };
    std::atomic<float> pendingAnalysisRms { 0.0f };
    std::atomic<float> pendingCorrelation { 0.0f };
    MeterBallistics analysisRmsBallistics;
    float smoothedCorrelation = 0.0f;
    float displayedCorrelation = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalysisPanel)
};