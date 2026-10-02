#pragma once

#include <string>
#include <vector>

namespace olam
{

    class World;

    // Generation summary (land share, biomes, rivers, ...) for logs and the viewer stats panel.
    // Only covers data that exists in the world.
    std::vector<std::string> describeWorldStats(const World &world);

} // namespace olam
