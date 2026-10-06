#pragma once

#include "settlement/SettlementMap.h"

#include <memory>
#include <string>

namespace olam
{

    class World;

    struct LocalMapResult
    {
        std::unique_ptr<SettlementMap> map;
        // Set when the site cannot host a settlement; map is then null.
        std::string error;
    };

    // Detailed local map centred on world tile `site`, interpreted from the surrounding world (no second world
    // generation). Deterministic and seamless: every tile depends only on the world and its absolute position, so
    // maps of neighbouring sites agree where they overlap.
    LocalMapResult generateLocalMap(const World &world, WorldCoord site, const SettlementConfig &config = {});

} // namespace olam
