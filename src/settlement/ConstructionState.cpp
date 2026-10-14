#include "settlement/ConstructionState.h"

#include "core/debug/Assert.h"
#include "core/serialization/ByteReader.h"
#include "core/serialization/ByteWriter.h"
#include "settlement/ResourceState.h"
#include "settlement/SettlementMap.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <set>

namespace olam
{

    namespace
    {

        constexpr std::uint32_t kMagic = 0x53434C4F; // "OLCS"
        // Version 1 (Phase 4B.1) had no construction orders, 2 (4B.2) no rooms, 3 (4B.3) no furniture, room
        // functions, materials or stockpiles; all still load.
        constexpr std::uint32_t kVersion = 4;
        // Index keys hold coordinates in 15 (edge y) and 16 bits.
        constexpr std::int32_t kMaxKeyCoord = 32766;
        // Largest stockpile side (tiles).
        constexpr std::int32_t kMaxStockpileSide = 32;

        std::uint32_t tileKey(LocalCoord tile)
        {
            OLAM_ASSERT(tile.x >= 0 && tile.y >= 0 && tile.x <= kMaxKeyCoord && tile.y <= kMaxKeyCoord);
            return (static_cast<std::uint32_t>(tile.y) << 16) | static_cast<std::uint32_t>(tile.x);
        }

        std::uint32_t edgeKey(LocalEdge edge)
        {
            OLAM_ASSERT(edge.x >= 0 && edge.y >= 0 && edge.x <= kMaxKeyCoord + 1 && edge.y <= kMaxKeyCoord + 1);
            return (static_cast<std::uint32_t>(edge.y) << 17) | (static_cast<std::uint32_t>(edge.x) << 1) |
                   static_cast<std::uint32_t>(edge.axis);
        }

        bool inKeyRange(LocalCoord tile) { return tile.x >= 0 && tile.y >= 0 && tile.x <= kMaxKeyCoord && tile.y <= kMaxKeyCoord; }

        // The more severe of two errors (lower values are more severe; None is no error).
        PlacementError worse(PlacementError a, PlacementError b)
        {
            if (a == PlacementError::None)
                return b;
            if (b == PlacementError::None)
                return a;
            return std::min(a, b);
        }

        // Understory that must be cleared before building; grasses, ferns and flowers are simply trampled.
        bool blocksConstruction(UnderstoryKind kind)
        {
            return kind == UnderstoryKind::Shrub || kind == UnderstoryKind::Bush || kind == UnderstoryKind::FallenLog ||
                   kind == UnderstoryKind::SmallRock;
        }

        // Largest elevation difference (dm) between a tile and its neighbours on the map.
        int tileRiseDm(const SettlementMap &map, LocalCoord tile)
        {
            const auto &elevation = map.terrain().elevation;
            const int here = elevation[map.index(tile)];
            int rise = 0;
            for (const LocalCoord offset : {LocalCoord{0, -1}, LocalCoord{1, 0}, LocalCoord{0, 1}, LocalCoord{-1, 0}})
            {
                const LocalCoord next{tile.x + offset.x, tile.y + offset.y};
                if (map.isValid(next))
                    rise = std::max(rise, std::abs(here - static_cast<int>(elevation[map.index(next)])));
            }
            return rise;
        }

        bool overlaps(const CellRect &a, const CellRect &b)
        {
            return a.min.x < b.max.x && b.min.x < a.max.x && a.min.y < b.max.y && b.min.y < a.max.y;
        }

        // Tiles a block of cells touches (inclusive corners).
        std::pair<LocalCoord, LocalCoord> cellTiles(const CellRect &cells)
        {
            return {cellTile(cells.min), cellTile({cells.max.x - 1, cells.max.y - 1})};
        }

        void writeGoods(ByteWriter &writer, const GoodAmounts &goods)
        {
            for (const std::uint32_t kg : goods.kg)
                writer.write(kg);
        }

        void readGoods(ByteReader &reader, GoodAmounts &goods)
        {
            for (std::uint32_t &kg : goods.kg)
                reader.read(kg);
        }

    } // namespace

    std::string_view toString(PlacementError error)
    {
        switch (error)
        {
        case PlacementError::None:
            return "ok";
        case PlacementError::OutOfBounds:
            return "outside the map";
        case PlacementError::InvalidMaterial:
            return "wrong material";
        case PlacementError::Occupied:
            return "occupied";
        case PlacementError::Water:
            return "water";
        case PlacementError::TooSteep:
            return "too steep";
        case PlacementError::Tree:
            return "tree in the way";
        case PlacementError::Obstacle:
            return "obstacle in the way";
        case PlacementError::Count:
            break;
        }
        return "?";
    }

    std::uint16_t requiredWork(const Structure &structure)
    {
        if (structure.kind == StructureKind::Furniture)
            return furnitureInfo(structure.furniture).workRequired;
        return structureProperties(structure.kind, structure.material).workRequired;
    }

    CellRect furnitureCells(FurnitureKind kind, LocalCoord cell, Facing facing)
    {
        const CellSize size = footprint(furnitureInfo(kind), facing);
        return {cell, {cell.x + size.w, cell.y + size.h}};
    }

    PlacementError ConstructionState::tileError(const SettlementMap &map, const ResourceState &resources, LocalCoord tile,
                                                bool floor) const
    {
        if (!map.isValid(tile))
            return PlacementError::OutOfBounds;
        const LocalTerrainData &terrain = map.terrain();
        const std::size_t i = map.index(tile);
        if (terrain.water[i] != LocalWater::None)
            return PlacementError::Water;
        if (floor && tileRiseDm(map, tile) > m_config.maxFloorRiseDm)
            return PlacementError::TooSteep;
        if (const TreeId tree = terrain.treeId[i]; tree.isValid() && !resources.treeFelled(map, tree))
            return PlacementError::Tree;
        if (const auto u = map.understoryAt(tile);
            u && blocksConstruction(map.understory().kind[*u]) && !resources.understoryCleared(map, *u))
            return PlacementError::Obstacle;
        return PlacementError::None;
    }

    PlacementError ConstructionState::checkEdge(const SettlementMap &map, const ResourceState &resources, StructureKind kind,
                                                StructureMaterial material, LocalEdge edge, ConstructionStage stage) const
    {
        OLAM_ASSERT(isEdgeStructure(kind));
        if (!isMaterialAllowed(kind, material))
            return PlacementError::InvalidMaterial;
        const auto [a, b] = edgeTiles(edge);
        // Border edges have only one tile on the map; walls need ground on both sides.
        if (!map.isValid(a) || !map.isValid(b))
            return PlacementError::OutOfBounds;
        if (const Structure *existing = structure(edgeAt(edge)))
        {
            const bool replaceable = kind == StructureKind::Door && existing->kind == StructureKind::Wall &&
                                     (stage == ConstructionStage::Completed || existing->stage != ConstructionStage::Completed);
            if (!replaceable)
                return PlacementError::Occupied;
        }
        // Furniture touching both tiles spans the edge.
        const std::vector<StructureId> onA = furnitureOnTile(a);
        for (const StructureId piece : furnitureOnTile(b))
        {
            if (std::find(onA.begin(), onA.end(), piece) != onA.end())
                return PlacementError::Occupied;
        }
        PlacementError error = worse(tileError(map, resources, a, false), tileError(map, resources, b, false));
        const auto &elevation = map.terrain().elevation;
        if (std::abs(static_cast<int>(elevation[map.index(a)]) - static_cast<int>(elevation[map.index(b)])) > m_config.maxEdgeRiseDm)
            error = worse(error, PlacementError::TooSteep);
        return error;
    }

    PlacementError ConstructionState::checkFloor(const SettlementMap &map, const ResourceState &resources,
                                                 StructureMaterial material, LocalCoord tile) const
    {
        if (!isMaterialAllowed(StructureKind::Floor, material))
            return PlacementError::InvalidMaterial;
        if (!map.isValid(tile))
            return PlacementError::OutOfBounds;
        if (floorAt(tile).isValid())
            return PlacementError::Occupied;
        return tileError(map, resources, tile, true);
    }

    PlacementError ConstructionState::checkFurniture(const SettlementMap &map, const ResourceState &resources, FurnitureKind kind,
                                                     StructureMaterial material, LocalCoord cell, Facing facing) const
    {
        if (kind >= FurnitureKind::Count || facing >= Facing::Count)
            return PlacementError::InvalidMaterial;
        if (!isFurnitureMaterial(kind, material))
            return PlacementError::InvalidMaterial;
        const CellRect cells = furnitureCells(kind, cell, facing);
        const auto [t0, t1] = cellTiles(cells);
        if (!map.isValid(t0) || !map.isValid(t1))
            return PlacementError::OutOfBounds;
        PlacementError error = PlacementError::None;
        for (int y = t0.y; y <= t1.y; ++y)
        {
            for (int x = t0.x; x <= t1.x; ++x)
            {
                const LocalCoord tile{x, y};
                if (m_stockpiles.stockpileAt(tile).isValid())
                    return PlacementError::Occupied;
                for (const StructureId other : furnitureOnTile(tile))
                {
                    if (overlaps(cells, furnitureCells(*structure(other))))
                        return PlacementError::Occupied;
                }
                // Tile borders inside the footprint must be free of walls and doors.
                if (x > t0.x && edgeAt({x, y, EdgeAxis::West}).isValid())
                    return PlacementError::Occupied;
                if (y > t0.y && edgeAt({x, y, EdgeAxis::North}).isValid())
                    return PlacementError::Occupied;
                error = worse(error, tileError(map, resources, tile, true));
            }
        }
        return error;
    }

    float ConstructionState::clearTile(const SettlementMap &map, ResourceState &resources, LocalCoord tile) const
    {
        float wood = 0.0f;
        if (const TreeId tree = map.terrain().treeId[map.index(tile)]; tree.isValid())
            wood = resources.fellTree(map, tree);
        if (const auto u = map.understoryAt(tile); u && blocksConstruction(map.understory().kind[*u]))
            resources.clearUnderstory(map, *u);
        return wood;
    }

    std::vector<LocalCoord> ConstructionState::siteTiles(const Structure &structure) const
    {
        switch (structure.kind)
        {
        case StructureKind::Floor:
            return {structure.tile};
        case StructureKind::Furniture:
        {
            std::vector<LocalCoord> tiles;
            const auto [t0, t1] = cellTiles(furnitureCells(structure));
            for (int y = t0.y; y <= t1.y; ++y)
            {
                for (int x = t0.x; x <= t1.x; ++x)
                    tiles.push_back({x, y});
            }
            return tiles;
        }
        default:
        {
            const auto [a, b] = edgeTiles(structure.edge());
            return {a, b};
        }
        }
    }

    LocalCoord ConstructionState::orderSite(const ConstructionOrder &order) const
    {
        if (order.components.empty())
            return {};
        const Structure &first = m_structures.at(order.components.front().value);
        return first.kind == StructureKind::Furniture ? cellTile(first.tile) : first.tile;
    }

    PlacementResult ConstructionState::placeEdge(const SettlementMap &map, ResourceState &resources, StructureKind kind,
                                                 StructureMaterial material, LocalEdge edge, const PlacementOptions &options)
    {
        OLAM_ASSERT(!options.order.isValid() || (options.stage == ConstructionStage::Planned && order(options.order)));
        PlacementResult result;
        result.error = checkEdge(map, resources, kind, material, edge, options.stage);
        // Clearable errors are the least severe, so nothing else is wrong once the site is cleared.
        if (isClearable(result.error) && options.clearObstructions && options.stage >= ConstructionStage::UnderConstruction)
        {
            for (const LocalCoord tile : edgeTiles(edge))
                result.woodKg += clearTile(map, resources, tile);
            result.error = checkEdge(map, resources, kind, material, edge, options.stage);
        }
        if (!placementAllowed(result.error, options))
            return result;
        result.error = PlacementError::None;

        Structure structure;
        structure.kind = kind;
        structure.material = material;
        structure.stage = options.stage;
        structure.tile = {edge.x, edge.y};
        structure.axis = edge.axis;
        structure.flipped = kind == StructureKind::Door && options.flipped;
        structure.order = options.order;
        // A replaced wall's order is pruned only after the door is added: the door may join the same order.
        const OrderId replacedOrder = edgeAt(edge).isValid() ? erase(edgeAt(edge)) : OrderId{};
        result.id = add(structure);
        pruneOrder(replacedOrder);
        updateRooms();
        return result;
    }

    PlacementResult ConstructionState::placeFloor(const SettlementMap &map, ResourceState &resources, StructureMaterial material,
                                                  LocalCoord tile, const PlacementOptions &options)
    {
        OLAM_ASSERT(!options.order.isValid() || (options.stage == ConstructionStage::Planned && order(options.order)));
        PlacementResult result;
        result.error = checkFloor(map, resources, material, tile);
        if (isClearable(result.error) && options.clearObstructions && options.stage >= ConstructionStage::UnderConstruction)
        {
            result.woodKg = clearTile(map, resources, tile);
            result.error = checkFloor(map, resources, material, tile);
        }
        if (!placementAllowed(result.error, options))
            return result;
        result.error = PlacementError::None;

        Structure structure;
        structure.kind = StructureKind::Floor;
        structure.material = material;
        structure.stage = options.stage;
        structure.tile = tile;
        structure.order = options.order;
        result.id = add(structure);
        return result;
    }

    PlacementResult ConstructionState::placeFurniture(const SettlementMap &map, ResourceState &resources, FurnitureKind kind,
                                                      StructureMaterial material, LocalCoord cell, const PlacementOptions &options)
    {
        OLAM_ASSERT(!options.order.isValid() || (options.stage == ConstructionStage::Planned && order(options.order)));
        Structure structure;
        structure.kind = StructureKind::Furniture;
        structure.material = material;
        structure.stage = options.stage;
        structure.tile = cell;
        structure.order = options.order;
        structure.furniture = kind;
        structure.facing = options.facing;

        PlacementResult result;
        result.error = checkFurniture(map, resources, kind, material, cell, options.facing);
        if (isClearable(result.error) && options.clearObstructions && options.stage >= ConstructionStage::UnderConstruction)
        {
            for (const LocalCoord tile : siteTiles(structure))
                result.woodKg += clearTile(map, resources, tile);
            result.error = checkFurniture(map, resources, kind, material, cell, options.facing);
        }
        if (!placementAllowed(result.error, options))
            return result;
        result.error = PlacementError::None;
        result.id = add(structure);
        return result;
    }

    StructureId ConstructionState::add(const Structure &structure)
    {
        const StructureId id{m_nextId++};
        if (structure.kind == StructureKind::Floor)
            m_floors[tileKey(structure.tile)] = id;
        else if (structure.kind == StructureKind::Furniture)
        {
            for (const LocalCoord tile : siteTiles(structure))
                m_furniture.emplace(tileKey(tile), id);
        }
        else
            m_edges[edgeKey(structure.edge())] = id;
        Structure &added = m_structures.emplace(id.value, structure).first->second;
        if (added.stage == ConstructionStage::Completed)
            added.workDone = requiredWork(added);
        if (added.order.isValid())
            m_orders.at(added.order.value).components.push_back(id);
        markRooms(added);
        ++m_revision;
        return id;
    }

    OrderId ConstructionState::erase(StructureId id)
    {
        const auto it = m_structures.find(id.value);
        OLAM_ASSERT(it != m_structures.end());
        const Structure &structure = it->second;
        if (structure.kind == StructureKind::Floor)
            m_floors.erase(tileKey(structure.tile));
        else if (structure.kind == StructureKind::Furniture)
        {
            for (const LocalCoord tile : siteTiles(structure))
            {
                const auto [first, last] = m_furniture.equal_range(tileKey(tile));
                for (auto f = first; f != last; ++f)
                {
                    if (f->second == id)
                    {
                        m_furniture.erase(f);
                        break;
                    }
                }
            }
        }
        else
            m_edges.erase(edgeKey(structure.edge()));
        markRooms(structure);
        const OrderId orderId = structure.order;
        if (orderId.isValid())
        {
            ConstructionOrder &order = m_orders.at(orderId.value);
            std::erase(order.components, id);
            // Materials already assigned to it go back to the site.
            if (structure.stage == ConstructionStage::ReadyToBuild || structure.stage == ConstructionStage::UnderConstruction)
                order.onSite += cost(structure);
        }
        m_structures.erase(it);
        ++m_revision;
        return orderId;
    }

    GoodAmounts ConstructionState::orderNeed(const ConstructionOrder &order) const
    {
        GoodAmounts need;
        for (const StructureId component : order.components)
        {
            const Structure &s = m_structures.at(component.value);
            if (s.stage == ConstructionStage::AwaitingMaterials)
                need += cost(s);
        }
        return need - order.onSite;
    }

    void ConstructionState::settleOrder(OrderId id, ConstructionOrder &order)
    {
        m_stockpiles.releaseAll(id);
        if (order.onSite.empty())
            return;
        StockpileId target = order.source;
        if (!m_stockpiles.stockpile(target))
        {
            const auto ids = m_stockpiles.ids();
            target = ids.empty() ? StockpileId{} : ids.front();
        }
        // Without any stockpile left the goods are lost.
        if (target.isValid())
            m_stockpiles.store(target, order.onSite);
        order.onSite = {};
    }

    void ConstructionState::pruneOrder(OrderId id)
    {
        const auto it = m_orders.find(id.value);
        if (it == m_orders.end())
            return;
        const bool done = std::all_of(it->second.components.begin(), it->second.components.end(), [&](StructureId c)
                                      { return m_structures.at(c.value).stage == ConstructionStage::Completed; });
        if (!done)
        {
            // Still open: give back reservations it no longer needs (a supplied part was removed).
            const GoodAmounts excess = m_stockpiles.reservedFor(id) - orderNeed(it->second);
            if (!excess.empty())
                m_stockpiles.release(id, excess, orderSite(it->second));
            return;
        }
        for (const StructureId component : it->second.components)
            m_structures.at(component.value).order = {};
        settleOrder(id, it->second);
        m_orders.erase(it);
        ++m_revision;
    }

    bool ConstructionState::remove(StructureId id)
    {
        if (!m_structures.contains(id.value))
            return false;
        pruneOrder(erase(id));
        updateRooms();
        return true;
    }

    std::size_t ConstructionState::removeInRect(LocalCoord a, LocalCoord b)
    {
        const std::vector<StructureId> ids = structuresInRect(a, b);
        for (const StructureId id : ids)
            pruneOrder(erase(id));
        updateRooms();
        return ids.size();
    }

    StructureId ConstructionState::edgeAt(LocalEdge edge) const
    {
        if (edge.x < 0 || edge.y < 0 || edge.x > kMaxKeyCoord + 1 || edge.y > kMaxKeyCoord + 1)
            return {};
        const auto it = m_edges.find(edgeKey(edge));
        return it != m_edges.end() ? it->second : StructureId{};
    }

    StructureId ConstructionState::floorAt(LocalCoord tile) const
    {
        if (!inKeyRange(tile))
            return {};
        const auto it = m_floors.find(tileKey(tile));
        return it != m_floors.end() ? it->second : StructureId{};
    }

    StructureId ConstructionState::furnitureAt(LocalCoord cell) const
    {
        const LocalCoord tile = cellTile(cell);
        if (!inKeyRange(tile))
            return {};
        const auto [first, last] = m_furniture.equal_range(tileKey(tile));
        for (auto it = first; it != last; ++it)
        {
            const CellRect cells = furnitureCells(m_structures.at(it->second.value));
            if (cell.x >= cells.min.x && cell.x < cells.max.x && cell.y >= cells.min.y && cell.y < cells.max.y)
                return it->second;
        }
        return {};
    }

    std::vector<StructureId> ConstructionState::furnitureOnTile(LocalCoord tile) const
    {
        std::vector<StructureId> ids;
        if (!inKeyRange(tile))
            return ids;
        const auto [first, last] = m_furniture.equal_range(tileKey(tile));
        for (auto it = first; it != last; ++it)
            ids.push_back(it->second);
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    const Structure *ConstructionState::structure(StructureId id) const
    {
        const auto it = m_structures.find(id.value);
        return it != m_structures.end() ? &it->second : nullptr;
    }

    std::vector<StructureId> ConstructionState::structuresInRect(LocalCoord a, LocalCoord b) const
    {
        const int x0 = std::clamp(std::min(a.x, b.x), 0, kMaxKeyCoord);
        const int y0 = std::clamp(std::min(a.y, b.y), 0, kMaxKeyCoord);
        const int x1 = std::clamp(std::max(a.x, b.x), 0, kMaxKeyCoord);
        const int y1 = std::clamp(std::max(a.y, b.y), 0, kMaxKeyCoord);
        std::vector<StructureId> ids;
        // Keys are row-major, so every row is one index range.
        for (int y = y0; y <= y1; ++y)
        {
            const auto last = m_floors.upper_bound(tileKey({x1, y}));
            for (auto it = m_floors.lower_bound(tileKey({x0, y})); it != last; ++it)
                ids.push_back(it->second);
        }
        std::set<StructureId> furniture;
        for (int y = y0; y <= y1; ++y)
        {
            const auto last = m_furniture.upper_bound(tileKey({x1, y}));
            for (auto it = m_furniture.lower_bound(tileKey({x0, y})); it != last; ++it)
                furniture.insert(it->second);
        }
        ids.insert(ids.end(), furniture.begin(), furniture.end());
        // Edges on or inside the rectangle: North edges of rows y0..y1 + 1, West edges of columns x0..x1 + 1.
        for (int y = y0; y <= y1 + 1; ++y)
        {
            const auto last = m_edges.upper_bound(edgeKey({x1 + 1, y, EdgeAxis::West}));
            for (auto it = m_edges.lower_bound(edgeKey({x0, y, EdgeAxis::North})); it != last; ++it)
            {
                const Structure &s = m_structures.at(it->second.value);
                if (s.axis == EdgeAxis::North ? s.tile.x <= x1 : s.tile.y <= y1)
                    ids.push_back(it->second);
            }
        }
        return ids;
    }

    OrderId ConstructionState::createOrder()
    {
        const OrderId id{m_nextOrderId++};
        m_orders.emplace(id.value, ConstructionOrder{});
        ++m_revision;
        return id;
    }

    const ConstructionOrder *ConstructionState::order(OrderId id) const
    {
        const auto it = m_orders.find(id.value);
        return it != m_orders.end() ? &it->second : nullptr;
    }

    ConstructionStage ConstructionState::orderStage(OrderId id) const
    {
        const ConstructionOrder *o = order(id);
        if (!o)
            return ConstructionStage::Count;
        ConstructionStage stage = o->components.empty() ? ConstructionStage::Planned : ConstructionStage::Completed;
        for (const StructureId component : o->components)
            stage = std::min(stage, m_structures.at(component.value).stage);
        return stage;
    }

    OrderId ConstructionState::orderAt(LocalEdge edge) const
    {
        const Structure *s = structure(edgeAt(edge));
        return s ? s->order : OrderId{};
    }

    OrderId ConstructionState::orderAt(LocalCoord tile) const
    {
        if (const Structure *s = structure(floorAt(tile)); s && s->order.isValid())
            return s->order;
        for (const StructureId piece : furnitureOnTile(tile))
        {
            if (const OrderId o = m_structures.at(piece.value).order; o.isValid())
                return o;
        }
        return {};
    }

    GoodAmounts ConstructionState::cost(const Structure &structure) const
    {
        if (structure.kind == StructureKind::Furniture)
            return m_costs.furnitureCost(structure.furniture, structure.material);
        return m_costs.structure(structure.kind, structure.material);
    }

    GoodAmounts ConstructionState::orderCost(OrderId id) const
    {
        GoodAmounts total;
        if (const ConstructionOrder *o = order(id))
        {
            for (const StructureId component : o->components)
                total += cost(m_structures.at(component.value));
        }
        return total;
    }

    GoodAmounts ConstructionState::orderShortfall(OrderId id) const
    {
        const ConstructionOrder *o = order(id);
        return o ? orderNeed(*o) - m_stockpiles.reservedFor(id) : GoodAmounts{};
    }

    bool ConstructionState::confirmOrder(OrderId id)
    {
        const ConstructionOrder *o = order(id);
        if (!o)
            return false;
        bool changed = false;
        for (const StructureId component : o->components)
        {
            Structure &s = m_structures.at(component.value);
            if (s.stage == ConstructionStage::Planned)
            {
                s.stage = ConstructionStage::AwaitingMaterials;
                changed = true;
            }
        }
        m_revision += changed ? 1 : 0;
        return changed;
    }

    GoodAmounts ConstructionState::allocateMaterials(OrderId id)
    {
        const auto it = m_orders.find(id.value);
        if (it == m_orders.end())
            return {};
        const LocalCoord site = orderSite(it->second);
        const GoodAmounts need = orderNeed(it->second);
        const GoodAmounts reserved = m_stockpiles.reservedFor(id);
        const GoodAmounts excess = reserved - need;
        if (!excess.empty())
            m_stockpiles.release(id, excess, site);
        const GoodAmounts missing = need - reserved;
        const GoodAmounts got = m_stockpiles.reserve(id, missing, site);
        if (!excess.empty() || !got.empty())
            ++m_revision;
        return missing - got;
    }

    bool ConstructionState::debugDeliver(OrderId id)
    {
        if (!order(id))
            return false;
        allocateMaterials(id);
        ConstructionOrder &o = m_orders.at(id.value);
        const GoodAmounts taken = m_stockpiles.take(id, orderSite(o), o.source);
        o.onSite += taken;
        bool changed = !taken.empty();
        for (const StructureId component : o.components)
        {
            Structure &s = m_structures.at(component.value);
            const GoodAmounts needed = cost(s);
            if (s.stage == ConstructionStage::AwaitingMaterials && o.onSite.covers(needed))
            {
                o.onSite -= needed;
                s.stage = ConstructionStage::ReadyToBuild;
                changed = true;
            }
        }
        m_revision += changed ? 1 : 0;
        return changed;
    }

    std::optional<float> ConstructionState::beginConstruction(const SettlementMap &map, ResourceState &resources, StructureId id)
    {
        const auto it = m_structures.find(id.value);
        if (it == m_structures.end() || it->second.stage != ConstructionStage::ReadyToBuild)
            return std::nullopt;
        Structure &s = it->second;
        float wood = 0.0f;
        for (const LocalCoord tile : siteTiles(s))
            wood += clearTile(map, resources, tile);
        s.stage = ConstructionStage::UnderConstruction;
        ++m_revision;
        return wood;
    }

    bool ConstructionState::addWork(StructureId id, std::uint16_t units)
    {
        const bool worked = work(id, units);
        updateRooms();
        return worked;
    }

    bool ConstructionState::work(StructureId id, std::uint16_t units)
    {
        const auto it = m_structures.find(id.value);
        if (it == m_structures.end() || it->second.stage != ConstructionStage::UnderConstruction || units == 0)
            return false;
        Structure &s = it->second;
        const std::uint16_t required = requiredWork(s);
        s.workDone = static_cast<std::uint16_t>(std::min<int>(required, s.workDone + units));
        ++m_revision;
        if (s.workDone >= required)
        {
            s.stage = ConstructionStage::Completed;
            markRooms(s);
            pruneOrder(s.order);
        }
        return true;
    }

    void ConstructionState::markRooms(const Structure &structure)
    {
        if (isEdgeStructure(structure.kind) && structure.stage == ConstructionStage::Completed)
            m_rooms.markDirty(structure.edge());
    }

    void ConstructionState::updateRooms()
    {
        if (m_rooms.dirty())
            m_rooms.update(*this);
    }

    bool ConstructionState::setRoof(RoomId id, RoofSettings roof)
    {
        const Room *room = m_rooms.room(id);
        if (!room || room->roof == roof || !m_rooms.setRoof(id, roof))
            return false;
        ++m_revision;
        return true;
    }

    bool ConstructionState::setRoomFunction(RoomId id, RoomFunction function)
    {
        if (!m_rooms.setFunction(id, function))
            return false;
        ++m_revision;
        return true;
    }

    GoodAmounts ConstructionState::roofCost(RoomId id) const
    {
        const Room *room = m_rooms.room(id);
        if (!room || !room->roofed() || room->roofComplete())
            return {};
        const auto missing = static_cast<std::uint32_t>(room->tiles.size()) - room->roofBuiltTiles;
        return m_costs.roof(room->roof.material) * missing;
    }

    bool ConstructionState::buildRoof(RoomId id)
    {
        const Room *room = m_rooms.room(id);
        if (!room || !room->roofed() || room->roofComplete())
            return false;
        const LocalCoord center{(room->min.x + room->max.x) / 2, (room->min.y + room->max.y) / 2};
        if (!m_stockpiles.consume(roofCost(id), center))
            return false;
        m_rooms.setRoofBuilt(id);
        ++m_revision;
        return true;
    }

    std::optional<float> ConstructionState::debugCompleteOrder(const SettlementMap &map, ResourceState &resources, OrderId id)
    {
        if (!order(id))
            return std::nullopt;
        confirmOrder(id);
        debugDeliver(id);
        float wood = 0.0f;
        // Completing the last structure closes the order, so work from a copy of its component list.
        const std::vector<StructureId> components = order(id)->components;
        for (const StructureId component : components)
        {
            if (const auto cleared = beginConstruction(map, resources, component))
                wood += *cleared;
            work(component, std::numeric_limits<std::uint16_t>::max());
        }
        updateRooms();
        return wood;
    }

    std::size_t ConstructionState::cancelOrder(OrderId id)
    {
        const auto it = m_orders.find(id.value);
        if (it == m_orders.end())
            return 0;
        std::size_t removed = 0;
        for (const StructureId component : std::vector<StructureId>(it->second.components))
        {
            Structure &s = m_structures.at(component.value);
            if (s.stage == ConstructionStage::Completed)
            {
                s.order = {};
                continue;
            }
            erase(component);
            ++removed;
        }
        settleOrder(id, m_orders.at(id.value));
        m_orders.erase(id.value);
        ++m_revision;
        updateRooms();
        return removed;
    }

    PlacementError ConstructionState::checkStockpile(const SettlementMap &map, const ResourceState &resources, LocalCoord a,
                                                     LocalCoord b) const
    {
        const LocalCoord min{std::min(a.x, b.x), std::min(a.y, b.y)};
        const LocalCoord max{std::max(a.x, b.x), std::max(a.y, b.y)};
        if (!map.isValid(min) || !map.isValid(max) || max.x - min.x >= kMaxStockpileSide || max.y - min.y >= kMaxStockpileSide)
            return PlacementError::OutOfBounds;
        PlacementError error = PlacementError::None;
        for (int y = min.y; y <= max.y; ++y)
        {
            for (int x = min.x; x <= max.x; ++x)
            {
                const LocalCoord tile{x, y};
                if (m_stockpiles.stockpileAt(tile).isValid() || !furnitureOnTile(tile).empty())
                    return PlacementError::Occupied;
                error = worse(error, tileError(map, resources, tile, true));
            }
        }
        return error;
    }

    StockpileResult ConstructionState::createStockpile(const SettlementMap &map, ResourceState &resources, LocalCoord a, LocalCoord b,
                                                       bool clearObstructions)
    {
        StockpileResult result;
        result.error = checkStockpile(map, resources, a, b);
        const LocalCoord min{std::min(a.x, b.x), std::min(a.y, b.y)};
        const LocalCoord max{std::max(a.x, b.x), std::max(a.y, b.y)};
        if (isClearable(result.error) && clearObstructions)
        {
            for (int y = min.y; y <= max.y; ++y)
            {
                for (int x = min.x; x <= max.x; ++x)
                    result.woodKg += clearTile(map, resources, {x, y});
            }
            result.error = checkStockpile(map, resources, a, b);
        }
        if (result.error != PlacementError::None)
            return result;
        result.id = m_stockpiles.add(min, max);
        ++m_revision;
        return result;
    }

    bool ConstructionState::removeStockpile(StockpileId id)
    {
        if (!m_stockpiles.stockpile(id))
            return false;
        m_stockpiles.erase(id);
        ++m_revision;
        return true;
    }

    bool ConstructionState::debugAddGoods(StockpileId id, const GoodAmounts &goods)
    {
        if (!m_stockpiles.stockpile(id) || goods.empty())
            return false;
        m_stockpiles.store(id, goods);
        ++m_revision;
        return true;
    }

    GoodAmounts defaultFoundingGoods()
    {
        return {{Good::Wood, 4000}, {Good::Stone, 1500}, {Good::Clay, 800}, {Good::Thatch, 1200}, {Good::Food, 1000}};
    }

    StockpileResult createFoundingStockpile(const SettlementMap &map, ResourceState &resources, ConstructionState &construction,
                                            LocalCoord near, const GoodAmounts &goods)
    {
        constexpr int kMaxRadius = 256;
        // Rings of growing radius around `near`, each row by row: deterministic and close to the centre.
        for (int r = 0; r <= kMaxRadius; ++r)
        {
            for (int dy = -r; dy <= r; ++dy)
            {
                for (int dx = -r; dx <= r; ++dx)
                {
                    if (std::max(std::abs(dx), std::abs(dy)) != r)
                        continue;
                    const LocalCoord min{near.x + dx - 1, near.y + dy - 1};
                    const LocalCoord max{min.x + 2, min.y + 2};
                    const PlacementError error = construction.checkStockpile(map, resources, min, max);
                    if (error != PlacementError::None && !isClearable(error))
                        continue;
                    StockpileResult result = construction.createStockpile(map, resources, min, max, true);
                    if (result.id.isValid())
                    {
                        construction.debugAddGoods(result.id, goods);
                        return result;
                    }
                }
            }
        }
        return {StockpileId{}, PlacementError::OutOfBounds, 0.0f};
    }

    void ConstructionState::writeState(ByteWriter &writer) const
    {
        writer.write(m_nextId);
        writer.write(static_cast<std::uint32_t>(m_structures.size()));
        for (const auto &[id, s] : m_structures)
        {
            writer.write(id);
            writer.write(s.kind);
            writer.write(s.material);
            writer.write(s.stage);
            writer.write(s.tile.x);
            writer.write(s.tile.y);
            writer.write(s.axis);
            writer.write(s.flipped);
            writer.write(s.workDone);
            writer.write(s.order);
            writer.write(s.furniture);
            writer.write(s.facing);
        }
        writer.write(m_nextOrderId);
        writer.write(static_cast<std::uint32_t>(m_orders.size()));
        for (const auto &[id, o] : m_orders)
        {
            writer.write(id);
            writer.write(static_cast<std::uint32_t>(o.components.size()));
            for (const StructureId component : o.components)
                writer.write(component);
            writeGoods(writer, o.onSite);
            writer.write(o.source);
        }
        m_rooms.write(writer);
        m_stockpiles.write(writer);
    }

    std::uint64_t ConstructionState::hash() const
    {
        ByteWriter writer;
        writeState(writer);
        return writer.hash();
    }

    std::vector<std::uint8_t> ConstructionState::serialize(const SettlementMap &map) const
    {
        ByteWriter writer;
        writer.write(kMagic);
        writer.write(kVersion);
        writer.write(map.worldSeed());
        writer.write(map.site().x);
        writer.write(map.site().y);
        writeState(writer);
        return writer.takeBytes();
    }

    std::optional<std::string> ConstructionState::deserialize(const SettlementMap &map, std::span<const std::uint8_t> bytes)
    {
        ByteReader reader(bytes);
        std::uint32_t magic = 0;
        std::uint32_t version = 0;
        std::uint64_t worldSeed = 0;
        WorldCoord site;
        reader.read(magic);
        reader.read(version);
        reader.read(worldSeed);
        reader.read(site.x);
        reader.read(site.y);
        if (reader.failed() || magic != kMagic)
            return "not a construction state";
        if (version < 1 || version > kVersion)
            return "unsupported construction state version " + std::to_string(version);
        if (worldSeed != map.worldSeed() || !(site == map.site()))
            return "construction state belongs to another settlement";

        ConstructionState loaded(m_config, m_costs);
        std::uint32_t nextId = 0;
        std::uint32_t count = 0;
        reader.read(nextId);
        reader.read(count);
        for (std::uint32_t i = 0; i < count && !reader.failed(); ++i)
        {
            std::uint32_t id = 0;
            Structure s;
            reader.read(id);
            reader.read(s.kind);
            reader.read(s.material);
            reader.read(s.stage);
            reader.read(s.tile.x);
            reader.read(s.tile.y);
            reader.read(s.axis);
            reader.read(s.flipped);
            reader.read(s.workDone);
            if (version >= 2)
                reader.read(s.order);
            if (version >= 4)
            {
                reader.read(s.furniture);
                reader.read(s.facing);
            }
            if (reader.failed())
                break;
            const bool furniture = s.kind == StructureKind::Furniture;
            if (id == 0 || id >= nextId || loaded.m_structures.contains(id) || s.kind >= StructureKind::Count ||
                s.stage >= ConstructionStage::Count || s.axis >= EdgeAxis::Count || s.furniture >= FurnitureKind::Count ||
                s.facing >= Facing::Count ||
                !(furniture ? isFurnitureMaterial(s.furniture, s.material) : isMaterialAllowed(s.kind, s.material)))
                return "corrupt structure entry";
            if (furniture)
            {
                const CellRect cells = furnitureCells(s);
                const auto [t0, t1] = cellTiles(cells);
                if (!map.isValid(t0) || !map.isValid(t1))
                    return "structure off the map or overlapping";
                for (const LocalCoord tile : loaded.siteTiles(s))
                {
                    for (const StructureId other : loaded.furnitureOnTile(tile))
                    {
                        if (overlaps(cells, furnitureCells(loaded.m_structures.at(other.value))))
                            return "structure off the map or overlapping";
                    }
                }
                for (const LocalCoord tile : loaded.siteTiles(s))
                    loaded.m_furniture.emplace(tileKey(tile), StructureId{id});
            }
            else if (s.kind == StructureKind::Floor ? !map.isValid(s.tile) || loaded.floorAt(s.tile).isValid()
                                                    : !map.isValid(edgeTiles(s.edge())[0]) || !map.isValid(s.tile) ||
                                                          loaded.edgeAt(s.edge()).isValid())
                return "structure off the map or overlapping";
            else if (s.kind == StructureKind::Floor)
                loaded.m_floors[tileKey(s.tile)] = StructureId{id};
            else
                loaded.m_edges[edgeKey(s.edge())] = StructureId{id};
            loaded.m_structures.emplace(id, s);
        }
        std::uint32_t nextOrderId = 1;
        std::set<std::uint32_t> listed;
        if (version >= 2)
        {
            reader.read(nextOrderId);
            reader.read(count);
            for (std::uint32_t i = 0; i < count && !reader.failed(); ++i)
            {
                std::uint32_t id = 0;
                std::uint32_t components = 0;
                reader.read(id);
                reader.read(components);
                if (reader.failed())
                    break;
                if (id == 0 || id >= nextOrderId || loaded.m_orders.contains(id) || components > loaded.m_structures.size())
                    return "corrupt construction order";
                ConstructionOrder &o = loaded.m_orders[id];
                for (std::uint32_t c = 0; c < components && !reader.failed(); ++c)
                {
                    StructureId component;
                    reader.read(component);
                    const auto s = loaded.m_structures.find(component.value);
                    if (!reader.failed() &&
                        (s == loaded.m_structures.end() || s->second.order.value != id || !listed.insert(component.value).second))
                        return "construction order lists a foreign structure";
                    o.components.push_back(component);
                }
                if (version >= 4)
                {
                    readGoods(reader, o.onSite);
                    reader.read(o.source);
                }
            }
        }
        if (reader.failed())
            return "truncated construction state";
        // Every structure of an order must be listed by it (each exactly once, checked above).
        std::size_t members = 0;
        for (const auto &[id, s] : loaded.m_structures)
        {
            if (s.order.isValid() && !loaded.m_orders.contains(s.order.value))
                return "structure of an unknown construction order";
            members += s.order.isValid() ? 1 : 0;
        }
        if (listed.size() != members)
            return "construction orders and structures disagree";
        // Rooms follow from the structures; the blob only adds their ids and player settings (none before version 3).
        for (const auto &[id, s] : loaded.m_structures)
            loaded.markRooms(s);
        loaded.updateRooms();
        if (version >= 3)
        {
            if (auto error = loaded.m_rooms.restore(reader, version))
                return error;
        }
        if (version >= 4)
        {
            if (auto error = loaded.m_stockpiles.read(reader))
                return error;
            for (const auto &[key, goods] : loaded.m_stockpiles.m_reservations)
            {
                if (!loaded.m_orders.contains(key.second))
                    return "stockpile reservation for an unknown order";
            }
        }
        loaded.m_nextId = nextId;
        loaded.m_nextOrderId = nextOrderId;
        loaded.m_revision = m_revision + 1;
        *this = std::move(loaded);
        return std::nullopt;
    }

} // namespace olam
