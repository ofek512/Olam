#pragma once

#include "core/math/Vec2.h"

#include <string>
#include <vector>

namespace olam
{

    class SettlementMap;
    class ResourceState;

    // Inspector lines for the local tile under a map-space position (units = local tiles). No SDL. With `state`,
    // shows what is left after gathering and felling.
    std::vector<std::string> describeLocalTile(const SettlementMap &map, Vec2 mapPosition, bool pinned,
                                               const ResourceState *state = nullptr);

} // namespace olam
