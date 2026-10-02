#pragma once

#include "core/random/Seed.h"

#include <cstdint>

namespace olam::worldgen
{

    // Fixed subsystem ids for deriveSeed(). Never change an existing value: it would change every world.
    inline constexpr std::uint64_t kTerrainSeedId = seedId("TERRAIN");
    inline constexpr std::uint64_t kClimateSeedId = seedId("CLIMATE");
    inline constexpr std::uint64_t kHydrologySeedId = seedId("HYDRO");
    inline constexpr std::uint64_t kResourcesSeedId = seedId("RESOURCE");

} // namespace olam::worldgen
