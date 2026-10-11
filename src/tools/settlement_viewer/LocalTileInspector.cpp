#include "tools/settlement_viewer/LocalTileInspector.h"

#include "settlement/ResourceState.h"
#include "settlement/SettlementMap.h"

#include <cmath>
#include <format>

namespace olam
{

    namespace
    {

        void describeForage(const SettlementMap &map, LocalCoord coord, std::vector<std::string> &lines, const ResourceState *state)
        {
            const auto f = map.forageAt(coord);
            if (!f)
                return;
            const ForageKind kind = map.forage().kind[*f];
            const ForageInfo &info = forageInfo(kind);
            const float maximum = map.forage().kilograms(*f);
            const float left = state ? state->forageRemaining(map, *f) : maximum;
            lines.push_back(std::format("Forage: {}   {:.1f} / {:.1f} kg", toString(kind), left, maximum));
            if (info.renewable)
                lines.push_back(std::format("  renewable, regrows {:.0f}%/year{}", info.regrowthPerYear * 100.0f,
                                            info.food ? std::format(", {:.0f} kcal/kg", info.kcalPerKg) : ""));
            else
                lines.push_back(std::format("  finite, replenished {:.0f}%/year", info.regrowthPerYear * 100.0f));
        }

        void describeSpring(const SettlementMap &map, LocalCoord coord, std::vector<std::string> &lines)
        {
            for (const LocalSpring &spring : map.springs())
            {
                if (spring.x == coord.x && spring.y == coord.y)
                    lines.push_back(std::string(toString(spring.kind)));
            }
        }

    } // namespace

    std::vector<std::string> describeLocalTile(const SettlementMap &map, Vec2 mapPosition, bool pinned, const ResourceState *state)
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
            describeSpring(map, coord, lines);
            describeForage(map, coord, lines, state);
            return lines;
        }
        lines.push_back(std::format("Ground: {}   cover {}%", toString(terrain.ground[i]), terrain.groundCover[i] * 100 / 255));
        lines.push_back(std::format("Soil: {}   fertility {}%", toString(terrain.soil[i]), terrain.fertility[i] * 100 / 255));
        if (terrain.resource[i] != LocalResource::None)
            lines.push_back(std::format("Resource: {}", toString(terrain.resource[i])));
        for (const MineralOccurrence &m : map.minerals())
        {
            const float dx = static_cast<float>(coord.x) + 0.5f - m.centerX;
            const float dy = static_cast<float>(coord.y) + 0.5f - m.centerY;
            const bool covers = m.exposed ? terrain.resource[i] == m.mineral : dx * dx + dy * dy <= m.radius * m.radius;
            if (!covers || dx * dx + dy * dy > 2.25f * m.radius * m.radius)
                continue;
            lines.push_back(std::format("{}{} ({})   grade {}   {} t in {} tiles", m.exposed ? "" : "Buried ",
                                        toString(m.mineral), toString(m.source), m.grade, m.tonnes, m.tileCount));
            break;
        }
        if (const TreeId tree = terrain.treeId[i]; tree.isValid())
        {
            const std::size_t t = tree.index();
            const float wood = treeWoodKg(map.trees().kind[t], map.trees().growth[t]);
            if (state && state->treeFelled(map, tree))
                lines.push_back(std::format("Stump of a {} (felled)", toString(map.trees().kind[t])));
            else
                lines.push_back(std::format("Tree #{}: {}   growth {}%   wood {:.0f} kg", t, toString(map.trees().kind[t]),
                                            map.trees().growth[t] * 100 / 255, wood));
        }
        if (const auto u = map.understoryAt(coord))
            lines.push_back(std::format("Understory: {}", toString(map.understory().kind[*u])));
        describeForage(map, coord, lines, state);
        return lines;
    }

} // namespace olam
