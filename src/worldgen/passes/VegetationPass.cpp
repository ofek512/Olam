#include "worldgen/passes/VegetationPass.h"

#include "core/math/MathUtil.h"
#include "core/noise/Noise.h"
#include "world/World.h"
#include "world/queries/TerrainQueries.h"
#include "worldgen/WorldGenContext.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace olam
{

    namespace
    {

        // Typical canopy cover (%) of each biome under average conditions.
        float potentialTreeCover(Biome biome)
        {
            switch (biome)
            {
            case Biome::Tundra:
                return 2.0f;
            case Biome::BorealForest:
                return 60.0f;
            case Biome::TemperateRainforest:
                return 85.0f;
            case Biome::TemperateForest:
                return 70.0f;
            case Biome::TemperateGrassland:
                return 8.0f;
            case Biome::Shrubland:
                return 15.0f;
            case Biome::ColdDesert:
                return 1.0f;
            case Biome::Savanna:
                return 20.0f;
            case Biome::TropicalDryForest:
                return 55.0f;
            case Biome::TropicalRainforest:
                return 90.0f;
            case Biome::Alpine:
                return 3.0f;
            case Biome::Wetland:
                return 25.0f;
            case Biome::None:
            case Biome::Ice:
            case Biome::HotDesert:
            case Biome::Count:
                break;
            }
            return 0.0f;
        }

        VegetationType categorize(Biome biome, float cover, float aridity, float elevationM)
        {
            if (biome == Biome::Wetland)
                return VegetationType::Wetland;
            if (cover >= 75.0f)
                return VegetationType::DenseForest;
            if (cover >= 45.0f)
                return VegetationType::Forest;
            if (cover >= 20.0f)
                return VegetationType::LightForest;
            switch (biome)
            {
            case Biome::Ice:
                return VegetationType::Barren;
            case Biome::HotDesert:
            case Biome::ColdDesert:
                return aridity < 0.1f ? VegetationType::Barren : VegetationType::Scrub;
            case Biome::Alpine:
                return elevationM >= 3000.0f ? VegetationType::Barren : VegetationType::Grass;
            case Biome::Shrubland:
                return VegetationType::Scrub;
            default:
                return VegetationType::Grass;
            }
        }

    } // namespace

    std::optional<std::string> VegetationPass::validatePreconditions(const WorldGenContext &context) const
    {
        if (context.world().geography().biome.empty() || context.world().geography().fertility.empty())
            return std::string("requires BiomePass and FertilityPass");
        return std::nullopt;
    }

    void VegetationPass::run(WorldGenContext &context)
    {
        World &world = context.world();
        const VegetationSettings &settings = world.config().generation.vegetation;
        const auto &geography = world.geography();
        const auto &climate = world.climate();
        const auto &elevation = world.terrain().elevation;
        const auto tileKm = static_cast<float>(world.config().tileSizeMeters / 1000.0);
        const noise::FractalParams params{3, 1.0f / settings.noiseWavelengthKm, 2.0f, 0.5f};
        const std::uint64_t seed = context.seedFor(*this);
        noise::FractalSampler coverNoise(seed, params);

        Layer<VegetationType> vegetation(world.width(), world.height(), VegetationType::None);
        Layer<std::uint8_t> treeCover(world.width(), world.height(), 0);
        for (int y = 0; y < world.height(); ++y)
        {
            for (int x = 0; x < world.width(); ++x)
            {
                const std::size_t i = vegetation.index(x, y);
                const Biome biome = geography.biome[i];
                if (biome == Biome::None)
                    continue;
                const float aridity = static_cast<float>(climate.moisture[i]) / 255.0f * 2.0f;
                const float fertility = static_cast<float>(geography.fertility[i]) / 255.0f;
                constexpr CurvePoint kSlope[] = {{0.0f, 1.0f}, {0.15f, 1.0f}, {0.35f, 0.5f}};
                const float variation =
                    1.0f + settings.noiseAmplitude * coverNoise.fbm((static_cast<float>(x) + 0.5f) * tileKm,
                                                                    (static_cast<float>(y) + 0.5f) * tileKm);
                const float cover = std::clamp(potentialTreeCover(biome) * lerp(0.7f, 1.15f, clamp01(aridity / 1.2f)) *
                                                   (0.6f + 0.4f * fertility) * evaluateCurve(kSlope, slopeAt(world, {x, y})) *
                                                   variation,
                                               0.0f, 100.0f);
                const auto percent = static_cast<std::uint8_t>(std::floor(cover + 0.5f));
                treeCover[i] = percent;
                vegetation[i] = categorize(biome, static_cast<float>(percent), aridity, static_cast<float>(elevation[i]));
            }
        }
        world.geography().vegetation = std::move(vegetation);
        world.geography().treeCover = std::move(treeCover);
    }

} // namespace olam
