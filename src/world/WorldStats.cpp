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
        return lines;
    }

} // namespace olam
