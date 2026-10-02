#include "world/WorldStats.h"

#include "world/World.h"

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
        return lines;
    }

} // namespace olam
