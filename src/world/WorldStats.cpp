#include "world/WorldStats.h"

#include "world/World.h"
#include "world/queries/HydrologyQueries.h"

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
            std::size_t counts[static_cast<std::size_t>(MineralType::Count)] = {};
            for (const Deposit &deposit : resources.deposits)
                ++counts[static_cast<std::size_t>(deposit.mineral)];
            // Two lines keep the stats panel narrow.
            std::string metals = std::format("Deposits {}  ", resources.deposits.size());
            std::string others = "        ";
            for (std::size_t m = 0; m < static_cast<std::size_t>(MineralType::Count); ++m)
            {
                std::string &line = m < static_cast<std::size_t>(MineralType::Stone) ? metals : others;
                line += std::format(" {} {}", toString(static_cast<MineralType>(m)), counts[m]);
            }
            lines.push_back(metals);
            lines.push_back(others);
        }
        return lines;
    }

} // namespace olam
