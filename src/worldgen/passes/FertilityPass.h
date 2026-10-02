#pragma once

#include "world/WorldTypes.h"
#include "worldgen/SeedIds.h"
#include "worldgen/WorldGenerationPass.h"

namespace olam
{

    // Natural fertility = temperature x moisture x terrain x soil factors (each 0..1), with river floodplains
    // irrigated and enriched by silt.
    class FertilityPass : public WorldGenerationPass
    {
    public:
        std::string_view name() const override { return "Fertility"; }
        std::uint64_t seedId() const override { return worldgen::kFertilitySeedId; }
        std::optional<std::string> validatePreconditions(const WorldGenContext &context) const override;
        void run(WorldGenContext &context) override;
    };

    namespace fertility
    {
        float temperatureFactor(float celsius);
        // aridity = rainfall / potential evaporation; waterlogged land is slightly less fertile.
        float moistureFactor(float aridity);
        float terrainFactor(float slope, float elevationM);
        float soilFactor(SoilType soil);
    } // namespace fertility

} // namespace olam
