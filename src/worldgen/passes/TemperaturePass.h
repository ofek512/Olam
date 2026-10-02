#pragma once

#include "worldgen/SeedIds.h"
#include "worldgen/WorldGenerationPass.h"

namespace olam
{

    // Mean annual temperature from latitude, altitude (lapse rate), continentality and small noise.
    class TemperaturePass : public WorldGenerationPass
    {
    public:
        std::string_view name() const override { return "Temperature"; }
        std::uint64_t seedId() const override { return worldgen::kClimateSeedId; }
        std::optional<std::string> validatePreconditions(const WorldGenContext &context) const override;
        void run(WorldGenContext &context) override;
    };

} // namespace olam
