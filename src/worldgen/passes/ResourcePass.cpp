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
        constexpr std::size_t kOriginCount = static_cast<std::size_t>(DepositOrigin::Count);
        constexpr std::size_t kProvinceCount = static_cast<std::size_t>(GeologicalProvince::Count);

        // Chance (before district noise) that a land tile of each province hosts ore veins.
        constexpr float kVeinPotential[kProvinceCount] = {
            0.0f,  // Oceanic
            0.03f, // Basin: bedrock buried under sediments
            0.5f,  // Shield
            1.0f,  // Ancient orogen
            1.0f,  // Active orogen
            0.3f,  // Rift
        };

        // Relative vein metal mix per province: Iron, Copper, Tin, Coal, Gold, Silver, Salt.
        constexpr float kVeinMetals[kProvinceCount][kMineralCount] = {
            {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // Oceanic
            {1.0f, 1.0f, 0.0f, 0.0f, 0.2f, 0.6f, 0.0f}, // Basin
            {3.0f, 1.0f, 0.5f, 0.0f, 3.0f, 0.5f, 0.0f}, // Shield
            {2.0f, 2.0f, 2.0f, 0.0f, 1.5f, 3.0f, 0.0f}, // Ancient orogen (Erzgebirge, Bohemia, Cornwall)
            {2.0f, 4.0f, 0.5f, 0.0f, 2.0f, 2.0f, 0.0f}, // Active orogen (porphyry copper, epithermal gold/silver)
            {1.0f, 2.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.0f}, // Rift
        };

        // Vein exposure and access: worn uplands are best, flat lowlands are mostly covered by soil.
        float veinExposure(TerrainClass terrain)
        {
            switch (terrain)
            {
            case TerrainClass::Plains:
                return 0.6f;
            case TerrainClass::Hills:
                return 1.0f;
            case TerrainClass::Plateau:
            case TerrainClass::Mountains:
                return 0.9f;
            }
            return 0.0f;
        }

        // Beds lie in lowlands and gentle hills.
        float bedExposure(TerrainClass terrain)
        {
            switch (terrain)
            {
            case TerrainClass::Plains:
            case TerrainClass::Plateau:
                return 0.8f;
            case TerrainClass::Hills:
                return 1.0f;
            case TerrainClass::Mountains:
                return 0.3f;
            }
            return 0.0f;
        }

        struct Range
        {
            float min;
            float max;
        };

        // Places deposits as clusters and enforces spacing between deposits of the same mineral and origin.
        class DepositPlacer
        {
        public:
            DepositPlacer(World &world, float spacingTiles)
                : m_world(world), m_resources(world.resources()), m_spacingSquared(spacingTiles * spacingTiles)
            {
            }

            bool isFree(std::size_t index) const
            {
                return m_world.hydrology().surfaceWater[index] == SurfaceWater::Land &&
                       !m_resources.depositId[index].isValid();
            }

            bool crowded(MineralType mineral, DepositOrigin origin, WorldCoord center) const
            {
                const auto &centers = m_centers[static_cast<std::size_t>(origin)][static_cast<std::size_t>(mineral)];
                return std::any_of(centers.begin(), centers.end(), [&](WorldCoord other)
                                   {
                                       const auto dx = static_cast<float>(other.x - center.x);
                                       const auto dy = static_cast<float>(other.y - center.y);
                                       return dx * dx + dy * dy < m_spacingSquared; });
            }

            // Grows breadth-first from `center` over free land tiles accepted by `accept`, up to `size` tiles.
            template <typename Accept>
            const Deposit &place(MineralType mineral, DepositOrigin origin, std::size_t center, std::size_t size,
                                 float richness, Accept accept)
            {
                Deposit deposit;
                deposit.id = DepositId::fromIndex(m_resources.deposits.size());
                deposit.mineral = mineral;
                deposit.origin = origin;
                deposit.center = m_world.coordFromIndex(center);
                deposit.richness = static_cast<std::uint8_t>(std::clamp(std::floor(richness + 0.5f), 1.0f, 100.0f));

                m_cluster.clear();
                m_cluster.push_back(static_cast<std::uint32_t>(center));
                m_resources.depositId[center] = deposit.id;
                for (std::size_t head = 0; head < m_cluster.size() && m_cluster.size() < size; ++head)
                {
                    const WorldCoord coord = m_world.coordFromIndex(m_cluster[head]);
                    for (std::size_t d = 0; d < kDirection8Count && m_cluster.size() < size; ++d)
                    {
                        const WorldCoord n = neighbor(coord, static_cast<Direction8>(d));
                        if (!m_world.isValid(n))
                            continue;
                        const std::size_t ni = m_world.index(n);
                        if (!isFree(ni) || !accept(ni))
                            continue;
                        m_resources.depositId[ni] = deposit.id;
                        m_cluster.push_back(static_cast<std::uint32_t>(ni));
                    }
                }
                deposit.tileCount = static_cast<std::uint32_t>(m_cluster.size());
                m_centers[static_cast<std::size_t>(origin)][static_cast<std::size_t>(mineral)].push_back(deposit.center);
                m_resources.deposits.push_back(deposit);
                return m_resources.deposits.back();
            }

        private:
            World &m_world;
            ResourceData &m_resources;
            float m_spacingSquared;
            std::vector<WorldCoord> m_centers[kOriginCount][kMineralCount];
            std::vector<std::uint32_t> m_cluster;
        };

        // Draws random land tiles, accepts each with probability(index) and lets chooseMineral pick the mineral
        // (MineralType::Count rejects). Stops after `target` deposits or a bounded number of attempts.
        template <typename Probability, typename ChooseMineral>
        void scatter(World &world, DepositPlacer &placer, Pcg32 &rng, std::size_t target, DepositOrigin origin,
                     Range tiles, Range richness, Probability probability, ChooseMineral chooseMineral)
        {
            const auto tileCount = static_cast<std::uint32_t>(world.tileCount());
            // Rare settings (wetlands, salt basins) need many draws; rejected draws are cheap.
            const std::size_t attempts = target * 400 + 100;
            std::size_t placed = 0;
            for (std::size_t attempt = 0; attempt < attempts && placed < target; ++attempt)
            {
                const std::size_t index = rng.nextBounded(tileCount);
                const float roll = rng.nextFloat01();
                if (!placer.isFree(index))
                    continue;
                const float p = probability(index);
                if (!(roll < p))
                    continue;
                const MineralType mineral = chooseMineral(index);
                if (mineral == MineralType::Count || placer.crowded(mineral, origin, world.coordFromIndex(index)))
                    continue;
                const auto size = static_cast<std::size_t>(
                    rng.nextInt(static_cast<std::int32_t>(tiles.min), static_cast<std::int32_t>(tiles.max)));
                const float grade = lerp(richness.min, richness.max, p * (0.5f + 0.5f * rng.nextFloat01()));
                placer.place(mineral, origin, index, size, grade, [&](std::size_t n)
                             { return probability(n) >= 0.5f * p; });
                ++placed;
            }
        }

        float kmCoord(int tile, float tileKm)
        {
            return (static_cast<float>(tile) + 0.5f) * tileKm;
        }

    } // namespace

    std::optional<std::string> ResourcePass::validatePreconditions(const WorldGenContext &context) const
    {
        const World &world = context.world();
        if (world.geography().vegetation.empty() || world.terrain().province.empty() ||
            world.hydrology().flowDirection.empty())
            return std::string("requires TectonicsPass, HydrologyPass and VegetationPass");
        return std::nullopt;
    }

    void ResourcePass::run(WorldGenContext &context)
    {
        World &world = context.world();
        const ResourceSettings &settings = world.config().generation.resources;
        const std::uint64_t seed = context.seedFor(*this);
        const auto tileKm = static_cast<float>(world.config().tileSizeMeters / 1000.0);
        const auto &terrain = world.terrain();
        const auto &climate = world.climate();
        const auto &hydrology = world.hydrology();
        const auto &geography = world.geography();

        std::size_t landTiles = 0;
        for (const SurfaceWater kind : hydrology.surfaceWater.values())
            landTiles += kind == SurfaceWater::Land ? 1u : 0u;
        const double landKm2 = static_cast<double>(landTiles) * tileKm * tileKm;
        const auto targetFor = [&](float perMillionKm2)
        { return static_cast<std::size_t>(std::floor(perMillionKm2 * settings.densityScale * landKm2 / 1.0e6 + 0.5)); };

        ResourceData &resources = world.resources();
        resources.depositId.resize(world.width(), world.height(), DepositId{});
        resources.deposits.clear();
        DepositPlacer placer(world, settings.minSpacingKm / tileKm);

        const auto celsiusAt = [&](std::size_t i)
        { return static_cast<float>(climate.meanAnnualTemperature[i]) / 10.0f; };
        const auto aridityAt = [&](std::size_t i)
        { return static_cast<float>(climate.moisture[i]) / 255.0f * 2.0f; };
        const auto terrainAt = [&](std::size_t i)
        { return terrainClassAt(world, world.coordFromIndex(i)); };
        const auto slopeOf = [&](std::size_t i)
        { return slopeAt(world, world.coordFromIndex(i)); };
        const auto nextToWater = [&](std::size_t i, SurfaceWater kind)
        {
            const WorldCoord coord = world.coordFromIndex(i);
            for (std::size_t d = 0; d < kDirection8Count; ++d)
            {
                const WorldCoord n = neighbor(coord, static_cast<Direction8>(d));
                if (world.isValid(n) && hydrology.surfaceWater[world.index(n)] == kind)
                    return true;
            }
            return false;
        };
        // Regional noise in 0..1 (districts and basins), evaluated at a tile.
        const auto regional = [&](noise::FractalSampler &sampler, std::size_t i, float from, float to)
        {
            const WorldCoord coord = world.coordFromIndex(i);
            return smoothstep(from, to, sampler.fbm(kmCoord(coord.x, tileKm), kmCoord(coord.y, tileKm)));
        };

        // 1. Hydrothermal veins: orogens (young and ancient), shields and granite, clustered into ore districts.
        noise::FractalSampler districts(deriveSeed(seed, olam::seedId("DISTRICT")),
                                        {2, 1.0f / settings.provinceWavelengthKm, 2.0f, 0.5f});
        const auto veinPotential = [&](std::size_t i)
        {
            const auto province = static_cast<std::size_t>(terrain.province[i]);
            const float granite = terrain.rockType[i] == RockType::Igneous ? 1.3f : 1.0f;
            return std::min(1.0f, kVeinPotential[province] * granite) * veinExposure(terrainAt(i)) *
                   regional(districts, i, -0.3f, 0.3f);
        };
        {
            Pcg32 rng(deriveSeed(seed, olam::seedId("VEINS")));
            scatter(world, placer, rng, targetFor(settings.veinsPerMillionKm2), DepositOrigin::Vein, {2, 8}, {25, 100},
                    veinPotential, [&](std::size_t i)
                    {
                        float weights[kMineralCount];
                        float total = 0.0f;
                        for (std::size_t m = 0; m < kMineralCount; ++m)
                        {
                            weights[m] = kVeinMetals[static_cast<std::size_t>(terrain.province[i])][m];
                            // Tin forms around granite intrusions.
                            if (m == static_cast<std::size_t>(MineralType::Tin) && terrain.rockType[i] == RockType::Igneous)
                                weights[m] *= 3.0f;
                            total += weights[m];
                        }
                        float pick = rng.nextFloat01() * total;
                        for (std::size_t m = 0; m < kMineralCount; ++m)
                        {
                            if (weights[m] > 0.0f && pick < weights[m])
                                return static_cast<MineralType>(m);
                            pick -= weights[m];
                        }
                        return MineralType::Count; });
        }

        // 2. Placers: gold and tin eroded from veins settle in river beds downstream.
        {
            Pcg32 rng(deriveSeed(seed, olam::seedId("PLACER")));
            const std::size_t veinCount = resources.deposits.size();
            for (std::size_t v = 0; v < veinCount; ++v)
            {
                const Deposit vein = resources.deposits[v];
                if (vein.mineral != MineralType::Gold && vein.mineral != MineralType::Tin)
                    continue;
                std::size_t current = world.index(vein.center);
                float travelled = 0.0f;
                float sinceLast = settings.placerSpacingKm;
                int placed = 0;
                while (travelled < settings.placerMaxKm && placed < 3)
                {
                    const Direction8 direction = hydrology.flowDirection[current];
                    if (direction == kNoFlow)
                        break;
                    const WorldCoord next = neighbor(world.coordFromIndex(current), direction);
                    if (!world.isValid(next) || hydrology.surfaceWater[world.index(next)] != SurfaceWater::Land)
                        break;
                    const float step = tileKm * (isDiagonal(direction) ? 1.41421356f : 1.0f);
                    travelled += step;
                    sinceLast += step;
                    current = world.index(next);
                    if (!hydrology.riverId[current].isValid() || !placer.isFree(current) ||
                        sinceLast < settings.placerSpacingKm || slopeOf(current) > 0.05f)
                        continue;
                    if (rng.nextFloat01() >= 0.5f)
                        continue;
                    const float grade = static_cast<float>(vein.richness) * (0.3f + 0.4f * rng.nextFloat01());
                    const auto size = static_cast<std::size_t>(rng.nextInt(1, 3));
                    placer.place(vein.mineral, DepositOrigin::Placer, current, size, grade, [&](std::size_t n)
                                 { return hydrology.riverId[n].isValid(); });
                    sinceLast = 0.0f;
                    ++placed;
                }
            }
        }

        // 3. Bedded deposits: coal measures and ironstone in sedimentary basins (former swamps and shallow seas),
        //    banded iron in old shields. Independent of today's climate.
        noise::FractalSampler coalBasins(deriveSeed(seed, olam::seedId("COALBSN")), {2, 1.0f / 300.0f, 2.0f, 0.5f});
        noise::FractalSampler ironBeds(deriveSeed(seed, olam::seedId("IRONBED")), {2, 1.0f / 300.0f, 2.0f, 0.5f});
        {
            Pcg32 rng(deriveSeed(seed, olam::seedId("COAL")));
            scatter(world, placer, rng, targetFor(settings.coalPerMillionKm2), DepositOrigin::Bedded, {6, 20}, {30, 100}, [&](std::size_t i)
                    {
                        if (terrain.province[i] != GeologicalProvince::Basin)
                            return 0.0f;
                        return bedExposure(terrainAt(i)) * regional(coalBasins, i, -0.1f, 0.3f); }, [](std::size_t)
                    { return MineralType::Coal; });
        }
        {
            Pcg32 rng(deriveSeed(seed, olam::seedId("IRONSTN")));
            scatter(world, placer, rng, targetFor(settings.ironstonePerMillionKm2), DepositOrigin::Bedded, {3, 10}, {20, 70}, [&](std::size_t i)
                    {
                        if (terrain.province[i] != GeologicalProvince::Basin)
                            return 0.0f;
                        return bedExposure(terrainAt(i)) * regional(ironBeds, i, -0.4f, 0.2f); }, [](std::size_t)
                    { return MineralType::Iron; });
        }
        {
            Pcg32 rng(deriveSeed(seed, olam::seedId("BANDED")));
            scatter(world, placer, rng, targetFor(settings.bandedIronPerMillionKm2), DepositOrigin::Bedded, {6, 16}, {60, 100}, [&](std::size_t i)
                    {
                        if (terrain.province[i] != GeologicalProvince::Shield)
                            return 0.0f;
                        return regional(ironBeds, i, 0.0f, 0.4f); }, [](std::size_t)
                    { return MineralType::Iron; });
        }

        // 4. Rock salt and brine springs from evaporated ancient seas, in any climate.
        noise::FractalSampler saltBasins(deriveSeed(seed, olam::seedId("SALTBSN")), {2, 1.0f / 400.0f, 2.0f, 0.5f});
        {
            Pcg32 rng(deriveSeed(seed, olam::seedId("ROCKSALT")));
            scatter(world, placer, rng, targetFor(settings.rockSaltPerMillionKm2), DepositOrigin::Evaporite, {3, 10}, {30, 90}, [&](std::size_t i)
                    {
                        if (terrain.province[i] != GeologicalProvince::Basin)
                            return 0.0f;
                        return bedExposure(terrainAt(i)) * regional(saltBasins, i, 0.1f, 0.45f); }, [](std::size_t)
                    { return MineralType::Salt; });
        }

        // 5. Salt pans: sunny, dry-ish low coasts and desert playas.
        {
            Pcg32 rng(deriveSeed(seed, olam::seedId("SALTPAN")));
            scatter(world, placer, rng, targetFor(settings.saltPansPerMillionKm2), DepositOrigin::SaltPan, {2, 8}, {20, 70}, [&](std::size_t i)
                    {
                        if (slopeOf(i) > 0.02f)
                            return 0.0f;
                        const float aridity = aridityAt(i);
                        float p = 0.0f;
                        if (terrain.elevation[i] < 30 && celsiusAt(i) >= 12.0f && nextToWater(i, SurfaceWater::Ocean))
                            p = clamp01((1.2f - aridity) / 0.8f);
                        if (aridity < 0.2f && terrainAt(i) == TerrainClass::Plains)
                            p = std::max(p, 1.0f - aridity / 0.2f);
                        return p; }, [](std::size_t)
                    { return MineralType::Salt; });
        }

        // 6. Bog iron: iron precipitated in cool wetlands, peat bogs and lake shores; many small, low-grade deposits.
        {
            Pcg32 rng(deriveSeed(seed, olam::seedId("BOGIRON")));
            scatter(world, placer, rng, targetFor(settings.bogIronPerMillionKm2), DepositOrigin::Bog, {1, 4}, {10, 45}, [&](std::size_t i)
                    {
                        float wet = 0.0f;
                        if (geography.biome[i] == Biome::Wetland)
                            wet = 1.0f;
                        else if (geography.soil[i] == SoilType::Peat)
                            wet = 0.8f;
                        else if (nextToWater(i, SurfaceWater::Lake))
                            wet = 0.6f;
                        else if (geography.soil[i] == SoilType::Alluvial && aridityAt(i) >= 0.9f)
                            wet = 0.3f;
                        if (wet <= 0.0f)
                            return 0.0f;
                        const float celsius = celsiusAt(i);
                        const float window = smoothstep(-6.0f, -2.0f, celsius) * (1.0f - smoothstep(14.0f, 20.0f, celsius));
                        return wet * window * (slopeOf(i) <= 0.01f ? 1.0f : 0.3f); }, [](std::size_t)
                    { return MineralType::Iron; });
        }
    }

} // namespace olam
