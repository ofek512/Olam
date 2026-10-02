#pragma once

#include "core/random/CoordinateHash.h"
#include "core/random/Seed.h"
#include "world/WorldCoord.h"

#include <cstdint>

namespace olam::world_viewer
{

    // Debug-only visualization randomness; never stored in World.
    inline constexpr std::uint64_t kDebugViewSeedId = seedId("DBGVIEW");

    constexpr std::uint64_t debugViewSeed(std::uint64_t worldSeed)
    {
        return deriveSeed(worldSeed, kDebugViewSeedId);
    }

    constexpr std::uint64_t debugHash(std::uint64_t worldSeed, WorldCoord coord)
    {
        return coordinateHash(debugViewSeed(worldSeed), coord.x, coord.y);
    }

    constexpr std::uint8_t debugValue(std::uint64_t hash)
    {
        return static_cast<std::uint8_t>(hash >> 56);
    }

} // namespace olam::world_viewer
