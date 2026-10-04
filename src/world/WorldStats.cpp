#include "world/WorldStats.h"

#include "world/World.h"
#include "world/queries/HydrologyQueries.h"
#include "world/queries/LandmassQueries.h"

#include <algorithm>
#include <format>

namespace olam
{

    std::vector<std::string> describeWorldStats(const World &world)
    {
        const WorldConfig &config = world.config();
        std::vector<std::string> lines;
        lines.push_back(std::format("Seed {}", world.seed()));
        lines.push_back(std::format("Map {} x {} tiles ({:.0f} x {:.0f} km)", config.width, config.height,
                                    config.width * config.tileSizeMeters / 1000.0,
                                    config.height * config.tileSizeMeters / 1000.0));

        const auto &elevation = world.terrain().elevation;
        if (!elevation.empty())
        {
            int highest = elevation[0];
            int deepest = elevation[0];
            std::size_t aboveSea = 0;
            for (const std::int16_t meters : elevation.values())
            {
                highest = std::max<int>(highest, meters);
                deepest = std::min<int>(deepest, meters);
                aboveSea += meters >= 0 ? 1u : 0u;
            }
            lines.push_back(std::format("Above sea level {:.1f} %   highest {} m   deepest {} m",
                                        100.0 * static_cast<double>(aboveSea) / static_cast<double>(elevation.size()),
                                        highest, deepest));
        }

        const auto &province = world.terrain().province;
        if (!province.empty() && !elevation.empty())
        {
            std::size_t counts[static_cast<std::size_t>(GeologicalProvince::Count)] = {};
            std::int64_t heights[static_cast<std::size_t>(GeologicalProvince::Count)] = {};
            std::size_t land = 0;
            for (std::size_t i = 0; i < province.size(); ++i)
            {
                if (elevation[i] < 0)
                    continue;
                ++land;
                ++counts[static_cast<std::size_t>(province[i])];
                heights[static_cast<std::size_t>(province[i])] += elevation[i];
            }
            std::string line = "Land provinces";
            for (std::size_t p = 1; p < static_cast<std::size_t>(GeologicalProvince::Count); ++p)
            {
                const double share = 100.0 * static_cast<double>(counts[p]) / static_cast<double>(std::max<std::size_t>(land, 1));
                const double meanHeight =
                    static_cast<double>(heights[p]) / static_cast<double>(std::max<std::size_t>(counts[p], 1));
                line += std::format("   {} {:.0f} % ({:.0f} m)", toString(static_cast<GeologicalProvince>(p)), share, meanHeight);
            }
            lines.push_back(line);
        }

        const auto &water = world.hydrology().surfaceWater;
        if (!water.empty())
        {
            std::size_t counts[static_cast<std::size_t>(SurfaceWater::Count)] = {};
            for (const SurfaceWater kind : water.values())
                ++counts[static_cast<std::size_t>(kind)];
            const auto percent = [&](SurfaceWater kind)
            { return 100.0 * static_cast<double>(counts[static_cast<std::size_t>(kind)]) / static_cast<double>(water.size()); };
            std::uint16_t farthest = 0;
            for (const std::uint16_t km : world.hydrology().distanceToOceanKm.values())
                farthest = std::max(farthest, km);
            lines.push_back(std::format("Land {:.1f} %   ocean {:.1f} %   lakes {:.1f} %   max {} km from sea",
                                        percent(SurfaceWater::Land), percent(SurfaceWater::Ocean),
                                        percent(SurfaceWater::Lake), farthest));

            const LandmassSummary landmasses = analyzeLandmasses(world);
            const auto count = [&](LandmassClass kind)
            { return landmasses.classCounts[static_cast<std::size_t>(kind)]; };
            lines.push_back(std::format("Landmasses {} (continents {}, large islands {}, islands {})   largest {:.0f} %   "
                                        "coast {:.0f} km   {}",
                                        landmasses.landmasses.size(), count(LandmassClass::Continent),
                                        count(LandmassClass::LargeIsland), count(LandmassClass::Island),
                                        landmasses.share(0) * 100.0, landmasses.coastlineKm, toString(landmasses.structure)));
        }

        const auto &rainfall = world.climate().annualRainfall;
        if (!rainfall.empty() && !water.empty())
        {
            const auto &moisture = world.climate().moisture;
            std::uint64_t landRain = 0;
            std::size_t landTiles = 0;
            std::size_t aridTiles = 0;
            std::uint16_t wettest = 0;
            for (std::size_t i = 0; i < rainfall.size(); ++i)
            {
                wettest = std::max(wettest, rainfall[i]);
                if (water[i] != SurfaceWater::Land)
                    continue;
                ++landTiles;
                landRain += rainfall[i];
                // Aridity index below 0.2 (255 = 2.0).
                aridTiles += moisture[i] < 26 ? 1u : 0u;
            }
            const double landCount = static_cast<double>(std::max<std::size_t>(landTiles, 1));
            lines.push_back(std::format("Land rain mean {:.0f} mm   arid land {:.1f} %   wettest {} mm",
                                        static_cast<double>(landRain) / landCount,
                                        100.0 * static_cast<double>(aridTiles) / landCount, wettest));
        }

        const HydrologyData &hydrology = world.hydrology();
        if (!hydrology.discharge.empty())
        {
            const HydrologySettings &settings = config.generation.hydrology;
            std::size_t counts[static_cast<std::size_t>(RiverClass::Count)] = {};
            std::size_t longest = 0;
            for (const River &river : hydrology.rivers)
            {
                ++counts[static_cast<std::size_t>(riverClassForDischarge(settings, river.mouthDischarge))];
                longest = std::max(longest, river.path.size());
            }
            const double largest = hydrology.rivers.empty() ? 0.0 : hydrology.rivers.front().mouthDischarge / 100.0;
            lines.push_back(std::format("Rivers {} (major {}, river {})   longest {:.0f} km   largest {:.0f} m3/s",
                                        hydrology.rivers.size(), counts[static_cast<std::size_t>(RiverClass::Major)],
                                        counts[static_cast<std::size_t>(RiverClass::River)],
                                        static_cast<double>(longest) * config.tileSizeMeters / 1000.0, largest));
            std::uint32_t largestLake = 0;
            for (const Lake &lake : hydrology.lakes)
                largestLake = std::max(largestLake, lake.tileCount);
            const double tileKm2 = config.tileSizeMeters * config.tileSizeMeters / 1.0e6;
            lines.push_back(std::format("Lakes {}   largest {:.0f} km2", hydrology.lakes.size(), largestLake * tileKm2));

            if (!hydrology.watersheds.empty())
            {
                std::size_t large = 0;
                for (const Watershed &watershed : hydrology.watersheds)
                    large += watershed.tileCount * tileKm2 >= 50000.0 ? 1u : 0u;
                const Watershed &biggest = hydrology.watersheds.front();
                lines.push_back(std::format("Watersheds {} (>= 50,000 km2: {})   largest {:.0f} km2, {:.0f} m3/s",
                                            hydrology.watersheds.size(), large, biggest.tileCount * tileKm2,
                                            biggest.outletDischarge / 100.0));
            }
        }

        const auto &biome = world.geography().biome;
        if (!biome.empty())
        {
            std::size_t counts[static_cast<std::size_t>(Biome::Count)] = {};
            std::size_t land = 0;
            for (const Biome value : biome.values())
            {
                ++counts[static_cast<std::size_t>(value)];
                land += value != Biome::None ? 1u : 0u;
            }
            // The four most common land biomes (ties: enum order).
            std::string line = "Biomes";
            for (int rank = 0; rank < 4; ++rank)
            {
                std::size_t best = 0;
                std::size_t bestCount = 0;
                for (std::size_t b = 1; b < static_cast<std::size_t>(Biome::Count); ++b)
                {
                    if (counts[b] > bestCount)
                    {
                        best = b;
                        bestCount = counts[b];
                    }
                }
                if (bestCount == 0)
                    break;
                line += std::format("   {} {:.0f} %", toString(static_cast<Biome>(best)),
                                    100.0 * static_cast<double>(bestCount) / static_cast<double>(std::max<std::size_t>(land, 1)));
                counts[best] = 0;
            }
            lines.push_back(line);
        }

        const auto &fertility = world.geography().fertility;
        if (!fertility.empty())
        {
            std::uint64_t sum = 0;
            std::size_t land = 0;
            std::size_t prime = 0;
            for (std::size_t i = 0; i < fertility.size(); ++i)
            {
                if (world.hydrology().surfaceWater[i] != SurfaceWater::Land)
                    continue;
                ++land;
                sum += fertility[i];
                prime += fertility[i] >= 179 ? 1u : 0u;
            }
            const double landCount = static_cast<double>(std::max<std::size_t>(land, 1));
            lines.push_back(std::format("Fertility mean {:.0f} %   prime land (>= 70 %) {:.1f} %",
                                        static_cast<double>(sum) / landCount / 2.55,
                                        100.0 * static_cast<double>(prime) / landCount));
        }

        const auto &vegetation = world.geography().vegetation;
        if (!vegetation.empty())
        {
            std::size_t land = 0;
            std::size_t forest = 0;
            std::uint64_t cover = 0;
            for (std::size_t i = 0; i < vegetation.size(); ++i)
            {
                if (vegetation[i] == VegetationType::None)
                    continue;
                ++land;
                cover += world.geography().treeCover[i];
                const VegetationType kind = vegetation[i];
                forest += kind == VegetationType::LightForest || kind == VegetationType::Forest ||
                                  kind == VegetationType::DenseForest
                              ? 1u
                              : 0u;
            }
            const double landCount = static_cast<double>(std::max<std::size_t>(land, 1));
            lines.push_back(std::format("Forested land {:.1f} %   mean tree cover {:.0f} %",
                                        100.0 * static_cast<double>(forest) / landCount,
                                        static_cast<double>(cover) / landCount));
        }

        const ResourceData &resources = world.resources();
        if (!resources.depositId.empty())
        {
            std::size_t minerals[static_cast<std::size_t>(MineralType::Count)] = {};
            std::size_t origins[static_cast<std::size_t>(DepositOrigin::Count)] = {};
            for (const Deposit &deposit : resources.deposits)
            {
                ++minerals[static_cast<std::size_t>(deposit.mineral)];
                ++origins[static_cast<std::size_t>(deposit.origin)];
            }
            std::string byMineral = std::format("Deposits {}  ", resources.deposits.size());
            for (std::size_t m = 0; m < static_cast<std::size_t>(MineralType::Count); ++m)
                byMineral += std::format(" {} {}", toString(static_cast<MineralType>(m)), minerals[m]);
            std::string byOrigin = "        ";
            for (std::size_t o = 0; o < static_cast<std::size_t>(DepositOrigin::Count); ++o)
                byOrigin += std::format(" {} {}", toString(static_cast<DepositOrigin>(o)), origins[o]);
            lines.push_back(byMineral);
            lines.push_back(byOrigin);

            const DepositCoverage coverage = depositCoverage(world, 64.0);
            std::string covered = std::format("64 km blocks with:");
            for (std::size_t m = 0; m < static_cast<std::size_t>(MineralType::Count); ++m)
                covered += std::format(" {} {:.0f}%", toString(static_cast<MineralType>(m)), coverage.mineral[m] * 100.0);
            covered += std::format("  other metal {:.0f}%", coverage.nonIronMetal * 100.0);
            lines.push_back(covered);
        }
        return lines;
    }

    DepositCoverage depositCoverage(const World &world, double blockKm)
    {
        DepositCoverage result;
        const ResourceData &resources = world.resources();
        const auto &water = world.hydrology().surfaceWater;
        if (resources.depositId.empty() || water.empty())
            return result;

        const int blockTiles = std::max(1, static_cast<int>(std::floor(blockKm * 1000.0 / world.config().tileSizeMeters + 0.5)));
        const int blocksX = (world.width() + blockTiles - 1) / blockTiles;
        const int blocksY = (world.height() + blockTiles - 1) / blockTiles;
        const auto blockCount = static_cast<std::size_t>(blocksX) * static_cast<std::size_t>(blocksY);
        std::vector<std::uint32_t> land(blockCount, 0);
        std::vector<std::uint32_t> tiles(blockCount, 0);
        std::vector<std::uint8_t> present(blockCount, 0);
        for (int y = 0; y < world.height(); ++y)
        {
            for (int x = 0; x < world.width(); ++x)
            {
                const std::size_t block = static_cast<std::size_t>(y / blockTiles) * static_cast<std::size_t>(blocksX) +
                                          static_cast<std::size_t>(x / blockTiles);
                const std::size_t i = world.index({x, y});
                ++tiles[block];
                land[block] += water[i] == SurfaceWater::Land ? 1u : 0u;
                const DepositId id = resources.depositId[i];
                if (id.isValid())
                    present[block] |= static_cast<std::uint8_t>(1u << static_cast<unsigned>(resources.deposits[id.index()].mineral));
            }
        }

        // Only mostly-land blocks count: a settlement area is mainly land.
        constexpr std::uint8_t kOtherMetals = (1u << static_cast<unsigned>(MineralType::Copper)) |
                                              (1u << static_cast<unsigned>(MineralType::Tin)) |
                                              (1u << static_cast<unsigned>(MineralType::Gold)) |
                                              (1u << static_cast<unsigned>(MineralType::Silver));
        std::size_t counts[static_cast<std::size_t>(MineralType::Count)] = {};
        std::size_t otherMetal = 0;
        for (std::size_t b = 0; b < blockCount; ++b)
        {
            if (land[b] * 2 < tiles[b])
                continue;
            ++result.blocks;
            for (std::size_t m = 0; m < static_cast<std::size_t>(MineralType::Count); ++m)
                counts[m] += (present[b] >> m) & 1u;
            otherMetal += (present[b] & kOtherMetals) != 0 ? 1u : 0u;
        }
        const double blocks = static_cast<double>(std::max<std::size_t>(result.blocks, 1));
        for (std::size_t m = 0; m < static_cast<std::size_t>(MineralType::Count); ++m)
            result.mineral[m] = static_cast<double>(counts[m]) / blocks;
        result.nonIronMetal = static_cast<double>(otherMetal) / blocks;
        return result;
    }

} // namespace olam
