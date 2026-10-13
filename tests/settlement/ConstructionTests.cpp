#include "TestFramework.h"
#include "settlement/ConstructionTestUtil.h"

#include <span>
#include <vector>

using namespace olam;

namespace
{

    const SettlementMap *fixture()
    {
        const SettlementMap *map = test::constructionTestMap();
        OLAM_CHECK(map != nullptr);
        return map;
    }

    // Walls around a w x h block, a door in its north wall and floors inside, all completed.
    void buildHut(const SettlementMap &map, ResourceState &resources, ConstructionState &construction, LocalCoord at, int w, int h)
    {
        for (int x = at.x; x < at.x + w; ++x)
        {
            construction.placeEdge(map, resources, StructureKind::Wall, StructureMaterial::Timber, {x, at.y, EdgeAxis::North});
            construction.placeEdge(map, resources, StructureKind::Wall, StructureMaterial::Timber, {x, at.y + h, EdgeAxis::North});
        }
        for (int y = at.y; y < at.y + h; ++y)
        {
            construction.placeEdge(map, resources, StructureKind::Wall, StructureMaterial::RoughStone, {at.x, y, EdgeAxis::West});
            construction.placeEdge(map, resources, StructureKind::Wall, StructureMaterial::RoughStone, {at.x + w, y, EdgeAxis::West});
        }
        construction.placeEdge(map, resources, StructureKind::Door, StructureMaterial::Timber, {at.x + 1, at.y, EdgeAxis::North});
        for (int y = at.y; y < at.y + h; ++y)
        {
            for (int x = at.x; x < at.x + w; ++x)
                construction.placeFloor(map, resources, StructureMaterial::WoodPlank, {x, y});
        }
    }

} // namespace

OLAM_TEST(tile_edges_have_one_form_and_separate_two_tiles)
{
    // The south edge of a tile is the north edge of the tile below; the east edge is the west edge of the next.
    OLAM_CHECK(tileEdges({5, 5})[2] == tileEdges({5, 6})[0]);
    OLAM_CHECK(tileEdges({5, 5})[1] == tileEdges({6, 5})[3]);
    const auto [north, south] = edgeTiles({5, 5, EdgeAxis::North});
    OLAM_CHECK(north == (LocalCoord{5, 4}) && south == (LocalCoord{5, 5}));
    const auto [west, east] = edgeTiles({5, 5, EdgeAxis::West});
    OLAM_CHECK(west == (LocalCoord{4, 5}) && east == (LocalCoord{5, 5}));
    // Edges run between tile corners.
    OLAM_CHECK(edgeEnd({5, 5, EdgeAxis::North}) == (LocalCoord{6, 5}));
    OLAM_CHECK(edgeEnd({5, 5, EdgeAxis::West}) == (LocalCoord{5, 6}));
}

OLAM_TEST(construction_catalogue_is_consistent)
{
    for (std::size_t k = 0; k < static_cast<std::size_t>(StructureKind::Count); ++k)
    {
        const auto kind = static_cast<StructureKind>(k);
        OLAM_CHECK(!structureMaterials(kind).empty());
        for (const StructureMaterial material : structureMaterials(kind))
        {
            OLAM_CHECK(isMaterialAllowed(kind, material));
            OLAM_CHECK(structureProperties(kind, material).workRequired > 0);
            OLAM_CHECK(structureProperties(kind, material).maxIntegrity > 0);
        }
    }
    OLAM_CHECK(!isMaterialAllowed(StructureKind::Floor, StructureMaterial::Timber));
    OLAM_CHECK(structureProperties(StructureKind::Wall, StructureMaterial::Timber).blocksMovement);
    OLAM_CHECK(!structureProperties(StructureKind::Door, StructureMaterial::Timber).blocksMovement);
    OLAM_CHECK(structureProperties(StructureKind::Wall, StructureMaterial::RoughStone).maxIntegrity >
               structureProperties(StructureKind::Wall, StructureMaterial::Timber).maxIntegrity);
}

OLAM_TEST(walls_doors_and_floors_are_placed_on_clear_ground)
{
    const SettlementMap *map = fixture();
    if (!map)
        return;
    const auto area = test::findClearArea(*map, 4, 3);
    OLAM_CHECK(area.has_value());
    if (!area)
        return;
    const std::uint64_t generatedHash = hashSettlementMap(*map);
    ResourceState resources;
    ConstructionState construction;

    const LocalEdge edge{area->x, area->y, EdgeAxis::North};
    const PlacementResult wall = construction.placeEdge(*map, resources, StructureKind::Wall, StructureMaterial::Timber, edge);
    OLAM_CHECK(wall.error == PlacementError::None && wall.id.isValid());
    OLAM_CHECK(construction.edgeAt(edge) == wall.id);
    const Structure *s = construction.structure(wall.id);
    OLAM_CHECK(s && s->kind == StructureKind::Wall && s->material == StructureMaterial::Timber && s->edge() == edge);
    OLAM_CHECK(s && s->stage == ConstructionStage::Completed &&
               s->workDone == structureProperties(StructureKind::Wall, StructureMaterial::Timber).workRequired);
    // The same edge seen from the tile above is the same wall.
    OLAM_CHECK(construction.edgeAt(tileEdges({area->x, area->y - 1})[2]) == wall.id);

    // A second wall on the edge conflicts; a floor beneath it does not.
    OLAM_CHECK(construction.placeEdge(*map, resources, StructureKind::Wall, StructureMaterial::RoughStone, edge).error ==
               PlacementError::Occupied);
    const PlacementResult floor = construction.placeFloor(*map, resources, StructureMaterial::PackedEarth, *area);
    OLAM_CHECK(floor.error == PlacementError::None && construction.floorAt(*area) == floor.id);
    OLAM_CHECK(construction.placeFloor(*map, resources, StructureMaterial::WoodPlank, *area).error == PlacementError::Occupied);

    // A door fills an opening: an empty edge, or the place of a wall it replaces.
    const LocalEdge side{area->x, area->y, EdgeAxis::West};
    const PlacementResult door = construction.placeEdge(*map, resources, StructureKind::Door, StructureMaterial::Timber, side,
                                                        {.flipped = true});
    OLAM_CHECK(door.error == PlacementError::None && construction.structure(door.id)->flipped);
    const PlacementResult replacing = construction.placeEdge(*map, resources, StructureKind::Door, StructureMaterial::Timber, edge);
    OLAM_CHECK(replacing.error == PlacementError::None);
    OLAM_CHECK(construction.structure(wall.id) == nullptr && construction.edgeAt(edge) == replacing.id);
    // Walls do not replace doors, and doors do not stack.
    OLAM_CHECK(construction.placeEdge(*map, resources, StructureKind::Wall, StructureMaterial::Timber, side).error ==
               PlacementError::Occupied);
    OLAM_CHECK(construction.placeEdge(*map, resources, StructureKind::Door, StructureMaterial::Timber, side).error ==
               PlacementError::Occupied);
    OLAM_CHECK(construction.structureCount() == 3);

    // Construction is an overlay: the generated map and its natural resources are untouched.
    OLAM_CHECK(hashSettlementMap(*map) == generatedHash);
    OLAM_CHECK(resources.revision() == 0);
}

OLAM_TEST(invalid_placement_is_rejected)
{
    const SettlementMap *map = fixture();
    if (!map)
        return;
    const ResourceState resources;
    const ConstructionState construction;
    const LocalTerrainData &terrain = map->terrain();

    const auto water = test::findTile(*map, [&](LocalCoord t) { return terrain.water[map->index(t)] != LocalWater::None; });
    OLAM_CHECK(water.has_value());
    if (water)
    {
        OLAM_CHECK(construction.checkFloor(*map, resources, StructureMaterial::PackedEarth, *water) == PlacementError::Water);
        for (const LocalEdge edge : tileEdges(*water))
        {
            const PlacementError error = construction.checkEdge(*map, resources, StructureKind::Wall, StructureMaterial::Timber, edge);
            OLAM_CHECK(error == PlacementError::Water || error == PlacementError::OutOfBounds);
            OLAM_CHECK(construction.checkEdge(*map, resources, StructureKind::Door, StructureMaterial::Timber, edge) == error);
        }
    }

    // Off the map, and on the map border where a wall would have ground on one side only.
    OLAM_CHECK(construction.checkFloor(*map, resources, StructureMaterial::PackedEarth, {-1, 3}) == PlacementError::OutOfBounds);
    OLAM_CHECK(construction.checkFloor(*map, resources, StructureMaterial::PackedEarth, {3, map->height()}) == PlacementError::OutOfBounds);
    OLAM_CHECK(construction.checkEdge(*map, resources, StructureKind::Wall, StructureMaterial::Timber, {0, 10, EdgeAxis::West}) ==
               PlacementError::OutOfBounds);
    OLAM_CHECK(construction.checkEdge(*map, resources, StructureKind::Wall, StructureMaterial::Timber,
                                      {10, map->height(), EdgeAxis::North}) == PlacementError::OutOfBounds);
    OLAM_CHECK(construction.checkEdge(*map, resources, StructureKind::Wall, StructureMaterial::Timber, {-5, -5, EdgeAxis::North}) ==
               PlacementError::OutOfBounds);

    // Materials must suit the kind.
    const auto area = test::findClearArea(*map, 1, 1);
    OLAM_CHECK(area.has_value());
    if (area)
    {
        OLAM_CHECK(construction.checkFloor(*map, resources, StructureMaterial::Timber, *area) == PlacementError::InvalidMaterial);
        OLAM_CHECK(construction.checkEdge(*map, resources, StructureKind::Door, StructureMaterial::RoughStone,
                                          {area->x, area->y, EdgeAxis::North}) == PlacementError::InvalidMaterial);
    }

    // Slopes beyond the configured rise: with no rise allowed, any uneven dry tile is too steep.
    const ConstructionState strict(ConstructionConfig{.maxFloorRiseDm = 0, .maxEdgeRiseDm = 0});
    const auto uneven = test::findTile(*map, [&](LocalCoord t)
                                       {
                                           const std::size_t i = map->index(t);
                                           return terrain.water[i] == LocalWater::None && !terrain.treeId[i].isValid() &&
                                                  terrain.elevation[i] != terrain.elevation[map->index({t.x + 1, t.y})] &&
                                                  terrain.water[map->index({t.x + 1, t.y})] == LocalWater::None &&
                                                  !terrain.treeId[map->index({t.x + 1, t.y})].isValid() && !map->understoryAt(t) &&
                                                  !map->understoryAt({t.x + 1, t.y}); });
    OLAM_CHECK(uneven.has_value());
    if (uneven)
    {
        OLAM_CHECK(strict.checkFloor(*map, resources, StructureMaterial::PackedEarth, *uneven) == PlacementError::TooSteep);
        OLAM_CHECK(strict.checkEdge(*map, resources, StructureKind::Wall, StructureMaterial::Timber,
                                    {uneven->x + 1, uneven->y, EdgeAxis::West}) == PlacementError::TooSteep);
    }
}

OLAM_TEST(obstructions_block_until_cleared)
{
    const SettlementMap *map = fixture();
    if (!map)
        return;
    ResourceState resources;
    ConstructionState construction;
    const LocalTerrainData &terrain = map->terrain();

    // A tree in the way is felled (for its wood) only when clearing is asked for.
    const auto tree = test::findTile(*map, [&](LocalCoord t)
                                     { return terrain.treeId[map->index(t)].isValid() && terrain.water[map->index(t)] == LocalWater::None &&
                                              construction.checkFloor(*map, resources, StructureMaterial::PackedEarth, t) == PlacementError::Tree; });
    OLAM_CHECK(tree.has_value());
    if (tree)
    {
        OLAM_CHECK(construction.placeFloor(*map, resources, StructureMaterial::PackedEarth, *tree).error == PlacementError::Tree);
        OLAM_CHECK(resources.revision() == 0);
        const PlacementResult cleared =
            construction.placeFloor(*map, resources, StructureMaterial::PackedEarth, *tree, {.clearObstructions = true});
        OLAM_CHECK(cleared.error == PlacementError::None && cleared.woodKg > 0.0f);
        OLAM_CHECK(resources.treeFelled(*map, terrain.treeId[map->index(*tree)]));
    }

    // Shrubs and bushes must be cleared too; grass and flowers are trampled.
    const auto shrub = test::findTile(*map, [&](LocalCoord t)
                                      {
                                          const auto u = map->understoryAt(t);
                                          return u &&
                                                 (map->understory().kind[*u] == UnderstoryKind::Shrub ||
                                                  map->understory().kind[*u] == UnderstoryKind::Bush) &&
                                                 construction.checkFloor(*map, resources, StructureMaterial::PackedEarth, t) ==
                                                     PlacementError::Obstacle; });
    OLAM_CHECK(shrub.has_value());
    if (shrub)
    {
        const PlacementResult cleared =
            construction.placeFloor(*map, resources, StructureMaterial::PackedEarth, *shrub, {.clearObstructions = true});
        OLAM_CHECK(cleared.error == PlacementError::None && cleared.woodKg == 0.0f);
        OLAM_CHECK(resources.understoryCleared(*map, *map->understoryAt(*shrub)));
    }
    const auto grass = test::findTile(*map, [&](LocalCoord t)
                                      {
                                          const auto u = map->understoryAt(t);
                                          return u && (map->understory().kind[*u] == UnderstoryKind::TallGrass ||
                                                       map->understory().kind[*u] == UnderstoryKind::Flowers); });
    if (grass)
        OLAM_CHECK(construction.checkFloor(*map, resources, StructureMaterial::PackedEarth, *grass) != PlacementError::Obstacle);

    // Clearing never overrides hard errors such as water.
    const auto water = test::findTile(*map, [&](LocalCoord t) { return terrain.water[map->index(t)] != LocalWater::None; });
    if (water)
        OLAM_CHECK(construction.placeFloor(*map, resources, StructureMaterial::PackedEarth, *water, {.clearObstructions = true}).error ==
                   PlacementError::Water);
}

OLAM_TEST(structures_can_be_removed_and_ids_are_not_reused)
{
    const SettlementMap *map = fixture();
    if (!map)
        return;
    const auto area = test::findClearArea(*map, 4, 3);
    OLAM_CHECK(area.has_value());
    if (!area)
        return;
    ResourceState resources;
    ConstructionState construction;
    buildHut(*map, resources, construction, *area, 4, 3);
    // 14 perimeter walls (one replaced by the door) and 12 floors.
    OLAM_CHECK(construction.structureCount() == 26);

    const LocalEdge doorEdge{area->x + 1, area->y, EdgeAxis::North};
    const StructureId door = construction.edgeAt(doorEdge);
    OLAM_CHECK(construction.structure(door)->kind == StructureKind::Door);
    OLAM_CHECK(construction.remove(door));
    OLAM_CHECK(!construction.remove(door));
    OLAM_CHECK(!construction.edgeAt(doorEdge).isValid() && construction.structure(door) == nullptr);
    const PlacementResult again = construction.placeEdge(*map, resources, StructureKind::Wall, StructureMaterial::Timber, doorEdge);
    OLAM_CHECK(again.error == PlacementError::None && again.id.value > door.value);

    // A rectangle query covers floors on its tiles and the edges on or inside it, nothing outside.
    const LocalCoord a = *area;
    const LocalCoord b{area->x + 1, area->y + 1};
    const std::vector<StructureId> inRect = construction.structuresInRect(b, a);
    // 4 floors; 2 north walls on top, 2 west walls on the left, nothing on the interior edges.
    OLAM_CHECK(inRect.size() == 8);
    OLAM_CHECK(construction.structure(inRect.front())->kind == StructureKind::Floor);
    OLAM_CHECK(construction.removeInRect(a, b) == 8);
    OLAM_CHECK(construction.structureCount() == 18);
    OLAM_CHECK(construction.removeInRect({area->x - 1, area->y - 1}, {area->x + 4, area->y + 3}) == 18);
    OLAM_CHECK(construction.structureCount() == 0);
}

OLAM_TEST(identical_construction_commands_give_identical_state)
{
    const SettlementMap *map = fixture();
    if (!map)
        return;
    const auto area = test::findClearArea(*map, 5, 4);
    OLAM_CHECK(area.has_value());
    if (!area)
        return;
    const auto run = [&]()
    {
        ResourceState resources;
        ConstructionState construction;
        buildHut(*map, resources, construction, *area, 5, 4);
        construction.remove(construction.floorAt(*area));
        construction.placeEdge(*map, resources, StructureKind::Door, StructureMaterial::Timber, {area->x, area->y + 1, EdgeAxis::West},
                               {.flipped = true});
        return std::pair{construction.hash(), construction.serialize(*map)};
    };
    const auto first = run();
    const auto second = run();
    OLAM_CHECK(first.first == second.first);
    OLAM_CHECK(first.second == second.second);
    OLAM_CHECK(first.first != ConstructionState{}.hash());
}

OLAM_TEST(construction_state_round_trips_and_checks_its_map)
{
    const SettlementMap *map = fixture();
    if (!map)
        return;
    const auto area = test::findClearArea(*map, 3, 3);
    OLAM_CHECK(area.has_value());
    if (!area)
        return;
    ResourceState resources;
    ConstructionState construction;
    buildHut(*map, resources, construction, *area, 3, 3);
    const auto bytes = construction.serialize(*map);

    ConstructionState loaded;
    OLAM_CHECK(!loaded.deserialize(*map, bytes).has_value());
    OLAM_CHECK(loaded.hash() == construction.hash());
    OLAM_CHECK(loaded.structureCount() == construction.structureCount());
    OLAM_CHECK(loaded.edgeAt({area->x + 1, area->y, EdgeAxis::North}) == construction.edgeAt({area->x + 1, area->y, EdgeAxis::North}));
    OLAM_CHECK(loaded.floorAt(*area) == construction.floorAt(*area));
    OLAM_CHECK(loaded.serialize(*map) == bytes);
    // New ids continue after the loaded ones.
    const PlacementResult next = loaded.placeFloor(*map, resources, StructureMaterial::PackedEarth, {area->x - 1, area->y});
    if (next.error == PlacementError::None)
        OLAM_CHECK(construction.structure(next.id) == nullptr);

    // Truncated data and data of another settlement are rejected and leave the state untouched.
    ConstructionState fresh;
    OLAM_CHECK(fresh.deserialize(*map, std::span<const std::uint8_t>(bytes).first(bytes.size() - 3)).has_value());
    std::vector<std::uint8_t> otherSite = bytes;
    otherSite[16] ^= 0x01;
    OLAM_CHECK(fresh.deserialize(*map, otherSite).has_value());
    std::vector<std::uint8_t> badVersion = bytes;
    badVersion[4] = 99;
    OLAM_CHECK(fresh.deserialize(*map, badVersion).has_value());
    OLAM_CHECK(fresh.structureCount() == 0);
}
