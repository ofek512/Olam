#pragma once

#include "core/math/Vec2.h"

#include <string>
#include <vector>

namespace olam
{

    class SettlementMap;

    // Inspector lines for the local tile under a map-space position (units = local tiles). No SDL.
    std::vector<std::string> describeLocalTile(const SettlementMap &map, Vec2 mapPosition, bool pinned);

} // namespace olam
