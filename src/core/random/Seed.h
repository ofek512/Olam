#pragma once

#include "core/random/SplitMix64.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace olam
{

    // Independent seed for a subsystem; changing one subsystem's id/usage never affects another's seed.
    constexpr std::uint64_t deriveSeed(std::uint64_t seed, std::uint64_t subsystemId)
    {
        return splitMix64(seed ^ subsystemId);
    }

    // Packs up to 8 ASCII characters into a readable fixed 64-bit subsystem id, e.g. seedId("TERRAIN").
    template <std::size_t N>
    constexpr std::uint64_t seedId(const char (&name)[N])
    {
        static_assert(N >= 2 && N <= 9, "seed id names must be 1-8 characters");
        std::uint64_t id = 0;
        for (std::size_t i = 0; i + 1 < N; ++i)
            id = (id << 8) | static_cast<std::uint8_t>(name[i]);
        return id;
    }

    // A decimal uint64 string is used as-is; any other text is hashed (xxHash64 of its UTF-8 bytes).
    std::uint64_t parseSeed(std::string_view text);

} // namespace olam
