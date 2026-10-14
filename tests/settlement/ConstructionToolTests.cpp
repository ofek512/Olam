#include "TestFramework.h"
#include "settlement/ConstructionTestUtil.h"

#include "tools/settlement_viewer/ConstructionTool.h"
#include "tools/settlement_viewer/ConstructionViews.h"

#include <algorithm>
#include <vector>

using namespace olam;

OLAM_TEST(construction_snaps_to_corners_and_edges)
{
    OLAM_CHECK(nearestCorner({3.4f, 7.6f}) == (LocalCoord{3, 8}));
    OLAM_CHECK(nearestCorner({-0.6f, 0.4f}) == (LocalCoord{-1, 0}));
    // Each side of a tile snaps to its own edge.
    OLAM_CHECK(nearestEdge({5.5f, 5.1f}) == (LocalEdge{5, 5, EdgeAxis::North}));
    OLAM_CHECK(nearestEdge({5.5f, 5.9f}) == (LocalEdge{5, 6, EdgeAxis::North}));
    OLAM_CHECK(nearestEdge({5.1f, 5.5f}) == (LocalEdge{5, 5, EdgeAxis::West}));
    OLAM_CHECK(nearestEdge({5.9f, 5.5f}) == (LocalEdge{6, 5, EdgeAxis::West}));
}

OLAM_TEST(wall_drags_follow_tile_edges)
{
    OLAM_CHECK(wallSegments({2, 2}, {2, 2}).empty());
    // A straight run, in either direction.
    const auto run = wallSegments({6, 3}, {2, 3});
    OLAM_CHECK(run.size() == 4);
    OLAM_CHECK(std::all_of(run.begin(), run.end(), [](LocalEdge e) { return e.axis == EdgeAxis::North && e.y == 3 && e.x >= 2 && e.x < 6; }));
    const auto column = wallSegments({1, 1}, {1, 4});
    OLAM_CHECK(column.size() == 3 && column.front() == (LocalEdge{1, 1, EdgeAxis::West}));

    // A diagonal drag outlines the rectangle: every segment lies on its border, end to end.
    const auto outline = wallSegments({2, 3}, {5, 1});
    OLAM_CHECK(outline.size() == 2 * (3 + 2));
    for (const LocalEdge e : outline)
    {
        const LocalCoord end = edgeEnd(e);
        const bool onBorder = e.axis == EdgeAxis::North ? (e.y == 1 || e.y == 3) && e.x >= 2 && end.x <= 5
                                                        : (e.x == 2 || e.x == 5) && e.y >= 1 && end.y <= 3;
        OLAM_CHECK(onBorder);
    }
    OLAM_CHECK(tilesInRect({3, 2}, {1, 3}).size() == 6);
    OLAM_CHECK(tilesInRect({3, 2}, {1, 3}).front() == (LocalCoord{1, 2}));
}

OLAM_TEST(build_tool_cycles_materials_per_kind)
{
    ConstructionTool tool;
    OLAM_CHECK(tool.tool() == BuildTool::Wall && tool.material() == StructureMaterial::Timber);
    tool.cycleMaterial(1);
    OLAM_CHECK(tool.material() == StructureMaterial::WattleAndDaub);
    tool.cycleMaterial(-2);
    OLAM_CHECK(tool.material() == StructureMaterial::RoughStone);
    tool.selectTool(BuildTool::Floor);
    OLAM_CHECK(tool.material() == StructureMaterial::PackedEarth);
    tool.selectTool(BuildTool::Wall);
    OLAM_CHECK(tool.material() == StructureMaterial::RoughStone);
    tool.selectTool(BuildTool::Remove);
    OLAM_CHECK(!tool.kind().has_value());
    tool.beginDrag({1.0f, 1.0f});
    tool.selectTool(BuildTool::Door);
    OLAM_CHECK(!tool.dragging() && tool.kind() == StructureKind::Door);
}

OLAM_TEST(build_tool_places_valid_parts_and_removes)
{
    const SettlementMap *map = test::constructionTestMap();
    OLAM_CHECK(map != nullptr);
    if (!map)
        return;
    const auto area = test::findClearArea(*map, 4, 3);
    OLAM_CHECK(area.has_value());
    if (!area)
        return;
    ResourceState resources;
    ConstructionState construction;
    ConstructionTool tool;
    const auto x = static_cast<float>(area->x);
    const auto y = static_cast<float>(area->y);

    // Drag a wall outline around the 4 x 3 block; the preview is exactly what gets placed.
    tool.beginDrag({x + 0.1f, y - 0.2f});
    const Vec2 corner{x + 3.9f, y + 3.1f};
    const auto preview = tool.preview(*map, resources, construction, corner);
    OLAM_CHECK(preview.size() == 14);
    OLAM_CHECK(std::all_of(preview.begin(), preview.end(), [](const PreviewItem &i) { return i.error == PlacementError::None; }));
    const ToolResult walls = tool.commit(*map, resources, construction, corner, {});
    OLAM_CHECK(walls.placed == 14 && walls.skipped == 0 && !tool.dragging());

    // Redrawing the north wall one tile longer only adds the missing segment.
    tool.beginDrag({x, y});
    const ToolResult longer = tool.commit(*map, resources, construction, {x + 5.0f, y}, {});
    OLAM_CHECK(longer.skipped >= 4 && longer.skipReason == PlacementError::Occupied);
    OLAM_CHECK(longer.placed + longer.skipped == 5);

    // A door click replaces the wall it is on; a floor drag fills the inside.
    tool.selectTool(BuildTool::Door);
    tool.flipDoor();
    const ToolResult door = tool.commit(*map, resources, construction, {x + 1.5f, y + 0.1f}, {});
    OLAM_CHECK(door.placed == 1);
    const Structure *placed = construction.structure(construction.edgeAt({area->x + 1, area->y, EdgeAxis::North}));
    OLAM_CHECK(placed && placed->kind == StructureKind::Door && placed->flipped);
    tool.selectTool(BuildTool::Floor);
    tool.beginDrag({x + 0.5f, y + 0.5f});
    OLAM_CHECK(tool.commit(*map, resources, construction, {x + 3.5f, y + 2.5f}, {}).placed == 12);

    // A click near a wall removes the wall; in the middle of a tile, the floor.
    tool.selectTool(BuildTool::Remove);
    const std::size_t before = construction.structureCount();
    OLAM_CHECK(tool.commit(*map, resources, construction, {x + 2.5f, y + 0.1f}, {}).removed == 1);
    OLAM_CHECK(!construction.edgeAt({area->x + 2, area->y, EdgeAxis::North}).isValid());
    OLAM_CHECK(construction.floorAt({area->x + 2, area->y}).isValid());
    OLAM_CHECK(tool.commit(*map, resources, construction, {x + 2.5f, y + 0.5f}, {}).removed == 1);
    OLAM_CHECK(!construction.floorAt({area->x + 2, area->y}).isValid());
    OLAM_CHECK(construction.structureCount() == before - 2);
    // A drag removes everything in the rectangle.
    tool.beginDrag({x - 1.5f, y - 1.5f});
    tool.commit(*map, resources, construction, {x + 5.5f, y + 4.5f}, {});
    OLAM_CHECK(construction.structureCount() == 0);
}

OLAM_TEST(structure_quads_cover_visible_structures)
{
    const SettlementMap *map = test::constructionTestMap();
    if (!map)
        return;
    const auto area = test::findClearArea(*map, 2, 2);
    if (!area)
        return;
    ResourceState resources;
    ConstructionState construction;
    construction.placeFloor(*map, resources, StructureMaterial::WoodPlank, *area);
    construction.placeEdge(*map, resources, StructureKind::Wall, StructureMaterial::Timber, {area->x, area->y, EdgeAxis::North});
    construction.placeEdge(*map, resources, StructureKind::Door, StructureMaterial::Timber, {area->x, area->y, EdgeAxis::West});

    std::vector<StructureQuad> quads;
    const Rect visible{static_cast<float>(area->x) - 4.0f, static_cast<float>(area->y) - 4.0f, 8.0f, 8.0f};
    collectStructureQuads(construction, visible, 16.0f, quads);
    // Floor first, then the wall and the door (bar and leaf).
    OLAM_CHECK(quads.size() == 4);
    OLAM_CHECK(quads.front().rect.w == 1.0f && quads.front().rect.h == 1.0f);
    collectStructureQuads(construction, {visible.x + 100.0f, visible.y, 8.0f, 8.0f}, 16.0f, quads);
    OLAM_CHECK(quads.empty());

    // Completed structures are opaque; plans are translucent.
    OLAM_CHECK(structureColor(StructureKind::Wall, StructureMaterial::Timber, ConstructionStage::Completed)[3] == 255);
    OLAM_CHECK(structureColor(StructureKind::Wall, StructureMaterial::Timber, ConstructionStage::Planned)[3] < 255);
}

OLAM_TEST(room_overlay_draws_roofs_between_floors_and_walls)
{
    const SettlementMap *map = test::constructionTestMap();
    if (!map)
        return;
    const auto area = test::findClearArea(*map, 2, 2);
    if (!area)
        return;
    ResourceState resources;
    ConstructionState construction;
    for (int i = 0; i < 2; ++i)
    {
        for (const LocalEdge edge : {LocalEdge{area->x + i, area->y, EdgeAxis::North}, LocalEdge{area->x + i, area->y + 2, EdgeAxis::North},
                                     LocalEdge{area->x, area->y + i, EdgeAxis::West}, LocalEdge{area->x + 2, area->y + i, EdgeAxis::West}})
            construction.placeEdge(*map, resources, StructureKind::Wall, StructureMaterial::Timber, edge);
    }
    construction.placeFloor(*map, resources, StructureMaterial::WoodPlank, *area);
    OLAM_CHECK(construction.rooms().roomCount() == 1);

    std::vector<StructureQuad> quads;
    const Rect visible{static_cast<float>(area->x) - 4.0f, static_cast<float>(area->y) - 4.0f, 8.0f, 8.0f};
    const auto roofQuads = [&](bool built)
    {
        const auto roof = roofColor(StructureMaterial::Thatch, built);
        return std::count_if(quads.begin(), quads.end(), [&](const StructureQuad &q) { return q.rgba == roof; });
    };
    collectStructureQuads(construction, visible, 16.0f, quads, RoomOverlay::None);
    const std::size_t plain = quads.size();
    OLAM_CHECK(plain == 1 + 8 && roofQuads(false) == 0);
    // One run per row of the room, after the floor and before the walls; faint until the roof is paid for.
    collectStructureQuads(construction, visible, 16.0f, quads, RoomOverlay::Roofs);
    OLAM_CHECK(quads.size() == plain + 2 && roofQuads(false) == 2);
    OLAM_CHECK(quads[1].rect.w == 2.0f);
    test::addStockpile(*map, resources, construction);
    OLAM_CHECK(construction.buildRoof(construction.rooms().roomAt(*area)));
    collectStructureQuads(construction, visible, 16.0f, quads, RoomOverlay::Roofs);
    OLAM_CHECK(roofQuads(true) == 2 && quads[1].rgba == roofColor(StructureMaterial::Thatch));
    // Disabled roofs are not drawn; the rooms overlay still shows the room.
    const RoomId room = construction.rooms().roomAt(*area);
    construction.setRoof(room, {RoofMode::Disabled, StructureMaterial::Thatch});
    collectStructureQuads(construction, visible, 16.0f, quads, RoomOverlay::Roofs);
    OLAM_CHECK(quads.size() == plain);
    collectStructureQuads(construction, visible, 16.0f, quads, RoomOverlay::Rooms);
    OLAM_CHECK(quads.size() == plain + 2 && quads[1].rgba == roomColor(room));
}

OLAM_TEST(build_tool_places_furniture_and_stockpiles)
{
    const SettlementMap *map = test::constructionTestMap();
    if (!map)
        return;
    const auto area = test::findClearArea(*map, 3, 3);
    if (!area)
        return;
    ResourceState resources;
    ConstructionState construction;
    ConstructionTool tool;
    const auto x = static_cast<float>(area->x);
    const auto y = static_cast<float>(area->y);

    // A bed centred on the cursor, inside the tile under it.
    tool.selectFurniture(FurnitureKind::Bed);
    OLAM_CHECK(tool.tool() == BuildTool::Furniture && tool.kind() == StructureKind::Furniture);
    OLAM_CHECK(tool.material() == StructureMaterial::Timber && tool.materials().size() == 1);
    OLAM_CHECK(tool.furnitureCell({x + 0.5f, y + 0.5f}) == (LocalCoord{area->x * 4 + 1, area->y * 4}));
    const auto items = tool.preview(*map, resources, construction, {x + 0.5f, y + 0.5f});
    OLAM_CHECK(items.size() == 1 && items.front().kind == StructureKind::Furniture && items.front().error == PlacementError::None);
    OLAM_CHECK(tool.commit(*map, resources, construction, {x + 0.5f, y + 0.5f}, {}).placed == 1);
    // Turned east it is 4 x 2 cells.
    tool.rotate();
    OLAM_CHECK(tool.facing() == Facing::East);
    OLAM_CHECK(tool.furnitureCell({x + 1.5f, y + 1.5f}) == (LocalCoord{area->x * 4 + 4, area->y * 4 + 5}));
    // The remove tool picks furniture under the cursor first.
    tool.selectTool(BuildTool::Remove);
    OLAM_CHECK(tool.commit(*map, resources, construction, {x + 0.5f, y + 0.5f}, {}).removed == 1);
    OLAM_CHECK(construction.structureCount() == 0);

    // A stockpile is one rectangle, made if every tile is fine.
    tool.selectTool(BuildTool::Stockpile);
    tool.beginDrag({x + 0.5f, y + 0.5f});
    const auto tiles = tool.preview(*map, resources, construction, {x + 2.5f, y + 1.5f});
    OLAM_CHECK(tiles.size() == 6 && tiles.front().stockpile);
    const ToolResult made = tool.commit(*map, resources, construction, {x + 2.5f, y + 1.5f}, {});
    OLAM_CHECK(made.placed == 1 && construction.stockpiles().count() == 1);
    const Stockpile *stockpile = construction.stockpiles().stockpile(construction.stockpiles().stockpileAt(*area));
    OLAM_CHECK(stockpile && stockpile->max == (LocalCoord{area->x + 2, area->y + 1}));
    // Not over another one.
    tool.beginDrag({x + 0.5f, y + 0.5f});
    OLAM_CHECK(tool.commit(*map, resources, construction, {x + 0.5f, y + 2.5f}, {}).skipReason == PlacementError::Occupied);
}
