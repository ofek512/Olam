#pragma once

#include "worldgen/SeedIds.h"
#include "worldgen/WorldGenerationPass.h"

namespace olam
{

    // Prevailing winds by latitude band carry moisture from the sea; rain falls over land, extra on windward slopes,
    // leaving rain shadows. Produces annualRainfall and moisture (aridity index).
    class RainfallPass : public WorldGenerationPass
    {
    public:
        std::string_view name() const override { return "Rainfall"; }
        std::uint64_t seedId() const override { return worldgen::kRainfallSeedId; }
        std::optional<std::string> validatePreconditions(const WorldGenContext &context) const override;
        void run(WorldGenContext &context) override;
    };

    // Potential evapotranspiration (mm/year) from mean annual temperature; simple linear estimate.
    float potentialEvaporationMm(float celsius);

} // namespace olam
