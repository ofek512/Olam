#pragma once

#include "core/math/Vec2.h"

#include <string>
#include <vector>

namespace olam
{

    class World;

    struct InspectorOptions
    {
        bool pinned = false;
        bool showDebugHash = false;
    };

    // Inspector lines for the tile under a world-space position. Only shows data that exists in World.
    std::vector<std::string> describeTile(const World &world, Vec2 worldPosition, const InspectorOptions &options);

} // namespace olam
