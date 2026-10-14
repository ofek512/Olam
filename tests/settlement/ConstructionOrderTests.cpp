#include "TestFramework.h"
#include "settlement/ConstructionTestUtil.h"

#include "core/serialization/ByteWriter.h"

#include <vector>

using namespace olam;

namespace
{

    // Plans walls around a w x h block and floors inside it into `order`; returns the walls' ids.
    std::vector<StructureId> planHut(const SettlementMap &map, ResourceState &resources, ConstructionState &construction,
                                     OrderId order, LocalCoord at, int w, int h)
    {
        const PlacementOptions plan{.stage = ConstructionStage::Planned, .order = order};
        std::vector<StructureId> walls;
        const auto wall = [&](LocalEdge edge)
        { walls.push_back(construction.placeEdge(map, resources, StructureKind::Wall, StructureMaterial::Timber, edge, plan).id); };
        for (int x = at.x; x < at.x + w; ++x)
        {
            wall({x, at.y, EdgeAxis::North});
            wall({x, at.y + h, EdgeAxis::North});
        }
        for (int y = at.y; y < at.y + h; ++y)
        {
            wall({at.x, y, EdgeAxis::West});
            wall({at.x + w, y, EdgeAxis::West});
        }
        for (int y = at.y; y < at.y + h; ++y)
        {
            for (int x = at.x; x < at.x + w; ++x)
                construction.placeFloor(map, resources, StructureMaterial::PackedEarth, {x, y}, plan);
        }
        return walls;
    }

    std::size_t countStage(const ConstructionState &construction, OrderId order, ConstructionStage stage)
    {
        std::size_t count = 0;
        for (const StructureId id : construction.order(order)->components)
            count += construction.structure(id)->stage == stage ? 1 : 0;
        return count;
    }

} // namespace

OLAM_TEST(blueprints_are_ghosts_until_cancelled)
{
    const SettlementMap *map = test::constructionTestMap();
    OLAM_CHECK(map != nullptr);
    if (!map)
        return;
    const auto area = test::findClearArea(*map, 3, 2);
    OLAM_CHECK(area.has_value());
    if (!area)
        return;
    const std::uint64_t generatedHash = hashSettlementMap(*map);
    ResourceState resources;
    ConstructionState construction;
    test::addStockpile(*map, resources, construction);

    const OrderId order = construction.createOrder();
    OLAM_CHECK(construction.order(order) && construction.orderStage(order) == ConstructionStage::Planned);
    const auto walls = planHut(*map, resources, construction, order, *area, 3, 2);
    OLAM_CHECK(construction.order(order)->components.size() == 10 + 6);
    OLAM_CHECK(countStage(construction, order, ConstructionStage::Planned) == 16);
    OLAM_CHECK(construction.structure(walls.front())->order == order);
    OLAM_CHECK(construction.orderAt(*area) == order);
    // Plans hold their place: nothing else can be built there.
    const LocalEdge edge{area->x, area->y, EdgeAxis::North};
    OLAM_CHECK(construction.placeEdge(*map, resources, StructureKind::Wall, StructureMaterial::Timber, edge).error ==
               PlacementError::Occupied);
    // But they change nothing in the natural map.
    OLAM_CHECK(resources.revision() == 0 && hashSettlementMap(*map) == generatedHash);

    OLAM_CHECK(construction.cancelOrder(order) == 16);
    OLAM_CHECK(!construction.order(order) && construction.structureCount() == 0 && construction.orderCount() == 0);
    OLAM_CHECK(!construction.edgeAt(edge).isValid());
    OLAM_CHECK(construction.cancelOrder(order) == 0);
    OLAM_CHECK(construction.orderStage(order) == ConstructionStage::Count);
}

OLAM_TEST(construction_stages_advance_once_in_order)
{
    const SettlementMap *map = test::constructionTestMap();
    if (!map)
        return;
    const auto area = test::findClearArea(*map, 2, 2);
    OLAM_CHECK(area.has_value());
    if (!area)
        return;
    ResourceState resources;
    ConstructionState construction;
    test::addStockpile(*map, resources, construction);
    const OrderId order = construction.createOrder();
    const auto walls = planHut(*map, resources, construction, order, *area, 2, 2);
    const StructureId first = walls.front();
    const std::uint16_t required = structureProperties(StructureKind::Wall, StructureMaterial::Timber).workRequired;

    // Out-of-order steps do nothing.
    OLAM_CHECK(!construction.debugDeliver(order));
    OLAM_CHECK(!construction.beginConstruction(*map, resources, first).has_value());
    OLAM_CHECK(!construction.addWork(first, 1));
    OLAM_CHECK(construction.structure(first)->stage == ConstructionStage::Planned);

    // Each step happens once: repeating it is a no-op (no double delivery).
    OLAM_CHECK(construction.confirmOrder(order) && !construction.confirmOrder(order));
    OLAM_CHECK(construction.orderStage(order) == ConstructionStage::AwaitingMaterials);
    OLAM_CHECK(construction.debugDeliver(order) && !construction.debugDeliver(order));
    OLAM_CHECK(construction.orderStage(order) == ConstructionStage::ReadyToBuild);
    OLAM_CHECK(construction.beginConstruction(*map, resources, first).has_value());
    OLAM_CHECK(!construction.beginConstruction(*map, resources, first).has_value());

    // Component-level progress; the order's stage is its least advanced part.
    OLAM_CHECK(construction.addWork(first, static_cast<std::uint16_t>(required - 1)));
    OLAM_CHECK(construction.structure(first)->stage == ConstructionStage::UnderConstruction);
    OLAM_CHECK(construction.structure(first)->workDone == required - 1);
    OLAM_CHECK(construction.addWork(first, 50));
    OLAM_CHECK(construction.structure(first)->stage == ConstructionStage::Completed);
    OLAM_CHECK(construction.structure(first)->workDone == required);
    OLAM_CHECK(!construction.addWork(first, 1));
    OLAM_CHECK(construction.orderStage(order) == ConstructionStage::ReadyToBuild);

    // Completing the rest closes the order; the structures stay, built.
    const std::vector<StructureId> rest = construction.order(order)->components;
    for (const StructureId id : rest)
    {
        construction.beginConstruction(*map, resources, id);
        construction.addWork(id, 1000);
    }
    OLAM_CHECK(!construction.order(order) && construction.orderCount() == 0);
    OLAM_CHECK(construction.structureCount() == rest.size());
    for (const StructureId id : rest)
        OLAM_CHECK(construction.structure(id)->stage == ConstructionStage::Completed && !construction.structure(id)->order.isValid());
    OLAM_CHECK(!construction.debugCompleteOrder(*map, resources, order).has_value());
}

OLAM_TEST(blueprints_over_obstructions_clear_them_when_building_starts)
{
    const SettlementMap *map = test::constructionTestMap();
    if (!map)
        return;
    ResourceState resources;
    ConstructionState construction;
    test::addStockpile(*map, resources, construction);
    const LocalTerrainData &terrain = map->terrain();
    const auto tree = test::findTile(*map, [&](LocalCoord t)
                                     { return construction.checkFloor(*map, resources, StructureMaterial::PackedEarth, t) == PlacementError::Tree; });
    OLAM_CHECK(tree.has_value());
    if (!tree)
        return;
    const TreeId treeId = terrain.treeId[map->index(*tree)];

    // Direct building is refused; a plan is accepted and leaves the tree standing.
    OLAM_CHECK(construction.placeFloor(*map, resources, StructureMaterial::WoodPlank, *tree).error == PlacementError::Tree);
    const OrderId order = construction.createOrder();
    const PlacementOptions plan{.stage = ConstructionStage::Planned, .order = order};
    const PlacementResult planned = construction.placeFloor(*map, resources, StructureMaterial::WoodPlank, *tree, plan);
    OLAM_CHECK(planned.error == PlacementError::None && planned.woodKg == 0.0f);
    OLAM_CHECK(!resources.treeFelled(*map, treeId) && resources.revision() == 0);
    // Plans never override hard errors.
    const auto water = test::findTile(*map, [&](LocalCoord t) { return terrain.water[map->index(t)] != LocalWater::None; });
    if (water)
        OLAM_CHECK(construction.placeFloor(*map, resources, StructureMaterial::WoodPlank, *water, plan).error == PlacementError::Water);

    // The tree is felled when construction begins, not before.
    OLAM_CHECK(construction.confirmOrder(order) && construction.debugDeliver(order));
    OLAM_CHECK(!resources.treeFelled(*map, treeId));
    const auto wood = construction.debugCompleteOrder(*map, resources, order);
    OLAM_CHECK(wood.has_value() && *wood > 0.0f);
    OLAM_CHECK(resources.treeFelled(*map, treeId));
    OLAM_CHECK(construction.structure(planned.id)->stage == ConstructionStage::Completed);
}

OLAM_TEST(unfinished_blueprints_can_be_edited_and_cancelled_partly)
{
    const SettlementMap *map = test::constructionTestMap();
    if (!map)
        return;
    const auto area = test::findClearArea(*map, 3, 3);
    OLAM_CHECK(area.has_value());
    if (!area)
        return;
    ResourceState resources;
    ConstructionState construction;
    test::addStockpile(*map, resources, construction);
    const OrderId order = construction.createOrder();
    const auto walls = planHut(*map, resources, construction, order, *area, 3, 3);
    const std::size_t parts = construction.order(order)->components.size();
    OLAM_CHECK(construction.confirmOrder(order));

    // Adding to a confirmed order plans the new part; the order is back to Planned until confirmed again.
    const PlacementOptions plan{.stage = ConstructionStage::Planned, .order = order};
    const LocalEdge doorEdge{area->x + 1, area->y, EdgeAxis::North};
    const PlacementResult door = construction.placeEdge(*map, resources, StructureKind::Door, StructureMaterial::Timber, doorEdge, plan);
    // The planned door replaced a planned wall of the same order.
    OLAM_CHECK(door.error == PlacementError::None && construction.order(order)->components.size() == parts);
    OLAM_CHECK(construction.orderStage(order) == ConstructionStage::Planned);
    OLAM_CHECK(construction.confirmOrder(order));
    // Removing a planned part takes it out of the order.
    OLAM_CHECK(construction.remove(walls.back()));
    OLAM_CHECK(construction.order(order)->components.size() == parts - 1);

    // Build one wall, then cancel: the built wall stays, everything else goes.
    OLAM_CHECK(construction.debugDeliver(order));
    const StructureId built = walls.front();
    construction.beginConstruction(*map, resources, built);
    construction.addWork(built, 1000);
    OLAM_CHECK(construction.cancelOrder(order) == parts - 2);
    OLAM_CHECK(construction.structureCount() == 1 && construction.structure(built)->stage == ConstructionStage::Completed);
    OLAM_CHECK(!construction.structure(built)->order.isValid());

    // A planned door may not replace finished work; an instant one may.
    const LocalEdge builtEdge = construction.structure(built)->edge();
    const OrderId next = construction.createOrder();
    OLAM_CHECK(construction.placeEdge(*map, resources, StructureKind::Door, StructureMaterial::Timber, builtEdge,
                                      {.stage = ConstructionStage::Planned, .order = next})
                   .error == PlacementError::Occupied);
    OLAM_CHECK(construction.placeEdge(*map, resources, StructureKind::Door, StructureMaterial::Timber, builtEdge).error ==
               PlacementError::None);
    // An order nothing could be planned into stays empty until cancelled.
    OLAM_CHECK(construction.order(next) && construction.order(next)->components.empty());
    construction.cancelOrder(next);
    OLAM_CHECK(construction.orderCount() == 0);
}

OLAM_TEST(construction_orders_are_deterministic_and_round_trip)
{
    const SettlementMap *map = test::constructionTestMap();
    if (!map)
        return;
    const auto area = test::findClearArea(*map, 4, 3);
    OLAM_CHECK(area.has_value());
    if (!area)
        return;
    const auto run = [&]()
    {
        ResourceState resources;
        ConstructionState construction;
        test::addStockpile(*map, resources, construction);
        const OrderId a = construction.createOrder();
        planHut(*map, resources, construction, a, *area, 2, 3);
        const OrderId b = construction.createOrder();
        planHut(*map, resources, construction, b, {area->x + 2, area->y}, 2, 3);
        construction.confirmOrder(a);
        construction.debugDeliver(a);
        const StructureId first = construction.order(a)->components.front();
        construction.beginConstruction(*map, resources, first);
        construction.addWork(first, 7);
        return construction;
    };
    const ConstructionState first = run();
    const ConstructionState second = run();
    OLAM_CHECK(first.hash() == second.hash());
    const auto bytes = first.serialize(*map);
    OLAM_CHECK(bytes == second.serialize(*map));

    ConstructionState loaded;
    OLAM_CHECK(!loaded.deserialize(*map, bytes).has_value());
    OLAM_CHECK(loaded.hash() == first.hash() && loaded.orderCount() == 2);
    OLAM_CHECK(loaded.serialize(*map) == bytes);
    // New orders continue after the loaded ones.
    OLAM_CHECK(!first.order(loaded.createOrder()));

    // Version 1 blobs (no orders) still load.
    ByteWriter v1;
    v1.write(std::uint32_t{0x53434C4F});
    v1.write(std::uint32_t{1});
    v1.write(map->worldSeed());
    v1.write(map->site().x);
    v1.write(map->site().y);
    v1.write(std::uint32_t{2});
    v1.write(std::uint32_t{1});
    v1.write(std::uint32_t{1});
    v1.write(StructureKind::Floor);
    v1.write(StructureMaterial::PackedEarth);
    v1.write(ConstructionStage::Completed);
    v1.write(area->x);
    v1.write(area->y);
    v1.write(EdgeAxis::North);
    v1.write(false);
    v1.write(std::uint16_t{20});
    ConstructionState old;
    OLAM_CHECK(!old.deserialize(*map, v1.bytes()).has_value());
    OLAM_CHECK(old.structureCount() == 1 && old.floorAt(*area).isValid() && old.orderCount() == 0);
}
