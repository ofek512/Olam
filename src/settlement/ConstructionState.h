#pragma once

#include "settlement/Construction.h"

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

    // One placed wall, door or floor.
    struct Structure
    {
        StructureKind kind = StructureKind::Wall;
        StructureMaterial material = StructureMaterial::Timber;
        ConstructionStage stage = ConstructionStage::Planned;
        // Floors: the tile. Walls and doors: the edge's x and y, with `axis`.
        LocalCoord tile;
        EdgeAxis axis = EdgeAxis::North;
        // Doors: swings towards the first tile of edgeTiles() instead of the second.
        bool flipped = false;
        // Builder work done so far (see StructureProperties::workRequired).
        std::uint16_t workDone = 0;
        // Construction order it is part of until the order is completed (invalid for direct building).
        OrderId order;

        LocalEdge edge() const { return {tile.x, tile.y, axis}; }
    };

    // Why a structure cannot be placed. Enum values may be persisted: append only, never reorder.
    enum class PlacementError : std::uint8_t
    {
        None,
        OutOfBounds,
        InvalidMaterial,
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
        // Planned for a blueprint, Completed for direct (developer) building.
        ConstructionStage stage = ConstructionStage::Completed;
        // Order a planned structure joins (only with stage Planned).
        OrderId order;
        // Direct building: fell trees and clear understory in the way instead of rejecting the placement. Plans
        // never touch the site; it is cleared when construction begins.
        bool clearObstructions = false;
        // Doors only.
        bool flipped = false;
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
    };

    struct PlacementResult
    {
        StructureId id;
        PlacementError error = PlacementError::None;
        // Wood of trees felled to clear the site (kg).
        float woodKg = 0.0f;
    };

    // Everything built or planned on a settlement map: a sparse overlay like ResourceState (the generated map
    // stays immutable). Structures are kept by id with ordered edge and tile indices, so memory follows the number
    // of structures, not the map size, and iteration and saves are deterministic.
    class ConstructionState
    {
    public:
        explicit ConstructionState(ConstructionConfig config = {}) : m_config(config) {}

        const ConstructionConfig &config() const { return m_config; }

        // Why a wall or door cannot go on `edge` (None if it can). A door may replace a wall, but a planned door
        // only an unfinished one (replacing finished work is renovation).
        PlacementError checkEdge(const SettlementMap &map, const ResourceState &resources, StructureKind kind,
                                 StructureMaterial material, LocalEdge edge,
                                 ConstructionStage stage = ConstructionStage::Completed) const;
        // Why a floor cannot cover `tile` (None if it can).
        PlacementError checkFloor(const SettlementMap &map, const ResourceState &resources, StructureMaterial material,
                                  LocalCoord tile) const;

        // Places a wall or door (a door replaces a wall on the edge) if placementAllowed(); clearing obstructions
        // changes `resources`.
        PlacementResult placeEdge(const SettlementMap &map, ResourceState &resources, StructureKind kind,
                                  StructureMaterial material, LocalEdge edge, const PlacementOptions &options = {});
        PlacementResult placeFloor(const SettlementMap &map, ResourceState &resources, StructureMaterial material,
                                   LocalCoord tile, const PlacementOptions &options = {});

        // False if there is no such structure. Ids are never reused. Removing a planned structure takes it out of
        // its order; removing a completed one demolishes it at once (developer mode).
        bool remove(StructureId id);
        // Floors on, and walls and doors on or inside, the tile rectangle (inclusive corners); returns the count.
        std::size_t removeInRect(LocalCoord a, LocalCoord b);

        StructureId edgeAt(LocalEdge edge) const;
        StructureId floorAt(LocalCoord tile) const;
        // Null for an unknown or removed id.
        const Structure *structure(StructureId id) const;
        std::size_t structureCount() const { return m_structures.size(); }
        // Floors on, and walls and doors on or inside, the tile rectangle (inclusive corners): floors row by row,
        // then edges row by row.
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

        // Lifecycle steps. Each only moves structures in the expected stage, so repeating a step does nothing
        // (no double delivery or completion); they return whether anything changed.
        // Planned -> AwaitingMaterials for the order's planned structures.
        bool confirmOrder(OrderId id);
        // Developer stand-in for material delivery until stockpiles exist (Phase 4B.5): AwaitingMaterials ->
        // ReadyToBuild.
        bool debugDeliver(OrderId id);
        // ReadyToBuild -> UnderConstruction: clears the site; returns the wood of felled trees (kg), or nullopt
        // if the structure is not ready.
        std::optional<float> beginConstruction(const SettlementMap &map, ResourceState &resources, StructureId id);
        // Builder work on a structure under construction; at its workRequired it is completed. A fully completed
        // order closes (its structures then belong to no order).
        bool addWork(StructureId id, std::uint16_t units);
        // Developer shortcut: confirms, delivers, begins and finishes every structure of the order. Returns the
        // wood of felled trees (kg), or nullopt for an unknown order.
        std::optional<float> debugCompleteOrder(const SettlementMap &map, ResourceState &resources, OrderId id);
        // Removes the order's unfinished structures (completed ones stay built) and the order; returns the
        // number of structures removed.
        std::size_t cancelOrder(OrderId id);

        // Bumped on every change, so renderers know when to refresh.
        std::uint64_t revision() const { return m_revision; }
        // Platform-independent hash of every structure and order.
        std::uint64_t hash() const;

        // Versioned binary form, bound to the map's site and world seed.
        std::vector<std::uint8_t> serialize(const SettlementMap &map) const;
        // Error message on a corrupt buffer or a buffer for another map.
        std::optional<std::string> deserialize(const SettlementMap &map, std::span<const std::uint8_t> bytes);

    private:
        // Problems of one tile under a structure (slope only for floors).
        PlacementError tileError(const SettlementMap &map, const ResourceState &resources, LocalCoord tile, bool floor) const;
        // Fells the tree and clears the understory on `tile`; returns the wood (kg).
        float clearTile(const SettlementMap &map, ResourceState &resources, LocalCoord tile) const;
        StructureId add(const Structure &structure);
        // Erases a structure and its index entry; returns its order, which the caller must prune.
        OrderId erase(StructureId id);
        // Drops an empty order, and closes one whose structures are all completed.
        void pruneOrder(OrderId id);
        void writeState(ByteWriter &writer) const;

        ConstructionConfig m_config;
        std::map<std::uint32_t, Structure> m_structures;
        // Edge key (y << 17 | x << 1 | axis) and tile key (y << 16 | x), both row-major.
        std::map<std::uint32_t, StructureId> m_edges;
        std::map<std::uint32_t, StructureId> m_floors;
        std::map<std::uint32_t, ConstructionOrder> m_orders;
        std::uint32_t m_nextId = 1;
        std::uint32_t m_nextOrderId = 1;
        std::uint64_t m_revision = 0;
    };

} // namespace olam
