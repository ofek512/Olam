#include "worldgen/passes/TemperaturePass.h"

#include "core/math/MathUtil.h"
#include "core/noise/Noise.h"
#include "world/World.h"
#include "worldgen/WorldGenContext.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace olam
{

    namespace
    {

        // Sea-level mean annual temperature (C) by absolute latitude, roughly Earth-like.
        constexpr std::array<CurvePoint, 10> kLatitudeTemperature = {{
            {0.0f, 27.0f},
            {10.0f, 26.5f},
            {20.0f, 24.5f},
            {30.0f, 19.5f},
            {40.0f, 13.5f},
            {50.0f, 6.5f},
            {60.0f, 0.0f},
            {70.0f, -8.0f},
            {80.0f, -17.0f},
            {90.0f, -25.0f},
        }};

    } // namespace

    std::optional<std::string> TemperaturePass::validatePreconditions(const WorldGenContext &context) const
    {
        if (context.world().hydrology().distanceToOceanKm.empty())
            return std::string("requires OceanPass");
        return std::nullopt;
    }

    void TemperaturePass::run(WorldGenContext &context)
    {
        World &world = context.world();
        const TemperatureSettings &settings = world.config().generation.temperature;
        const std::uint64_t seed = context.seedFor(*this);
        const auto &elevation = world.terrain().elevation;
        const auto &distance = world.hydrology().distanceToOceanKm;
        const auto tileKm = static_cast<float>(world.config().tileSizeMeters / 1000.0);
        const noise::FractalParams noiseParams{3, 1.0f / settings.noiseWavelengthKm, 2.0f, 0.5f};

        auto &temperature = world.climate().meanAnnualTemperature;
        temperature.resize(world.width(), world.height(), 0);

        for (int y = 0; y < world.height(); ++y)
        {
            const auto latitude = static_cast<float>(std::fabs(world.latitudeAt(y)));
            const float seaLevelTemperature = evaluateCurve(kLatitudeTemperature, latitude);
            // Continentality barely matters in the tropics.
            const float continentality = settings.continentalCooling * smoothstep(15.0f, 55.0f, latitude);

            for (int x = 0; x < world.width(); ++x)
            {
                const std::size_t i = temperature.index(x, y);
                const float altitudeKm = static_cast<float>(std::max<int>(elevation[i], 0)) / 1000.0f;
                const float inland = std::min(1.0f, static_cast<float>(distance[i]) / 1000.0f);
                const float kmX = (static_cast<float>(x) + 0.5f) * tileKm;
                const float kmY = (static_cast<float>(y) + 0.5f) * tileKm;

                const float celsius = seaLevelTemperature - settings.lapseRatePerKm * altitudeKm - continentality * inland +
                                      settings.noiseAmplitude * noise::fbm(seed, kmX, kmY, noiseParams);
                const float tenths = std::clamp(std::floor(celsius * 10.0f + 0.5f), -800.0f, 600.0f);
                temperature[i] = static_cast<std::int16_t>(tenths);
            }
        }
    }

} // namespace olam
