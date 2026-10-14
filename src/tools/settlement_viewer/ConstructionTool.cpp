#include "tools/settlement_viewer/ConstructionTool.h"

#include "settlement/ResourceState.h"
#include "settlement/SettlementMap.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace olam
{

    namespace
    {

        // Rectangle drags (floors, removal) are capped so the per-frame preview stays small.
        constexpr int kMaxRectTiles = 128;

        LocalCoord tileAt(Vec2 position)
        {
            return {static_cast<std::int32_t>(std::floor(position.x)), static_cast<std::int32_t>(std::floor(position.y))};
        }

        // `to`, moved towards `from` so that the rectangle they span is at most kMaxRectTiles wide and high.
        LocalCoord clampRect(LocalCoord from, LocalCoord to)
        {
            return {std::clamp(to.x, from.x - (kMaxRectTiles - 1), from.x + (kMaxRectTiles - 1)),
                    std::clamp(to.y, from.y - (kMaxRectTiles - 1), from.y + (kMaxRectTiles - 1))};
        }

        PreviewItem itemOf(const Structure &structure)
        {
            return {structure.kind, structure.edge(), PlacementError::None, structure.furniture, structure.facing};
        }

    } // namespace

    std::string_view toString(BuildTool tool)
    {
        switch (tool)
        {
        case BuildTool::Wall:
            return "Wall";
        case BuildTool::Door:
            return "Door";
        case BuildTool::Floor:
            return "Floor";
        case BuildTool::Remove:
            return "Remove";
        case BuildTool::Furniture:
            return "Furniture";
        case BuildTool::Stockpile:
            return "Stockpile";
        case BuildTool::Count:
            break;
        }
        return "?";
    }

    LocalCoord nearestCorner(Vec2 position)
    {
        return {static_cast<std::int32_t>(std::floor(position.x + 0.5f)), static_cast<std::int32_t>(std::floor(position.y + 0.5f))};
    }

    LocalEdge nearestEdge(Vec2 position)
    {
        const LocalCoord tile = tileAt(position);
        const float fx = position.x - static_cast<float>(tile.x);
        const float fy = position.y - static_cast<float>(tile.y);
        // Ties go to north, west, south, east in that order.
        LocalEdge edge{tile.x, tile.y, EdgeAxis::North};
        float best = fy;
        if (fx < best)
        {
            best = fx;
            edge = {tile.x, tile.y, EdgeAxis::West};
        }
        if (1.0f - fy < best)
        {
            best = 1.0f - fy;
            edge = {tile.x, tile.y + 1, EdgeAxis::North};
        }
        if (1.0f - fx < best)
            edge = {tile.x + 1, tile.y, EdgeAxis::West};
        return edge;
    }

    std::vector<LocalEdge> wallSegments(LocalCoord a, LocalCoord b)
    {
        std::vector<LocalEdge> edges;
        const int x0 = std::min(a.x, b.x);
        const int x1 = std::max(a.x, b.x);
        const int y0 = std::min(a.y, b.y);
        const int y1 = std::max(a.y, b.y);
        const auto row = [&](int y)
        {
            for (int x = x0; x < x1; ++x)
                edges.push_back({x, y, EdgeAxis::North});
        };
        const auto column = [&](int x)
        {
            for (int y = y0; y < y1; ++y)
                edges.push_back({x, y, EdgeAxis::West});
        };
        if (y0 == y1)
        {
            row(y0);
        }
        else if (x0 == x1)
        {
            column(x0);
        }
        else
        {
            row(y0);
            row(y1);
            column(x0);
            column(x1);
        }
        return edges;
    }

    std::vector<LocalCoord> tilesInRect(LocalCoord a, LocalCoord b)
    {
        std::vector<LocalCoord> tiles;
        for (int y = std::min(a.y, b.y); y <= std::max(a.y, b.y); ++y)
        {
            for (int x = std::min(a.x, b.x); x <= std::max(a.x, b.x); ++x)
                tiles.push_back({x, y});
        }
        return tiles;
    }

    void ConstructionTool::selectTool(BuildTool tool)
    {
        m_tool = tool;
        cancelDrag();
    }

    std::optional<StructureKind> ConstructionTool::kind() const
    {
        switch (m_tool)
        {
        case BuildTool::Wall:
            return StructureKind::Wall;
        case BuildTool::Door:
            return StructureKind::Door;
        case BuildTool::Floor:
            return StructureKind::Floor;
        case BuildTool::Furniture:
            return StructureKind::Furniture;
        case BuildTool::Remove:
        case BuildTool::Stockpile:
        case BuildTool::Count:
            break;
        }
        return std::nullopt;
    }

    std::span<const StructureMaterial> ConstructionTool::materials() const
    {
        const auto k = kind();
        if (!k)
            return {};
        return *k == StructureKind::Furniture ? furnitureMaterials(m_furniture) : structureMaterials(*k);
    }

    StructureMaterial ConstructionTool::material() const
    {
        const auto k = kind();
        if (!k)
            return StructureMaterial::Timber;
        const std::uint8_t index = *k == StructureKind::Furniture ? m_furnitureMaterial[static_cast<std::size_t>(m_furniture)]
                                                                  : m_material[static_cast<std::size_t>(*k)];
        return materials()[index];
    }

    void ConstructionTool::cycleMaterial(int step)
    {
        const auto k = kind();
        if (!k)
            return;
        const int count = static_cast<int>(materials().size());
        std::uint8_t &index = *k == StructureKind::Furniture ? m_furnitureMaterial[static_cast<std::size_t>(m_furniture)]
                                                             : m_material[static_cast<std::size_t>(*k)];
        index = static_cast<std::uint8_t>(((index + step) % count + count) % count);
    }

    void ConstructionTool::selectMaterial(StructureMaterial material)
    {
        const auto all = materials();
        const auto it = std::find(all.begin(), all.end(), material);
        if (it == all.end())
            return;
        const auto index = static_cast<std::uint8_t>(it - all.begin());
        if (*kind() == StructureKind::Furniture)
            m_furnitureMaterial[static_cast<std::size_t>(m_furniture)] = index;
        else
            m_material[static_cast<std::size_t>(*kind())] = index;
    }

    void ConstructionTool::selectFurniture(FurnitureKind kind)
    {
        m_furniture = kind;
        selectTool(BuildTool::Furniture);
    }

    void ConstructionTool::rotate() { m_facing = static_cast<Facing>((static_cast<int>(m_facing) + 1) % static_cast<int>(Facing::Count)); }

    LocalCoord ConstructionTool::furnitureCell(Vec2 position) const
    {
        const CellSize size = footprint(furnitureInfo(m_furniture), m_facing);
        const auto cells = static_cast<float>(kCellsPerTile);
        // The footprint's centre goes to the cell corner nearest the cursor.
        return {static_cast<std::int32_t>(std::floor(position.x * cells - static_cast<float>(size.w) * 0.5f + 0.5f)),
                static_cast<std::int32_t>(std::floor(position.y * cells - static_cast<float>(size.h) * 0.5f + 0.5f))};
    }

    std::vector<PreviewItem> ConstructionTool::preview(const SettlementMap &map, const ResourceState &resources,
                                                       const ConstructionState &construction, Vec2 cursor,
                                                       const PlacementOptions &options) const
    {
        std::vector<PreviewItem> items;
        const Vec2 anchor = anchorOr(cursor);
        switch (m_tool)
        {
        case BuildTool::Wall:
            for (const LocalEdge edge : wallSegments(nearestCorner(anchor), nearestCorner(cursor)))
                items.push_back({StructureKind::Wall, edge,
                                 construction.checkEdge(map, resources, StructureKind::Wall, material(), edge, options.stage)});
            break;
        case BuildTool::Door:
        {
            const LocalEdge edge = nearestEdge(cursor);
            items.push_back({StructureKind::Door, edge,
                             construction.checkEdge(map, resources, StructureKind::Door, material(), edge, options.stage)});
            break;
        }
        case BuildTool::Floor:
        {
            const LocalCoord from = tileAt(anchor);
            for (const LocalCoord tile : tilesInRect(from, clampRect(from, tileAt(cursor))))
                items.push_back({StructureKind::Floor, {tile.x, tile.y}, construction.checkFloor(map, resources, material(), tile)});
            break;
        }
        case BuildTool::Furniture:
        {
            const LocalCoord cell = furnitureCell(cursor);
            items.push_back({StructureKind::Furniture, {cell.x, cell.y},
                             construction.checkFurniture(map, resources, m_furniture, material(), cell, m_facing), m_furniture, m_facing});
            break;
        }
        case BuildTool::Stockpile:
        {
            const LocalCoord from = tileAt(anchor);
            const LocalCoord to = clampRect(from, tileAt(cursor));
            const PlacementError error = construction.checkStockpile(map, resources, from, to);
            for (const LocalCoord tile : tilesInRect(from, to))
            {
                PreviewItem item{StructureKind::Floor, {tile.x, tile.y}, error};
                item.stockpile = true;
                items.push_back(item);
            }
            break;
        }
        case BuildTool::Remove:
        {
            const LocalCoord from = tileAt(anchor);
            const LocalCoord to = clampRect(from, tileAt(cursor));
            if (!(from == to))
            {
                for (const StructureId id : construction.structuresInRect(from, to))
                    items.push_back(itemOf(*construction.structure(id)));
                break;
            }
            // A click removes furniture under it first.
            const auto cells = static_cast<float>(kCellsPerTile);
            const LocalCoord cell{static_cast<std::int32_t>(std::floor(cursor.x * cells)), static_cast<std::int32_t>(std::floor(cursor.y * cells))};
            if (const Structure *piece = construction.structure(construction.furnitureAt(cell)))
            {
                items.push_back(itemOf(*piece));
                break;
            }
            // Then the wall or door it is close to, else the floor under it, else the nearest edge's.
            const LocalEdge edge = nearestEdge(cursor);
            const auto [ex, ey] = edgeTiles(edge)[1];
            const float distance = edge.axis == EdgeAxis::North ? std::abs(cursor.y - static_cast<float>(ey))
                                                                : std::abs(cursor.x - static_cast<float>(ex));
            const Structure *onEdge = construction.structure(construction.edgeAt(edge));
            const Structure *floor = construction.structure(construction.floorAt(from));
            if (onEdge && (distance < 0.3f || !floor))
                items.push_back(itemOf(*onEdge));
            else if (floor)
                items.push_back(itemOf(*floor));
            break;
        }
        case BuildTool::Count:
            break;
        }
        return items;
    }

    ToolResult ConstructionTool::commit(const SettlementMap &map, ResourceState &resources, ConstructionState &construction,
                                        Vec2 cursor, PlacementOptions options)
    {
        const std::vector<PreviewItem> items = preview(map, resources, construction, cursor, options);
        cancelDrag();
        options.flipped = m_doorFlipped;
        options.facing = m_facing;
        ToolResult result;
        if (m_tool == BuildTool::Stockpile)
        {
            if (items.empty())
                return result;
            const LocalCoord from{items.front().edge.x, items.front().edge.y};
            const LocalCoord to{items.back().edge.x, items.back().edge.y};
            const StockpileResult made = construction.createStockpile(map, resources, from, to,
                                                                      options.clearObstructions && options.stage == ConstructionStage::Completed);
            result.woodKg = made.woodKg;
            result.placed = made.id.isValid() ? 1 : 0;
            result.skipped = made.id.isValid() ? 0 : 1;
            result.skipReason = made.error;
            return result;
        }
        for (const PreviewItem &item : items)
        {
            if (m_tool == BuildTool::Remove)
            {
                StructureId id;
                if (item.kind == StructureKind::Floor)
                    id = construction.floorAt({item.edge.x, item.edge.y});
                else if (item.kind == StructureKind::Furniture)
                    id = construction.furnitureAt({item.edge.x, item.edge.y});
                else
                    id = construction.edgeAt(item.edge);
                result.removed += construction.remove(id) ? 1 : 0;
                continue;
            }
            PlacementResult placed;
            if (item.kind == StructureKind::Floor)
                placed = construction.placeFloor(map, resources, material(), {item.edge.x, item.edge.y}, options);
            else if (item.kind == StructureKind::Furniture)
                placed = construction.placeFurniture(map, resources, item.furniture, material(), {item.edge.x, item.edge.y}, options);
            else
                placed = construction.placeEdge(map, resources, item.kind, material(), item.edge, options);
            result.woodKg += placed.woodKg;
            if (placed.error == PlacementError::None)
            {
                ++result.placed;
                continue;
            }
            ++result.skipped;
            if (result.skipReason == PlacementError::None || placed.error < result.skipReason)
                result.skipReason = placed.error;
        }
        return result;
    }

} // namespace olam
