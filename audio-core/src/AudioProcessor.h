#pragma once

#include <app-state/AppState.h>

#include <array>
#include <bitset>
#include <cstddef>
#include <span>

namespace audio_core
{
struct AnalysisTaps
{
    std::array<float, inputCount> inputPeak {};
    std::array<float, inputCount> postGainPeak {};
    std::array<float, inputCount> postEqualizerPeak {};
    std::array<float, outputCount> postMixPeak {};
    std::array<float, outputCount> outputPeak {};
};

struct AnalysisBufferViews
{
    std::array<std::span<float>, inputCount> input;
    std::array<std::span<float>, inputCount> postGain;
    std::array<std::span<float>, inputCount> postEqualizer;
    std::array<std::span<float>, outputCount> postMix;
    std::array<std::span<float>, outputCount> output;
};

struct BiquadCoefficients
{
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
};

struct ProcessorState
{
    AppState appState;
    double sampleRate = 0.0;
    std::array<float, inputCount> inputGains {{ 1.0f, 1.0f }};
    std::bitset<inputCount> phaseInverted;
    std::array<float, outputCount> outputGains {{ 1.0f, 1.0f }};
    std::array<std::array<BiquadCoefficients, 3>, inputCount> equalizer;
};

class AudioProcessor
{
public:
    static ProcessorState prepareState(const AppState& state, double sampleRate);

    void prepare(double newSampleRate);
    void setState(const AppState& newState);
    void setProcessorState(const ProcessorState& newState) noexcept;
    void reset();

    void process(std::array<std::span<const float>, inputCount> inputs,
                 std::array<std::span<float>, outputCount> outputs,
                 AnalysisTaps& analysis,
                 AnalysisBufferViews* analysisBuffers = nullptr) noexcept;

private:
    struct Biquad
    {
        BiquadCoefficients coefficients;
        float z1 = 0.0f;
        float z2 = 0.0f;

        float process(float sample) noexcept;
        void reset() noexcept;
    };

    struct InputProcessingState
    {
        float gain = 1.0f;
        std::array<Biquad, 3> equalizer;
    };

    static BiquadCoefficients makePeaking(double sampleRate,
                                          float frequency,
                                          float gainDecibels,
                                          float quality);
    void updateProcessingState();

    double sampleRate = 48'000.0;
    AppState state;
    std::array<InputProcessingState, inputCount> inputProcessing;
    std::bitset<inputCount> phaseInverted;
    std::array<float, outputCount> outputGains {{ 1.0f, 1.0f }};
};
}
