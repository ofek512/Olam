#include "worldgen/passes/FertilityPass.h"

#include "core/math/MathUtil.h"
#include "world/World.h"
#include "world/queries/TerrainQueries.h"
#include "worldgen/WorkingLayers.h"
#include "worldgen/WorldGenContext.h"

#include <algorithm>
#include <cmath>

namespace olam
{

    namespace fertility
    {

        float temperatureFactor(float celsius)
        {
            constexpr CurvePoint kCurve[] = {{-5.0f, 0.0f}, {0.0f, 0.2f}, {5.0f, 0.6f}, {10.0f, 0.95f}, {15.0f, 1.0f}, {22.0f, 0.95f}, {28.0f, 0.75f}};
            return evaluateCurve(kCurve, celsius);
        }

        float moistureFactor(float aridity)
        {
            constexpr CurvePoint kCurve[] = {{0.0f, 0.0f}, {0.1f, 0.1f}, {0.3f, 0.45f}, {0.6f, 0.85f}, {1.0f, 1.0f}, {1.6f, 0.85f}, {2.0f, 0.7f}};
            return evaluateCurve(kCurve, aridity);
        }

        float terrainFactor(float slope, float elevationM)
        {
            constexpr CurvePoint kSlope[] = {{0.0f, 1.0f}, {0.03f, 0.9f}, {0.08f, 0.55f}, {0.15f, 0.2f}, {0.25f, 0.05f}};
            constexpr CurvePoint kElevation[] = {{1500.0f, 1.0f}, {3000.0f, 0.3f}, {4500.0f, 0.1f}};
            return evaluateCurve(kSlope, slope) * evaluateCurve(kElevation, elevationM);
        }

        float soilFactor(SoilType soil)
        {
            switch (soil)
            {
            case SoilType::Rocky:
                return 0.2f;
            case SoilType::Sandy:
                return 0.5f;
            case SoilType::Loam:
                return 1.0f;
            case SoilType::Clay:
                return 0.75f;
            case SoilType::Alluvial:
                return 1.0f;
            case SoilType::Peat:
                return 0.35f;
            case SoilType::Permafrost:
                return 0.05f;
            case SoilType::Laterite:
                return 0.45f;
            case SoilType::None:
            case SoilType::Count:
                break;
            }
            return 0.0f;
        }

    } // namespace fertility

    std::optional<std::string> FertilityPass::validatePreconditions(const WorldGenContext &context) const
    {
        if (context.world().geography().soil.empty() || !context.findWorkingLayer(worldgen::kFloodplain))
            return std::string("requires SoilPass and HydrologyPass");
        return std::nullopt;
    }

    void FertilityPass::run(WorldGenContext &context)
    {
        World &world = context.world();
        const FertilitySettings &settings = world.config().generation.fertility;
        const auto &elevation = world.terrain().elevation;
        const auto &climate = world.climate();
        const auto &water = world.hydrology().surfaceWater;
        const auto &soil = world.geography().soil;
        const Layer<float> &floodplain = *context.findWorkingLayer(worldgen::kFloodplain);

        auto &result = world.geography().fertility;
        result.resize(world.width(), world.height(), 0);
        for (int y = 0; y < world.height(); ++y)
        {
            for (int x = 0; x < world.width(); ++x)
            {
                const std::size_t i = result.index(x, y);
                if (water[i] != SurfaceWater::Land)
                    continue;
                const float celsius = static_cast<float>(climate.meanAnnualTemperature[i]) / 10.0f;
                const float aridity = static_cast<float>(climate.moisture[i]) / 255.0f * 2.0f;
                const float moisture =
                    std::max(fertility::moistureFactor(aridity), settings.riverIrrigation * floodplain[i]);
                const float value = fertility::temperatureFactor(celsius) * moisture *
                                    fertility::terrainFactor(slopeAt(world, {x, y}), static_cast<float>(elevation[i])) *
                                    fertility::soilFactor(soil[i]) * (1.0f + settings.floodplainBonus * floodplain[i]);
                result[i] = static_cast<std::uint8_t>(std::floor(clamp01(value) * 255.0f + 0.5f));
            }
        }
    }

} // namespace olam
