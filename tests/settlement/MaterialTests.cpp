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

    // Plans timber walls on `edges` into a new order and confirms it.
    OrderId planWalls(const Site &s, ResourceState &resources, ConstructionState &construction, const std::vector<LocalEdge> &edges,
                      bool confirm = true)
    {
        const OrderId order = construction.createOrder();
        for (const LocalEdge edge : edges)
            OLAM_CHECK(construction.placeEdge(*s.map, resources, StructureKind::Wall, StructureMaterial::Timber, edge,
                                              {.stage = ConstructionStage::Planned, .order = order})
                           .error == PlacementError::None);
        if (confirm)
            construction.confirmOrder(order);
        return order;
    }

    GoodAmounts wood(std::uint32_t kg) { return {{Good::Wood, kg}}; }

    std::size_t countStage(const ConstructionState &construction, OrderId order, ConstructionStage stage)
    {
        std::size_t count = 0;
        for (const StructureId id : construction.order(order)->components)
            count += construction.structure(id)->stage == stage ? 1 : 0;
        return count;
    }

    constexpr std::uint32_t kTimberWall = 120;

} // namespace

OLAM_TEST(construction_costs_follow_the_cost_table)
{
    const auto s = site(3, 2);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    OLAM_CHECK(construction.costs().structure(StructureKind::Wall, StructureMaterial::Timber) == wood(kTimberWall));
    const OrderId order = planWalls(*s, resources, construction, test::outlineEdges(s->at, 3, 2), false);
    OLAM_CHECK(construction.orderCost(order) == wood(10 * kTimberWall));
    // A planned door replacing a planned wall, and plank floors.
    construction.placeEdge(*s->map, resources, StructureKind::Door, StructureMaterial::Timber, {s->at.x + 1, s->at.y, EdgeAxis::North},
                           {.stage = ConstructionStage::Planned, .order = order});
    for (int x = s->at.x; x < s->at.x + 3; ++x)
        construction.placeFloor(*s->map, resources, StructureMaterial::WoodPlank, {x, s->at.y}, {.stage = ConstructionStage::Planned, .order = order});
    OLAM_CHECK(construction.orderCost(order) == wood(9 * kTimberWall + 40 + 3 * 50));
    // Costs are configurable data.
    ConstructionCosts cheap = defaultConstructionCosts();
    cheap.structures[static_cast<std::size_t>(StructureKind::Wall)][static_cast<std::size_t>(StructureMaterial::Timber)] = wood(1);
    const ConstructionState custom({}, cheap);
    OLAM_CHECK(custom.costs().structure(StructureKind::Wall, StructureMaterial::Timber) == wood(1));
    // Packed earth and developer building cost nothing.
    OLAM_CHECK(construction.costs().structure(StructureKind::Floor, StructureMaterial::PackedEarth).empty());
}

OLAM_TEST(reserved_goods_are_not_available_to_other_orders)
{
    const auto s = site(8, 2);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    const StockpileId stock = test::addStockpile(*s->map, resources, construction, wood(1000));
    OLAM_CHECK(stock.isValid());
    const OrderId hut = planWalls(*s, resources, construction, test::outlineEdges(s->at, 3, 2));
    const OrderId fence = planWalls(*s, resources, construction, {{s->at.x + 5, s->at.y, EdgeAxis::North}});
    // Planned (unconfirmed) parts reserve nothing.
    const OrderId sketch = planWalls(*s, resources, construction, {{s->at.x + 6, s->at.y, EdgeAxis::North}}, false);
    OLAM_CHECK(construction.allocateMaterials(sketch).empty() && construction.stockpiles().reservedFor(sketch).empty());

    OLAM_CHECK(construction.allocateMaterials(hut) == wood(200));
    OLAM_CHECK(construction.stockpiles().reservedFor(hut) == wood(1000) && construction.stockpiles().available(stock).empty());
    OLAM_CHECK(construction.orderShortfall(hut) == wood(200));
    // The second order finds nothing: the hut holds it all.
    OLAM_CHECK(construction.allocateMaterials(fence) == wood(kTimberWall));
    // Allocating again changes nothing.
    OLAM_CHECK(construction.allocateMaterials(hut) == wood(200) && construction.stockpiles().reservedFor(hut) == wood(1000));

    construction.debugAddGoods(stock, wood(500));
    OLAM_CHECK(construction.allocateMaterials(hut).empty() && construction.allocateMaterials(fence).empty());
    OLAM_CHECK(construction.stockpiles().reservedFor(hut) == wood(1200) && construction.stockpiles().reservedFor(fence) == wood(kTimberWall));
    OLAM_CHECK(construction.stockpiles().available(stock) == wood(1500 - 1200 - kTimberWall));
    OLAM_CHECK(construction.stockpiles().stockpile(stock)->stored == wood(1500));
}

OLAM_TEST(deliveries_happen_once_and_never_overdraw)
{
    const auto s = site(3, 2);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    const StockpileId stock = test::addStockpile(*s->map, resources, construction, wood(1000));
    const OrderId order = planWalls(*s, resources, construction, test::outlineEdges(s->at, 3, 2));

    // 1000 kg is enough for eight 120 kg walls; the rest waits, the remainder stays on site.
    OLAM_CHECK(construction.debugDeliver(order));
    OLAM_CHECK(construction.stockpiles().stockpile(stock)->stored.empty());
    OLAM_CHECK(countStage(construction, order, ConstructionStage::ReadyToBuild) == 8);
    OLAM_CHECK(countStage(construction, order, ConstructionStage::AwaitingMaterials) == 2);
    OLAM_CHECK(construction.order(order)->onSite == wood(40) && construction.order(order)->source == stock);
    OLAM_CHECK(construction.orderShortfall(order) == wood(200));
    // Nothing more to deliver: no duplicate delivery, no negative stock.
    OLAM_CHECK(!construction.debugDeliver(order));
    OLAM_CHECK(construction.stockpiles().stockpile(stock)->stored.empty() && construction.order(order)->onSite == wood(40));

    construction.debugAddGoods(stock, wood(300));
    OLAM_CHECK(construction.debugDeliver(order));
    OLAM_CHECK(countStage(construction, order, ConstructionStage::ReadyToBuild) == 10);
    OLAM_CHECK(construction.stockpiles().stockpile(stock)->stored == wood(100) && construction.order(order)->onSite.empty());

    // Building consumes nothing more; the completed order closes.
    construction.debugCompleteOrder(*s->map, resources, order);
    OLAM_CHECK(!construction.order(order) && construction.stockpiles().stockpile(stock)->stored == wood(100));
    OLAM_CHECK(construction.stockpiles().reservationCount() == 0);
}

OLAM_TEST(partial_supply_builds_what_it_can)
{
    const auto s = site(3, 2);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    test::addStockpile(*s->map, resources, construction, wood(3 * kTimberWall));
    const OrderId order = planWalls(*s, resources, construction, test::outlineEdges(s->at, 3, 2), false);
    // Without stock at all, the developer shortcut builds nothing.
    ConstructionState empty;
    const OrderId lonely = planWalls(*s, resources, empty, test::outlineEdges(s->at, 3, 2), false);
    empty.debugCompleteOrder(*s->map, resources, lonely);
    OLAM_CHECK(empty.orderStage(lonely) == ConstructionStage::AwaitingMaterials && empty.rooms().roomCount() == 0);

    construction.debugCompleteOrder(*s->map, resources, order);
    OLAM_CHECK(countStage(construction, order, ConstructionStage::Completed) == 3);
    OLAM_CHECK(countStage(construction, order, ConstructionStage::AwaitingMaterials) == 7);
    OLAM_CHECK(construction.orderStage(order) == ConstructionStage::AwaitingMaterials);
}

OLAM_TEST(cancelling_and_removing_return_materials)
{
    const auto s = site(3, 2);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    const StockpileId stock = test::addStockpile(*s->map, resources, construction, wood(2000));
    const auto stored = [&]() { return construction.stockpiles().stockpile(stock)->stored; };

    // Reserved only: cancelling releases the reservation.
    OrderId order = planWalls(*s, resources, construction, test::outlineEdges(s->at, 3, 2));
    construction.allocateMaterials(order);
    OLAM_CHECK(construction.stockpiles().available(stock) == wood(800));
    // Removing a part awaiting materials trims the reservation.
    construction.remove(construction.order(order)->components.back());
    OLAM_CHECK(construction.stockpiles().reservedFor(order) == wood(9 * kTimberWall));
    construction.cancelOrder(order);
    OLAM_CHECK(construction.stockpiles().available(stock) == wood(2000) && construction.stockpiles().reservationCount() == 0);

    // Delivered: cancelling brings the goods back, including those already assigned to walls.
    order = planWalls(*s, resources, construction, test::outlineEdges(s->at, 3, 2));
    construction.debugDeliver(order);
    OLAM_CHECK(stored() == wood(800));
    const StructureId wall = construction.order(order)->components.front();
    construction.remove(wall);
    OLAM_CHECK(construction.order(order)->onSite == wood(kTimberWall));
    construction.cancelOrder(order);
    OLAM_CHECK(stored() == wood(2000) && construction.structureCount() == 0);

    // Leftovers of a completed order go back too.
    order = planWalls(*s, resources, construction, test::outlineEdges(s->at, 3, 2));
    construction.debugDeliver(order);
    construction.remove(construction.order(order)->components.front());
    construction.debugCompleteOrder(*s->map, resources, order);
    OLAM_CHECK(!construction.order(order));
    OLAM_CHECK(stored() == wood(2000 - 9 * kTimberWall));
}

OLAM_TEST(roofs_are_paid_for_from_stockpiles)
{
    const auto s = site(3, 4);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    const StockpileId stock = test::addStockpile(*s->map, resources, construction, {});
    for (const LocalEdge edge : test::outlineEdges(s->at, 3, 3))
        construction.placeEdge(*s->map, resources, StructureKind::Wall, StructureMaterial::Timber, edge);
    const RoomId room = construction.rooms().roomAt(s->at);
    const GoodAmounts perTile = construction.costs().roof(StructureMaterial::Thatch);
    OLAM_CHECK(perTile == (GoodAmounts{{Good::Thatch, 40}, {Good::Wood, 20}}));
    // Enclosed and supported, so a roof is wanted, but not built yet.
    OLAM_CHECK(construction.rooms().room(room)->roofed() && !construction.rooms().room(room)->roofComplete());
    OLAM_CHECK(construction.roofCost(room) == perTile * 9);
    OLAM_CHECK(!construction.buildRoof(room));

    // Reserved goods are not taken for roofs.
    construction.debugAddGoods(stock, perTile * 9);
    const OrderId order = planWalls(*s, resources, construction, {{s->at.x, s->at.y + 4, EdgeAxis::North}});
    construction.debugAddGoods(stock, wood(kTimberWall - 20));
    construction.allocateMaterials(order);
    OLAM_CHECK(!construction.buildRoof(room));
    construction.cancelOrder(order);
    OLAM_CHECK(construction.buildRoof(room));
    OLAM_CHECK(construction.rooms().room(room)->roofComplete() && construction.roofCost(room).empty());
    OLAM_CHECK(construction.stockpiles().stockpile(stock)->stored == wood(kTimberWall - 20));
    OLAM_CHECK(!construction.buildRoof(room));

    // An extension: walled in first, then the old wall comes down; only the new tiles need roofing.
    for (int x = s->at.x; x < s->at.x + 3; ++x)
        construction.placeEdge(*s->map, resources, StructureKind::Wall, StructureMaterial::Timber, {x, s->at.y + 4, EdgeAxis::North});
    construction.placeEdge(*s->map, resources, StructureKind::Wall, StructureMaterial::Timber, {s->at.x, s->at.y + 3, EdgeAxis::West});
    construction.placeEdge(*s->map, resources, StructureKind::Wall, StructureMaterial::Timber, {s->at.x + 3, s->at.y + 3, EdgeAxis::West});
    for (int x = s->at.x; x < s->at.x + 3; ++x)
        construction.remove(construction.edgeAt({x, s->at.y + 3, EdgeAxis::North}));
    OLAM_CHECK(construction.rooms().roomAt(s->at) == room && construction.rooms().room(room)->tiles.size() == 12);
    OLAM_CHECK(construction.roofCost(room) == perTile * 3);
    // A timber roof prices the missing tiles in timber.
    construction.setRoof(room, {RoofMode::Auto, StructureMaterial::Timber});
    OLAM_CHECK(construction.roofCost(room) == construction.costs().roof(StructureMaterial::Timber) * 3);
}

OLAM_TEST(stockpile_placement_is_validated)
{
    const auto s = site(6, 3);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    const SettlementMap &map = *s->map;
    const LocalCoord a = s->at;
    const LocalCoord b{s->at.x + 1, s->at.y + 1};
    const StockpileResult first = construction.createStockpile(map, resources, a, b);
    OLAM_CHECK(first.error == PlacementError::None && first.id.isValid());
    OLAM_CHECK(construction.stockpiles().stockpileAt(b) == first.id && !construction.stockpiles().stockpileAt({b.x + 1, b.y}).isValid());
    // No overlapping stockpiles, no furniture on them and no stockpile over furniture.
    OLAM_CHECK(construction.checkStockpile(map, resources, b, {b.x + 1, b.y}) == PlacementError::Occupied);
    OLAM_CHECK(construction.checkFurniture(map, resources, FurnitureKind::Chair, StructureMaterial::Timber, test::tileCell(a), Facing::North) ==
               PlacementError::Occupied);
    construction.placeFurniture(map, resources, FurnitureKind::Chair, StructureMaterial::Timber, test::tileCell({s->at.x + 4, s->at.y}));
    OLAM_CHECK(construction.checkStockpile(map, resources, {s->at.x + 3, s->at.y}, {s->at.x + 4, s->at.y}) == PlacementError::Occupied);
    // Not on water, not huge.
    const auto water = test::findTile(map, [&](LocalCoord t) { return map.terrain().water[map.index(t)] != LocalWater::None; });
    if (water)
        OLAM_CHECK(construction.checkStockpile(map, resources, *water, *water) == PlacementError::Water);
    OLAM_CHECK(construction.checkStockpile(map, resources, {10, 10}, {60, 12}) == PlacementError::OutOfBounds);

    // Removing a stockpile drops its goods and the reservations held there.
    construction.debugAddGoods(first.id, wood(500));
    const OrderId order = planWalls(*s, resources, construction, {{s->at.x + 5, s->at.y, EdgeAxis::North}});
    construction.allocateMaterials(order);
    OLAM_CHECK(construction.stockpiles().reservedFor(order) == wood(kTimberWall));
    OLAM_CHECK(construction.removeStockpile(first.id) && !construction.removeStockpile(first.id));
    OLAM_CHECK(construction.stockpiles().reservedFor(order).empty() && construction.stockpiles().count() == 0);
    OLAM_CHECK(construction.orderShortfall(order) == wood(kTimberWall));
}

OLAM_TEST(materials_round_trip_and_stay_deterministic)
{
    const auto s = site(3, 2);
    if (!s)
        return;
    const auto run = [&]()
    {
        ResourceState resources;
        ConstructionState construction;
        const StockpileId stock = test::addStockpile(*s->map, resources, construction, wood(700));
        const OrderId order = planWalls(*s, resources, construction, test::outlineEdges(s->at, 3, 2));
        construction.debugDeliver(order);
        construction.debugAddGoods(stock, wood(300));
        construction.allocateMaterials(order);
        return construction;
    };
    const ConstructionState first = run();
    OLAM_CHECK(first.hash() == run().hash());
    const auto bytes = first.serialize(*s->map);
    ConstructionState loaded;
    OLAM_CHECK(!loaded.deserialize(*s->map, bytes).has_value());
    OLAM_CHECK(loaded.hash() == first.hash() && loaded.serialize(*s->map) == bytes);
    OLAM_CHECK(loaded.stockpiles().totalStored() == first.stockpiles().totalStored());
    OLAM_CHECK(loaded.stockpiles().reservationCount() == 1);
    // A reservation larger than the stock is rejected: the last 20 bytes are its goods.
    auto greedy = bytes;
    greedy[greedy.size() - 20 + 2] = 0x7F;
    OLAM_CHECK(loaded.deserialize(*s->map, greedy).has_value());
    OLAM_CHECK(loaded.hash() == first.hash());
}

OLAM_TEST(wooden_house_can_be_planned_supplied_and_built)
{
    const auto s = site(4, 3);
    if (!s)
        return;
    ResourceState resources;
    ConstructionState construction;
    const StockpileId stock = test::addStockpile(*s->map, resources, construction,
                                                 {{Good::Wood, 3000}, {Good::Thatch, 600}, {Good::Stone, 500}, {Good::Clay, 300}});
    // Plan: timber walls, a door, plank floors and a bed, all in one blueprint.
    const OrderId order = planWalls(*s, resources, construction, test::outlineEdges(s->at, 4, 3), false);
    const PlacementOptions plan{.stage = ConstructionStage::Planned, .order = order};
    construction.placeEdge(*s->map, resources, StructureKind::Door, StructureMaterial::Timber, {s->at.x + 1, s->at.y + 3, EdgeAxis::North}, plan);
    for (int y = s->at.y; y < s->at.y + 3; ++y)
    {
        for (int x = s->at.x; x < s->at.x + 4; ++x)
            construction.placeFloor(*s->map, resources, StructureMaterial::WoodPlank, {x, y}, plan);
    }
    construction.placeFurniture(*s->map, resources, FurnitureKind::Bed, StructureMaterial::Timber, test::tileCell(s->at), plan);
    // Inspect its cost: 13 walls, a door, 12 plank floors and a bed.
    const GoodAmounts cost = construction.orderCost(order);
    OLAM_CHECK(cost == (GoodAmounts{{Good::Wood, 13 * kTimberWall + 40 + 12 * 50 + 40}, {Good::Thatch, 5}}));

    // Allocate supplies, then complete it in debug mode.
    construction.confirmOrder(order);
    OLAM_CHECK(construction.allocateMaterials(order).empty());
    OLAM_CHECK(construction.stockpiles().reservedFor(order) == cost);
    construction.debugCompleteOrder(*s->map, resources, order);
    OLAM_CHECK(!construction.order(order));
    OLAM_CHECK(construction.stockpiles().stockpile(stock)->stored[Good::Wood] == 3000 - cost[Good::Wood]);

    // The room appears, gets its roof from the stockpile and works as a bedroom.
    const RoomId room = construction.rooms().roomAt(s->at);
    OLAM_CHECK(construction.rooms().room(room) != nullptr);
    OLAM_CHECK(construction.buildRoof(room));
    construction.setRoomFunction(room, RoomFunction::Bedroom);
    const RoomStatus status = evaluateRoom(construction, room);
    OLAM_CHECK(status.functional && status.roofComplete && status.sleepers == 1);
}

OLAM_TEST(founding_stockpile_lands_on_open_ground_near_the_centre)
{
    const SettlementMap *map = test::constructionTestMap();
    if (!map)
        return;
    const auto run = [&]()
    {
        ResourceState resources;
        ConstructionState construction;
        const LocalCoord center{map->width() / 2, map->height() / 2};
        const StockpileResult result = createFoundingStockpile(*map, resources, construction, center, defaultFoundingGoods());
        return std::pair{result, construction.hash()};
    };
    const auto [result, hash] = run();
    OLAM_CHECK(result.id.isValid() && result.error == PlacementError::None);
    // Deterministic, and supplies enough for a first timber house.
    OLAM_CHECK(run().second == hash);
    OLAM_CHECK(defaultFoundingGoods()[Good::Wood] >= 20 * kTimberWall && defaultFoundingGoods()[Good::Thatch] > 0);
}
