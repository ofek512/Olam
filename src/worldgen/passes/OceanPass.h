#pragma once

#include "worldgen/SeedIds.h"
#include "worldgen/WorldGenerationPass.h"

namespace olam
{

    // Classifies below-sea tiles connected to the map edge (or large inland seas) as Ocean; computes distanceToOcean.
    class OceanPass : public WorldGenerationPass
    {
    public:
        std::string_view name() const override { return "Ocean"; }
        std::uint64_t seedId() const override { return worldgen::kOceanSeedId; }
        std::optional<std::string> validatePreconditions(const WorldGenContext &context) const override;
        void run(WorldGenContext &context) override;
    };

} // namespace olam
