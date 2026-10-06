#pragma once

#include "settlement/SettlementConfig.h"
#include "world/WorldCoord.h"

#include <optional>
#include <string>
#include <vector>

namespace olam
{

    class World;

    // World tiles the local map reaches beyond the site tile on each side.
    int localMapReachTiles(const SettlementConfig &config, const WorldConfig &world);

    // Why a settlement cannot be founded on `site` (water, too close to the world edge, world not generated);
    // nullopt when it can.
    std::optional<std::string> siteError(const World &world, WorldCoord site, const SettlementConfig &config);

    // Harsh conditions worth telling the player about; the site is still valid.
    std::vector<std::string> siteWarnings(const World &world, WorldCoord site, const SettlementConfig &config);

} // namespace olam
