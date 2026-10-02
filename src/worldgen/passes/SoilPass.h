#pragma once

#include "worldgen/SeedIds.h"
#include "worldgen/WorldGenerationPass.h"

namespace olam
{

    // Soil type from climate (permafrost, laterite, desert sand), terrain (rocky slopes), water (alluvial
    // floodplains, peat) and rock type plus regional noise (sandy / loam / clay).
    class SoilPass : public WorldGenerationPass
    {
    public:
        std::string_view name() const override { return "Soil"; }
        std::uint64_t seedId() const override { return worldgen::kSoilSeedId; }
        std::optional<std::string> validatePreconditions(const WorldGenContext &context) const override;
        void run(WorldGenContext &context) override;
    };

} // namespace olam
