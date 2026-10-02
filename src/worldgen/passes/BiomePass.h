#pragma once

#include "world/WorldTypes.h"
#include "worldgen/SeedIds.h"
#include "worldgen/WorldGenerationPass.h"

namespace olam
{

    // Whittaker-style biome from mean annual temperature and aridity, overridden by alpine (above the tree line)
    // and wetland (flat, wet ground by rivers, lakes or on peat).
    class BiomePass : public WorldGenerationPass
    {
    public:
        std::string_view name() const override { return "Biome"; }
        std::uint64_t seedId() const override { return worldgen::kBiomeSeedId; }
        std::optional<std::string> validatePreconditions(const WorldGenContext &context) const override;
        void run(WorldGenContext &context) override;
    };

    // Climate biome only (no alpine / wetland); aridity = rainfall / potential evaporation.
    Biome climateBiome(float celsius, float aridity);

} // namespace olam
