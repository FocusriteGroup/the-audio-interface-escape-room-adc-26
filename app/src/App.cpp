#include "App.h"

#include "LookAndFeel.h"

#include <Analysis.h>
#include <logging/DiagnosticLog.h>
#include <audio-processing/OutputBuffer.h>
#include <ProcessorSetup.h>
#include <app-state/AppState.h>
#include <mission-setup/MissionSetup.h>
#include <EscapeRoomMission.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

using namespace theme;

namespace
{
const auto initialSetup = audio_core::setupForMission(ESCAPE_ROOM_MISSION_NAME);
}

App::App(bool openAudioDevice)
    : inputPanels {{ InputPanel(0, initialSetup.frequencies[0], initialSetup.amplitudes[0]),
                     InputPanel(1, initialSetup.frequencies[1], initialSetup.amplitudes[1]) }},
      outputPanels {{ OutputPanel(0), OutputPanel(1) }}
{
    setLookAndFeel(&appLookAndFeel);
    appState = initialSetup.product;

    comparisonState = appState;
    deviceDiagnostics.onMarkState = [this]
    {
        comparisonState = appState;
        deviceDiagnostics.setAppStateView(appState.serialize(),
                                          audio_core::AppState::diff(comparisonState, appState));
    };

    for (std::size_t sourceIndex = 0; sourceIndex < audio_core::inputCount; ++sourceIndex)
    {
        sourceFrequencies[sourceIndex].store(inputPanels[sourceIndex].getFrequency());
        sourceAmplitudes[sourceIndex].store(
            static_cast<float>(inputPanels[sourceIndex].getAmplitude()));
    }

    wireChildComponents();
    addAndMakeVisible(header);
    for (std::size_t input = 0; input < audio_core::inputCount; ++input)
    {
        addAndMakeVisible(inputPanels[input]);
        addAndMakeVisible(eqPanels[input]);
    }
    for (std::size_t output = 0; output < audio_core::outputCount; ++output)
        addAndMakeVisible(outputPanels[output]);
    addAndMakeVisible(routingPanel);
    addAndMakeVisible(resetSourcesButton);
    addAndMakeVisible(monoAuditionButton);
    addAndMakeVisible(analysisPanel);
    addAndMakeVisible(deviceDiagnostics);

    updateAppStateControls();
    loadBundledInput();
    publishAppState();
    setSize(1440, 920);
    if (openAudioDevice)
    {
        setAudioChannels(0, 2);
        // Without a request JUCE picks the device's smallest buffer (16 samples on
        // CoreAudio), where Debug builds can underrun regardless of the exercise
        // fault, and the device's own rate (often 44.1 kHz on a Mac). The slides
        // and reference scenarios assume 48 kHz / 128. JUCE falls back to the
        // nearest rate and size the device supports.
        auto setup = deviceManager.getAudioDeviceSetup();
        setup.sampleRate = 48'000.0;
        setup.bufferSize = 128;
        if (deviceManager.setAudioDeviceSetup(setup, true).isNotEmpty())
            audio_core::logDiagnostic("could not apply 48 kHz / 128 samples");
    }
    startTimerHz(30);
}

void App::wireChildComponents()
{
    header.onSavePreset = [this] { savePreset(); };
    header.onLoadPreset = [this] { loadPreset(); };
    header.onPlayToggled = [this](bool shouldPlay)
    {
        isPlaying.store(shouldPlay);
        repaint();
    };
    header.onCaptureToggled = [this](bool enabled)
    {
        captureRequested.store(enabled);
        if (enabled)
        {
            replayRequested.store(false);
            header.setReplayToggled(false);
        }
    };
    header.onReplayToggled = [this](bool enabled)
    {
        replayRequested.store(enabled);
        if (enabled)
        {
            captureRequested.store(false);
            header.setCaptureToggled(false);
        }
    };

    resetSourcesButton.setTooltip("Restore this exercise's sources and restart both together");
    resetSourcesButton.onClick = [this]
    {
        for (auto& inputPanel : inputPanels)
            inputPanel.resetSource();
        resetSourcesRequested.store(true);
    };

    monoAuditionButton.setTooltip("Sum both outputs to mono, (L+R)/2, to check mono compatibility");
    monoAuditionButton.onClick = [this]
    {
        monoAudition.store(monoAuditionButton.getToggleState());
        analysisPanel.resetMeasurements();
    };

    for (std::size_t inputIndex = 0; inputIndex < audio_core::inputCount; ++inputIndex)
    {
        auto& input = inputPanels[inputIndex];
        input.onSourceTypeChanged = [this, inputIndex](SourceType type)
        {
            sourceTypes[inputIndex].store(type, std::memory_order_relaxed);
        };
        input.onFrequencyChanged = [this, inputIndex](double frequency)
        {
            sourceFrequencies[inputIndex].store(frequency, std::memory_order_relaxed);
        };
        input.onAmplitudeChanged = [this, inputIndex](double amplitude)
        {
            sourceAmplitudes[inputIndex].store(static_cast<float>(amplitude),
                                            std::memory_order_relaxed);
        };
        input.onGainChanged = [this, inputIndex](float gainDecibels)
        {
            appState.inputs[inputIndex].gainDecibels = gainDecibels;
            publishAppState();
        };
        input.onMuteChanged = [this, inputIndex](bool muted)
        {
            appState.inputs[inputIndex].muted = muted;
            publishAppState();
        };
        input.onPhaseChanged = [this, inputIndex](bool inverted)
        {
            appState.inputs[inputIndex].phaseInverted = inverted;
            publishAppState();
        };

        eqPanels[inputIndex].onGainChanged = [this, inputIndex](std::size_t band, float gain)
        {
            auto& eq = appState.inputs[inputIndex].equalizer;
            if (band == 0)
                eq.lowGainDecibels = gain;
            else if (band == 1)
                eq.midGainDecibels = gain;
            else
                eq.highGainDecibels = gain;
            publishAppState();
        };

    }

    for (std::size_t outputIndex = 0; outputIndex < audio_core::outputCount; ++outputIndex)
    {
        auto& output = outputPanels[outputIndex];
        output.onGainChanged = [this, outputIndex](float gainDecibels)
        {
            appState.outputs[outputIndex].gainDecibels = gainDecibels;
            publishAppState();
        };
        output.onMuteChanged = [this, outputIndex](bool muted)
        {
            appState.outputs[outputIndex].muted = muted;
            publishAppState();
        };
    }

    routingPanel.onRouteChanged = [this](std::size_t outputIndex, std::size_t inputIndex, bool enabled)
    {
        appState.routing[outputIndex][inputIndex] = enabled ? 1.0f : 0.0f;
        publishAppState();
        repaint();
    };
}

App::~App()
{
    shutdownAudio();
    setLookAndFeel(nullptr);
}

void App::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate.store(sampleRate);
    audioProcessor.prepare(sampleRate);
    audioProcessor.setProcessorState(audio_core::prepareForSampleRate(appState, sampleRate));
    loopbackCapture.reset();
    capturePeak.store(0.0f);
    captureFrames.store(0);
    captureClips.store(0);
    replayRequested.store(false);
    wasCapturing = false;
    wasReplaying = false;
    analysisPanel.setSampleRate(sampleRate);
    sourcePositions = {};
    sinePhases = {};
    impulseSampleCounters = {};
    noiseStates = {{ 0x6d2b79f5u, 0x1b56c4e9u }};
    audio_core::logDiagnosticValue("audio sample rate", sampleRate);
    audio_core::logDiagnosticValue("audio buffer size", samplesPerBlockExpected);
}

void App::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    const auto callbackStartedAt = juce::Time::getHighResolutionTicks();
    const auto finishCallback = [this, callbackStartedAt, samples = bufferToFill.numSamples]
    {
        const auto elapsedTicks = juce::Time::getHighResolutionTicks() - callbackStartedAt;
        const auto elapsedMilliseconds = 1'000.0
            * juce::Time::highResolutionTicksToSeconds(elapsedTicks);
        deviceDiagnostics.publishAudioCallback(
            elapsedMilliseconds, samples, currentSampleRate.load(std::memory_order_relaxed));
    };

    bufferToFill.clearActiveBufferRegion();
    if (bufferToFill.numSamples > static_cast<int>(maximumBlockSize)
        || bufferToFill.buffer->getNumChannels() < static_cast<int>(audio_core::outputCount))
    {
        finishCallback();
        return;
    }

    if (appStateExchange.consumeLatest(pendingProcessorState))
        audioProcessor.setProcessorState(pendingProcessorState);

    if (resetSourcesRequested.exchange(false))
    {
        sourcePositions = {};
        sinePhases = {};
        impulseSampleCounters = {};
        noiseStates = {{ 0x6d2b79f5u, 0x1b56c4e9u }};
    }

    if (!isPlaying.load() && !replayRequested.load())
    {
        deviceDiagnostics.publishClipCount(0);
        analysisPanel.pushOscilloscopeBuffer(*bufferToFill.buffer,
                                             bufferToFill.startSample,
                                             bufferToFill.numSamples);
        finishCallback();
        return;
    }

    const auto sourceLength = sourceAudio.getNumSamples();
    const auto deviceSampleRate = currentSampleRate.load(std::memory_order_relaxed);
    const auto sourceIncrement = sourceSampleRate / deviceSampleRate;
    const auto impulsePeriod = static_cast<std::uint64_t>(std::max(1.0, deviceSampleRate));
    std::array<double, 2> frequencies;
    std::array<float, 2> amplitudes;
    std::array<SourceType, 2> types;
    for (std::size_t input = 0; input < audio_core::inputCount; ++input)
    {
        frequencies[input] = std::min(sourceFrequencies[input].load(std::memory_order_relaxed),
                                         0.49 * deviceSampleRate);
        amplitudes[input] = sourceAmplitudes[input].load(std::memory_order_relaxed);
        types[input] = sourceTypes[input].load(std::memory_order_relaxed);
    }
    // Inputs set to the same source (and, for sines, the same frequency) stay in
    // step however the settings were reached, so matching settings always give
    // identical signals. Noise keeps its own state on each input.
    if (types[0] == types[1]
        && (types[0] != SourceType::sine || juce::exactlyEqual(frequencies[0], frequencies[1])))
    {
        sinePhases[1] = sinePhases[0];
        sourcePositions[1] = sourcePositions[0];
        impulseSampleCounters[1] = impulseSampleCounters[0];
    }
    constexpr auto twoPi = 2.0 * std::numbers::pi;
    for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
    {
        for (std::size_t input = 0; input < audio_core::inputCount; ++input)
        {
            auto value = 0.0f;
            switch (types[input])
            {
                case SourceType::bundledWav:
                    if (sourceLength > 0)
                    {
                        const auto firstSourceSample = static_cast<int>(sourcePositions[input]);
                        const auto nextSourceSample = (firstSourceSample + 1) % sourceLength;
                        const auto fraction = static_cast<float>(
                            sourcePositions[input] - firstSourceSample);
                        const auto first = sourceAudio.getSample(0, firstSourceSample);
                        const auto next = sourceAudio.getSample(0, nextSourceSample);
                        value = amplitudes[input] * (first + fraction * (next - first));
                        sourcePositions[input] += sourceIncrement;
                        if (sourcePositions[input] >= sourceLength)
                            sourcePositions[input] = std::fmod(
                                sourcePositions[input], static_cast<double>(sourceLength));
                    }
                    break;

                case SourceType::sine:
                    value = amplitudes[input]
                        * static_cast<float>(std::sin(sinePhases[input]));
                    sinePhases[input] += twoPi * frequencies[input] / deviceSampleRate;
                    if (sinePhases[input] >= twoPi)
                        sinePhases[input] -= twoPi;
                    break;

                case SourceType::impulse:
                    if (impulseSampleCounters[input] == 0)
                        value = amplitudes[input];
                    if (++impulseSampleCounters[input] >= impulsePeriod)
                        impulseSampleCounters[input] = 0;
                    break;

                case SourceType::noise:
                {
                    auto& state = noiseStates[input];
                    state ^= state << 13;
                    state ^= state >> 17;
                    state ^= state << 5;
                    const auto normalized = 2.0f * static_cast<float>(state)
                            / static_cast<float>(std::numeric_limits<std::uint32_t>::max())
                        - 1.0f;
                    value = amplitudes[input] * normalized;
                    break;
                }

                case SourceType::silence:
                    break;
            }
            inputScratch[input][static_cast<std::size_t>(sample)] = value;
        }
    }

    const auto samples = static_cast<std::size_t>(bufferToFill.numSamples);
    std::array<std::span<const float>, audio_core::inputCount> inputs;
    for (std::size_t input = 0; input < audio_core::inputCount; ++input)
        inputs[input] = std::span<const float>(inputScratch[input].data(), samples);
    std::array<std::span<float>, audio_core::outputCount> outputs;
    for (std::size_t output = 0; output < audio_core::outputCount; ++output)
        outputs[output] = std::span<float>(bufferToFill.buffer->getWritePointer(
            static_cast<int>(output), bufferToFill.startSample), samples);
    const auto makeInputStageViews = [this, samples](std::size_t stage)
    {
        std::array<std::span<float>, audio_core::inputCount> views;
        for (std::size_t input = 0; input < audio_core::inputCount; ++input)
            views[input] = std::span<float>(inputAnalysisScratch[stage][input].data(), samples);
        return views;
    };
    const auto makeOutputStageViews = [this, samples](std::size_t stage)
    {
        std::array<std::span<float>, audio_core::outputCount> views;
        for (std::size_t output = 0; output < audio_core::outputCount; ++output)
            views[output] = std::span<float>(outputAnalysisScratch[stage][output].data(), samples);
        return views;
    };
    audio_core::AnalysisBufferViews analysisBuffers {
        makeInputStageViews(0), makeInputStageViews(1), makeInputStageViews(2),
        makeOutputStageViews(0), makeOutputStageViews(1)
    };
    audio_core::AnalysisTaps productAnalysis;
    audioProcessor.process(inputs, analysisBuffers.output, productAnalysis, &analysisBuffers);
    for (std::size_t output = 0; output < audio_core::outputCount; ++output)
    {
        audio_core::copyOutputBlock(analysisBuffers.output[output], outputs[output]);
        std::copy(outputs[output].begin(), outputs[output].end(),
                  analysisBuffers.output[output].begin());
        productAnalysis.outputPeak[output] = audio_core::analyze(outputs[output]).peak;
    }

    const auto capturing = captureRequested.load();
    const auto replaying = replayRequested.load() && !capturing;
    if (capturing)
    {
        if (!wasCapturing)
        {
            loopbackCapture.reset();
            capturePeak.store(0.0f);
            captureClips.store(0);
        }
        const auto before = loopbackCapture.frameCount();
        std::array<std::span<const float>, audio_core::outputCount> captureBus;
        for (std::size_t output = 0; output < audio_core::outputCount; ++output)
            captureBus[output] = outputs[output];
        loopbackCapture.append(captureBus);
        captureFrames.store(loopbackCapture.frameCount());
        // Every block while recording, including once the capture is full.
        captureClips.store(audio_core::updateCaptureClipCount(
            captureClips.load(), loopbackCapture, loopbackCapture.frameCount() - before));
        if (loopbackCapture.frameCount() > before)
            for (std::size_t channel = 0;
                 channel < std::min(audio_core::inputCount, audio_core::outputCount); ++channel)
            {
                const auto meterPeak = audio_core::recordingMeterPeak(inputs[channel],
                                                                      outputs[channel]);
                accumulatePeak(capturePeak, meterPeak);
                accumulatePeak(pendingCapturePeak, meterPeak);
            }
    }
    if (replaying)
    {
        if (!wasReplaying)
            loopbackCapture.rewind();
        loopbackCapture.replay(outputs);
        for (std::size_t output = 0; output < audio_core::outputCount; ++output)
        {
            std::copy(outputs[output].begin(), outputs[output].end(),
                      analysisBuffers.output[output].begin());
            productAnalysis.outputPeak[output] = audio_core::analyze(outputs[output]).peak;
        }
    }
    wasCapturing = capturing;
    wasReplaying = replaying;

    const auto outputCorrelation = audio_core::correlation(outputs[0], outputs[1]);
    analysisPanel.publishCorrelation(outputCorrelation);
    if (monoAudition.load(std::memory_order_relaxed))
    {
        for (std::size_t sample = 0; sample < samples; ++sample)
        {
            const auto mono = 0.5f * (outputs[0][sample] + outputs[1][sample]);
            outputs[0][sample] = mono;
            outputs[1][sample] = mono;
            outputAnalysisScratch[1][0][sample] = mono;
            outputAnalysisScratch[1][1][sample] = mono;
        }
    }
    for (std::size_t output = 0; output < audio_core::outputCount; ++output)
    {
        productAnalysis.outputPeak[output] = audio_core::analyze(outputs[output]).peak;
        accumulatePeak(pendingOutputPeaks[output], productAnalysis.outputPeak[output]);
    }
    for (std::size_t input = 0; input < audio_core::inputCount; ++input)
        accumulatePeak(pendingInputPeaks[input], productAnalysis.inputPeak[input]);

    const auto tapIndex = analysisPanel.selectedTapIndex();
    const auto inputTap = tapIndex < 3;
    const auto selectedTapChannels = inputTap ? audio_core::inputCount : audio_core::outputCount;
    const auto channelIndex = std::min(analysisPanel.selectedChannelIndex(),
                                       selectedTapChannels - 1);
    const auto selectedSamples = inputTap
        ? std::span<const float>(inputAnalysisScratch[tapIndex][channelIndex].data(), samples)
        : std::span<const float>(outputAnalysisScratch[tapIndex - 3][channelIndex].data(), samples);
    selectedAnalysisBuffer.clear();
    for (std::size_t channel = 0; channel < selectedTapChannels; ++channel)
    {
        const auto* channelData = inputTap
            ? inputAnalysisScratch[tapIndex][channel].data()
            : outputAnalysisScratch[tapIndex - 3][channel].data();
        selectedAnalysisBuffer.copyFrom(static_cast<int>(channel), 0,
            channelData, bufferToFill.numSamples);
    }
    const auto metrics = audio_core::analyze(selectedSamples);
    analysisPanel.publishPeak(metrics.peak);
    analysisPanel.publishRms(metrics.rootMeanSquare);
    deviceDiagnostics.publishClipCount(metrics.clippedSamples);
    analysisPanel.pushOscilloscopeBuffer(selectedAnalysisBuffer, 0, bufferToFill.numSamples);
    analysisPanel.pushSpectrumSamples(selectedSamples);

    // Ear protection: turn down only what reaches the speakers. The recording, meters
    // and analysis above all see the full-level signal.
    if (!juce::exactlyEqual(initialSetup.listeningGain, 1.0f))
        for (const auto output : outputs)
            for (auto& sample : output)
                sample *= initialSetup.listeningGain;
    finishCallback();
}

void App::releaseResources()
{
}

void App::accumulatePeak(std::atomic<float>& destination, float peak) noexcept
{
    auto current = destination.load(std::memory_order_relaxed);
    while (peak > current
           && !destination.compare_exchange_weak(current, peak,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed))
    {
    }
}

void App::updateMeter(MeterBallistics& ballistics,
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

juce::String App::formatDecibels(float linearLevel)
{
    if (linearLevel <= 0.0f)
        return "  -inf dB";
    const auto decibels = juce::Decibels::gainToDecibels(linearLevel, -60.0f);
    return juce::String::formatted("%6.1f dB", static_cast<double>(decibels));
}

void App::paint(juce::Graphics& graphics)
{
    graphics.fillAll(ink);

    const auto graph = signalGraphBounds();
    const auto sourceWidth = 140;
    const auto sourceX = graph.getX() + 4;
    const auto laneCentre = [graph](std::size_t index)
    {
        return graph.getY() + 92 + static_cast<int>(index) * 190;
    };

    auto drawNode = [&graphics](juce::Rectangle<int> bounds, float cornerRadius = 7.0f)
    {
        graphics.setColour(panel);
        graphics.fillRoundedRectangle(bounds.toFloat(), cornerRadius);
        graphics.setColour(panelOutline);
        graphics.drawRoundedRectangle(bounds.toFloat(), cornerRadius, 1.0f);
    };

    const auto input1 = inputNodeBounds(0);
    const auto input2 = inputNodeBounds(1);
    const auto matrix = routingNodeBounds();
    const auto output1 = outputNodeBounds(0);
    const auto output2 = outputNodeBounds(1);
    const auto source1 = juce::Rectangle<int>(sourceX, input1.getY(), sourceWidth, 172);
    const auto source2 = juce::Rectangle<int>(sourceX, input2.getY(), sourceWidth, 172);
    const auto leftCentre = [](juce::Rectangle<int> bounds)
    {
        return juce::Point<float>(static_cast<float>(bounds.getX()),
                                  static_cast<float>(bounds.getCentreY()));
    };
    const auto rightCentre = [](juce::Rectangle<int> bounds)
    {
        return juce::Point<float>(static_cast<float>(bounds.getRight()),
                                  static_cast<float>(bounds.getCentreY()));
    };

    auto drawCable = [&graphics](juce::Point<float> from,
                                 juce::Point<float> to,
                                 juce::Colour colour,
                                 float thickness)
    {
        juce::Path cable;
        const auto controlOffset = std::max(30.0f, (to.x - from.x) * 0.42f);
        cable.startNewSubPath(from);
        cable.cubicTo(from.translated(controlOffset, 0.0f),
                      to.translated(-controlOffset, 0.0f), to);
        graphics.setColour(colour);
        graphics.strokePath(cable, juce::PathStrokeType(thickness,
                                                        juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
    };

    const auto cableColour = isPlaying.load() ? signalGreen : panelOutline;
    drawCable(rightCentre(source1), leftCentre(input1), cableColour, 2.5f);
    drawCable(rightCentre(source2), leftCentre(input2), cableColour, 2.5f);
    drawCable(rightCentre(input1),
              juce::Point<float>(static_cast<float>(matrix.getX()),
                                 static_cast<float>(laneCentre(0))), cableColour, 2.5f);
    drawCable(rightCentre(input2),
              juce::Point<float>(static_cast<float>(matrix.getX()),
                                 static_cast<float>(laneCentre(1))), cableColour, 2.5f);
    drawCable(juce::Point<float>(static_cast<float>(matrix.getRight()),
                                 static_cast<float>(laneCentre(0))),
              leftCentre(output1), cableColour, 2.5f);
    drawCable(juce::Point<float>(static_cast<float>(matrix.getRight()),
                                 static_cast<float>(laneCentre(1))),
              leftCentre(output2), cableColour, 2.5f);

    drawNode(source1);
    drawNode(source2);
    drawNode(input1);
    drawNode(input2);
    drawNode(matrix);
    drawNode(output1);
    drawNode(output2);

    for (std::size_t outputIndex = 0; outputIndex < audio_core::outputCount; ++outputIndex)
    {
        for (std::size_t inputIndex = 0; inputIndex < audio_core::inputCount; ++inputIndex)
        {
            const auto active = appState.routing[outputIndex][inputIndex] != 0.0f;
            const auto from = juce::Point<float>(static_cast<float>(matrix.getX() + 16),
                                                  static_cast<float>(laneCentre(inputIndex)));
            const auto to = juce::Point<float>(static_cast<float>(matrix.getRight() - 16),
                                                static_cast<float>(laneCentre(outputIndex)));
            drawCable(from, to,
                      active ? accent.withAlpha(isPlaying.load() ? 0.95f : 0.55f)
                             : panelOutline.withAlpha(0.32f),
                      active ? 2.0f : 1.0f);
            graphics.setColour(active ? accent : panelOutline);
            graphics.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre(from));
            graphics.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre(to));
        }
    }

    graphics.setColour(textSecondary);
    graphics.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    graphics.drawText("INPUT STRIPS + EQ", input1.getX(), graph.getY() + 4,
                      input1.getWidth(), 24, juce::Justification::centred);
    graphics.drawText("ROUTING", matrix.getX(), graph.getY() + 4,
                      matrix.getWidth(), 24, juce::Justification::centred);
    graphics.drawText("OUTPUTS", output1.getX(), graph.getY() + 4,
                      output1.getWidth(), 24, juce::Justification::centred);

    for (const auto port : { rightCentre(source1), rightCentre(source2),
                             leftCentre(input1), rightCentre(input1),
                             leftCentre(input2), rightCentre(input2),
                             leftCentre(output1), leftCentre(output2) })
    {
        graphics.setColour(cableColour);
        graphics.fillEllipse(juce::Rectangle<float>(8.0f, 8.0f).withCentre(port));
    }

    const auto lowerArea = juce::Rectangle<int>(24, graph.getBottom() + 14,
                                                 getWidth() - 48,
                                                 getHeight() - graph.getBottom() - 38);
    const auto analyzer = lowerArea.withWidth(static_cast<int>(lowerArea.getWidth() * 0.66f));
    const auto device = lowerArea.withTrimmedLeft(analyzer.getWidth() + 18);
    drawNode(analyzer, 6.0f);
    drawNode(device, 6.0f);
}

void App::resized()
{
    header.setBounds(getLocalBounds().removeFromTop(Header::height));

    const auto graph = signalGraphBounds();
    resetSourcesButton.setBounds(graph.getX() + 4, graph.getY() + 4, 140, 24);
    for (std::size_t input = 0; input < audio_core::inputCount; ++input)
    {
        const auto inputBounds = inputNodeBounds(input);
        inputPanels[input].setBounds(graph.getX() + 4, inputBounds.getY(),
                                       InputPanel::width, InputPanel::height);
        eqPanels[input].setBounds(inputBounds.getX() + 122, inputBounds.getY() + 30,
                                    EqComponent::width, EqComponent::height);
    }
    for (std::size_t output = 0; output < audio_core::outputCount; ++output)
        outputPanels[output].setBounds(outputNodeBounds(output));
    routingPanel.setBounds(routingNodeBounds());
    // Under the output strips: it changes what both outputs play.
    const auto outputR = outputNodeBounds(1);
    monoAuditionButton.setBounds(outputR.getX(), outputR.getBottom() + 8, outputR.getWidth(), 26);

    const auto lowerArea = juce::Rectangle<int>(24, graph.getBottom() + 14,
                                                 getWidth() - 48,
                                                 getHeight() - graph.getBottom() - 38);
    const auto analyzer = lowerArea.withWidth(static_cast<int>(lowerArea.getWidth() * 0.66f))
                                   .reduced(14, 10);
    auto device = lowerArea.withTrimmedLeft(static_cast<int>(lowerArea.getWidth() * 0.66f) + 18)
                           .reduced(14, 10);
    analysisPanel.setBounds(analyzer);
    deviceDiagnostics.setBounds(device);
}

juce::Rectangle<int> App::signalGraphBounds() const
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(104);
    bounds.removeFromBottom(330);
    return bounds.reduced(24, 0);
}

juce::Rectangle<int> App::inputNodeBounds(std::size_t inputIndex) const
{
    const auto graph = signalGraphBounds();
    return { graph.getX() + 174,
             graph.getY() + 34 + static_cast<int>(inputIndex) * 190,
             370,
             172 };
}

juce::Rectangle<int> App::routingNodeBounds() const
{
    const auto graph = signalGraphBounds();
    return { graph.getCentreX() - 120, graph.getY() + 46, 240, 338 };
}

juce::Rectangle<int> App::outputNodeBounds(std::size_t outputIndex) const
{
    const auto graph = signalGraphBounds();
    return { graph.getRight() - 250,
             graph.getY() + 34 + static_cast<int>(outputIndex) * 190,
             250,
             172 };
}

void App::timerCallback()
{
    if (statePublicationPending)
        publishAppState();
    const auto now = juce::Time::getMillisecondCounter();
    if (header.captureControlsVisible())
    {
        // The live capture meter shows what recordingMeterPeak() reports, block by block;
        // the text holds the highest value since recording started.
        updateMeter(captureMeterBallistics,
                    pendingCapturePeak.exchange(0.0f, std::memory_order_acq_rel),
                    now, captureRequested.load());
        header.setCaptureMeter(captureMeterBallistics.smoothedPeak,
                               captureMeterBallistics.heldPeak);
        header.setCaptureStatus("CAPTURE " + formatDecibels(capturePeak.load())
            + "\n" + juce::String(static_cast<juce::int64>(captureFrames.load())) + " frames, "
            + juce::String(static_cast<juce::int64>(captureClips.load())) + " clips");
        header.setReplayToggled(replayRequested.load());
    }
    const auto playing = isPlaying.load() || replayRequested.load();
    for (std::size_t input = 0; input < audio_core::inputCount; ++input)
    {
        const auto peak = pendingInputPeaks[input].exchange(0.0f, std::memory_order_acq_rel);
        updateMeter(inputMeterBallistics[input], peak, now, playing);
        inputPanels[input].setMeter(
            inputMeterBallistics[input].smoothedPeak,
            inputMeterBallistics[input].heldPeak,
            playing ? "PRE " + formatDecibels(inputMeterBallistics[input].smoothedPeak)
                    : "PRE  --.- dB");
    }
    for (std::size_t output = 0; output < audio_core::outputCount; ++output)
    {
        const auto peak = pendingOutputPeaks[output].exchange(0.0f, std::memory_order_acq_rel);
        updateMeter(outputMeterBallistics[output], peak, now, playing);
        outputPanels[output].setMeter(
            outputMeterBallistics[output].smoothedPeak,
            outputMeterBallistics[output].heldPeak,
            playing ? formatDecibels(outputMeterBallistics[output].smoothedPeak)
                    : "  --.- dB");
    }

    analysisPanel.refresh(playing, now);
    deviceDiagnostics.refresh(playing);
}

void App::loadBundledInput()
{
    juce::WavAudioFormat wavFormat;
    const auto reader = std::unique_ptr<juce::AudioFormatReader>(
        wavFormat.createReaderFor(
            new juce::MemoryInputStream(BinaryData::drumloop_wav,
                                        BinaryData::drumloop_wavSize,
                                        false),
            true));

    if (reader == nullptr)
        return;

    sourceSampleRate = reader->sampleRate;
    sourceAudio.setSize(1, static_cast<int>(reader->lengthInSamples));
    juce::AudioBuffer<float> decoded(2, sourceAudio.getNumSamples());
    reader->read(&decoded, 0, decoded.getNumSamples(), 0, true, true);
    sourceAudio.copyFrom(0, 0, decoded, 0, 0, decoded.getNumSamples());
    if (reader->numChannels > 1)
    {
        sourceAudio.addFrom(0, 0, decoded, 1, 0, decoded.getNumSamples());
        sourceAudio.applyGain(0.5f);
    }
    const auto peak = sourceAudio.getMagnitude(0, sourceAudio.getNumSamples());
    if (peak > 0.0f)
        sourceAudio.applyGain(1.0f / peak);
}

void App::savePreset()
{
    const auto initialFile = juce::File::getCurrentWorkingDirectory()
        .getParentDirectory().getChildFile("assets").getChildFile("presets")
        .getChildFile("escape-room-preset.json");
    savePresetChooser = std::make_unique<juce::FileChooser>(
        "Save product preset", initialFile, "*.json", true, false, this);
    const auto safeThis = juce::Component::SafePointer<App>(this);
    savePresetChooser->launchAsync(
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [safeThis](const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            auto file = chooser.getResult();
            if (file == juce::File())
                return;
            if (file.getFileExtension().isEmpty())
                file = file.withFileExtension(".json");

            try
            {
                const auto stateToSave = safeThis->appState;
                const auto json = stateToSave.serialize();
                const auto text = juce::String::fromUTF8(
                    json.data(), static_cast<int>(json.size()));
                if (!file.replaceWithText(text, false, false, "\n"))
                    safeThis->showPresetError("Could not write " + file.getFullPathName());
            }
            catch (const std::exception& error)
            {
                safeThis->showPresetError("Could not save preset: " + juce::String(error.what()));
            }
        });
}

void App::loadPreset()
{
    const auto initialDirectory = juce::File::getCurrentWorkingDirectory()
        .getParentDirectory().getChildFile("assets").getChildFile("presets");
    loadPresetChooser = std::make_unique<juce::FileChooser>(
        "Load product preset", initialDirectory, "*.json", true, false, this);
    const auto safeThis = juce::Component::SafePointer<App>(this);
    loadPresetChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            const auto file = chooser.getResult();
            if (file == juce::File())
                return;

            try
            {
                if (!file.existsAsFile())
                    throw std::invalid_argument("Preset file could not be read");
                const auto text = file.loadFileAsString();
                const auto restored = audio_core::AppState::deserialize(
                    std::string_view(text.toRawUTF8(), text.getNumBytesAsUTF8()));
                safeThis->comparisonState = safeThis->appState;
                safeThis->appState = restored;
                safeThis->updateAppStateControls();
                safeThis->publishAppState();
                safeThis->repaint();
            }
            catch (const std::exception& error)
            {
                safeThis->showPresetError("Could not load preset: " + juce::String(error.what()));
            }
        });
}

void App::updateAppStateControls()
{
    for (std::size_t input = 0; input < audio_core::inputCount; ++input)
    {
        inputPanels[input].setFromState(appState.inputs[input]);
        eqPanels[input].setFromState(appState.inputs[input].equalizer);
    }
    for (std::size_t output = 0; output < audio_core::outputCount; ++output)
        outputPanels[output].setFromState(appState.outputs[output]);
    routingPanel.setFromState(appState);
}

void App::showPresetError(const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                           "Preset error", message, {}, this);
}

void App::publishAppState()
{
    deviceDiagnostics.setAppStateView(appState.serialize(),
                                      audio_core::AppState::diff(comparisonState, appState));
    const auto prepared = audio_core::prepareForSampleRate(appState, currentSampleRate.load());
    statePublicationPending = !appStateExchange.publish(prepared);
}
