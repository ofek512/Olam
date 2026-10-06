#include "settlement/SiteSelection.h"

#include "world/World.h"
#include "world/queries/TerrainQueries.h"

#include <cmath>
#include <format>

namespace olam
{

    int localMapReachTiles(const SettlementConfig &config, const WorldConfig &world)
    {
        const double halfWidthM = config.width * config.tileSizeMeters / 2.0;
        const double halfHeightM = config.height * config.tileSizeMeters / 2.0;
        const double half = std::max(halfWidthM, halfHeightM) / world.tileSizeMeters;
        return std::max(0, static_cast<int>(std::ceil(half - 0.5)));
    }

    std::optional<std::string> siteError(const World &world, WorldCoord site, const SettlementConfig &config)
    {
        if (auto error = validateSettlementConfig(config, world.config()))
            return error;
        if (!world.isValid(site))
            return std::string("outside the world");
        if (world.resources().depositId.empty() || world.geography().vegetation.empty())
            return std::string("the world is not fully generated");
        const SurfaceWater water = world.hydrology().surfaceWater[world.index(site)];
        if (water != SurfaceWater::Land)
            return std::format("cannot settle on {}", water == SurfaceWater::Lake ? "a lake" : "the ocean");
        const int reach = localMapReachTiles(config, world.config());
        if (site.x < reach || site.y < reach || site.x >= world.width() - reach || site.y >= world.height() - reach)
            return std::format("too close to the edge of the world (the local map needs {} tiles on each side)", reach);
        return std::nullopt;
    }

    std::vector<std::string> siteWarnings(const World &world, WorldCoord site, const SettlementConfig &config)
    {
        std::vector<std::string> warnings;
        if (siteError(world, site, config))
            return warnings;
        const std::size_t i = world.index(site);
        const float celsius = static_cast<float>(world.climate().meanAnnualTemperature[i]) / 10.0f;
        const float aridity = static_cast<float>(world.climate().moisture[i]) / 255.0f * 2.0f;

        if (world.terrain().elevation[i] >= 2500)
            warnings.push_back(std::format("High altitude ({} m)", world.terrain().elevation[i]));
        if (celsius <= -4.0f)
            warnings.push_back(std::format("Very cold ({:.1f} C mean): frozen ground, short seasons", celsius));
        if (aridity < 0.2f)
            warnings.push_back("Arid: very little rain");
        if (slopeAt(world, site) > 0.08f)
            warnings.push_back("Steep terrain");
        if (world.geography().fertility[i] < 51)
            warnings.push_back("Poor land for farming");

        // Fresh water: a lake, river or creek within the local map.
        const int reach = localMapReachTiles(config, world.config());
        const auto creek = static_cast<std::uint32_t>(std::floor(config.creekMinDischarge * 100.0f + 0.5f));
        bool water = false;
        for (int y = site.y - reach; y <= site.y + reach && !water; ++y)
        {
            for (int x = site.x - reach; x <= site.x + reach && !water; ++x)
            {
                const std::size_t n = world.index({x, y});
                water = world.hydrology().surfaceWater[n] == SurfaceWater::Lake ||
                        (world.hydrology().surfaceWater[n] == SurfaceWater::Land && world.hydrology().discharge[n] >= creek);
            }
        }
        if (!water)
            warnings.push_back("No fresh water nearby");
        return warnings;
    }

} // namespace olam
