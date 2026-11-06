#pragma once

#include <span>

namespace audio_core
{
void copyOutputBlock(std::span<const float> source, std::span<float> destination) noexcept;
}