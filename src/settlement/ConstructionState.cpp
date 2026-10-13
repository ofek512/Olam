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
        // Version 1 (Phase 4B.1) had no construction orders; it still loads.
        constexpr std::uint32_t kVersion = 2;
        // Index keys hold coordinates in 15 (edge y) and 16 bits.
        constexpr std::int32_t kMaxKeyCoord = 32766;

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

    float ConstructionState::clearTile(const SettlementMap &map, ResourceState &resources, LocalCoord tile) const
    {
        float wood = 0.0f;
        if (const TreeId tree = map.terrain().treeId[map.index(tile)]; tree.isValid())
            wood = resources.fellTree(map, tree);
        if (const auto u = map.understoryAt(tile); u && blocksConstruction(map.understory().kind[*u]))
            resources.clearUnderstory(map, *u);
        return wood;
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

    StructureId ConstructionState::add(const Structure &structure)
    {
        const StructureId id{m_nextId++};
        if (structure.kind == StructureKind::Floor)
            m_floors[tileKey(structure.tile)] = id;
        else
            m_edges[edgeKey(structure.edge())] = id;
        Structure &added = m_structures.emplace(id.value, structure).first->second;
        if (added.stage == ConstructionStage::Completed)
            added.workDone = structureProperties(added.kind, added.material).workRequired;
        if (added.order.isValid())
            m_orders.at(added.order.value).components.push_back(id);
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
        else
            m_edges.erase(edgeKey(structure.edge()));
        const OrderId orderId = structure.order;
        if (orderId.isValid())
            std::erase(m_orders.at(orderId.value).components, id);
        m_structures.erase(it);
        ++m_revision;
        return orderId;
    }

    void ConstructionState::pruneOrder(OrderId id)
    {
        const auto it = m_orders.find(id.value);
        if (it == m_orders.end())
            return;
        for (const StructureId component : it->second.components)
        {
            if (m_structures.at(component.value).stage != ConstructionStage::Completed)
                return;
        }
        for (const StructureId component : it->second.components)
            m_structures.at(component.value).order = {};
        m_orders.erase(it);
        ++m_revision;
    }

    bool ConstructionState::remove(StructureId id)
    {
        if (!m_structures.contains(id.value))
            return false;
        pruneOrder(erase(id));
        return true;
    }

    std::size_t ConstructionState::removeInRect(LocalCoord a, LocalCoord b)
    {
        const std::vector<StructureId> ids = structuresInRect(a, b);
        for (const StructureId id : ids)
            remove(id);
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
        if (tile.x < 0 || tile.y < 0 || tile.x > kMaxKeyCoord || tile.y > kMaxKeyCoord)
            return {};
        const auto it = m_floors.find(tileKey(tile));
        return it != m_floors.end() ? it->second : StructureId{};
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
        const Structure *s = structure(floorAt(tile));
        return s ? s->order : OrderId{};
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

    bool ConstructionState::debugDeliver(OrderId id)
    {
        const ConstructionOrder *o = order(id);
        if (!o)
            return false;
        bool changed = false;
        for (const StructureId component : o->components)
        {
            Structure &s = m_structures.at(component.value);
            if (s.stage == ConstructionStage::AwaitingMaterials)
            {
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
        if (s.kind == StructureKind::Floor)
        {
            wood = clearTile(map, resources, s.tile);
        }
        else
        {
            for (const LocalCoord tile : edgeTiles(s.edge()))
                wood += clearTile(map, resources, tile);
        }
        s.stage = ConstructionStage::UnderConstruction;
        ++m_revision;
        return wood;
    }

    bool ConstructionState::addWork(StructureId id, std::uint16_t units)
    {
        const auto it = m_structures.find(id.value);
        if (it == m_structures.end() || it->second.stage != ConstructionStage::UnderConstruction || units == 0)
            return false;
        Structure &s = it->second;
        const std::uint16_t required = structureProperties(s.kind, s.material).workRequired;
        s.workDone = static_cast<std::uint16_t>(std::min<int>(required, s.workDone + units));
        ++m_revision;
        if (s.workDone >= required)
        {
            s.stage = ConstructionStage::Completed;
            pruneOrder(s.order);
        }
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
            addWork(component, std::numeric_limits<std::uint16_t>::max());
        }
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
        m_orders.erase(id.value);
        ++m_revision;
        return removed;
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
        }
        writer.write(m_nextOrderId);
        writer.write(static_cast<std::uint32_t>(m_orders.size()));
        for (const auto &[id, o] : m_orders)
        {
            writer.write(id);
            writer.write(static_cast<std::uint32_t>(o.components.size()));
            for (const StructureId component : o.components)
                writer.write(component);
        }
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

        ConstructionState loaded(m_config);
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
            if (reader.failed())
                break;
            if (id == 0 || id >= nextId || loaded.m_structures.contains(id) || s.kind >= StructureKind::Count ||
                s.stage >= ConstructionStage::Count || s.axis >= EdgeAxis::Count || !isMaterialAllowed(s.kind, s.material))
                return "corrupt structure entry";
            if (s.kind == StructureKind::Floor ? !map.isValid(s.tile) || loaded.floorAt(s.tile).isValid()
                                               : !map.isValid(edgeTiles(s.edge())[0]) || !map.isValid(s.tile) ||
                                                     loaded.edgeAt(s.edge()).isValid())
                return "structure off the map or overlapping";
            if (s.kind == StructureKind::Floor)
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
        loaded.m_nextId = nextId;
        loaded.m_nextOrderId = nextOrderId;
        loaded.m_revision = m_revision + 1;
        *this = std::move(loaded);
        return std::nullopt;
    }

} // namespace olam
