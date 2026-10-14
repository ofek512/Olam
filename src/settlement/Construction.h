#pragma once

#include "settlement/LocalTypes.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace olam
{

    // Construction catalogue and edge coordinates of the local map (Phase 4B). Enum values are persisted in
    // construction state: append only, never reorder.

    enum class StructureKind : std::uint8_t
    {
        Wall,
        Door,
        Floor,
        Count,
    };

    std::string_view toString(StructureKind kind);

    // Walls and doors sit on tile edges; floors cover whole tiles.
    constexpr bool isEdgeStructure(StructureKind kind) { return kind != StructureKind::Floor; }

    enum class StructureMaterial : std::uint8_t
    {
        Timber,
        WattleAndDaub,
        RoughStone,
        PackedEarth,
        WoodPlank,
        // Roofs only (Phase 4B.3).
        Thatch,
        Count,
    };

    std::string_view toString(StructureMaterial material);

    // Lifecycle of every structure: planned by the player, supplied, then built.
    enum class ConstructionStage : std::uint8_t
    {
        Planned,
        AwaitingMaterials,
        // Materials delivered to the site.
        ReadyToBuild,
        UnderConstruction,
        Completed,
        Count,
    };

    std::string_view toString(ConstructionStage stage);

    // Basic physical properties of a structure kind built from one material.
    struct StructureProperties
    {
        // Damage it takes before failing (arbitrary units).
        std::uint16_t maxIntegrity;
        // Heat retention 0..255.
        std::uint8_t insulation;
        bool blocksMovement;
        bool blocksSight;
        // Builder work to complete it (integer units, consumed by builders from Phase 4C).
        std::uint16_t workRequired;
    };

    // Materials `kind` can be built from, in catalogue order (the first is the default).
    std::span<const StructureMaterial> structureMaterials(StructureKind kind);
    bool isMaterialAllowed(StructureKind kind, StructureMaterial material);
    // Asserts that the material is allowed.
    const StructureProperties &structureProperties(StructureKind kind, StructureMaterial material);

    // Materials a room's roof can be made of (the first is the default). Roofs are inferred per room, not placed.
    std::span<const StructureMaterial> roofMaterials();
    bool isRoofMaterial(StructureMaterial material);

    enum class EdgeAxis : std::uint8_t
    {
        // Horizontal edge along the top of a tile.
        North,
        // Vertical edge along the left of a tile.
        West,
        Count,
    };

    // A tile edge. A North edge runs from corner (x, y) to (x + 1, y), a West edge from (x, y) to (x, y + 1).
    // The south edge of tile (x, y) is the North edge of (x, y + 1), so every edge has exactly one form; edges on
    // the map's east and south border have x == width or y == height.
    struct LocalEdge
    {
        std::int32_t x = 0;
        std::int32_t y = 0;
        EdgeAxis axis = EdgeAxis::North;

        constexpr bool operator==(const LocalEdge &) const = default;
    };

    // The tiles an edge separates: north then south of a North edge, west then east of a West edge. Either may lie
    // off the map.
    constexpr std::array<LocalCoord, 2> edgeTiles(LocalEdge edge)
    {
        if (edge.axis == EdgeAxis::North)
            return {{{edge.x, edge.y - 1}, {edge.x, edge.y}}};
        return {{{edge.x - 1, edge.y}, {edge.x, edge.y}}};
    }

    // Corner where the edge ends; it starts at corner (x, y).
    constexpr LocalCoord edgeEnd(LocalEdge edge)
    {
        return edge.axis == EdgeAxis::North ? LocalCoord{edge.x + 1, edge.y} : LocalCoord{edge.x, edge.y + 1};
    }

    // The four edges of a tile: north, east, south, west.
    constexpr std::array<LocalEdge, 4> tileEdges(LocalCoord tile)
    {
        return {{{tile.x, tile.y, EdgeAxis::North},
                 {tile.x + 1, tile.y, EdgeAxis::West},
                 {tile.x, tile.y + 1, EdgeAxis::North},
                 {tile.x, tile.y, EdgeAxis::West}}};
    }

    // Placement limits; plain data so they can come from configuration later.
    struct ConstructionConfig
    {
        // Largest elevation difference (dm) between a floor tile and its four neighbours: 5 is 0.5 m over 2 m.
        int maxFloorRiseDm = 5;
        // Largest elevation difference (dm) across a wall or door edge.
        int maxEdgeRiseDm = 8;
        // Largest enclosed area (tiles) that is still a room; anything larger is open ground. It also bounds the
        // work of each room update: 1024 tiles is 4096 m2.
        int maxRoomTiles = 1024;
        // Automatic roofs need every tile of a room within this many steps of a tile along one of its walls: 3
        // allows rooms up to 8 tiles (16 m) across.
        int maxRoofSupportDistance = 3;
    };

} // namespace olam
