#pragma once

#include "settlement/Construction.h"
#include "settlement/ConstructionCosts.h"
#include "settlement/Furniture.h"
#include "settlement/Rooms.h"
#include "settlement/Stockpiles.h"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace olam
{

    class ByteWriter;
    class ResourceState;
    class SettlementMap;

    struct StructureTag;
    using StructureId = StrongId<StructureTag>;
    struct ConstructionOrderTag;
    using OrderId = StrongId<ConstructionOrderTag>;

    // One placed wall, door, floor or piece of furniture.
    struct Structure
    {
        StructureKind kind = StructureKind::Wall;
        StructureMaterial material = StructureMaterial::Timber;
        ConstructionStage stage = ConstructionStage::Planned;
        // Floors: the tile. Walls and doors: the edge's x and y, with `axis`. Furniture: the north-west cell of
        // its footprint (kCellsPerTile cells per tile).
        LocalCoord tile;
        EdgeAxis axis = EdgeAxis::North;
        // Doors: swings towards the first tile of edgeTiles() instead of the second.
        bool flipped = false;
        // Builder work done so far (see requiredWork()).
        std::uint16_t workDone = 0;
        // Construction order it is part of until the order is completed (invalid for direct building).
        OrderId order;
        // Furniture only.
        FurnitureKind furniture = FurnitureKind::Bed;
        Facing facing = Facing::North;

        LocalEdge edge() const { return {tile.x, tile.y, axis}; }
    };

    // Builder work to complete a structure.
    std::uint16_t requiredWork(const Structure &structure);

    // A block of furniture cells: [min, max) on both axes.
    struct CellRect
    {
        LocalCoord min;
        LocalCoord max;
    };

    CellRect furnitureCells(FurnitureKind kind, LocalCoord cell, Facing facing);
    inline CellRect furnitureCells(const Structure &s) { return furnitureCells(s.furniture, s.tile, s.facing); }

    // Why a structure cannot be placed. Enum values may be persisted: append only, never reorder.
    enum class PlacementError : std::uint8_t
    {
        None,
        OutOfBounds,
        InvalidMaterial,
        // Another structure, furniture, a stockpile or a wall through the footprint.
        Occupied,
        Water,
        TooSteep,
        // A standing tree is in the way.
        Tree,
        // A shrub, bush, fallen log or rock is in the way.
        Obstacle,
        Count,
    };

    std::string_view toString(PlacementError error);

    // Obstructions that clearing the site removes; every other error is final.
    constexpr bool isClearable(PlacementError error) { return error == PlacementError::Tree || error == PlacementError::Obstacle; }

    struct PlacementOptions
    {
        // Planned for a blueprint, Completed for direct (developer) building, which costs nothing.
        ConstructionStage stage = ConstructionStage::Completed;
        // Order a planned structure joins (only with stage Planned).
        OrderId order;
        // Direct building: fell trees and clear understory in the way instead of rejecting the placement. Plans
        // never touch the site; it is cleared when construction begins.
        bool clearObstructions = false;
        // Doors only.
        bool flipped = false;
        // Furniture only.
        Facing facing = Facing::North;
    };

    // Whether a placement with `error` goes ahead: plans may sit on clearable obstructions, direct building
    // only when asked to clear them.
    constexpr bool placementAllowed(PlacementError error, const PlacementOptions &options)
    {
        if (error == PlacementError::None)
            return true;
        return isClearable(error) && (options.stage < ConstructionStage::UnderConstruction || options.clearObstructions);
    }

    // A blueprint: structures planned, supplied and built together, e.g. one house. Its stage is not stored but
    // derived from its structures (see ConstructionState::orderStage), so the two cannot disagree.
    struct ConstructionOrder
    {
        // In placement order.
        std::vector<StructureId> components;
        // Goods delivered to the site and not yet assigned to a structure. A structure that becomes ReadyToBuild
        // takes its cost from here; one removed before completion gives it back.
        GoodAmounts onSite;
        // Stockpile the first delivery came from; leftovers return there.
        StockpileId source;
    };

    struct PlacementResult
    {
        StructureId id;
        PlacementError error = PlacementError::None;
        // Wood of trees felled to clear the site (kg).
        float woodKg = 0.0f;
    };

    struct StockpileResult
    {
        StockpileId id;
        PlacementError error = PlacementError::None;
        float woodKg = 0.0f;
    };

    // Everything built or planned on a settlement map: a sparse overlay like ResourceState (the generated map
    // stays immutable). Structures are kept by id with ordered edge, tile and furniture indices, so memory follows
    // the number of structures, not the map size, and iteration and saves are deterministic. The rooms enclosed by
    // completed walls and doors are kept up to date after every edit. It also owns the stockpiles, so material
    // reservations, deliveries and refunds always agree with the orders they belong to.
    class ConstructionState
    {
    public:
        explicit ConstructionState(ConstructionConfig config = {}, ConstructionCosts costs = defaultConstructionCosts())
            : m_config(config), m_costs(std::move(costs))
        {
        }

        const ConstructionConfig &config() const { return m_config; }
        const ConstructionCosts &costs() const { return m_costs; }

        // Why a wall or door cannot go on `edge` (None if it can). A door may replace a wall, but a planned door
        // only an unfinished one (replacing finished work is renovation). Walls never cut through furniture.
        PlacementError checkEdge(const SettlementMap &map, const ResourceState &resources, StructureKind kind,
                                 StructureMaterial material, LocalEdge edge,
                                 ConstructionStage stage = ConstructionStage::Completed) const;
        // Why a floor cannot cover `tile` (None if it can).
        PlacementError checkFloor(const SettlementMap &map, const ResourceState &resources, StructureMaterial material,
                                  LocalCoord tile) const;
        // Why furniture cannot stand with its north-west cell at `cell`: ground like a floor, no other furniture
        // or stockpile, and no wall or door (of any stage) through the footprint.
        PlacementError checkFurniture(const SettlementMap &map, const ResourceState &resources, FurnitureKind kind,
                                      StructureMaterial material, LocalCoord cell, Facing facing) const;

        // Places a wall or door (a door replaces a wall on the edge) if placementAllowed(); clearing obstructions
        // changes `resources`.
        PlacementResult placeEdge(const SettlementMap &map, ResourceState &resources, StructureKind kind,
                                  StructureMaterial material, LocalEdge edge, const PlacementOptions &options = {});
        PlacementResult placeFloor(const SettlementMap &map, ResourceState &resources, StructureMaterial material,
                                   LocalCoord tile, const PlacementOptions &options = {});
        PlacementResult placeFurniture(const SettlementMap &map, ResourceState &resources, FurnitureKind kind,
                                       StructureMaterial material, LocalCoord cell, const PlacementOptions &options = {});

        // False if there is no such structure. Ids are never reused. Removing a planned structure takes it out of
        // its order (a supplied one gives its materials back to the order); removing a completed one demolishes it
        // at once (developer mode).
        bool remove(StructureId id);
        // Floors and furniture on, and walls and doors on or inside, the tile rectangle (inclusive corners);
        // returns the count.
        std::size_t removeInRect(LocalCoord a, LocalCoord b);

        StructureId edgeAt(LocalEdge edge) const;
        StructureId floorAt(LocalCoord tile) const;
        // Furniture covering a cell.
        StructureId furnitureAt(LocalCoord cell) const;
        // Furniture touching a tile, ascending.
        std::vector<StructureId> furnitureOnTile(LocalCoord tile) const;
        // Null for an unknown or removed id.
        const Structure *structure(StructureId id) const;
        std::size_t structureCount() const { return m_structures.size(); }
        // Floors, furniture, and walls and doors on or inside the tile rectangle (inclusive corners): floors row
        // by row, then furniture by id, then edges row by row (the drawing order).
        std::vector<StructureId> structuresInRect(LocalCoord a, LocalCoord b) const;

        // A new, empty construction order; planned structures join it through PlacementOptions::order.
        OrderId createOrder();
        // Null for an unknown, cancelled or completed order.
        const ConstructionOrder *order(OrderId id) const;
        std::size_t orderCount() const { return m_orders.size(); }
        // Earliest stage of the order's structures (Planned when empty); Count for an unknown order.
        ConstructionStage orderStage(OrderId id) const;
        // Order of the structure on `edge` or `tile`, if any.
        OrderId orderAt(LocalEdge edge) const;
        OrderId orderAt(LocalCoord tile) const;

        // Materials (Phase 4B.5). Goods a structure consumes (direct developer building is free).
        GoodAmounts cost(const Structure &structure) const;
        // Everything the order's structures cost.
        GoodAmounts orderCost(OrderId id) const;
        // What its structures awaiting materials still need beyond what is on site and reserved.
        GoodAmounts orderShortfall(OrderId id) const;

        // Lifecycle steps. Each only moves structures in the expected stage, so repeating a step does nothing
        // (no double delivery or completion); they return whether anything changed.
        // Planned -> AwaitingMaterials for the order's planned structures.
        bool confirmOrder(OrderId id);
        // Reserves what the structures awaiting materials still need in the nearest stockpiles (and releases
        // reservations beyond that); returns what could not be reserved.
        GoodAmounts allocateMaterials(OrderId id);
        // Developer stand-in for hauling (Phase 4C): allocates, moves the order's reservations from their
        // stockpiles to the site, and makes every structure whose cost is on site ReadyToBuild, in placement
        // order.
        bool debugDeliver(OrderId id);
        // ReadyToBuild -> UnderConstruction: clears the site; returns the wood of felled trees (kg), or nullopt
        // if the structure is not ready.
        std::optional<float> beginConstruction(const SettlementMap &map, ResourceState &resources, StructureId id);
        // Builder work on a structure under construction; at its requiredWork() it is completed. A fully completed
        // order closes (its structures then belong to no order, leftovers go back to a stockpile).
        bool addWork(StructureId id, std::uint16_t units);
        // Developer shortcut: confirms, delivers, then begins and finishes every structure whose materials are on
        // site; parts the stockpiles cannot supply keep waiting. Returns the wood of felled trees (kg), or nullopt
        // for an unknown order.
        std::optional<float> debugCompleteOrder(const SettlementMap &map, ResourceState &resources, OrderId id);
        // Removes the order's unfinished structures (completed ones stay built) and the order, releasing its
        // reservations and returning delivered goods to a stockpile; returns the number of structures removed.
        std::size_t cancelOrder(OrderId id);

        // Stockpiles: tile rectangles of open ground that hold goods.
        const Stockpiles &stockpiles() const { return m_stockpiles; }
        // Why a stockpile cannot cover the tile rectangle (inclusive corners).
        PlacementError checkStockpile(const SettlementMap &map, const ResourceState &resources, LocalCoord a, LocalCoord b) const;
        // Creates the stockpile if every tile is fine (with `clearObstructions`, trees and shrubs are cleared).
        StockpileResult createStockpile(const SettlementMap &map, ResourceState &resources, LocalCoord a, LocalCoord b,
                                        bool clearObstructions = false);
        // Developer removal: its goods are lost and orders lose their reservations there.
        bool removeStockpile(StockpileId id);
        // Developer stand-in for gathering and the founding supplies.
        bool debugAddGoods(StockpileId id, const GoodAmounts &goods);

        // Rooms and buildings enclosed by the completed walls and doors.
        const RoomIndex &rooms() const { return m_rooms; }
        // The player's roof choice for a room (kept through renovations); returns whether it changed, so false
        // also for an unknown room or a material that is not in roofMaterials().
        bool setRoof(RoomId id, RoofSettings roof);
        // The player's designation of a room; returns whether it changed.
        bool setRoomFunction(RoomId id, RoomFunction function);
        // Goods still needed for the room's roof (empty when it has none or it is complete).
        GoodAmounts roofCost(RoomId id) const;
        // Builds the rest of the roof from unreserved stockpile goods, nearest first (developer stand-in for
        // roofers); false if the room needs no roof or the goods are missing.
        bool buildRoof(RoomId id);

        // Bumped on every change, so renderers know when to refresh.
        std::uint64_t revision() const { return m_revision; }
        // Platform-independent hash of every structure, order, room, stockpile and reservation.
        std::uint64_t hash() const;

        // Versioned binary form, bound to the map's site and world seed.
        std::vector<std::uint8_t> serialize(const SettlementMap &map) const;
        // Error message on a corrupt buffer or a buffer for another map.
        std::optional<std::string> deserialize(const SettlementMap &map, std::span<const std::uint8_t> bytes);

    private:
        // Problems of one tile under a structure (slope only for floors and furniture).
        PlacementError tileError(const SettlementMap &map, const ResourceState &resources, LocalCoord tile, bool floor) const;
        // Fells the tree and clears the understory on `tile`; returns the wood (kg).
        float clearTile(const SettlementMap &map, ResourceState &resources, LocalCoord tile) const;
        // Tiles a structure stands on (for clearing the site).
        std::vector<LocalCoord> siteTiles(const Structure &structure) const;
        // Where an order's materials go: its first structure.
        LocalCoord orderSite(const ConstructionOrder &order) const;
        StructureId add(const Structure &structure);
        // Erases a structure and its index entry; returns its order, which the caller must prune.
        OrderId erase(StructureId id);
        // Drops an empty order, and closes one whose structures are all completed. Otherwise trims its
        // reservations to what it still needs.
        void pruneOrder(OrderId id);
        // Releases an order's reservations and returns its goods on site to a stockpile.
        void settleOrder(OrderId id, ConstructionOrder &order);
        // Goods the order's structures awaiting materials need beyond what is on site.
        GoodAmounts orderNeed(const ConstructionOrder &order) const;
        // addWork() without the room update.
        bool work(StructureId id, std::uint16_t units);
        // A completed wall or door appeared or vanished: rooms next to it need re-detection.
        void markRooms(const Structure &structure);
        // Re-detects rooms around edges changed since the last update; every public edit ends with it.
        void updateRooms();
        void writeState(ByteWriter &writer) const;

        ConstructionConfig m_config;
        ConstructionCosts m_costs;
        std::map<std::uint32_t, Structure> m_structures;
        // Edge key (y << 17 | x << 1 | axis) and tile key (y << 16 | x), both row-major.
        std::map<std::uint32_t, StructureId> m_edges;
        std::map<std::uint32_t, StructureId> m_floors;
        // Tile key -> furniture touching the tile.
        std::multimap<std::uint32_t, StructureId> m_furniture;
        std::map<std::uint32_t, ConstructionOrder> m_orders;
        RoomIndex m_rooms;
        Stockpiles m_stockpiles;
        std::uint32_t m_nextId = 1;
        std::uint32_t m_nextOrderId = 1;
        std::uint64_t m_revision = 0;
    };

    // Founding supplies (the founding camp itself is Phase 4B.7): enough for first shelters, not wealth.
    GoodAmounts defaultFoundingGoods();

    // A 3 x 3 stockpile holding `goods` on the open ground nearest `near` (trees and shrubs there are cleared).
    StockpileResult createFoundingStockpile(const SettlementMap &map, ResourceState &resources, ConstructionState &construction,
                                            LocalCoord near, const GoodAmounts &goods);

} // namespace olam
