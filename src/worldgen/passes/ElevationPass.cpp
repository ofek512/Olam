#include "worldgen/passes/ElevationPass.h"

#include "core/math/MathUtil.h"
#include "core/noise/Noise.h"
#include "core/random/Pcg32.h"
#include "core/random/Seed.h"
#include "world/World.h"
#include "worldgen/WorkingLayers.h"
#include "worldgen/WorldGenContext.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace olam
{

    namespace
    {

        // Height (m) by percentile within land / within ocean: most land low, few high peaks; shelves near coasts.
        constexpr std::array<CurvePoint, 8> kLandCurve = {{
            {0.00f, 0.0f},
            {0.30f, 120.0f},
            {0.60f, 350.0f},
            {0.80f, 750.0f},
            {0.90f, 1300.0f},
            {0.96f, 2200.0f},
            {0.99f, 3300.0f},
            {1.00f, 4500.0f},
        }};

        constexpr std::array<CurvePoint, 7> kOceanCurve = {{
            {0.00f, -4500.0f},
            {0.05f, -4200.0f},
            {0.40f, -3800.0f},
            {0.70f, -3000.0f},
            {0.85f, -1500.0f},
            {0.93f, -200.0f},
            {1.00f, -1.0f},
        }};

        constexpr int kHistogramBins = 16384;

        void thermalSmooth(Layer<float> &height, int iterations, float talus)
        {
            const int width = height.width();
            const int h = height.height();
            Layer<float> next(width, h);
            for (int iteration = 0; iteration < iterations; ++iteration)
            {
                for (int y = 0; y < h; ++y)
                {
                    for (int x = 0; x < width; ++x)
                    {
                        const float centre = height.at(x, y);
                        const float average = 0.25f * (height.at(std::max(x - 1, 0), y) + height.at(std::min(x + 1, width - 1), y) +
                                                       height.at(x, std::max(y - 1, 0)) + height.at(x, std::min(y + 1, h - 1)));
                        const float excess = centre - average - talus;
                        next.at(x, y) = excess > 0.0f ? centre - 0.5f * excess : centre;
                    }
                }
                std::swap(height, next);
            }
        }

        void applyHypsometry(const Layer<float> &raw, float landFraction, Layer<std::int16_t> &elevation)
        {
            float minValue = raw[0];
            float maxValue = raw[0];
            for (const float v : raw.values())
            {
                minValue = std::min(minValue, v);
                maxValue = std::max(maxValue, v);
            }
            const float range = maxValue - minValue;
            const float scale = range > 0.0f ? static_cast<float>(kHistogramBins - 1) / range : 0.0f;

            std::vector<std::uint32_t> counts(kHistogramBins, 0);
            for (const float v : raw.values())
                ++counts[static_cast<std::size_t>((v - minValue) * scale)];

            std::vector<std::uint64_t> before(kHistogramBins, 0);
            std::uint64_t running = 0;
            for (int b = 0; b < kHistogramBins; ++b)
            {
                before[static_cast<std::size_t>(b)] = running;
                running += counts[static_cast<std::size_t>(b)];
            }

            const auto total = static_cast<double>(raw.size());
            const float seaThreshold = 1.0f - landFraction;
            for (std::size_t i = 0; i < raw.size(); ++i)
            {
                const float position = (raw[i] - minValue) * scale;
                const auto bin = static_cast<std::size_t>(position);
                const float fraction = position - static_cast<float>(bin);
                const auto percentile = static_cast<float>(
                    (static_cast<double>(before[bin]) + static_cast<double>(fraction) * counts[bin]) / total);

                float meters;
                if (percentile >= seaThreshold)
                    meters = std::max(0.0f, evaluateCurve(kLandCurve, (percentile - seaThreshold) / landFraction));
                else
                    meters = std::min(-1.0f, evaluateCurve(kOceanCurve, percentile / seaThreshold));
                elevation[i] = static_cast<std::int16_t>(std::floor(meters + 0.5f));
            }
        }

        // Land components (8-connected) smaller than minTiles become shallow sea.
        void removeSpecks(Layer<std::int16_t> &elevation, int minTiles)
        {
            if (minTiles <= 1)
                return;
            const int width = elevation.width();
            std::vector<std::uint8_t> visited(elevation.size(), 0);
            std::vector<std::uint32_t> component;

            for (std::size_t start = 0; start < elevation.size(); ++start)
            {
                if (visited[start] || elevation[start] < 0)
                    continue;
                component.clear();
                component.push_back(static_cast<std::uint32_t>(start));
                visited[start] = 1;
                for (std::size_t head = 0; head < component.size(); ++head)
                {
                    const int cx = static_cast<int>(component[head] % static_cast<std::uint32_t>(width));
                    const int cy = static_cast<int>(component[head] / static_cast<std::uint32_t>(width));
                    for (int dy = -1; dy <= 1; ++dy)
                    {
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            const int nx = cx + dx;
                            const int ny = cy + dy;
                            if ((dx == 0 && dy == 0) || !elevation.contains(nx, ny))
                                continue;
                            const std::size_t n = elevation.index(nx, ny);
                            if (!visited[n] && elevation[n] >= 0)
                            {
                                visited[n] = 1;
                                component.push_back(static_cast<std::uint32_t>(n));
                            }
                        }
                    }
                }
                if (component.size() < static_cast<std::size_t>(minTiles))
                {
                    for (const std::uint32_t tile : component)
                        elevation[tile] = -1;
                }
            }
        }

    } // namespace

    std::optional<std::string> ElevationPass::validatePreconditions(const WorldGenContext &context) const
    {
        if (context.world().terrain().plateId.empty() || !context.findWorkingLayer(worldgen::kPlateBase))
            return std::string("requires TectonicsPass");
        return std::nullopt;
    }

    void ElevationPass::run(WorldGenContext &context)
    {
        World &world = context.world();
        const ElevationSettings &settings = world.config().generation.elevation;
        const std::uint64_t seed = context.seedFor(*this);
        const int width = world.width();
        const int height = world.height();
        const auto tileKm = static_cast<float>(world.config().tileSizeMeters / 1000.0);

        const Layer<float> &base = *context.findWorkingLayer(worldgen::kPlateBase);
        const Layer<float> &uplift = *context.findWorkingLayer(worldgen::kUplift);
        const Layer<float> &rift = *context.findWorkingLayer(worldgen::kRift);

        const std::uint64_t detailSeed = deriveSeed(seed, olam::seedId("DETAIL"));
        const std::uint64_t ridgeSeed = deriveSeed(seed, olam::seedId("RIDGES"));
        const noise::FractalParams detailParams{settings.noiseOctaves, 1.0f / settings.noiseWavelengthKm, 2.0f, 0.5f};
        const noise::FractalParams ridgeParams{4, 1.0f / 300.0f, 2.0f, 0.5f};
        noise::FractalSampler detailNoise(detailSeed, detailParams);
        noise::FractalSampler ridgeNoise(ridgeSeed, ridgeParams);

        Layer<float> raw(width, height);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const std::size_t i = raw.index(x, y);
                const float kmX = (static_cast<float>(x) + 0.5f) * tileKm;
                const float kmY = (static_cast<float>(y) + 0.5f) * tileKm;
                float h = base[i] + settings.noiseStrength * detailNoise.fbm(kmX, kmY);
                if (uplift[i] > 0.0f)
                {
                    const float ridges = 0.35f + 0.65f * ridgeNoise.ridged(kmX, kmY);
                    h += settings.mountainStrength * uplift[i] * ridges;
                }
                h -= settings.riftStrength * rift[i];
                raw[i] = h;
            }
        }

        thermalSmooth(raw, settings.smoothingIterations, settings.smoothingTalus);

        Pcg32 rng(deriveSeed(seed, olam::seedId("LANDFRAC")));
        const float variation = settings.landFractionVariation * (2.0f * rng.nextFloat01() - 1.0f);
        const float landFraction = std::clamp(settings.landFraction + variation, 0.05f, 0.95f);

        Layer<std::int16_t> &elevation = world.terrain().elevation;
        elevation.resize(width, height, 0);
        applyHypsometry(raw, landFraction, elevation);
        removeSpecks(elevation, settings.minIslandTiles);
    }

} // namespace olam
