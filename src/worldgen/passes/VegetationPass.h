#pragma once

#include "worldgen/SeedIds.h"
#include "worldgen/WorldGenerationPass.h"

namespace olam
{

    // Tree cover from the biome's potential, moisture, fertility, slope and patchiness noise; the vegetation
    // category follows from cover and biome.
    class VegetationPass : public WorldGenerationPass
    {
    public:
        std::string_view name() const override { return "Vegetation"; }
        std::uint64_t seedId() const override { return worldgen::kVegetationSeedId; }
        std::optional<std::string> validatePreconditions(const WorldGenContext &context) const override;
        void run(WorldGenContext &context) override;
    };

} // namespace olam
