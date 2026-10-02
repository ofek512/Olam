#pragma once

#include "world/WorldTypes.h"

#include <cstddef>
#include <string>
#include <vector>

namespace olam
{

    class World;

    // Generation summary (land share, biomes, rivers, ...) for logs and the viewer stats panel.
    // Only covers data that exists in the world.
    std::vector<std::string> describeWorldStats(const World &world);

    // Share of mostly-land square blocks (blockKm wide, e.g. a settlement's surroundings) touching each mineral.
    struct DepositCoverage
    {
        double mineral[static_cast<std::size_t>(MineralType::Count)] = {};
        // Copper, tin, gold or silver.
        double nonIronMetal = 0.0;
        std::size_t blocks = 0;
    };

    DepositCoverage depositCoverage(const World &world, double blockKm);

} // namespace olam
