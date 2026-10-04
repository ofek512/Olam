#pragma once

#include "core/random/Seed.h"

#include <cstdint>

namespace olam::worldgen
{

    // Fixed subsystem ids for deriveSeed(). Never change an existing value: it would change every world.
    inline constexpr std::uint64_t kTectonicsSeedId = seedId("PLATES");
    inline constexpr std::uint64_t kTerrainSeedId = seedId("TERRAIN");
    inline constexpr std::uint64_t kOceanSeedId = seedId("OCEAN");
    inline constexpr std::uint64_t kClimateSeedId = seedId("CLIMATE");
    inline constexpr std::uint64_t kRainfallSeedId = seedId("RAINFALL");
    inline constexpr std::uint64_t kHydrologySeedId = seedId("HYDRO");
    inline constexpr std::uint64_t kWatershedSeedId = seedId("WATERSHD");
    inline constexpr std::uint64_t kSoilSeedId = seedId("SOIL");
    inline constexpr std::uint64_t kBiomeSeedId = seedId("BIOME");
    inline constexpr std::uint64_t kFertilitySeedId = seedId("FERTILE");
    inline constexpr std::uint64_t kVegetationSeedId = seedId("VEGETATE");
    inline constexpr std::uint64_t kResourcesSeedId = seedId("RESOURCE");

} // namespace olam::worldgen
