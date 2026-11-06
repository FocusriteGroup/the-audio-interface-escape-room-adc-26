#include <AudioProcessor.h>

#include <audio-processing/Gain.h>
#include <audio-processing/Routing.h>
#include <ProcessorSetup.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace
{
float updatePeak(float peak, float sample) noexcept
{
    return std::max(peak, std::abs(sample));
}
}

namespace audio_core
{
float AudioProcessor::Biquad::process(float sample) noexcept
{
    const auto output = coefficients.b0 * sample + z1;
    z1 = coefficients.b1 * sample - coefficients.a1 * output + z2;
    z2 = coefficients.b2 * sample - coefficients.a2 * output;
    return output;
}

void AudioProcessor::Biquad::reset() noexcept
{
    z1 = 0.0f;
    z2 = 0.0f;
}

void AudioProcessor::prepare(double newSampleRate)
{
    if (!std::isfinite(newSampleRate) || newSampleRate <= 0.0)
        throw std::invalid_argument("Sample rate must be positive");

    sampleRate = newSampleRate;
    updateProcessingState();
    reset();
}

ProcessorState AudioProcessor::prepareState(const AppState& appState,
                                                    double sampleRate)
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0)
        throw std::invalid_argument("Sample rate must be positive");

    ProcessorState prepared;
    prepared.appState = appState;
    prepared.sampleRate = sampleRate;

    constexpr std::array<float, 3> frequencies {{ 120.0f, 1'000.0f, 8'000.0f }};
    constexpr std::array<float, 3> qualities {{ 0.707f, 1.0f, 0.707f }};

    for (std::size_t inputIndex = 0; inputIndex < inputCount; ++inputIndex)
    {
        const auto& input = appState.inputs[inputIndex];
        prepared.inputGains[inputIndex] = decibelsToLinear(input.gainDecibels);
        prepared.phaseInverted.set(inputIndex, input.phaseInverted);
        const std::array<float, 3> gains {{
            input.equalizer.lowGainDecibels,
            input.equalizer.midGainDecibels,
            input.equalizer.highGainDecibels
        }};

        for (std::size_t band = 0; band < frequencies.size(); ++band)
            prepared.equalizer[inputIndex][band] =
                makePeaking(sampleRate, frequencies[band], gains[band], qualities[band]);
    }

    for (std::size_t outputIndex = 0; outputIndex < outputCount; ++outputIndex)
        prepared.outputGains[outputIndex] =
            decibelsToLinear(appState.outputs[outputIndex].gainDecibels);

    return prepared;
}

void AudioProcessor::setState(const AppState& newState)
{
    setProcessorState(prepareState(newState, sampleRate));
}

void AudioProcessor::setProcessorState(const ProcessorState& newState) noexcept
{
    if (newState.sampleRate != sampleRate)
        return;
    state = newState.appState;
    outputGains = newState.outputGains;
    phaseInverted = newState.phaseInverted;

    for (std::size_t inputIndex = 0; inputIndex < inputCount; ++inputIndex)
    {
        inputProcessing[inputIndex].gain = newState.inputGains[inputIndex];
        for (std::size_t band = 0; band < inputProcessing[inputIndex].equalizer.size(); ++band)
            inputProcessing[inputIndex].equalizer[band].coefficients =
                newState.equalizer[inputIndex][band];
    }
}

void AudioProcessor::reset()
{
    for (auto& input : inputProcessing)
        for (auto& filter : input.equalizer)
            filter.reset();
}

BiquadCoefficients AudioProcessor::makePeaking(double filterSampleRate,
                                                 float frequency,
                                                 float gainDecibels,
                                                 float quality)
{
    if (gainDecibels == 0.0f)
        return {};
    const auto amplitude = std::pow(10.0, static_cast<double>(gainDecibels) / 40.0);
    const auto omega = 2.0 * std::numbers::pi * frequency / filterSampleRate;
    const auto alpha = std::sin(omega) / (2.0 * quality);
    const auto cosine = std::cos(omega);
    const auto a0 = 1.0 + alpha / amplitude;

    return {
        static_cast<float>((1.0 + alpha * amplitude) / a0),
        static_cast<float>(-2.0 * cosine / a0),
        static_cast<float>((1.0 - alpha * amplitude) / a0),
        static_cast<float>(-2.0 * cosine / a0),
        static_cast<float>((1.0 - alpha / amplitude) / a0)
    };
}

void AudioProcessor::process(
    std::array<std::span<const float>, inputCount> inputs,
    std::array<std::span<float>, outputCount> outputs,
    AnalysisTaps& analysis,
    AnalysisBufferViews* analysisBuffers) noexcept
{
    analysis = {};
    const auto samples = std::min({ inputs[0].size(), inputs[1].size(),
                                    outputs[0].size(), outputs[1].size() });

    for (std::size_t sampleIndex = 0; sampleIndex < samples; ++sampleIndex)
    {
        std::array<float, inputCount> processedInputs {};

        for (std::size_t inputIndex = 0; inputIndex < inputCount; ++inputIndex)
        {
            const auto inputSample = inputs[inputIndex][sampleIndex];
            analysis.inputPeak[inputIndex] = updatePeak(analysis.inputPeak[inputIndex], inputSample);
            if (analysisBuffers != nullptr
                && sampleIndex < analysisBuffers->input[inputIndex].size())
                analysisBuffers->input[inputIndex][sampleIndex] = inputSample;

            auto processed = state.inputs[inputIndex].muted
                ? 0.0f
                : inputSample * inputProcessing[inputIndex].gain;
            if (phaseInverted[inputIndex])
                processed = -processed;

            analysis.postGainPeak[inputIndex] =
                updatePeak(analysis.postGainPeak[inputIndex], processed);
            if (analysisBuffers != nullptr
                && sampleIndex < analysisBuffers->postGain[inputIndex].size())
                analysisBuffers->postGain[inputIndex][sampleIndex] = processed;

            for (auto& filter : inputProcessing[inputIndex].equalizer)
                processed = filter.process(processed);

            processedInputs[inputIndex] = processed;
            analysis.postEqualizerPeak[inputIndex] =
                updatePeak(analysis.postEqualizerPeak[inputIndex], processed);
            if (analysisBuffers != nullptr
                && sampleIndex < analysisBuffers->postEqualizer[inputIndex].size())
                analysisBuffers->postEqualizer[inputIndex][sampleIndex] = processed;
        }

        for (std::size_t outputIndex = 0; outputIndex < outputCount; ++outputIndex)
        {
            const auto mixed = mixOutput(processedInputs, state, outputIndex);

            analysis.postMixPeak[outputIndex] =
                updatePeak(analysis.postMixPeak[outputIndex], mixed);
            if (analysisBuffers != nullptr
                && sampleIndex < analysisBuffers->postMix[outputIndex].size())
                analysisBuffers->postMix[outputIndex][sampleIndex] = mixed;

            const auto output = state.outputs[outputIndex].muted
                ? 0.0f
                : mixed * outputGains[outputIndex];
            outputs[outputIndex][sampleIndex] = output;
            analysis.outputPeak[outputIndex] =
                updatePeak(analysis.outputPeak[outputIndex], output);
            if (analysisBuffers != nullptr
                && sampleIndex < analysisBuffers->output[outputIndex].size())
                analysisBuffers->output[outputIndex][sampleIndex] = output;
        }
    }
}

void AudioProcessor::updateProcessingState()
{
    setProcessorState(prepareForSampleRate(state, sampleRate));
}
}
