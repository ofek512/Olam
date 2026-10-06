#include "tools/settlement_viewer/LocalTileInspector.h"

#include "settlement/SettlementMap.h"

#include <cmath>
#include <format>

namespace olam
{

    std::vector<std::string> describeLocalTile(const SettlementMap &map, Vec2 mapPosition, bool pinned)
    {
        const LocalCoord coord{static_cast<std::int32_t>(std::floor(mapPosition.x)),
                               static_cast<std::int32_t>(std::floor(mapPosition.y))};
        std::vector<std::string> lines;
        lines.push_back(pinned ? "LOCAL TILE (pinned, LMB to release)" : "LOCAL TILE (LMB to pin)");
        if (!map.isValid(coord))
        {
            lines.push_back("outside the map");
            return lines;
        }

        const std::size_t i = map.index(coord);
        const LocalTerrainData &terrain = map.terrain();
        const double tileM = map.config().tileSizeMeters;
        lines.push_back(std::format("Tile ({}, {})   {:.0f} x {:.0f} m from NW corner", coord.x, coord.y, coord.x * tileM,
                                    coord.y * tileM));
        lines.push_back(std::format("Elevation {:.1f} m", map.elevationMeters(i)));
        if (terrain.water[i] != LocalWater::None)
        {
            lines.push_back(std::format("Water: {}", toString(terrain.water[i])));
            lines.push_back(std::format("Bed: {}", toString(terrain.ground[i])));
            return lines;
        }
        lines.push_back(std::format("Ground: {}   cover {}%", toString(terrain.ground[i]), terrain.groundCover[i] * 100 / 255));
        lines.push_back(std::format("Soil: {}   fertility {}%", toString(terrain.soil[i]), terrain.fertility[i] * 100 / 255));
        if (terrain.resource[i] != LocalResource::None)
            lines.push_back(std::format("Resource: {}", toString(terrain.resource[i])));
        if (const TreeId tree = terrain.treeId[i]; tree.isValid())
            lines.push_back(std::format("Tree #{}   growth {}%", tree.index(), map.trees().growth[tree.index()] * 100 / 255));
        return lines;
    }

} // namespace olam
