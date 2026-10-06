#pragma once

#include "world/WorldConfig.h"

#include <cstdint>
#include <optional>
#include <string>

namespace olam
{

    // Size and resolution of a local settlement map. The map is centred on the chosen world tile's centre.
    struct SettlementConfig
    {
        std::int32_t width = 3072;
        std::int32_t height = 3072;
        // Must divide the world tile size exactly, so all local maps share one global grid and line up.
        double tileSizeMeters = 2.0;
        // World drainage (m^3/s) from which flow shows as a creek; world rivers start at the stream threshold.
        float creekMinDischarge = 1.0f;
    };

    std::optional<std::string> validateSettlementConfig(const SettlementConfig &config, const WorldConfig &world);

    // Local tiles per world tile along one axis (exact when the config is valid).
    std::int32_t localTilesPerWorldTile(const SettlementConfig &config, const WorldConfig &world);

} // namespace olam
