#include "TestFramework.h"
#include "settlement/ConstructionTestUtil.h"

#include <vector>

using namespace olam;

namespace
{

    struct Site
    {
        const SettlementMap *map = nullptr;
        LocalCoord at;
    };

    // The shared map and a clear w x h block on it.
    std::optional<Site> site(int w, int h)
    {
        const SettlementMap *map = test::constructionTestMap();
        OLAM_CHECK(map != nullptr);
        if (!map)
            return std::nullopt;
        const auto area = test::findClearArea(*map, w, h);
        OLAM_CHECK(area.has_value());
        if (!area)
            return std::nullopt;
        return Site{map, *area};
    }

    // The edges around a w x h block: top and bottom rows, then left and right columns.
    std::vector<LocalEdge> outline(LocalCoord at, int w, int h)
    {
        std::vector<LocalEdge> edges;
        for (int x = at.x; x < at.x + w; ++x)
        {
            edges.push_back({x, at.y, EdgeAxis::North});
            edges.push_back({x, at.y + h, EdgeAxis::North});
        }
        for (int y = at.y; y < at.y + h; ++y)
        {
            edges.push_back({at.x, y, EdgeAxis::West});
            edges.push_back({at.x + w, y, EdgeAxis::West});
        }
        return edges;
    }

    // A vertical wall line at column x from row y over h tiles.
    std::vector<LocalEdge> partition(int x, int y, int h)
    {
        std::vector<LocalEdge> edges;
        for (int row = y; row < y + h; ++row)
            edges.push_back({x, row, EdgeAxis::West});
        return edges;
    }

    void build(const Site &s, ResourceState &resources, ConstructionState &construction, const std::vector<LocalEdge> &edges,
               StructureKind kind = StructureKind::Wall, const PlacementOptions &options = {})
    {
        for (const LocalEdge edge : edges)
            OLAM_CHECK(construction.placeEdge(*s.map, resources, kind, StructureMaterial::Timber, edge, options).error ==
                       PlacementError::None);
    }

    void removeEdge(ConstructionState &construction, LocalEdge edge) { OLAM_CHECK(construction.remove(construction.edgeAt(edge))); }

    std::size_t roomSize(const ConstructionState &construction, RoomId id)
    {
        const Room *room = construction.rooms().room(id);
        return room ? room->tiles.size() : 0;
    }

} // namespace

OLAM_TEST(fully_enclosed_room_is_detected)
{
    const auto s = site(3, 2);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    const std::vector<LocalEdge> walls = outline(s->at, 3, 2);
    build(*s, resources, construction, {walls.begin(), walls.end() - 1});
    // One gap is enough to leave it open.
    OLAM_CHECK(construction.rooms().roomCount() == 0);
    build(*s, resources, construction, {walls.back()});

    OLAM_CHECK(construction.rooms().roomCount() == 1 && construction.rooms().buildingCount() == 1);
    const RoomId id = construction.rooms().roomAt(s->at);
    const Room *room = construction.rooms().room(id);
    OLAM_CHECK(room != nullptr);
    if (!room)
        return;
    OLAM_CHECK(room->tiles.size() == 6 && room->walls == 10 && room->doors == 0);
    OLAM_CHECK(room->min == s->at && room->max == (LocalCoord{s->at.x + 2, s->at.y + 1}));
    OLAM_CHECK(room->tiles.front() == s->at);
    for (const LocalCoord tile : room->tiles)
        OLAM_CHECK(construction.rooms().roomAt(tile) == id);
    // Tiles just outside are open ground.
    OLAM_CHECK(!construction.rooms().roomAt({s->at.x - 1, s->at.y}).isValid());
    OLAM_CHECK(!construction.rooms().roomAt({s->at.x + 3, s->at.y + 1}).isValid());
    const Building *building = construction.rooms().building(room->building);
    OLAM_CHECK(building && building->rooms == std::vector<RoomId>{id});
    // Small rooms get an automatic roof of the default material.
    OLAM_CHECK(room->roofSupported && room->roofed() && room->roof == RoofSettings{});
    OLAM_CHECK(construction.rooms().roofedCount() == 1);
}

OLAM_TEST(open_and_unfinished_enclosures_are_not_rooms)
{
    const auto s = site(3, 3);
    if (!s)
        return;
    ResourceState resources;
    // Unfinished walls are openings: a blueprint hut encloses nothing until it is built.
    {
        ConstructionState construction;
        test::addStockpile(*s->map, resources, construction);
        const OrderId order = construction.createOrder();
        build(*s, resources, construction, outline(s->at, 3, 3), StructureKind::Wall,
              {.stage = ConstructionStage::Planned, .order = order});
        OLAM_CHECK(construction.rooms().roomCount() == 0);
        construction.confirmOrder(order);
        construction.debugDeliver(order);
        const StructureId first = construction.order(order)->components.front();
        construction.beginConstruction(*s->map, resources, first);
        OLAM_CHECK(construction.rooms().roomCount() == 0);
        // Finishing the last wall closes the room, and its roof follows.
        construction.debugCompleteOrder(*s->map, resources, order);
        OLAM_CHECK(construction.rooms().roomCount() == 1);
        OLAM_CHECK(construction.rooms().roofedCount() == 1);
    }
    // An enclosure larger than maxRoomTiles counts as open ground.
    {
        ConstructionState construction({.maxRoomTiles = 8});
        build(*s, resources, construction, outline(s->at, 3, 3));
        OLAM_CHECK(construction.rooms().roomCount() == 0);
        // Dividing it gives rooms small enough.
        build(*s, resources, construction, partition(s->at.x + 1, s->at.y, 3));
        OLAM_CHECK(construction.rooms().roomCount() == 2);
        OLAM_CHECK(roomSize(construction, construction.rooms().roomAt(s->at)) == 3);
        OLAM_CHECK(roomSize(construction, construction.rooms().roomAt({s->at.x + 2, s->at.y})) == 6);
    }
    // Removing a wall opens the room again.
    {
        ConstructionState construction;
        const auto walls = outline(s->at, 3, 3);
        build(*s, resources, construction, walls);
        OLAM_CHECK(construction.rooms().roomCount() == 1);
        removeEdge(construction, walls[2]);
        OLAM_CHECK(construction.rooms().roomCount() == 0 && construction.rooms().buildingCount() == 0);
        OLAM_CHECK(!construction.rooms().roomAt(s->at).isValid());
    }
}

OLAM_TEST(two_room_house_with_doors_is_one_building)
{
    const auto s = site(5, 3);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    const LocalCoord at = s->at;
    build(*s, resources, construction, outline(at, 5, 3));
    build(*s, resources, construction, partition(at.x + 2, at.y, 3));
    const RoomId west = construction.rooms().roomAt(at);
    const RoomId east = construction.rooms().roomAt({at.x + 4, at.y});
    OLAM_CHECK(construction.rooms().roomCount() == 2 && west.isValid() && east.isValid() && !(west == east));
    OLAM_CHECK(roomSize(construction, west) == 6 && roomSize(construction, east) == 9);
    // Rooms that only share a wall are separate buildings (terraced houses).
    OLAM_CHECK(construction.rooms().buildingCount() == 2);

    // An outside door: the room stays enclosed (closed doors seal it) and keeps its id.
    build(*s, resources, construction, {{at.x + 1, at.y, EdgeAxis::North}}, StructureKind::Door);
    OLAM_CHECK(construction.rooms().roomAt(at) == west);
    OLAM_CHECK(construction.rooms().room(west)->doors == 1 && construction.rooms().room(west)->walls == 9);
    // A door in the partition joins both rooms into one building; the rooms keep their ids.
    const BuildingId before = construction.rooms().room(east)->building;
    build(*s, resources, construction, {{at.x + 2, at.y + 1, EdgeAxis::West}}, StructureKind::Door);
    OLAM_CHECK(construction.rooms().roomCount() == 2);
    OLAM_CHECK(construction.rooms().roomAt(at) == west && construction.rooms().roomAt({at.x + 4, at.y}) == east);
    OLAM_CHECK(construction.rooms().buildingCount() == 1);
    const Room *westRoom = construction.rooms().room(west);
    const Room *eastRoom = construction.rooms().room(east);
    OLAM_CHECK(westRoom->building == eastRoom->building && westRoom->building == before);
    OLAM_CHECK(construction.rooms().building(before)->rooms.size() == 2);
    OLAM_CHECK(westRoom->roofed() && eastRoom->roofed());
    // An unfinished door is an opening: replacing the partition door with a planned one merges the rooms.
    removeEdge(construction, {at.x + 2, at.y + 1, EdgeAxis::West});
    const OrderId order = construction.createOrder();
    build(*s, resources, construction, {{at.x + 2, at.y + 1, EdgeAxis::West}}, StructureKind::Door,
          {.stage = ConstructionStage::Planned, .order = order});
    OLAM_CHECK(construction.rooms().roomCount() == 1 && roomSize(construction, east) == 15);
}

OLAM_TEST(rooms_split_and_merge_keeping_identity)
{
    const auto s = site(5, 3);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    const LocalCoord at = s->at;
    build(*s, resources, construction, outline(at, 5, 2));
    const RoomId hall = construction.rooms().roomAt(at);
    const BuildingId house = construction.rooms().room(hall)->building;
    OLAM_CHECK(construction.setRoof(hall, {RoofMode::Auto, StructureMaterial::Timber}));

    // Dividing the hall: the larger part keeps its id and building, the smaller is new but inherits its roof.
    build(*s, resources, construction, partition(at.x + 3, at.y, 2));
    OLAM_CHECK(construction.rooms().roomCount() == 2);
    OLAM_CHECK(construction.rooms().roomAt(at) == hall && roomSize(construction, hall) == 6);
    const RoomId side = construction.rooms().roomAt({at.x + 4, at.y});
    OLAM_CHECK(side.isValid() && !(side == hall) && roomSize(construction, side) == 4);
    OLAM_CHECK(construction.rooms().room(side)->roof.material == StructureMaterial::Timber);
    OLAM_CHECK(construction.rooms().room(hall)->building == house);
    OLAM_CHECK(!(construction.rooms().room(side)->building == house));

    // Removing half of the partition merges them again, under the larger room's id and building.
    removeEdge(construction, {at.x + 3, at.y, EdgeAxis::West});
    OLAM_CHECK(construction.rooms().roomCount() == 1 && construction.rooms().buildingCount() == 1);
    OLAM_CHECK(construction.rooms().roomAt({at.x + 4, at.y + 1}) == hall && roomSize(construction, hall) == 10);
    OLAM_CHECK(construction.rooms().room(hall)->building == house);
    OLAM_CHECK(!construction.rooms().room(side));

    // Extending the house: the extension is walled in first (a room of its own), then the old wall comes down
    // and the merged room keeps the hall's identity and roof.
    for (int x = at.x; x < at.x + 5; ++x)
        build(*s, resources, construction, {{x, at.y + 3, EdgeAxis::North}});
    build(*s, resources, construction, {{at.x, at.y + 2, EdgeAxis::West}, {at.x + 5, at.y + 2, EdgeAxis::West}});
    OLAM_CHECK(construction.rooms().roomCount() == 2);
    for (int x = at.x; x < at.x + 5; ++x)
        removeEdge(construction, {x, at.y + 2, EdgeAxis::North});
    OLAM_CHECK(construction.rooms().roomCount() == 1 && construction.rooms().buildingCount() == 1);
    OLAM_CHECK(construction.rooms().roomAt(at) == hall && roomSize(construction, hall) == 15);
    OLAM_CHECK(construction.rooms().room(hall)->building == house);
    OLAM_CHECK(construction.rooms().room(hall)->roof.material == StructureMaterial::Timber);
}

OLAM_TEST(automatic_roofs_follow_structural_changes)
{
    const auto s = site(3, 3);
    if (!s)
        return;
    ResourceState resources;
    // Automatic roofs only where every tile touches a wall.
    ConstructionState construction({.maxRoofSupportDistance = 0});
    const LocalCoord at = s->at;
    build(*s, resources, construction, outline(at, 3, 3));
    const RoomId id = construction.rooms().roomAt(at);
    // The centre tile is too far from the walls for an automatic roof.
    OLAM_CHECK(construction.rooms().room(id) && !construction.rooms().room(id)->roofSupported);
    OLAM_CHECK(!construction.rooms().room(id)->roofed() && construction.rooms().roofedCount() == 0);

    // A partition carries the roof: both narrower rooms are roofed.
    build(*s, resources, construction, partition(at.x + 1, at.y, 3));
    OLAM_CHECK(construction.rooms().roomCount() == 2 && construction.rooms().roofedCount() == 2);
    // Only walls carry roofs: a door in the middle of the partition leaves the larger room unsupported again.
    build(*s, resources, construction, {{at.x + 1, at.y + 1, EdgeAxis::West}}, StructureKind::Door);
    OLAM_CHECK(construction.rooms().roomCount() == 2 && construction.rooms().roofedCount() == 1);
    const Room *large = construction.rooms().room(construction.rooms().roomAt({at.x + 1, at.y + 1}));
    OLAM_CHECK(large && large->tiles.size() == 6 && !large->roofSupported && !large->roofed());
    OLAM_CHECK(construction.rooms().room(construction.rooms().roomAt(at))->roofed());
    // Removing the partition: one wide room again, without a roof.
    for (const LocalEdge edge : partition(at.x + 1, at.y, 3))
        removeEdge(construction, edge);
    OLAM_CHECK(construction.rooms().roomCount() == 1 && construction.rooms().roofedCount() == 0);
    OLAM_CHECK(construction.rooms().roomAt(at) == id);
}

OLAM_TEST(manual_roof_overrides_are_respected)
{
    const auto s = site(3, 3);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction({.maxRoofSupportDistance = 0});
    const LocalCoord at = s->at;
    build(*s, resources, construction, outline(at, 3, 3));
    const RoomId id = construction.rooms().roomAt(at);
    OLAM_CHECK(!construction.rooms().room(id)->roofed());

    // Forcing a roof overrides the support check; disabling it overrides it the other way.
    const std::uint64_t revision = construction.revision();
    OLAM_CHECK(construction.setRoof(id, {RoofMode::Enabled, StructureMaterial::Thatch}));
    OLAM_CHECK(construction.revision() > revision && construction.rooms().room(id)->roofed());
    OLAM_CHECK(!construction.setRoof(id, {RoofMode::Enabled, StructureMaterial::Thatch}));
    // Only roof materials, and only known rooms.
    OLAM_CHECK(!construction.setRoof(id, {RoofMode::Enabled, StructureMaterial::WoodPlank}));
    OLAM_CHECK(!construction.setRoof(RoomId{999}, {}));
    OLAM_CHECK(construction.rooms().room(id)->roof.material == StructureMaterial::Thatch);

    // Renovations keep the choice: a new door, then a partition that splits the room.
    build(*s, resources, construction, {{at.x + 1, at.y, EdgeAxis::North}}, StructureKind::Door);
    OLAM_CHECK(construction.rooms().room(id) && construction.rooms().room(id)->roof.mode == RoofMode::Enabled);
    OLAM_CHECK(construction.setRoof(id, {RoofMode::Disabled, StructureMaterial::Timber}));
    build(*s, resources, construction, partition(at.x + 2, at.y, 3));
    for (const RoomId room : construction.rooms().roomIds())
    {
        OLAM_CHECK(construction.rooms().room(room)->roofSupported);
        OLAM_CHECK(!construction.rooms().room(room)->roofed());
        OLAM_CHECK(construction.rooms().room(room)->roof == (RoofSettings{RoofMode::Disabled, StructureMaterial::Timber}));
    }
    // Back to automatic: supported rooms are roofed.
    OLAM_CHECK(construction.setRoof(id, {RoofMode::Auto, StructureMaterial::Timber}));
    OLAM_CHECK(construction.rooms().room(id)->roofed());
}

OLAM_TEST(room_detection_is_local_and_matches_full_detection)
{
    const auto s = site(8, 3);
    if (!s)
        return;
    ResourceState resources;
    const LocalCoord at = s->at;
    const auto run = [&]()
    {
        ConstructionState construction;
        build(*s, resources, construction, outline(at, 3, 3));
        build(*s, resources, construction, outline({at.x + 5, at.y}, 3, 3));
        build(*s, resources, construction, {{at.x + 1, at.y + 3, EdgeAxis::North}}, StructureKind::Door);
        return construction;
    };
    ConstructionState construction = run();
    const RoomId far = construction.rooms().roomAt({at.x + 5, at.y});
    const Room before = *construction.rooms().room(far);
    // Edits to one hut leave the other untouched.
    build(*s, resources, construction, partition(at.x + 1, at.y, 3));
    removeEdge(construction, {at.x + 1, at.y + 1, EdgeAxis::West});
    OLAM_CHECK(construction.rooms().roomAt({at.x + 5, at.y}) == far);
    OLAM_CHECK(construction.rooms().room(far)->tiles == before.tiles && construction.rooms().room(far)->building == before.building);
    // The first hut is one room again, with wall stubs that are not part of its outline.
    OLAM_CHECK(construction.rooms().roomCount() == 2);
    OLAM_CHECK(construction.rooms().room(construction.rooms().roomAt(at))->walls == 11);

    // Loading detects rooms from scratch and matches them to the saved ones: the same rooms, tile for tile.
    ConstructionState fresh;
    OLAM_CHECK(!fresh.deserialize(*s->map, construction.serialize(*s->map)).has_value());
    OLAM_CHECK(fresh.rooms().roomCount() == construction.rooms().roomCount());
    OLAM_CHECK(fresh.rooms().buildingCount() == construction.rooms().buildingCount());
    for (const RoomId id : construction.rooms().roomIds())
    {
        const Room &room = *construction.rooms().room(id);
        const Room *found = fresh.rooms().room(fresh.rooms().roomAt(room.tiles.front()));
        OLAM_CHECK(found && found->tiles == room.tiles && found->walls == room.walls && found->doors == room.doors);
    }

    // Identical commands give identical rooms.
    OLAM_CHECK(run().hash() == run().hash());
}

OLAM_TEST(rooms_round_trip_and_reject_mismatched_blobs)
{
    const auto s = site(5, 3);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    const LocalCoord at = s->at;
    build(*s, resources, construction, outline(at, 5, 3));
    build(*s, resources, construction, partition(at.x + 2, at.y, 3));
    // Split and merge so ids are no longer what a fresh detection would assign.
    build(*s, resources, construction, partition(at.x + 4, at.y, 3));
    removeEdge(construction, {at.x + 4, at.y, EdgeAxis::West});
    build(*s, resources, construction, {{at.x + 2, at.y + 1, EdgeAxis::West}}, StructureKind::Door);
    const RoomId east = construction.rooms().roomAt({at.x + 3, at.y});
    OLAM_CHECK(construction.setRoof(east, {RoofMode::Disabled, StructureMaterial::Timber}));

    const auto bytes = construction.serialize(*s->map);
    ConstructionState loaded;
    OLAM_CHECK(!loaded.deserialize(*s->map, bytes).has_value());
    OLAM_CHECK(loaded.hash() == construction.hash() && loaded.serialize(*s->map) == bytes);
    OLAM_CHECK(loaded.rooms().roomAt({at.x + 3, at.y}) == east);
    OLAM_CHECK(loaded.rooms().room(east)->roof == (RoofSettings{RoofMode::Disabled, StructureMaterial::Timber}));
    OLAM_CHECK(loaded.rooms().room(east)->building == construction.rooms().room(east)->building);

    // A room anchored off its first tile, or an unknown roof mode, is rejected and leaves the state alone. The
    // last room entry (23 bytes: id, building, anchor x and y, mode, material, function, built roof) comes just
    // before the empty stockpile list (three 4-byte counters).
    const std::size_t last = bytes.size() - 12 - 23;
    auto moved = bytes;
    moved[last + 8] += 1;
    OLAM_CHECK(loaded.deserialize(*s->map, moved).has_value());
    auto badMode = bytes;
    badMode[last + 16] = 7;
    OLAM_CHECK(loaded.deserialize(*s->map, badMode).has_value());
    auto truncated = bytes;
    truncated.resize(truncated.size() - 1);
    OLAM_CHECK(loaded.deserialize(*s->map, truncated).has_value());
    OLAM_CHECK(loaded.hash() == construction.hash());
}
