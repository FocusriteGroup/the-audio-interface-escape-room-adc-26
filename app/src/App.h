#pragma once

#include "panels/AnalysisPanel.h"
#include "panels/DeviceDiagnosticsPanel.h"
#include "components/EqComponent.h"
#include "Header.h"
#include "panels/InputPanel.h"
#include "panels/OutputPanel.h"
#include "panels/RoutingPanel.h"
#include "LookAndFeel.h"
#include "components/LevelMeterComponent.h"

#include <JuceHeader.h>

#include <AudioProcessor.h>
#include <LoopbackCapture.h>
#include <state-exchange/StateExchange.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

class App final : public juce::AudioAppComponent,
                            private juce::Timer
{
public:
    explicit App(bool openAudioDevice = true);
    ~App() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    friend class WorkshopComponentTests;
    static constexpr std::size_t maximumBlockSize = 8'192;

    using SourceType = ::SourceType;

    struct MeterBallistics
    {
        float smoothedPeak = 0.0f;
        float heldPeak = 0.0f;
        std::uint32_t holdUntilMs = 0;
    };

    void timerCallback() override;
    void loadBundledInput();
    void savePreset();
    void loadPreset();
    void updateAppStateControls();
    void wireChildComponents();
    void showPresetError(const juce::String& message);
    void publishAppState();
    static void accumulatePeak(std::atomic<float>& destination, float peak) noexcept;
    static void updateMeter(MeterBallistics& ballistics,
                            float measuredPeak,
                            std::uint32_t now,
                            bool playing);
    static juce::String formatDecibels(float linearLevel);
    juce::Rectangle<int> signalGraphBounds() const;
    juce::Rectangle<int> inputNodeBounds(std::size_t inputIndex) const;
    juce::Rectangle<int> routingNodeBounds() const;
    juce::Rectangle<int> outputNodeBounds(std::size_t outputIndex) const;

    AppLookAndFeel appLookAndFeel;
    Header header;
    std::array<InputPanel, audio_core::inputCount> inputPanels;
    std::array<EqComponent, audio_core::inputCount> eqPanels;
    RoutingPanel routingPanel;
    std::array<OutputPanel, audio_core::outputCount> outputPanels;
    juce::TextButton resetSourcesButton { "RESET SOURCES" };
    juce::ToggleButton monoAuditionButton { "MONO AUDITION" };
    AnalysisPanel analysisPanel;
    DeviceDiagnosticsPanel deviceDiagnostics { deviceManager };

    juce::AudioBuffer<float> sourceAudio;
    std::array<std::atomic<SourceType>, 2> sourceTypes {{ SourceType::sine,
                                                          SourceType::sine }};
    std::array<std::atomic<double>, 2> sourceFrequencies;
    std::array<std::atomic<float>, 2> sourceAmplitudes;
    std::atomic<bool> resetSourcesRequested { false };
    std::atomic<bool> monoAudition { false };
    std::array<double, 2> sourcePositions {};
    std::array<double, 2> sinePhases {};
    std::array<std::uint32_t, 2> noiseStates {{ 0x6d2b79f5u, 0x1b56c4e9u }};
    std::array<std::uint64_t, 2> impulseSampleCounters {};
    double sourceSampleRate = 48'000.0;
    std::unique_ptr<juce::FileChooser> savePresetChooser;
    std::unique_ptr<juce::FileChooser> loadPresetChooser;
    audio_core::AppState appState;
    audio_core::AppState comparisonState;
    audio_core::AudioProcessor audioProcessor;
    audio_core::StateExchange appStateExchange;
    bool statePublicationPending = false;
    audio_core::LoopbackCapture loopbackCapture { 192'000 };
    std::atomic<bool> captureRequested { false };
    std::atomic<bool> replayRequested { false };
    std::atomic<float> capturePeak { 0.0f };
    std::atomic<float> pendingCapturePeak { 0.0f };
    MeterBallistics captureMeterBallistics;
    std::atomic<std::size_t> captureFrames { 0 };
    std::atomic<std::size_t> captureClips { 0 };
    bool wasCapturing = false;
    bool wasReplaying = false;
    audio_core::ProcessorState pendingProcessorState;
    std::array<std::array<float, maximumBlockSize>, audio_core::inputCount> inputScratch;
    std::array<std::array<std::array<float, maximumBlockSize>, audio_core::inputCount>, 3>
        inputAnalysisScratch;
    std::array<std::array<std::array<float, maximumBlockSize>, audio_core::outputCount>, 2>
        outputAnalysisScratch;
    juce::AudioBuffer<float> selectedAnalysisBuffer {
        static_cast<int>(audio_core::outputCount), static_cast<int>(maximumBlockSize) };
    std::atomic<double> currentSampleRate { 48'000.0 };
    std::atomic<bool> isPlaying { false };
    std::array<std::atomic<float>, audio_core::inputCount> pendingInputPeaks {{ 0.0f, 0.0f }};
    std::array<std::atomic<float>, audio_core::outputCount> pendingOutputPeaks {{ 0.0f, 0.0f }};
    std::array<MeterBallistics, audio_core::inputCount> inputMeterBallistics;
    std::array<MeterBallistics, audio_core::outputCount> outputMeterBallistics;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(App)
};
