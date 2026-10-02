#pragma once

#include "world/WorldTypes.h"
#include "worldgen/SeedIds.h"
#include "worldgen/WorldGenerationPass.h"

namespace olam
{

    class World;

    // Mineral deposits by how they form: hydrothermal veins in orogens / shields / granite, placers downstream of
    // gold and tin veins, bedded coal and iron in sedimentary basins and shields, evaporite salt in basins, salt
    // pans on warm coasts and desert playas, and bog iron in cool wetlands. Sampled with PCG and minimum spacing.
    class ResourcePass : public WorldGenerationPass
    {
    public:
        std::string_view name() const override { return "Resources"; }
        std::uint64_t seedId() const override { return worldgen::kResourcesSeedId; }
        std::optional<std::string> validatePreconditions(const WorldGenContext &context) const override;
        void run(WorldGenContext &context) override;
    };

} // namespace olam
