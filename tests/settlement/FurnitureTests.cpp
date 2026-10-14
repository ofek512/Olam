#include "TestFramework.h"
#include "settlement/ConstructionTestUtil.h"

#include <algorithm>
#include <vector>

using namespace olam;

namespace
{

    struct Site
    {
        const SettlementMap *map = nullptr;
        LocalCoord at;

        // A cell `dx`, `dy` cells from the area's first cell.
        LocalCoord cell(int dx, int dy) const { return {at.x * kCellsPerTile + dx, at.y * kCellsPerTile + dy}; }
    };

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

    // Completed walls around a w x h block.
    void walls(const Site &s, ResourceState &resources, ConstructionState &construction, LocalCoord at, int w, int h)
    {
        for (const LocalEdge edge : test::outlineEdges(at, w, h))
            OLAM_CHECK(construction.placeEdge(*s.map, resources, StructureKind::Wall, StructureMaterial::Timber, edge).error ==
                       PlacementError::None);
    }

    // Completed furniture of its default material.
    StructureId furnish(const Site &s, ResourceState &resources, ConstructionState &construction, FurnitureKind kind, LocalCoord cell,
                        Facing facing = Facing::North)
    {
        const PlacementResult placed = construction.placeFurniture(*s.map, resources, kind, furnitureMaterials(kind).front(), cell,
                                                                   {.facing = facing});
        OLAM_CHECK(placed.error == PlacementError::None);
        return placed.id;
    }

} // namespace

OLAM_TEST(furniture_catalogue_is_consistent)
{
    const ConstructionCosts costs = defaultConstructionCosts();
    for (const FurnitureInfo &info : furnitureCatalogue())
    {
        OLAM_CHECK(furnitureInfo(info.kind).name == info.name);
        OLAM_CHECK(info.width > 0 && info.depth > 0 && info.workRequired > 0 && info.maxIntegrity > 0);
        OLAM_CHECK(!furnitureMaterials(info.kind).empty());
        for (const StructureMaterial material : furnitureMaterials(info.kind))
            OLAM_CHECK(!costs.furnitureCost(info.kind, material).empty());
        // Every piece serves some room.
        OLAM_CHECK(!functionsUsing(info.kind).empty());
    }
    // Turning swaps the footprint.
    const CellSize north = footprint(furnitureInfo(FurnitureKind::Workbench), Facing::North);
    const CellSize east = footprint(furnitureInfo(FurnitureKind::Workbench), Facing::East);
    OLAM_CHECK(north.w == east.h && north.h == east.w);
    OLAM_CHECK(cellTile({-1, 7}) == (LocalCoord{-1, 1}) && cellTile({4, 3}) == (LocalCoord{1, 0}));
    const auto bed = functionsUsing(FurnitureKind::Bed);
    OLAM_CHECK(std::find(bed.begin(), bed.end(), RoomFunction::Bedroom) != bed.end());
    for (const RoomFunctionInfo &info : roomFunctionCatalogue())
        OLAM_CHECK(roomFunctionInfo(info.function).name == info.name && !info.buildingName.empty());
}

OLAM_TEST(furniture_footprints_collide_and_respect_walls)
{
    const auto s = site(4, 3);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    const SettlementMap &map = *s->map;

    // A bed (1 x 2 m, 2 x 4 cells) in the corner, another right beside it.
    const StructureId first = furnish(*s, resources, construction, FurnitureKind::Bed, s->cell(0, 0));
    OLAM_CHECK(construction.furnitureAt(s->cell(1, 3)) == first && !construction.furnitureAt(s->cell(2, 0)).isValid());
    furnish(*s, resources, construction, FurnitureKind::Bed, s->cell(2, 0));
    OLAM_CHECK(construction.furnitureOnTile(s->at).size() == 2);
    // Overlaps are rejected, at any size.
    OLAM_CHECK(construction.checkFurniture(map, resources, FurnitureKind::Bed, StructureMaterial::Timber, s->cell(0, 0), Facing::North) ==
               PlacementError::Occupied);
    OLAM_CHECK(construction.checkFurniture(map, resources, FurnitureKind::Chair, StructureMaterial::Timber, s->cell(1, 1), Facing::North) ==
               PlacementError::Occupied);
    // A bed turned east is 4 x 2 cells and fits under them.
    furnish(*s, resources, construction, FurnitureKind::Bed, s->cell(0, 4), Facing::East);
    // Wrong materials and the map edge.
    OLAM_CHECK(construction.checkFurniture(map, resources, FurnitureKind::Bed, StructureMaterial::RoughStone, s->cell(0, 8),
                                           Facing::North) == PlacementError::InvalidMaterial);
    OLAM_CHECK(construction.checkFurniture(map, resources, FurnitureKind::Chair, StructureMaterial::Timber, {-1, -1}, Facing::North) ==
               PlacementError::OutOfBounds);

    // A workbench may span two tiles, but no wall can then go between them.
    const StructureId bench = furnish(*s, resources, construction, FurnitureKind::Workbench, s->cell(2, 8));
    OLAM_CHECK(construction.furnitureOnTile({s->at.x + 1, s->at.y + 2}) == std::vector<StructureId>{bench});
    OLAM_CHECK(construction.checkEdge(map, resources, StructureKind::Wall, StructureMaterial::Timber, {s->at.x + 1, s->at.y + 2, EdgeAxis::West}) ==
               PlacementError::Occupied);
    // ... and furniture never straddles a wall (of any stage).
    OLAM_CHECK(construction.placeEdge(map, resources, StructureKind::Wall, StructureMaterial::Timber, {s->at.x + 3, s->at.y, EdgeAxis::West},
                                      {.stage = ConstructionStage::Planned, .order = construction.createOrder()})
                   .error == PlacementError::None);
    OLAM_CHECK(construction.checkFurniture(map, resources, FurnitureKind::Workbench, StructureMaterial::Timber, s->cell(10, 0),
                                           Facing::North) == PlacementError::Occupied);
    OLAM_CHECK(construction.checkFurniture(map, resources, FurnitureKind::Workbench, StructureMaterial::Timber, s->cell(12, 0),
                                           Facing::North) == PlacementError::None);

    // Removal frees the cells.
    OLAM_CHECK(construction.remove(first));
    OLAM_CHECK(!construction.furnitureAt(s->cell(0, 0)).isValid() && construction.furnitureOnTile(s->at).size() == 1);
    OLAM_CHECK(construction.checkFurniture(map, resources, FurnitureKind::Chair, StructureMaterial::Timber, s->cell(1, 1), Facing::North) ==
               PlacementError::None);
    // A rectangle removal takes furniture too.
    const std::size_t before = construction.structureCount();
    OLAM_CHECK(construction.removeInRect({s->at.x + 1, s->at.y + 2}, {s->at.x + 1, s->at.y + 2}) == 1);
    OLAM_CHECK(construction.structureCount() == before - 1 && !construction.structure(bench));
}

OLAM_TEST(room_functions_need_their_furniture)
{
    const auto s = site(3, 3);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    walls(*s, resources, construction, s->at, 3, 3);
    const RoomId room = construction.rooms().roomAt(s->at);

    RoomStatus status = evaluateRoom(construction, room);
    OLAM_CHECK(status.function == RoomFunction::Unassigned && !status.functional && status.suggested == RoomFunction::Unassigned);
    OLAM_CHECK(status.tiles == 9 && status.condition == 50);
    // The designation is the player's.
    OLAM_CHECK(construction.setRoomFunction(room, RoomFunction::Bedroom) && !construction.setRoomFunction(room, RoomFunction::Bedroom));
    status = evaluateRoom(construction, room);
    OLAM_CHECK(!status.functional && status.missing == std::vector<std::string_view>{"a bed or double bed"});

    // A planned bed is not a bed yet.
    const OrderId order = construction.createOrder();
    OLAM_CHECK(construction.placeFurniture(*s->map, resources, FurnitureKind::Bed, StructureMaterial::Timber, s->cell(0, 0),
                                           {.stage = ConstructionStage::Planned, .order = order})
                   .error == PlacementError::None);
    status = evaluateRoom(construction, room);
    OLAM_CHECK(!status.functional && status.unfinishedFurniture == 1 && status.furniture == 0);
    construction.cancelOrder(order);

    // A built one makes it work; it also suggests the function, but never changes it.
    const StructureId bed = furnish(*s, resources, construction, FurnitureKind::Bed, s->cell(0, 0));
    status = evaluateRoom(construction, room);
    OLAM_CHECK(status.functional && status.missing.empty() && status.sleepers == 1 && status.serviceCapacity == 1);
    OLAM_CHECK(status.service == RoomService::Housing && status.suggested == RoomFunction::Bedroom);
    construction.setRoomFunction(room, RoomFunction::Workshop);
    status = evaluateRoom(construction, room);
    OLAM_CHECK(status.function == RoomFunction::Workshop && !status.functional && status.suggested == RoomFunction::Bedroom);
    construction.setRoomFunction(room, RoomFunction::Bedroom);

    // Renovation: a partition splits the room; both parts keep the designation, only the one with the bed works.
    for (int y = s->at.y; y < s->at.y + 3; ++y)
        OLAM_CHECK(construction.placeEdge(*s->map, resources, StructureKind::Wall, StructureMaterial::Timber, {s->at.x + 2, y, EdgeAxis::West})
                       .error == PlacementError::None);
    OLAM_CHECK(construction.rooms().roomAt(s->at) == room);
    const RoomId side = construction.rooms().roomAt({s->at.x + 2, s->at.y});
    OLAM_CHECK(evaluateRoom(construction, room).functional);
    const RoomStatus sideStatus = evaluateRoom(construction, side);
    OLAM_CHECK(sideStatus.function == RoomFunction::Bedroom && !sideStatus.functional && sideStatus.missing.size() == 1);
    // Taking the bed out breaks it again.
    construction.remove(bed);
    OLAM_CHECK(!evaluateRoom(construction, room).functional);
    // Unknown rooms evaluate to nothing.
    OLAM_CHECK(!evaluateRoom(construction, RoomId{999}).functional);
}

OLAM_TEST(dormitory_and_private_home_differ_in_capacity_and_comfort)
{
    const auto s = site(6, 3);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    test::addStockpile(*s->map, resources, construction);
    // A 3 x 3 dormitory with four bedrolls, and a 2 x 2 bedroom with a bed and a chest.
    walls(*s, resources, construction, s->at, 3, 3);
    const LocalCoord home{s->at.x + 4, s->at.y};
    walls(*s, resources, construction, home, 2, 2);
    const RoomId dormitory = construction.rooms().roomAt(s->at);
    const RoomId bedroom = construction.rooms().roomAt(home);
    for (int i = 0; i < 4; ++i)
        furnish(*s, resources, construction, FurnitureKind::Bedroll, s->cell(i * 3, 0));
    const Site homeSite{s->map, home};
    furnish(homeSite, resources, construction, FurnitureKind::Bed, homeSite.cell(0, 0));
    furnish(homeSite, resources, construction, FurnitureKind::StorageChest, homeSite.cell(4, 0));
    construction.setRoomFunction(dormitory, RoomFunction::Dormitory);
    construction.setRoomFunction(bedroom, RoomFunction::Bedroom);
    OLAM_CHECK(construction.buildRoof(dormitory) && construction.buildRoof(bedroom));

    const RoomStatus communal = evaluateRoom(construction, dormitory);
    const RoomStatus home1 = evaluateRoom(construction, bedroom);
    OLAM_CHECK(communal.functional && communal.sleepers == 4 && communal.serviceCapacity == 4);
    OLAM_CHECK(home1.functional && home1.sleepers == 1);
    // Bedrolls count in a dormitory but never make a bedroom.
    OLAM_CHECK(communal.suggested == RoomFunction::Dormitory);
    // Private housing: more privacy and comfort per sleeper; communal: more sleepers for its size.
    OLAM_CHECK(home1.privacy > communal.privacy && home1.comfort > communal.comfort);
    OLAM_CHECK(communal.sleepers * home1.tiles > home1.sleepers * communal.tiles);
    OLAM_CHECK(home1.roofComplete && communal.roofComplete && home1.furnishing == 4);

    // Buildings are named by their rooms.
    const BuildingSummary house = summarizeBuilding(construction, construction.rooms().room(bedroom)->building);
    const BuildingSummary hall = summarizeBuilding(construction, construction.rooms().room(dormitory)->building);
    OLAM_CHECK(house.name == "House" && house.privateSleepers == 1 && house.communalSleepers == 0);
    OLAM_CHECK(hall.name == "Dormitory" && hall.communalSleepers == 4);

    // A bedroom sleeps at most two: a double bed on top of the bed is crowding.
    furnish(homeSite, resources, construction, FurnitureKind::DoubleBed, homeSite.cell(4, 4));
    const RoomStatus crowded = evaluateRoom(construction, bedroom);
    OLAM_CHECK(crowded.sleepers == 2 && crowded.crowding == 1 && crowded.privacy == 80);
}

OLAM_TEST(room_functions_report_their_services)
{
    const auto s = site(3, 3);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    walls(*s, resources, construction, s->at, 3, 3);
    const RoomId room = construction.rooms().roomAt(s->at);
    furnish(*s, resources, construction, FurnitureKind::Table, s->cell(0, 0));
    furnish(*s, resources, construction, FurnitureKind::Chair, s->cell(0, 2));
    furnish(*s, resources, construction, FurnitureKind::Chair, s->cell(1, 2));
    furnish(*s, resources, construction, FurnitureKind::Altar, s->cell(4, 0));
    furnish(*s, resources, construction, FurnitureKind::StorageChest, s->cell(8, 0));
    furnish(*s, resources, construction, FurnitureKind::Shelf, s->cell(8, 2));
    furnish(*s, resources, construction, FurnitureKind::CookingHearth, s->cell(0, 6));
    furnish(*s, resources, construction, FurnitureKind::Workbench, s->cell(4, 6));

    const auto service = [&](RoomFunction function)
    {
        construction.setRoomFunction(room, function);
        return evaluateRoom(construction, room);
    };
    OLAM_CHECK(service(RoomFunction::DiningHall).serviceCapacity == 2);
    OLAM_CHECK(service(RoomFunction::Shrine).serviceCapacity == 9 * 2);
    OLAM_CHECK(service(RoomFunction::Storage).serviceCapacity == 150 + 300);
    OLAM_CHECK(service(RoomFunction::Kitchen).serviceCapacity == 1);
    OLAM_CHECK(service(RoomFunction::Workshop).serviceCapacity == 1);
    const RoomStatus infirmary = service(RoomFunction::Infirmary);
    OLAM_CHECK(!infirmary.functional && infirmary.serviceCapacity == 0 && infirmary.missing.front() == "a bed");
    OLAM_CHECK(infirmary.suggested == RoomFunction::Storage);
    // Seats without a table are no dining hall.
    OLAM_CHECK(construction.remove(construction.furnitureAt(s->cell(0, 0))));
    const RoomStatus noTable = service(RoomFunction::DiningHall);
    OLAM_CHECK(!noTable.functional && noTable.missing == std::vector<std::string_view>{"a table"});
}

OLAM_TEST(furniture_and_room_functions_round_trip)
{
    const auto s = site(3, 3);
    if (!s)
        return;
    const auto run = [&]()
    {
        ResourceState resources;
        ConstructionState construction;
        walls(*s, resources, construction, s->at, 3, 3);
        furnish(*s, resources, construction, FurnitureKind::Bed, s->cell(0, 0), Facing::South);
        furnish(*s, resources, construction, FurnitureKind::Workbench, s->cell(4, 6), Facing::East);
        construction.setRoomFunction(construction.rooms().roomAt(s->at), RoomFunction::Bedroom);
        return construction;
    };
    const ConstructionState first = run();
    OLAM_CHECK(first.hash() == run().hash());
    const auto bytes = first.serialize(*s->map);
    ConstructionState loaded;
    OLAM_CHECK(!loaded.deserialize(*s->map, bytes).has_value());
    OLAM_CHECK(loaded.hash() == first.hash() && loaded.serialize(*s->map) == bytes);
    const Structure *bench = loaded.structure(loaded.furnitureAt(s->cell(4, 6)));
    OLAM_CHECK(bench && bench->furniture == FurnitureKind::Workbench && bench->facing == Facing::East);
    OLAM_CHECK(loaded.rooms().room(loaded.rooms().roomAt(s->at))->function == RoomFunction::Bedroom);
    OLAM_CHECK(evaluateRoom(loaded, loaded.rooms().roomAt(s->at)).functional);
}
