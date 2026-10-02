#pragma once

#include "worldgen/SeedIds.h"
#include "worldgen/WorldGenerationPass.h"

namespace olam
{

    // Voronoi plates with drift; produces plateId, rockType and boundary working layers for ElevationPass.
    class TectonicsPass : public WorldGenerationPass
    {
    public:
        std::string_view name() const override { return "Tectonics"; }
        std::uint64_t seedId() const override { return worldgen::kTectonicsSeedId; }
        void run(WorldGenContext &context) override;
    };

} // namespace olam
