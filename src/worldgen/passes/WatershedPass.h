#pragma once

#include "worldgen/SeedIds.h"
#include "worldgen/WorldGenerationPass.h"

namespace olam
{

    // Drainage basins from the flow network: every land / lake tile belongs to the catchment of the outlet it drains
    // to; catchments below HydrologySettings::minWatershedKm2 merge into the neighbour sharing the longest border.
    class WatershedPass : public WorldGenerationPass
    {
    public:
        std::string_view name() const override { return "Watersheds"; }
        std::uint64_t seedId() const override { return worldgen::kWatershedSeedId; }
        std::optional<std::string> validatePreconditions(const WorldGenContext &context) const override;
        void run(WorldGenContext &context) override;
    };

} // namespace olam
