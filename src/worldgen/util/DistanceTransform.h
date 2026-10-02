#pragma once

#include "core/containers/Layer.h"

#include <cstdint>

namespace olam
{

    // Distance units of chamferDistance: 5 per orthogonal step, 7 per diagonal step (~2% error vs Euclidean).
    inline constexpr std::int32_t kChamferOrthogonal = 5;
    inline constexpr std::int32_t kChamferDiagonal = 7;
    inline constexpr std::int32_t kChamferUnreached = INT32_MAX / 2;

    // Two-pass 8-neighbour chamfer distance to the nearest tile with sources[i] != 0.
    // If nearestSource is non-null it receives the index of that source (UINT32_MAX when unreached).
    Layer<std::int32_t> chamferDistance(const Layer<std::uint8_t> &sources, Layer<std::uint32_t> *nearestSource = nullptr);

} // namespace olam
