#include "worldgen/passes/BiomePass.h"

#include "world/World.h"
#include "world/queries/TerrainQueries.h"
#include "worldgen/WorkingLayers.h"
#include "worldgen/WorldGenContext.h"

namespace olam
{

    Biome climateBiome(float celsius, float aridity)
    {
        if (celsius <= -10.0f)
            return Biome::Ice;
        if (celsius < -5.0f)
            return Biome::Tundra;

        if (aridity < 0.2f)
            return celsius < 12.0f ? Biome::ColdDesert : Biome::HotDesert;

        if (celsius < 3.0f)
            return aridity < 0.5f ? Biome::TemperateGrassland : Biome::BorealForest;

        if (celsius < 18.0f)
        {
            if (aridity < 0.5f)
                return Biome::TemperateGrassland;
            if (aridity < 0.65f)
                return celsius >= 12.0f ? Biome::Shrubland : Biome::TemperateGrassland;
            if (celsius < 6.0f)
                return Biome::BorealForest;
            return aridity >= 1.5f ? Biome::TemperateRainforest : Biome::TemperateForest;
        }

        if (celsius < 22.0f)
        {
            if (aridity < 0.5f)
                return Biome::Shrubland;
            if (aridity < 0.65f)
                return Biome::Savanna;
            return aridity >= 1.5f ? Biome::TemperateRainforest : Biome::TemperateForest;
        }

        if (aridity < 0.8f)
            return Biome::Savanna;
        if (aridity < 1.3f)
            return Biome::TropicalDryForest;
        return Biome::TropicalRainforest;
    }

    std::optional<std::string> BiomePass::validatePreconditions(const WorldGenContext &context) const
    {
        if (context.world().geography().soil.empty() || !context.findWorkingLayer(worldgen::kFloodplain))
            return std::string("requires SoilPass and HydrologyPass");
        return std::nullopt;
    }

    void BiomePass::run(WorldGenContext &context)
    {
        World &world = context.world();
        const BiomeSettings &settings = world.config().generation.biome;
        const auto &elevation = world.terrain().elevation;
        const auto &climate = world.climate();
        const auto &water = world.hydrology().surfaceWater;
        const auto &soil = world.geography().soil;
        const Layer<float> &floodplain = *context.findWorkingLayer(worldgen::kFloodplain);

        const auto nextToLake = [&](WorldCoord coord)
        {
            for (std::size_t d = 0; d < kDirection8Count; ++d)
            {
                const WorldCoord n = neighbor(coord, static_cast<Direction8>(d));
                if (world.isValid(n) && water[world.index(n)] == SurfaceWater::Lake)
                    return true;
            }
            return false;
        };

        auto &biome = world.geography().biome;
        biome.resize(world.width(), world.height(), Biome::None);
        for (int y = 0; y < world.height(); ++y)
        {
            for (int x = 0; x < world.width(); ++x)
            {
                const std::size_t i = biome.index(x, y);
                if (water[i] != SurfaceWater::Land)
                    continue;
                const float celsius = static_cast<float>(climate.meanAnnualTemperature[i]) / 10.0f;
                const float aridity = static_cast<float>(climate.moisture[i]) / 255.0f * 2.0f;
                Biome result = climateBiome(celsius, aridity);

                if (result != Biome::Ice && static_cast<float>(elevation[i]) >= settings.alpineMinElevationM &&
                    celsius < settings.alpineMaxC)
                {
                    result = Biome::Alpine;
                }
                else if (result != Biome::Ice && result != Biome::Tundra)
                {
                    const bool flat = slopeAt(world, {x, y}) <= settings.wetlandMaxSlope;
                    const bool wet = aridity >= settings.wetlandMinAridity;
                    if (soil[i] == SoilType::Peat ||
                        (flat && wet && (floodplain[i] >= settings.wetlandFloodplain || nextToLake({x, y}))))
                        result = Biome::Wetland;
                }
                biome[i] = result;
            }
        }
    }

} // namespace olam
