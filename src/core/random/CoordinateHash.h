#pragma once

#include "core/random/SplitMix64.h"

#include <cstdint>

namespace olam
{

    // Stateless per-tile randomness: same (seed, x, y) always gives the same value, in any iteration order.
    constexpr std::uint64_t coordinateHash(std::uint64_t seed, std::int32_t x, std::int32_t y)
    {
        const std::uint64_t packed =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) | static_cast<std::uint32_t>(y);
        return splitMix64(seed ^ splitMix64(packed));
    }

} // namespace olam
