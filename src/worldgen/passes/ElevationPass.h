#pragma once

#include "worldgen/SeedIds.h"
#include "worldgen/WorldGenerationPass.h"

namespace olam
{

    // Plate base + boundary uplift/rifts + noise, thermal smoothing, then an Earth-like hypsometric curve that
    // also places sea level for the target land share. Produces elevation.
    class ElevationPass : public WorldGenerationPass
    {
    public:
        std::string_view name() const override { return "Elevation"; }
        std::uint64_t seedId() const override { return worldgen::kTerrainSeedId; }
        std::optional<std::string> validatePreconditions(const WorldGenContext &context) const override;
        void run(WorldGenContext &context) override;
    };

} // namespace olam
