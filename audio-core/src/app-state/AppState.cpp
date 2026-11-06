#include <app-state/AppState.h>
#include <app-state/LegacyMapping.h>

#include <nlohmann/json.hpp>

#include <cmath>
#include <stdexcept>

namespace
{
using Json = nlohmann::json;
constexpr int appStateSchemaVersion = 2;

float readNumber(const Json& json, float minimum, float maximum)
{
    if (!json.is_number())
        throw std::invalid_argument("Preset control must be a number");
    const auto value = json.get<double>();
    if (!std::isfinite(value) || value < minimum || value > maximum)
        throw std::invalid_argument("Preset control is outside its supported range");
    return static_cast<float>(value);
}

int readInteger(const Json& json, int minimum, int maximum)
{
    if (!json.is_number_integer() || json < minimum || json > maximum)
        throw std::invalid_argument("Preset version/channel must be a supported integer");
    return json.get<int>();
}

void requireArraySize(const Json& json, std::size_t expectedSize)
{
    if (!json.is_array() || json.size() != expectedSize)
        throw std::invalid_argument("Preset array has an unsupported size");
}

Json serializeEqualizer(const audio_core::EqualizerState& equalizer)
{
    return {
        { "lowGainDb", equalizer.lowGainDecibels },
        { "midGainDb", equalizer.midGainDecibels },
        { "highGainDb", equalizer.highGainDecibels }
    };
}

audio_core::EqualizerState deserializeEqualizer(const Json& json)
{
    return {
        readNumber(json.at("lowGainDb"), -12.0f, 12.0f),
        readNumber(json.at("midGainDb"), -12.0f, 12.0f),
        readNumber(json.at("highGainDb"), -12.0f, 12.0f)
    };
}

audio_core::AppState deserializeVersionOne(const Json& json)
{
    audio_core::AppState state;
    state.routing = {};

    const auto& mappings = json.at("outputChannelMappings");
    requireArraySize(mappings, audio_core::outputCount);

    for (std::size_t output = 0; output < audio_core::outputCount; ++output)
    {
        const auto oneBasedInput = readInteger(mappings.at(output), 1,
                                               static_cast<int>(audio_core::inputCount));
        state.routing[output][audio_core::legacyInputIndex(oneBasedInput)] = 1.0f;
    }

    return state;
}
}

namespace audio_core
{
std::string AppState::serialize() const
{
    Json serializedInputs = Json::array();
    for (const auto& input : inputs)
    {
        serializedInputs.push_back({
            { "gainDb", input.gainDecibels },
            { "muted", input.muted },
            { "phaseInverted", input.phaseInverted },
            { "equalizer", serializeEqualizer(input.equalizer) }
        });
    }

    Json serializedOutputs = Json::array();
    for (const auto& output : outputs)
        serializedOutputs.push_back({ { "gainDb", output.gainDecibels }, { "muted", output.muted } });

    return Json {
        { "schemaVersion", appStateSchemaVersion },
        { "inputs", serializedInputs },
        { "outputs", serializedOutputs },
        { "routing", routing }
    }.dump(2);
}

std::string AppState::diff(const AppState& before, const AppState& after)
{
    return Json::diff(Json::parse(before.serialize()),
                      Json::parse(after.serialize())).dump(2);
}

AppState AppState::deserialize(std::string_view jsonText)
{
    try
    {
        const auto json = Json::parse(jsonText);
        const auto version = readInteger(json.at("schemaVersion"), 1,
                         appStateSchemaVersion);
        if (version == 1)
            return deserializeVersionOne(json);
        if (version != appStateSchemaVersion)
            throw std::invalid_argument("Unsupported app-state schema version");

        AppState state;
        const auto& inputs = json.at("inputs");
        const auto& outputs = json.at("outputs");
        requireArraySize(inputs, inputCount);
        requireArraySize(outputs, outputCount);

        for (std::size_t index = 0; index < inputCount; ++index)
        {
            const auto& input = inputs.at(index);
            state.inputs[index] = {
                readNumber(input.at("gainDb"), -24.0f, 24.0f),
                input.at("muted").get<bool>(),
                input.at("phaseInverted").get<bool>(),
                deserializeEqualizer(input.at("equalizer"))
            };

        }
        for (std::size_t index = 0; index < outputCount; ++index)
        {
            const auto& output = outputs.at(index);
            state.outputs[index] = {
                readNumber(output.at("gainDb"), -24.0f, 12.0f),
                output.at("muted").get<bool>()
            };
        }

        const auto& routing = json.at("routing");
        requireArraySize(routing, outputCount);
        for (std::size_t output = 0; output < outputCount; ++output)
        {
            requireArraySize(routing.at(output), inputCount);
            for (std::size_t input = 0; input < inputCount; ++input)
                state.routing[output][input] = readNumber(routing.at(output).at(input),
                                                         0.0f, 1.0f);
        }
        return state;
    }
    catch (const std::invalid_argument&)
    {
        throw;
    }
    catch (const std::exception& error)
    {
        throw std::invalid_argument(std::string("Invalid app state: ") + error.what());
    }
}
}
