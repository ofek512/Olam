#include "worldgen/passes/SoilPass.h"

#include "core/noise/Noise.h"
#include "world/World.h"
#include "world/queries/TerrainQueries.h"
#include "worldgen/WorkingLayers.h"
#include "worldgen/WorldGenContext.h"

namespace olam
{

    std::optional<std::string> SoilPass::validatePreconditions(const WorldGenContext &context) const
    {
        if (context.world().hydrology().discharge.empty() || !context.findWorkingLayer(worldgen::kFloodplain))
            return std::string("requires HydrologyPass");
        return std::nullopt;
    }

    void SoilPass::run(WorldGenContext &context)
    {
        World &world = context.world();
        const SoilSettings &settings = world.config().generation.soil;
        const auto &terrain = world.terrain();
        const auto &climate = world.climate();
        const auto &water = world.hydrology().surfaceWater;
        const Layer<float> &floodplain = *context.findWorkingLayer(worldgen::kFloodplain);
        const auto tileKm = static_cast<float>(world.config().tileSizeMeters / 1000.0);
        const noise::FractalParams params{3, 1.0f / settings.textureNoiseWavelengthKm, 2.0f, 0.5f};
        const std::uint64_t seed = context.seedFor(*this);

        auto &soil = world.geography().soil;
        soil.resize(world.width(), world.height(), SoilType::None);
        for (int y = 0; y < world.height(); ++y)
        {
            for (int x = 0; x < world.width(); ++x)
            {
                const std::size_t i = soil.index(x, y);
                if (water[i] != SurfaceWater::Land)
                    continue;

                const float celsius = static_cast<float>(climate.meanAnnualTemperature[i]) / 10.0f;
                const float aridity = static_cast<float>(climate.moisture[i]) / 255.0f * 2.0f;
                const float slope = slopeAt(world, {x, y});
                const RockType rock = terrain.rockType[i];
                const bool hardRock = rock == RockType::Igneous || rock == RockType::Metamorphic;

                SoilType type = SoilType::Loam;
                if (celsius <= settings.permafrostMaxC)
                {
                    type = SoilType::Permafrost;
                }
                else if (slope >= settings.rockySlope || (hardRock && slope >= settings.rockyHardRockSlope) ||
                         static_cast<float>(terrain.elevation[i]) >= settings.rockyElevationM)
                {
                    type = SoilType::Rocky;
                }
                else if (floodplain[i] >= settings.alluvialFloodplain)
                {
                    type = SoilType::Alluvial;
                }
                else if (aridity >= settings.peatMinAridity && slope <= settings.peatMaxSlope && celsius <= settings.peatMaxC)
                {
                    type = SoilType::Peat;
                }
                else if (celsius >= settings.lateriteMinC && aridity >= settings.lateriteMinAridity)
                {
                    type = SoilType::Laterite;
                }
                else if (aridity < settings.sandyMaxAridity)
                {
                    type = SoilType::Sandy;
                }
                else
                {
                    // Texture: sedimentary basins and wetter climates weather to clay; granite and dry land to sand.
                    const float rockBias = rock == RockType::Sedimentary ? 0.2f : (rock == RockType::Igneous ? -0.2f : 0.0f);
                    const float moistureBias = 0.3f * (aridity - 0.6f);
                    const float variation = settings.textureNoise *
                                            noise::fbm(seed, (static_cast<float>(x) + 0.5f) * tileKm,
                                                       (static_cast<float>(y) + 0.5f) * tileKm, params);
                    const float texture = rockBias + moistureBias + variation;
                    if (texture > 0.35f)
                        type = SoilType::Clay;
                    else if (texture < -0.35f)
                        type = SoilType::Sandy;
                }
                soil[i] = type;
            }
        }
    }

} // namespace olam
