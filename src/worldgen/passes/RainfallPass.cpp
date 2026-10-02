#include "worldgen/passes/RainfallPass.h"

#include "core/math/MathUtil.h"
#include "core/noise/Noise.h"
#include "world/World.h"
#include "worldgen/WorldGenContext.h"
#include "worldgen/util/Blur.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace olam
{

    namespace
    {

        // Global circulation: wet equator (ITCZ), dry subtropics (~30 deg), wet mid-latitude storm tracks, dry poles.
        constexpr std::array<CurvePoint, 10> kRainBelts = {{
            {0.0f, 1.6f},
            {10.0f, 1.4f},
            {20.0f, 0.7f},
            {30.0f, 0.45f},
            {40.0f, 0.9f},
            {50.0f, 1.15f},
            {60.0f, 1.0f},
            {70.0f, 0.6f},
            {80.0f, 0.4f},
            {90.0f, 0.3f},
        }};

        // Moisture the air can carry, rising with temperature (simplified Clausius-Clapeyron).
        float moistureCapacity(float celsius)
        {
            return std::clamp(0.35f + 0.045f * (celsius + 10.0f), 0.2f, 2.2f);
        }

        // Trade winds and polar easterlies blow from the east; mid-latitude westerlies from the west.
        bool windFromWest(float absoluteLatitude)
        {
            return absoluteLatitude >= 30.0f && absoluteLatitude < 60.0f;
        }

    } // namespace

    float potentialEvaporationMm(float celsius)
    {
        return std::clamp(300.0f + 45.0f * celsius, 150.0f, 1700.0f);
    }

    std::optional<std::string> RainfallPass::validatePreconditions(const WorldGenContext &context) const
    {
        if (context.world().climate().meanAnnualTemperature.empty())
            return std::string("requires TemperaturePass");
        return std::nullopt;
    }

    void RainfallPass::run(WorldGenContext &context)
    {
        World &world = context.world();
        const RainfallSettings &settings = world.config().generation.rainfall;
        const std::uint64_t seed = context.seedFor(*this);
        const int width = world.width();
        const int height = world.height();
        const auto tileKm = static_cast<float>(world.config().tileSizeMeters / 1000.0);
        const auto &elevation = world.terrain().elevation;
        const auto &water = world.hydrology().surfaceWater;
        const auto &temperature = world.climate().meanAnnualTemperature;

        const float pickup = std::min(1.0f, settings.oceanEvaporationPer100Km * tileKm / 100.0f);
        const float landRain = std::min(1.0f, settings.landRainPerKm * tileKm);
        const float seaRain = std::min(1.0f, settings.seaRainPerKm * tileKm);

        // Rain intensity in moisture units per km, before latitude belts and noise.
        Layer<float> intensity(width, height);
        for (int y = 0; y < height; ++y)
        {
            const bool fromWest = windFromWest(static_cast<float>(std::fabs(world.latitudeAt(y))));
            const int start = fromWest ? 0 : width - 1;
            const int step = fromWest ? 1 : -1;

            const std::size_t first = intensity.index(start, y);
            const float startCapacity = moistureCapacity(static_cast<float>(temperature[first]) / 10.0f);
            float carried = water[first] == SurfaceWater::Ocean ? startCapacity : settings.edgeInflow * startCapacity;
            float previousHeight = static_cast<float>(std::max<int>(elevation[first], 0));

            for (int x = start; x >= 0 && x < width; x += step)
            {
                const std::size_t i = intensity.index(x, y);
                const float capacity = moistureCapacity(static_cast<float>(temperature[i]) / 10.0f);
                if (water[i] != SurfaceWater::Land)
                {
                    carried += (capacity - carried) * pickup;
                    const float rain = carried * seaRain;
                    carried -= rain;
                    intensity[i] = rain / tileKm;
                    previousHeight = 0.0f;
                    continue;
                }

                const float ground = static_cast<float>(std::max<int>(elevation[i], 0));
                const float rise = std::max(0.0f, ground - previousHeight);
                previousHeight = ground;

                // Cold air holds less: excess condenses immediately.
                float rain = std::max(0.0f, carried - capacity);
                carried -= rain;
                const float fraction = std::min(0.8f, landRain + rise / settings.orographicRiseM);
                const float shower = carried * fraction;
                carried -= shower * (1.0f - settings.recycling);
                rain += shower;
                intensity[i] = rain / tileKm;
            }
        }

        boxBlur(intensity, std::max(1, static_cast<int>(settings.blurKm / tileKm * 0.5f)), 2);

        const noise::FractalParams noiseParams{3, 1.0f / settings.noiseWavelengthKm, 2.0f, 0.5f};
        auto &climate = world.climate();
        climate.annualRainfall.resize(width, height, 0);
        climate.moisture.resize(width, height, 0);
        for (int y = 0; y < height; ++y)
        {
            const float belt = evaluateCurve(kRainBelts, static_cast<float>(std::fabs(world.latitudeAt(y))));
            for (int x = 0; x < width; ++x)
            {
                const std::size_t i = intensity.index(x, y);
                const float kmX = (static_cast<float>(x) + 0.5f) * tileKm;
                const float kmY = (static_cast<float>(y) + 0.5f) * tileKm;
                const float variation = 1.0f + settings.noiseAmplitude * noise::fbm(seed, kmX, kmY, noiseParams);
                const float mm = std::clamp(intensity[i] * settings.mmScale * belt * variation, 0.0f, 65535.0f);
                climate.annualRainfall[i] = static_cast<std::uint16_t>(std::floor(mm + 0.5f));

                const float aridity = mm / potentialEvaporationMm(static_cast<float>(temperature[i]) / 10.0f);
                climate.moisture[i] = static_cast<std::uint8_t>(std::floor(clamp01(aridity * 0.5f) * 255.0f + 0.5f));
            }
        }
    }

} // namespace olam
