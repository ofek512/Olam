#pragma once

#include "worldgen/SeedIds.h"
#include "worldgen/WorldGenerationPass.h"

namespace olam
{

    // Priority-flood drainage: D8 flow directions toward the sea or map edge, lakes in large/deep depressions,
    // discharge from runoff, and river main stems. Also leaves the floodplain working layer for later passes.
    class HydrologyPass : public WorldGenerationPass
    {
    public:
        std::string_view name() const override { return "Hydrology"; }
        std::uint64_t seedId() const override { return worldgen::kHydrologySeedId; }
        std::optional<std::string> validatePreconditions(const WorldGenContext &context) const override;
        void run(WorldGenContext &context) override;
    };

    // Mean annual runoff (mm/year) from rainfall and potential evaporation (Turc-Pike evapotranspiration).
    float runoffMm(float rainfallMm, float potentialEvaporationMm);

} // namespace olam
