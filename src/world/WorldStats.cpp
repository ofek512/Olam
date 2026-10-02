#include "world/WorldStats.h"

#include "world/World.h"

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
        return lines;
    }

} // namespace olam
