#include "worldgen/passes/ResourcePass.h"

#include "core/math/MathUtil.h"
#include "core/noise/Noise.h"
#include "core/random/Pcg32.h"
#include "core/random/Seed.h"
#include "world/World.h"
#include "world/queries/TerrainQueries.h"
#include "worldgen/WorldGenContext.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace olam
{

    namespace
    {

        constexpr std::size_t kMineralCount = static_cast<std::size_t>(MineralType::Count);

        struct MineralProfile
        {
            // Sedimentary, igneous, metamorphic.
            float rock[3];
            // Plains, hills, plateau, mountains.
            float terrain[4];
            // Base deposits per million km^2 of land.
            float density;
        };

        constexpr MineralProfile kProfiles[kMineralCount] = {
            {{0.6f, 0.4f, 1.0f}, {0.5f, 1.0f, 0.9f, 0.8f}, 40.0f},  // Iron
            {{0.15f, 1.0f, 0.6f}, {0.1f, 0.7f, 0.6f, 1.0f}, 25.0f}, // Copper
            {{0.05f, 1.0f, 0.5f}, {0.2f, 1.0f, 0.8f, 0.8f}, 8.0f},  // Tin
            {{1.0f, 0.0f, 0.2f}, {0.8f, 1.0f, 0.7f, 0.3f}, 30.0f},  // Coal
            {{0.1f, 1.0f, 0.8f}, {0.1f, 0.6f, 0.6f, 1.0f}, 8.0f},   // Gold
            {{0.1f, 1.0f, 0.7f}, {0.1f, 0.7f, 0.6f, 1.0f}, 10.0f},  // Silver
            {{1.0f, 0.9f, 1.0f}, {0.3f, 1.0f, 0.9f, 1.0f}, 60.0f},  // Stone
            {{1.0f, 0.5f, 0.5f}, {1.0f, 0.4f, 0.3f, 0.0f}, 50.0f},  // Clay
            {{1.0f, 0.2f, 0.2f}, {1.0f, 0.4f, 0.6f, 0.1f}, 15.0f},  // Salt
        };

        float clayFactor(SoilType soil)
        {
            switch (soil)
            {
            case SoilType::Clay:
                return 1.0f;
            case SoilType::Alluvial:
                return 0.8f;
            case SoilType::Loam:
                return 0.4f;
            default:
                return 0.1f;
            }
        }

    } // namespace

    float mineralSuitability(const World &world, MineralType mineral, std::size_t index)
    {
        if (world.hydrology().surfaceWater[index] != SurfaceWater::Land)
            return 0.0f;
        const MineralProfile &profile = kProfiles[static_cast<std::size_t>(mineral)];
        const WorldCoord coord = world.coordFromIndex(index);
        float value = profile.rock[static_cast<std::size_t>(world.terrain().rockType[index])] *
                      profile.terrain[static_cast<std::size_t>(terrainClassAt(world, coord))];
        if (mineral == MineralType::Clay)
            value *= clayFactor(world.geography().soil[index]);
        if (mineral == MineralType::Salt)
        {
            // Evaporites form in dry basins.
            const float aridity = static_cast<float>(world.climate().moisture[index]) / 255.0f * 2.0f;
            value *= inverseLerp(0.5f, 0.1f, aridity);
        }
        return value;
    }

    std::optional<std::string> ResourcePass::validatePreconditions(const WorldGenContext &context) const
    {
        if (context.world().geography().soil.empty())
            return std::string("requires SoilPass");
        return std::nullopt;
    }

    void ResourcePass::run(WorldGenContext &context)
    {
        World &world = context.world();
        const ResourceSettings &settings = world.config().generation.resources;
        const std::uint64_t seed = context.seedFor(*this);
        const auto tileKm = static_cast<float>(world.config().tileSizeMeters / 1000.0);
        const std::size_t tileCount = world.tileCount();
        const auto &water = world.hydrology().surfaceWater;

        std::size_t landTiles = 0;
        for (const SurfaceWater kind : water.values())
            landTiles += kind == SurfaceWater::Land ? 1u : 0u;
        const double landKm2 = static_cast<double>(landTiles) * tileKm * tileKm;

        ResourceData &resources = world.resources();
        resources.depositId.resize(world.width(), world.height(), DepositId{});
        resources.deposits.clear();

        const float spacingTiles = settings.minSpacingKm / tileKm;
        const float spacingSquared = spacingTiles * spacingTiles;
        const noise::FractalParams provinceParams{2, 1.0f / settings.provinceWavelengthKm, 2.0f, 0.5f};
        std::vector<std::uint32_t> cluster;

        for (std::size_t m = 0; m < kMineralCount; ++m)
        {
            const auto mineral = static_cast<MineralType>(m);
            const std::uint64_t mineralSeed = deriveSeed(seed, m + 1);
            const std::uint64_t provinceSeed = deriveSeed(mineralSeed, olam::seedId("PROVINCE"));
            Pcg32 rng(mineralSeed);

            const auto target = static_cast<std::size_t>(
                std::floor(kProfiles[m].density * settings.densityScale * landKm2 / 1.0e6 + 0.5));
            const std::size_t attempts = target * 50 + 50;
            std::vector<WorldCoord> centers;

            const auto probability = [&](std::size_t index)
            {
                const WorldCoord coord = world.coordFromIndex(index);
                const float province = smoothstep(-0.2f, 0.4f,
                                                  noise::fbm(provinceSeed, (static_cast<float>(coord.x) + 0.5f) * tileKm,
                                                             (static_cast<float>(coord.y) + 0.5f) * tileKm, provinceParams));
                return mineralSuitability(world, mineral, index) * province;
            };

            for (std::size_t attempt = 0; attempt < attempts && centers.size() < target; ++attempt)
            {
                const std::size_t index = rng.nextBounded(static_cast<std::uint32_t>(tileCount));
                const float roll = rng.nextFloat01();
                if (water[index] != SurfaceWater::Land || resources.depositId[index].isValid())
                    continue;
                const float p = probability(index);
                if (!(roll < p))
                    continue;
                const WorldCoord center = world.coordFromIndex(index);
                const bool crowded = std::any_of(centers.begin(), centers.end(), [&](WorldCoord other)
                                                 {
                                                     const auto dx = static_cast<float>(other.x - center.x);
                                                     const auto dy = static_cast<float>(other.y - center.y);
                                                     return dx * dx + dy * dy < spacingSquared; });
                if (crowded)
                    continue;

                Deposit deposit;
                deposit.id = DepositId::fromIndex(resources.deposits.size());
                deposit.mineral = mineral;
                deposit.center = center;
                const auto size = static_cast<std::size_t>(rng.nextInt(settings.minDepositTiles, settings.maxDepositTiles));
                const float richness = 100.0f * p * (0.5f + 0.5f * rng.nextFloat01());
                deposit.richness = static_cast<std::uint8_t>(std::clamp(std::floor(richness + 0.5f), 1.0f, 100.0f));

                // Grow over neighbouring land that suits the mineral at least half as well as the centre.
                const float centerSuitability = mineralSuitability(world, mineral, index);
                cluster.clear();
                cluster.push_back(static_cast<std::uint32_t>(index));
                resources.depositId[index] = deposit.id;
                for (std::size_t head = 0; head < cluster.size() && cluster.size() < size; ++head)
                {
                    const WorldCoord coord = world.coordFromIndex(cluster[head]);
                    for (std::size_t d = 0; d < kDirection8Count && cluster.size() < size; ++d)
                    {
                        const WorldCoord n = neighbor(coord, static_cast<Direction8>(d));
                        if (!world.isValid(n))
                            continue;
                        const std::size_t ni = world.index(n);
                        if (resources.depositId[ni].isValid() ||
                            mineralSuitability(world, mineral, ni) < 0.5f * centerSuitability)
                            continue;
                        resources.depositId[ni] = deposit.id;
                        cluster.push_back(static_cast<std::uint32_t>(ni));
                    }
                }
                deposit.tileCount = static_cast<std::uint32_t>(cluster.size());
                centers.push_back(center);
                resources.deposits.push_back(deposit);
            }
        }
    }

} // namespace olam
