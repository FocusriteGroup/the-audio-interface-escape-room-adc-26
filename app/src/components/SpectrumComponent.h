#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <span>

class SpectrumComponent final : public juce::Component,
                                private juce::Timer
{
public:
    SpectrumComponent();

    void clear();
    void setActive(bool shouldBeActive);
    void pushSamples(std::span<const float> samples) noexcept;
    void setSampleRate(double newSampleRate) noexcept;
    void paint(juce::Graphics& graphics) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

private:
    friend class WorkshopComponentTests;
    void timerCallback() override;

    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int fifoCapacity = fftSize * 8;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window {
        fftSize, juce::dsp::WindowingFunction<float>::hann
    };
    juce::AbstractFifo fifo { fifoCapacity };
    std::array<float, fifoCapacity> pendingSamples {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2> magnitudes {};
    int collectedSamples = 0;
    bool active = false;
    std::atomic<double> sampleRate { 48'000.0 };
    juce::Point<float> mousePosition;
    bool mouseIsOver = false;
};
