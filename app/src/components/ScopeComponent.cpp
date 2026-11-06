#include "ScopeComponent.h"

#include "../LookAndFeel.h"

#include <algorithm>
#include <cmath>

using namespace theme;

ScopeComponent::ScopeComponent()
{
    setOpaque(true);
    startTimerHz(60);
}

void ScopeComponent::pushBuffer(const juce::AudioBuffer<float>& buffer,
                                int startSample,
                                int numSamples) noexcept
{
    if (capturePaused.load(std::memory_order_relaxed) || buffer.getNumChannels() == 0)
        return;

    const auto samplesToWrite = std::min(numSamples, pendingFifo.getFreeSpace());
    const auto* left = buffer.getReadPointer(0, startSample);
    const auto* right = buffer.getReadPointer(std::min(1, buffer.getNumChannels() - 1), startSample);

    pendingFifo.write(samplesToWrite).forEach(
        [this, left, right, sourceOffset = 0](int destinationIndex) mutable
        {
            pendingSamples[static_cast<std::size_t>(destinationIndex)] =
                { left[sourceOffset], right[sourceOffset] };
            ++sourceOffset;
        });
}

void ScopeComponent::setCapturePaused(bool shouldPause) noexcept
{
    capturePaused.store(shouldPause, std::memory_order_relaxed);
}

void ScopeComponent::setSampleRate(double newSampleRate) noexcept
{
    sampleRate.store(newSampleRate, std::memory_order_relaxed);
}

void ScopeComponent::setTimeWindowMilliseconds(double newTimeWindowMilliseconds)
{
    timeWindowMilliseconds = std::clamp(newTimeWindowMilliseconds, 2.0, 2'000.0);
    repaint();
}

void ScopeComponent::setVerticalZoom(float newZoom)
{
    verticalZoom = std::max(0.25f, newZoom);
    repaint();
}

void ScopeComponent::setTriggerEnabled(bool shouldTrigger)
{
    triggerEnabled = shouldTrigger;
    repaint();
}

void ScopeComponent::setTriggerChannel(std::size_t channel)
{
    triggerChannel = std::min(channel, std::size_t { 1 });
    repaint();
}

void ScopeComponent::setChannelNames(const juce::String& first, const juce::String& second)
{
    channelNames = { first, second };
    repaint();
}

void ScopeComponent::clear()
{
    clearRequested.store(true, std::memory_order_release);
}

void ScopeComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(ink);

    // Two stacked lanes, channel 1 above channel 2, on one shared time axis. Separate
    // lanes keep identical signals from hiding each other; the shared axis (and one
    // trigger) keeps their timing comparable, e.g. to see an inverted channel.
    const auto bounds = getLocalBounds().toFloat().reduced(4.0f);
    const auto plotArea = bounds.withTrimmedLeft(44.0f);
    constexpr auto laneGap = 6.0f;
    const auto laneHeight = (plotArea.getHeight() - laneGap) * 0.5f;
    const std::array<juce::Rectangle<float>, 2> lanes {{
        plotArea.withHeight(laneHeight),
        plotArea.withTrimmedTop(laneHeight + laneGap)
    }};
    const std::array<juce::Colour, 2> colours {{ signalRed, signalGreen }};

    const auto visibleSampleCount = std::min(
        historySampleCount,
        static_cast<int>(sampleRate.load(std::memory_order_relaxed)
                         * timeWindowMilliseconds / 1'000.0));

    const auto wrapHistoryIndex = [](int index)
    {
        return (index + historyCapacity) % historyCapacity;
    };
    auto firstSample = wrapHistoryIndex(historyWritePosition - visibleSampleCount);

    if (triggerEnabled && visibleSampleCount > 0 && historySampleCount > visibleSampleCount)
    {
        const auto preTriggerSamples = visibleSampleCount / 10;
        const auto newestTrigger = wrapHistoryIndex(firstSample + preTriggerSamples);
        const auto availableSearchSamples = historySampleCount - visibleSampleCount;
        const auto maximumSearchSamples = std::min(
            availableSearchSamples,
            juce::roundToInt(sampleRate.load(std::memory_order_relaxed)));

        for (int samplesBack = 0; samplesBack <= maximumSearchSamples; ++samplesBack)
        {
            const auto candidate = wrapHistoryIndex(newestTrigger - samplesBack);
            const auto previous = wrapHistoryIndex(candidate - 1);
            if (history[static_cast<std::size_t>(previous)][triggerChannel] <= 0.0f
                && history[static_cast<std::size_t>(candidate)][triggerChannel] > 0.0f)
            {
                firstSample = wrapHistoryIndex(candidate - preTriggerSamples);
                break;
            }
        }
    }

    for (std::size_t channel = 0; channel < lanes.size(); ++channel)
        paintLane(graphics, bounds.getX(), lanes[channel], channel, colours[channel],
                  firstSample, visibleSampleCount);
}

void ScopeComponent::paintLane(juce::Graphics& graphics, float labelLeft,
                               juce::Rectangle<float> plotBounds, std::size_t channel,
                               juce::Colour colour, int firstSample, int visibleSampleCount)
{
    // Scale: full scale at the lane edges (at 1x zoom) and silence at the centre.
    graphics.setColour(juce::Colours::white.withAlpha(0.03f));
    graphics.fillRect(plotBounds);
    graphics.setFont(juce::FontOptions(9.0f));
    for (int division = 0; division <= 2; ++division)
    {
        const auto y = plotBounds.getY() + plotBounds.getHeight() * division / 2.0f;
        graphics.setColour(juce::Colours::white.withAlpha(division == 1 ? 0.16f : 0.08f));
        graphics.drawHorizontalLine(juce::roundToInt(y), plotBounds.getX(), plotBounds.getRight());
        if (division == 2)
            continue;
        const auto amplitude = division == 1 ? 0.0f : 1.0f / verticalZoom;
        const auto scaleText = amplitude > 0.0f
            ? juce::String(juce::Decibels::gainToDecibels(amplitude), 1) + " dB"
            : juce::String("-inf dB");
        const auto labelY = std::clamp(y - 7.0f, plotBounds.getY(), plotBounds.getBottom() - 14.0f);
        graphics.setColour(textSecondary);
        graphics.drawText(scaleText, juce::Rectangle<float>(labelLeft, labelY, 40.0f, 14.0f),
                          juce::Justification::centredRight);
    }

    // Full scale (+/-1.0, 0 dBFS) is the lane edge at 1x zoom. Zoomed out, mark it,
    // shade beyond it and redraw the trace there in a warning colour: the output
    // device clips anything past full scale, even though the samples aren't clipped.
    const auto fullScaleOffset = plotBounds.getHeight() * 0.5f * verticalZoom;
    const auto showFullScale = fullScaleOffset < plotBounds.getHeight() * 0.5f - 1.0f;
    juce::RectangleList<int> beyondFullScale;
    if (showFullScale)
    {
        const auto top = plotBounds.getCentreY() - fullScaleOffset;
        const auto bottom = plotBounds.getCentreY() + fullScaleOffset;
        beyondFullScale.add(plotBounds.withBottom(top).toNearestInt());
        beyondFullScale.add(plotBounds.withTop(bottom).toNearestInt());
        graphics.setColour(overFullScale.withAlpha(0.07f));
        graphics.fillRectList(beyondFullScale);
        graphics.setColour(overFullScale.withAlpha(0.7f));
        const float dashes[] { 4.0f, 3.0f };
        for (const auto y : { top, bottom })
            graphics.drawDashedLine({ plotBounds.getX(), y, plotBounds.getRight(), y },
                                    dashes, 2, 1.0f);
    }

    if (visibleSampleCount > 0)
    {
        paintChannel(graphics, plotBounds, firstSample, visibleSampleCount, channel, colour);
        if (showFullScale)
        {
            juce::Graphics::ScopedSaveState state(graphics);
            graphics.reduceClipRegion(beyondFullScale);
            paintChannel(graphics, plotBounds, firstSample, visibleSampleCount, channel,
                         overFullScale);
        }
    }

    // Lane name in the trace's colour, marked when the zero trigger follows it.
    const juce::Font nameFont(juce::FontOptions(11.0f, juce::Font::bold));
    auto name = channelNames[channel];
    if (triggerEnabled && channel == triggerChannel)
        name << "  |  TRIGGER";
    const auto nameWidth = juce::GlyphArrangement::getStringWidth(nameFont, name);
    const auto nameBounds = juce::Rectangle<float>(plotBounds.getX() + 2.0f, plotBounds.getY() + 2.0f,
                                                   std::ceil(nameWidth) + 8.0f, 14.0f);
    graphics.setColour(ink.withAlpha(0.8f));
    graphics.fillRoundedRectangle(nameBounds, 2.0f);
    graphics.setColour(colour);
    graphics.setFont(nameFont);
    graphics.drawText(name, nameBounds.reduced(4.0f, 0.0f), juce::Justification::centredLeft);

    if (showFullScale)
    {
        const auto label = juce::Rectangle<float>(plotBounds.getRight() - 66.0f,
                                                  plotBounds.getCentreY() - fullScaleOffset - 13.0f,
                                                  64.0f, 12.0f);
        if (label.getY() >= plotBounds.getY())
        {
            graphics.setColour(ink.withAlpha(0.85f));
            graphics.fillRoundedRectangle(label, 2.0f);
            graphics.setColour(overFullScale);
            graphics.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            graphics.drawText("FULL SCALE", label, juce::Justification::centred);
        }
    }
}

void ScopeComponent::paintChannel(juce::Graphics& graphics, juce::Rectangle<float> plotBounds,
                                  int firstSample, int visibleSampleCount, std::size_t channel,
                                  juce::Colour colour)
{
    const auto pixelCount = std::max(1, juce::roundToInt(plotBounds.getWidth()));
    const auto sampleAt = [this, firstSample, channel](int offset)
    {
        return history[static_cast<std::size_t>((firstSample + offset) % historyCapacity)][channel];
    };
    const auto sampleToY = [plotBounds, zoom = verticalZoom](float sample)
    {
        return plotBounds.getCentreY() - sample * zoom * plotBounds.getHeight() * 0.5f;
    };

    graphics.setColour(colour);

    if (visibleSampleCount <= pixelCount)
    {
        juce::Path waveform;
        const auto denominator = static_cast<float>(std::max(1, visibleSampleCount - 1));

        for (int offset = 0; offset < visibleSampleCount; ++offset)
        {
            const auto x = plotBounds.getX()
                + static_cast<float>(offset) / denominator * plotBounds.getWidth();
            const auto y = sampleToY(sampleAt(offset));

            if (offset == 0)
                waveform.startNewSubPath(x, y);
            else
                waveform.lineTo(x, y);
        }

        juce::Graphics::ScopedSaveState state(graphics);
        graphics.reduceClipRegion(plotBounds.toNearestInt());
        graphics.strokePath(waveform,
                            juce::PathStrokeType(1.5f,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        return;
    }

    const auto samplesPerPixel = static_cast<float>(visibleSampleCount)
        / static_cast<float>(pixelCount);
    if (samplesPerPixel <= 8.0f)
    {
        juce::Path waveform;
        for (int pixel = 0; pixel < pixelCount; ++pixel)
        {
            const auto firstOffset = pixel * visibleSampleCount / pixelCount;
            const auto lastOffset = std::max(firstOffset + 1,
                                             (pixel + 1) * visibleSampleCount / pixelCount);
            auto sampleTotal = 0.0f;
            for (int offset = firstOffset; offset < lastOffset; ++offset)
                sampleTotal += sampleAt(offset);

            const auto average = sampleTotal / static_cast<float>(lastOffset - firstOffset);
            const auto x = plotBounds.getX()
                + static_cast<float>(pixel) / static_cast<float>(std::max(1, pixelCount - 1))
                    * plotBounds.getWidth();
            if (pixel == 0)
                waveform.startNewSubPath(x, sampleToY(average));
            else
                waveform.lineTo(x, sampleToY(average));
        }

        juce::Graphics::ScopedSaveState state(graphics);
        graphics.reduceClipRegion(plotBounds.toNearestInt());
        graphics.strokePath(waveform,
                            juce::PathStrokeType(1.5f,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        return;
    }

    auto previousTop = 0.0f;
    auto previousBottom = 0.0f;
    juce::Graphics::ScopedSaveState state(graphics);
    graphics.reduceClipRegion(plotBounds.toNearestInt());
    for (int pixel = 0; pixel < pixelCount; ++pixel)
    {
        const auto firstOffset = pixel * visibleSampleCount / pixelCount;
        const auto lastOffset = std::max(firstOffset + 1,
                                         (pixel + 1) * visibleSampleCount / pixelCount);
        auto minimum = sampleAt(firstOffset);
        auto maximum = minimum;

        for (int offset = firstOffset + 1; offset < lastOffset; ++offset)
        {
            const auto sample = sampleAt(offset);
            minimum = std::min(minimum, sample);
            maximum = std::max(maximum, sample);
        }

        const auto x = plotBounds.getX() + static_cast<float>(pixel);
        const auto top = sampleToY(maximum);
        const auto bottom = sampleToY(minimum);
        graphics.drawVerticalLine(juce::roundToInt(x), top, bottom);

        if (pixel > 0)
        {
            if (bottom < previousTop)
                graphics.drawLine(x - 1.0f, previousTop, x, bottom);
            else if (top > previousBottom)
                graphics.drawLine(x - 1.0f, previousBottom, x, top);
        }

        previousTop = top;
        previousBottom = bottom;
    }
}

void ScopeComponent::timerCallback()
{
    if (clearRequested.exchange(false, std::memory_order_acq_rel))
    {
        pendingFifo.read(pendingFifo.getNumReady()).forEach([](int) {});
        std::fill(history.begin(), history.end(), StereoFrame {});
        historyWritePosition = 0;
        historySampleCount = 0;
    }

    pendingFifo.read(pendingFifo.getNumReady()).forEach(
        [this](int sourceIndex)
        {
            history[static_cast<std::size_t>(historyWritePosition)] =
                pendingSamples[static_cast<std::size_t>(sourceIndex)];
            historyWritePosition = (historyWritePosition + 1) % historyCapacity;
            historySampleCount = std::min(historySampleCount + 1, historyCapacity);
        });

    repaint();
}
