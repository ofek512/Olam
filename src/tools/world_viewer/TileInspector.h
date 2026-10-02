#pragma once

#include "core/math/Vec2.h"

#include <string>
#include <vector>

namespace olam
{

    class World;

    // Inspector lines for the tile under a world-space position. Only shows data that exists in World.
    std::vector<std::string> describeTile(const World &world, Vec2 worldPosition);

} // namespace olam
