#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <vector>

class ScopeComponent final : public juce::Component,
                             private juce::Timer
{
public:
    ScopeComponent();

    void pushBuffer(const juce::AudioBuffer<float>& buffer,
                    int startSample,
                    int numSamples) noexcept;
    void setCapturePaused(bool shouldPause) noexcept;
    void setSampleRate(double newSampleRate) noexcept;
    void setTimeWindowMilliseconds(double newTimeWindowMilliseconds);
    void setVerticalZoom(float newZoom);
    void setTriggerEnabled(bool shouldTrigger);
    void setTriggerChannel(std::size_t channel);
    // Lane names; channels are inputs before the mixer and outputs after it.
    void setChannelNames(const juce::String& first, const juce::String& second);
    void clear();
    void paint(juce::Graphics& graphics) override;

private:
    friend class WorkshopComponentTests;
    void timerCallback() override;
    void paintLane(juce::Graphics& graphics, float labelLeft, juce::Rectangle<float> plotBounds,
                   std::size_t channel, juce::Colour colour, int firstSample,
                   int visibleSampleCount);
    void paintChannel(juce::Graphics& graphics, juce::Rectangle<float> plotBounds,
                      int firstSample, int visibleSampleCount, std::size_t channel,
                      juce::Colour colour);

    static constexpr int historyCapacity = 262'144;
    using StereoFrame = std::array<float, 2>;

    juce::AbstractFifo pendingFifo { historyCapacity };
    std::vector<StereoFrame> pendingSamples = std::vector<StereoFrame>(historyCapacity);
    std::vector<StereoFrame> history = std::vector<StereoFrame>(historyCapacity);
    std::atomic<bool> capturePaused { false };
    std::atomic<bool> clearRequested { false };
    std::atomic<double> sampleRate { 48'000.0 };
    int historyWritePosition = 0;
    int historySampleCount = 0;
    double timeWindowMilliseconds = 20.0;
    float verticalZoom = 1.0f;
    bool triggerEnabled = false;
    std::size_t triggerChannel = 0;
    std::array<juce::String, 2> channelNames { "CHANNEL 1", "CHANNEL 2" };
};
