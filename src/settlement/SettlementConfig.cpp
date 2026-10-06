#include "settlement/SettlementConfig.h"

#include <cmath>
#include <format>

namespace olam
{

    std::int32_t localTilesPerWorldTile(const SettlementConfig &config, const WorldConfig &world)
    {
        return static_cast<std::int32_t>(std::floor(world.tileSizeMeters / config.tileSizeMeters + 0.5));
    }

    std::optional<std::string> validateSettlementConfig(const SettlementConfig &config, const WorldConfig &world)
    {
        if (config.width < 64 || config.height < 64 || config.width > 8192 || config.height > 8192)
            return std::format("local map size {} x {} outside [64, 8192]", config.width, config.height);
        if (!(config.tileSizeMeters >= 0.5 && config.tileSizeMeters <= 50.0))
            return "local tile size must be within [0.5, 50] m";
        const double ratio = world.tileSizeMeters / config.tileSizeMeters;
        if (ratio < 1.0 || std::fabs(ratio - std::floor(ratio + 0.5)) > 1e-9)
            return std::format("world tile size {} m is not a multiple of the local tile size {} m", world.tileSizeMeters,
                               config.tileSizeMeters);
        if (!(config.creekMinDischarge > 0.0f))
            return "creek discharge threshold must be positive";
        return std::nullopt;
    }

} // namespace olam
