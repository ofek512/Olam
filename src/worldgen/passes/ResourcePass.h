#pragma once

#include "world/WorldTypes.h"
#include "worldgen/SeedIds.h"
#include "worldgen/WorldGenerationPass.h"

namespace olam
{

    class World;

    // Mineral deposits: a probability field per mineral (geology x terrain x climate x regional province noise)
    // sampled with PCG and a minimum spacing into deposit clusters with a richness.
    class ResourcePass : public WorldGenerationPass
    {
    public:
        std::string_view name() const override { return "Resources"; }
        std::uint64_t seedId() const override { return worldgen::kResourcesSeedId; }
        std::optional<std::string> validatePreconditions(const WorldGenContext &context) const override;
        void run(WorldGenContext &context) override;
    };

    // Geology x terrain x climate suitability (0..1) of a land tile for a mineral, without province noise.
    float mineralSuitability(const World &world, MineralType mineral, std::size_t index);

} // namespace olam
