#include "world/queries/AgricultureQueries.h"

#include "core/math/MathUtil.h"
#include "world/World.h"
#include "world/queries/TerrainQueries.h"

namespace olam
{

    AgricultureSuitability agricultureAt(const World &world, WorldCoord coord)
    {
        AgricultureSuitability result;
        const std::size_t i = world.index(coord);
        if (world.geography().fertility.empty() || world.hydrology().surfaceWater[i] != SurfaceWater::Land)
            return result;

        const float fertility = static_cast<float>(world.geography().fertility[i]) / 255.0f;
        const float celsius = static_cast<float>(world.climate().meanAnnualTemperature[i]) / 10.0f;
        const float aridity = static_cast<float>(world.climate().moisture[i]) / 255.0f * 2.0f;
        const float slope = slopeAt(world, coord);
        const SoilType soil = world.geography().soil[i];

        // Grain: fertile, flat, temperate to warm.
        constexpr CurvePoint kGrainTemperature[] = {{3.0f, 0.0f}, {8.0f, 1.0f}, {24.0f, 1.0f}, {30.0f, 0.4f}};
        constexpr CurvePoint kGrainSlope[] = {{0.0f, 1.0f}, {0.04f, 0.8f}, {0.1f, 0.2f}, {0.15f, 0.0f}};
        result.grain = fertility * evaluateCurve(kGrainTemperature, celsius) * evaluateCurve(kGrainSlope, slope);

        // Livestock: grazing on drier or hillier land than crops need; less dependent on soil.
        constexpr CurvePoint kPastureMoisture[] = {{0.05f, 0.0f}, {0.25f, 0.7f}, {0.5f, 1.0f}, {1.4f, 1.0f}, {2.0f, 0.6f}};
        constexpr CurvePoint kPastureSlope[] = {{0.0f, 1.0f}, {0.1f, 0.8f}, {0.25f, 0.2f}, {0.35f, 0.0f}};
        constexpr CurvePoint kPastureTemperature[] = {{-6.0f, 0.0f}, {0.0f, 0.6f}, {5.0f, 1.0f}, {28.0f, 0.8f}};
        result.livestock = (0.4f + 0.6f * fertility) * evaluateCurve(kPastureMoisture, aridity) *
                           evaluateCurve(kPastureSlope, slope) * evaluateCurve(kPastureTemperature, celsius);

        // Orchards and vineyards: warm, well drained, tolerate moderate slopes.
        constexpr CurvePoint kOrchardTemperature[] = {{6.0f, 0.0f}, {11.0f, 0.8f}, {16.0f, 1.0f}, {24.0f, 0.8f}, {30.0f, 0.3f}};
        constexpr CurvePoint kOrchardSlope[] = {{0.0f, 1.0f}, {0.12f, 0.9f}, {0.25f, 0.2f}};
        const float drainage = (soil == SoilType::Clay || soil == SoilType::Peat) ? 0.5f : 1.0f;
        result.orchard = (0.3f + 0.7f * fertility) * evaluateCurve(kOrchardTemperature, celsius) *
                         evaluateCurve(kOrchardSlope, slope) * drainage * inverseLerp(0.1f, 0.5f, aridity);
        return result;
    }

} // namespace olam
