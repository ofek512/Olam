#include "tools/settlement_viewer/LocalTileInspector.h"

#include "settlement/ConstructionState.h"
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

        std::string describeStructure(const Structure &s)
        {
            std::string text = std::format("{} ({}", toString(s.material), toString(s.stage));
            if (s.stage == ConstructionStage::UnderConstruction)
                text += std::format(" {}/{}", s.workDone, structureProperties(s.kind, s.material).workRequired);
            if (s.order.isValid())
                text += std::format(", order #{}", s.order.value);
            return text + ")";
        }

        void describeRoom(const ConstructionState &construction, LocalCoord coord, double tileM, std::vector<std::string> &lines)
        {
            const RoomId id = construction.rooms().roomAt(coord);
            const Room *room = construction.rooms().room(id);
            if (!room)
                return;
            const auto tiles = room->tiles.size();
            lines.push_back(std::format("Room #{} (building #{}): {} tiles, {:.0f} m2, {} door(s)", id.value, room->building.value, tiles,
                                        static_cast<double>(tiles) * tileM * tileM, room->doors));
            if (room->roofed())
                lines.push_back(std::format("Roof: {} ({})", toString(room->roof.material), toString(room->roof.mode)));
            else if (room->roof.mode == RoofMode::Auto)
                lines.push_back("Roof: none (auto: too wide for its walls)");
            else
                lines.push_back(std::format("Roof: none ({})", toString(room->roof.mode)));
        }

        void describeConstruction(const ConstructionState &construction, LocalCoord coord, double tileM, std::vector<std::string> &lines)
        {
            describeRoom(construction, coord, tileM, lines);
            if (const Structure *floor = construction.structure(construction.floorAt(coord)))
                lines.push_back("Floor: " + describeStructure(*floor));
            constexpr const char *kSides[] = {"N", "E", "S", "W"};
            const auto edges = tileEdges(coord);
            for (std::size_t side = 0; side < edges.size(); ++side)
            {
                const Structure *s = construction.structure(construction.edgeAt(edges[side]));
                if (!s)
                    continue;
                std::string swing;
                if (s->kind == StructureKind::Door)
                {
                    const bool towardsFirst = s->flipped;
                    swing = s->axis == EdgeAxis::North ? (towardsFirst ? ", opens N" : ", opens S") : (towardsFirst ? ", opens W" : ", opens E");
                }
                lines.push_back(std::format("{} {}: {}{}", kSides[side], toString(s->kind), describeStructure(*s), swing));
            }
        }

    } // namespace

    std::vector<std::string> describeLocalTile(const SettlementMap &map, Vec2 mapPosition, bool pinned, const ResourceState *state,
                                               const ConstructionState *construction)
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
        if (construction)
            describeConstruction(*construction, coord, tileM, lines);
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
            lines.push_back(std::format("Understory: {}{}", toString(map.understory().kind[*u]),
                                        state && state->understoryCleared(map, *u) ? " (cleared)" : ""));
        describeForage(map, coord, lines, state);
        return lines;
    }

} // namespace olam
