#pragma once

#include "core/types/StrongId.h"
#include "settlement/Construction.h"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace olam
{

    class ByteReader;
    class ByteWriter;
    class ConstructionState;

    struct RoomTag;
    using RoomId = StrongId<RoomTag>;
    struct BuildingTag;
    using BuildingId = StrongId<BuildingTag>;

    // The player's choice for a room's roof. Persisted: append only, never reorder.
    enum class RoofMode : std::uint8_t
    {
        // Roofed when its walls can carry a roof (ConstructionConfig::maxRoofSupportDistance).
        Auto,
        // Always roofed.
        Enabled,
        // Never roofed, e.g. a courtyard or a pen.
        Disabled,
        Count,
    };

    std::string_view toString(RoofMode mode);

    struct RoofSettings
    {
        RoofMode mode = RoofMode::Auto;
        // One of roofMaterials().
        StructureMaterial material = StructureMaterial::Thatch;

        constexpr bool operator==(const RoofSettings &) const = default;
    };

    // An enclosed area: tiles connected without crossing a completed wall or door, sealed off from open ground.
    struct Room
    {
        BuildingId building;
        // Row-major.
        std::vector<LocalCoord> tiles;
        // Bounding box (inclusive corners).
        LocalCoord min;
        LocalCoord max;
        // Walls and doors on its outline.
        std::uint32_t walls = 0;
        std::uint32_t doors = 0;
        // Every tile is close enough to a wall to carry an automatic roof.
        bool roofSupported = false;
        RoofSettings roof;

        bool roofed() const { return roof.mode == RoofMode::Enabled || (roof.mode == RoofMode::Auto && roofSupported); }
    };

    // Rooms joined by doors: one dwelling, one workshop. Rooms that only share a wall (terraced houses) are
    // separate buildings.
    struct Building
    {
        // Ascending.
        std::vector<RoomId> rooms;
    };

    // Consecutive tiles of one room in a row (inclusive), for drawing.
    struct RoomRun
    {
        std::int32_t y = 0;
        std::int32_t x0 = 0;
        std::int32_t x1 = 0;
        RoomId room;
    };

    // Rooms and buildings derived from the completed walls and doors of a ConstructionState, which owns and updates
    // it after every edit. Updates are local: only the areas next to changed edges are flooded again, each flood
    // stopping after ConstructionConfig::maxRoomTiles, so an edit never touches the whole map. Ids survive edits:
    // a re-detected room keeps the id (and roof settings) of the old room it overlaps most, so when rooms split or
    // merge the larger part keeps its identity; buildings do the same with their rooms.
    class RoomIndex
    {
    public:
        // Null for an unknown or vanished id.
        const Room *room(RoomId id) const;
        RoomId roomAt(LocalCoord tile) const;
        const Building *building(BuildingId id) const;
        std::size_t roomCount() const { return m_rooms.size(); }
        std::size_t buildingCount() const { return m_buildings.size(); }
        std::size_t roofedCount() const;
        // Ascending.
        std::vector<RoomId> roomIds() const;
        // Room tiles in the tile rectangle (inclusive corners) as row runs, row by row.
        std::vector<RoomRun> runsInRect(LocalCoord a, LocalCoord b) const;

    private:
        friend class ConstructionState;

        // The tiles on both sides of `edge` need re-detection: a completed wall or door appeared or vanished there.
        void markDirty(LocalEdge edge);
        bool dirty() const { return !m_dirty.empty(); }
        // Re-detects rooms around the dirty tiles.
        void update(const ConstructionState &construction);
        // False for an unknown room or a material that is not a roof material.
        bool setRoof(RoomId id, RoofSettings roof);
        void write(ByteWriter &writer) const;
        // Replaces the ids and roof settings of freshly detected rooms with saved ones (see write()); an error if
        // the saved rooms do not match the detected ones.
        std::optional<std::string> restore(ByteReader &reader);

        std::map<std::uint32_t, Room> m_rooms;
        // Tile key (y << 16 | x) -> room.
        std::map<std::uint32_t, RoomId> m_tiles;
        std::map<std::uint32_t, Building> m_buildings;
        std::set<std::uint32_t> m_dirty;
        std::uint32_t m_nextRoomId = 1;
        std::uint32_t m_nextBuildingId = 1;
    };

} // namespace olam
