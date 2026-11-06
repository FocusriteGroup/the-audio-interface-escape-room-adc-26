#include "SpectrumComponent.h"

#include "../LookAndFeel.h"

#include <algorithm>
#include <cmath>

using namespace theme;

SpectrumComponent::SpectrumComponent()
{
    setOpaque(true);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    clear();
    startTimerHz(30);
}

void SpectrumComponent::clear()
{
    fifo.read(fifo.getNumReady()).forEach([](int) {});
    collectedSamples = 0;
    fftData.fill(0.0f);
    magnitudes.fill(-100.0f);
    repaint();
}

void SpectrumComponent::setActive(bool shouldBeActive)
{
    if (active == shouldBeActive)
        return;

    active = shouldBeActive;
    if (!active)
        clear();
}

void SpectrumComponent::pushSamples(std::span<const float> samples) noexcept
{
    const auto samplesToWrite = std::min(static_cast<int>(samples.size()), fifo.getFreeSpace());
    fifo.write(samplesToWrite).forEach(
        [this, samples, sourceIndex = std::size_t { 0 }](int destinationIndex) mutable
        {
            pendingSamples[static_cast<std::size_t>(destinationIndex)] = samples[sourceIndex++];
        });
}

void SpectrumComponent::setSampleRate(double newSampleRate) noexcept
{
    sampleRate.store(newSampleRate, std::memory_order_relaxed);
}

void SpectrumComponent::timerCallback()
{
    if (!active)
    {
        clear();
        return;
    }

    const auto available = std::min(fifo.getNumReady(), fftSize - collectedSamples);
    fifo.read(available).forEach([this](int sourceIndex)
    {
        fftData[static_cast<std::size_t>(collectedSamples++)] =
            pendingSamples[static_cast<std::size_t>(sourceIndex)];
    });

    if (collectedSamples < fftSize)
        return;

    std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable(fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());
    for (std::size_t bin = 0; bin < magnitudes.size(); ++bin)
        magnitudes[bin] = juce::Decibels::gainToDecibels(
            2.0f * fftData[bin] / static_cast<float>(fftSize), -100.0f);

    collectedSamples = 0;
    repaint();
}

void SpectrumComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(ink);
    const auto bounds = getLocalBounds().toFloat().reduced(4.0f);
    const auto plotBounds = bounds.withTrimmedLeft(40.0f).withTrimmedBottom(16.0f);
    graphics.setFont(juce::FontOptions(9.0f));
    for (int division = 0; division <= 4; ++division)
    {
        const auto decibels = -25 * division;
        const auto y = plotBounds.getY() + plotBounds.getHeight() * division / 4.0f;
        graphics.setColour(juce::Colours::white.withAlpha(division == 0 ? 0.12f : 0.08f));
        graphics.drawHorizontalLine(juce::roundToInt(y), plotBounds.getX(), plotBounds.getRight());
        graphics.setColour(textSecondary);
        const auto labelY = std::clamp(y - 7.0f, bounds.getY(), bounds.getBottom() - 14.0f);
        graphics.drawText(juce::String(decibels) + " dB",
                          juce::Rectangle<float>(bounds.getX(), labelY, 36.0f, 14.0f),
                          juce::Justification::centredRight);
    }

    const auto rate = sampleRate.load(std::memory_order_relaxed);
    const auto maximumFrequency = std::min(20'000.0, rate * 0.5);
    const auto minimumFrequency = 20.0;
    const auto frequencyToX = [plotBounds, minimumFrequency, maximumFrequency](double frequency)
    {
        const auto normalized = std::log(frequency / minimumFrequency)
            / std::log(maximumFrequency / minimumFrequency);
        return plotBounds.getX() + static_cast<float>(normalized) * plotBounds.getWidth();
    };

    juce::Path spectrumPath;
    bool started = false;
    for (std::size_t bin = 1; bin < magnitudes.size(); ++bin)
    {
        const auto frequency = static_cast<double>(bin) * rate / fftSize;
        if (frequency < minimumFrequency || frequency > maximumFrequency)
            continue;

        const auto x = frequencyToX(frequency);
        const auto y = juce::jmap(std::clamp(magnitudes[bin], -100.0f, 0.0f),
                                  -100.0f, 0.0f, plotBounds.getBottom(), plotBounds.getY());
        if (!started)
        {
            spectrumPath.startNewSubPath(x, y);
            started = true;
        }
        else
        {
            spectrumPath.lineTo(x, y);
        }
    }

    graphics.setColour(signalGreen);
    graphics.strokePath(spectrumPath, juce::PathStrokeType(1.5f));
    graphics.setColour(textSecondary);
    graphics.setFont(juce::FontOptions(10.0f));
    const auto frequencyLabels = plotBounds.toNearestInt()
        .withY(juce::roundToInt(plotBounds.getBottom())).withHeight(16);
    graphics.drawText("20 Hz", frequencyLabels, juce::Justification::centredLeft);
    graphics.drawText(juce::String(maximumFrequency / 1'000.0, 0) + " kHz",
                      frequencyLabels, juce::Justification::centredRight);

    if (mouseIsOver && plotBounds.contains(mousePosition))
    {
        const auto normalizedX = (mousePosition.x - plotBounds.getX()) / plotBounds.getWidth();
        const auto pointerFrequency = minimumFrequency
            * std::pow(maximumFrequency / minimumFrequency, normalizedX);
        const auto pointerBin = std::clamp(
            juce::roundToInt(pointerFrequency * fftSize / rate),
            1, static_cast<int>(magnitudes.size()) - 2);
        const auto searchRadius = std::max(2, juce::roundToInt(pointerBin * 0.025));
        const auto firstBin = std::max(1, pointerBin - searchRadius);
        const auto lastBin = std::min(static_cast<int>(magnitudes.size()) - 2,
                                      pointerBin + searchRadius);
        auto peakBin = firstBin;
        for (int bin = firstBin + 1; bin <= lastBin; ++bin)
        {
            if (magnitudes[static_cast<std::size_t>(bin)]
                > magnitudes[static_cast<std::size_t>(peakBin)])
                peakBin = bin;
        }

        const auto leftMagnitude = magnitudes[static_cast<std::size_t>(peakBin - 1)];
        const auto centreMagnitude = magnitudes[static_cast<std::size_t>(peakBin)];
        const auto rightMagnitude = magnitudes[static_cast<std::size_t>(peakBin + 1)];
        const auto curvature = leftMagnitude - 2.0f * centreMagnitude + rightMagnitude;
        const auto binOffset = std::abs(curvature) > 0.0001f
            ? std::clamp(0.5f * (leftMagnitude - rightMagnitude) / curvature, -0.5f, 0.5f)
            : 0.0f;
        const auto frequency = (static_cast<double>(peakBin) + binOffset) * rate / fftSize;
        const auto decibels = centreMagnitude - 0.25f
            * (leftMagnitude - rightMagnitude) * binOffset;
        const auto peakX = frequencyToX(frequency);
        const auto peakY = juce::jmap(std::clamp(decibels, -100.0f, 0.0f),
                                      -100.0f, 0.0f,
                                      plotBounds.getBottom(), plotBounds.getY());

        graphics.setColour(textPrimary.withAlpha(0.45f));
        graphics.drawVerticalLine(juce::roundToInt(peakX),
                                  plotBounds.getY(), plotBounds.getBottom());
        graphics.drawHorizontalLine(juce::roundToInt(peakY),
                                    plotBounds.getX(), plotBounds.getRight());

        const auto frequencyText = frequency >= 1'000.0
            ? juce::String(frequency / 1'000.0, 2) + " kHz"
            : juce::String(frequency, 0) + " Hz";
        const auto readout = frequencyText + "  " + juce::String(decibels, 1) + " dB";
        auto readoutBounds = juce::Rectangle<float>(peakX + 8.0f, peakY - 25.0f,
                                                     112.0f, 20.0f);
        if (readoutBounds.getRight() > plotBounds.getRight())
            readoutBounds.setX(peakX - readoutBounds.getWidth() - 8.0f);
        if (readoutBounds.getY() < plotBounds.getY())
            readoutBounds.setY(peakY + 8.0f);

        graphics.setColour(panelRaised.withAlpha(0.94f));
        graphics.fillRoundedRectangle(readoutBounds, 3.0f);
        graphics.setColour(panelOutline);
        graphics.drawRoundedRectangle(readoutBounds, 3.0f, 1.0f);
        graphics.setColour(textPrimary);
        graphics.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        graphics.drawText(readout, readoutBounds.toNearestInt().reduced(5, 1),
                          juce::Justification::centred);
    }
}

void SpectrumComponent::mouseMove(const juce::MouseEvent& event)
{
    mousePosition = event.position;
    mouseIsOver = true;
    repaint();
}

void SpectrumComponent::mouseExit(const juce::MouseEvent&)
{
    mouseIsOver = false;
    repaint();
}
