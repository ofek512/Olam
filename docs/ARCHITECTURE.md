# Olam Architecture

Stable, project-wide decisions. World-generation specifics live in [WORLD_GENERATION.md](WORLD_GENERATION.md);
the full vision is in [agent.MD](../agent.MD); progress is tracked in [ROADMAP.md](ROADMAP.md).

## Layers

```text
Operating System
  -> platform (SDL3)
  -> core engine (logging, time, input, files, math, camera, hashing, RNG) + rendering
  -> world (strategic data model) -> worldgen (generation passes)
  -> tools / game systems
```

Lower layers never include headers from higher layers. Simulation and world data never depend on rendering.

## CMake targets

```text
olam_core ──> olam_world ──> olam_worldgen        (SDL-free, headless-buildable)
    │              │
    │              └──> olam_settlement ──> olam_settlement_views   (SDL-free)
    │
    └──> olam_engine (SDL3: platform + renderer)
                  │
olam_world + olam_engine ──> olam_world_viewer (tools/world_viewer)
olam_settlement_views + olam_engine ──> olam_settlement_viewer (tools/settlement_viewer)
                                   │
olam_worldgen + olam_world_viewer + olam_settlement_viewer ──> Olam (src/app, thin executable)
olam_worldgen + olam_settlement_views ──> olam_generate (src/tools/generate, headless CLI)
```

- `olam_core`: generic infrastructure only (no SDL, no world knowledge): logging, time, input state, files,
  math, camera, assertions, strong IDs, `Layer<T>`, hashing, deterministic RNG, and the immediate-mode UI
  (`UiContext`: panels and buttons laid out from `Input`, producing a draw list; no SDL, so screens are tested).
- `olam_world`: strategic world data model. No SDL, no rendering, no generation algorithms.
- `olam_worldgen`: generation pipeline and passes. No SDL.
- `olam_engine`: SDL platform layer, renderer, textures, debug text panels, `drawUi` (replays a UI draw list).
- `olam_world_viewer`: world debug visualization and tile inspector.
- `olam_settlement`: local settlement maps (`SettlementMap`, site checks, `generateLocalMap`). Depends on
  `olam_world` only: it reads a generated `World`, never runs world generation, and has no SDL.
- `olam_settlement_views`: SDL-free local map colouring, tile inspector, procedural sprite atlas and visible-sprite
  collection (viewer and headless PNG output), construction tools and drawing, and the settlement HUD.
- `olam_settlement_viewer`: SDL renderer for local maps.
- `Olam`: composes systems; contains no world logic.
- `olam_generate`: headless generation (`--seed`, `--size`, `--save`, `--png`, `--local`) printing per-pass timings,
  stats and the world hash; used for profiling and on machines without a display.

CMake options: `OLAM_BUILD_ENGINE` (SDL targets, default ON) and `OLAM_BUILD_TESTS` (default ON).
With `OLAM_BUILD_ENGINE=OFF` SDL is not fetched; if `olam_worldgen` then fails to build, a layering rule was broken.

Tests: one executable per library (`olam_core_tests`, `olam_world_tests`, `olam_worldgen_tests`,
`olam_settlement_tests`).

## World model

- **Finite, square grid**, fully generated up front, no wrapping. D8 (8-neighbour) adjacency for flow and movement.
- Represents a "known world" region of a larger planet: several landmasses, seas and islands.
- **Size**: default 2048 x 2048, max 4096 per side, min 16, each side a power of two; non-square allowed.
- **Scale**: ~1 km x 1 km per world tile (`WorldConfig::tileSizeMeters`, configurable; never hard-coded into simulation).
  World tiles are geographic areas: rivers and roads pass _through_ a tile.
- **Coordinates**: origin at the north-west (top-left), +x = east, +y = south, `int32`.
  Tile `(x, y)` covers `[x, x+1) x [y, y+1)` in world units; its centre is `(x + 0.5, y + 0.5)`.
- **Hierarchy**: world tiles -> irregular regions -> region graph (trade, kingdoms, armies).
- **Local maps are separate**: settlement maps use a much finer grid (~1-2 m per tile). `WorldTile` data and
  `LocalTile` data are separate types with separate responsibilities. Settlements are world entities (metadata
  only in `World`); detailed local maps are owned by a future `SettlementSimulationManager`. Unloaded
  settlements are simulated statistically.

## Local settlement maps (Phase 4A)

- A `SettlementMap` is a 3072 x 3072 grid of 2 m tiles (~6 km, `SettlementConfig`) centred on the centre of the
  chosen world tile. The world tile size must be a whole multiple of the local tile size.
- All local tiles lie on one **global local grid** (`SettlementMap::origin()`), and generation is a pure function
  of the world, its seed and the absolute position. Overlapping maps of neighbouring sites are therefore identical
  where they overlap (seamless), and a map can always be regenerated instead of saved (4A does not save maps).
- Layers are struct-of-arrays like the world: elevation (int16 dm relative to a base), water, ground, soil,
  fertility, ground cover, resource and a per-tile tree index. Trees (position, growth, `TreeKind`) are
  individual entities in a compact list; understory plants and small objects (`UnderstoryData`), forage patches
  (`ForageData`), springs and mineral occurrences are sparse lists without per-tile layers
  (`understoryAt` / `forageAt` binary-search). Geological resources (resource layer, minerals) and biological
  ones (trees, understory, forage) are separate, so one spot can hold a tree, mushrooms and buried iron.
- Generated data is immutable: forage stores its generated maximum only. Gathering state is `ResourceState`
  (`olam_settlement`): forage taken (kg) and felled trees, keyed by local tile in ordered containers, with
  `regrow(years)` and `treeWoodKg(kind, growth)`. Views, sprites and the inspector take it as an optional
  overlay; the viewer's test harvest (G gather 1 kg or fell a tree, Shift+G all, Y one year) edits it.
- Persistence: `ResourceState::serialize` / `deserialize` give a versioned binary blob bound to the map's world
  seed and site (round trip tested). A save needs the world, the settlement site and this blob, since the map
  itself is regenerated. Writing it into a save file is left to Phase 4B, when settlements get saved.
- Sites: any land tile far enough from the world edge (`siteError`); harsh sites only produce warnings
  (`siteWarnings`). The app holds one active settlement; founding elsewhere replaces it, a new world drops it.
- Generation details: [WORLD_GENERATION.md](WORLD_GENERATION.md#local-maps-phase-4a).

### Local map rendering (Phase 4A.5B)

One terrain representation, shown differently by zoom (screen px per tile, thresholds in `LocalSprites.h`):

| Zoom   | Look                                                                                             |
| ------ | ------------------------------------------------------------------------------------------------ |
| < 1    | Natural overview: the view texture box-filtered 4x (`downsampleRgba`), bilinear                  |
| 1 - 10 | View texture, 1 texel = 1 tile, nearest; tree crowns and rocks fade in over 3 - 4.5              |
| >= 6   | Tree shadows; understory and forage (berries, mushrooms, greens, herbs, deadwood, reeds) fade in |
| >= 10  | Pixel-art ground: a 16 px pattern per tile tinted by the tile colour, fading in over 10 - 14     |
| >= 12  | Sprites switch from bilinear to nearest (crisp pixel art, snapped to whole screen pixels)        |

- Base textures are built once per map / view change (never per frame). Object sprites exist only per frame,
  for the visible tiles plus a 4-tile margin (crowns reach beyond their anchor tile); nothing is allocated per
  tree. `collectLocalSprites` (SDL-free, tested) emits back-to-front instances: ground, shadows, then rocks and
  crowns row by row (trees are stored row-major, so the visible rows are one index range).
- Sprites come from one procedural greyscale atlas (`buildLocalSpriteAtlas`, no external images) at
  16 px per tile, tinted per instance; one `SpriteBatch` draw call (`SDL_RenderGeometryRaw`) per frame.
- Variation (crown shape per `TreeKind`, size class from growth and kind, shape variant, mirroring, hue,
  position jitter) hashes the global local coordinate, so overlapping maps look identical. Dry ground tints
  crowns olive. The base texture colours tree tiles by kind, so far zoom shows forest types.
- Only the Terrain view gets sprites; debug views stay flat textures.
- Headless check: `olam_generate --local <site> --local-zoom <px> [--local-crop x,y] --png f` renders the same
  composition on the CPU (960 x 540).

### Construction (Phase 4B)

Plan: [4BPlan.md](../4BPlan.md). Data lives in `olam_settlement` (no SDL); tools and drawing data in
`olam_settlement_views` (no SDL); the viewer only maps quads to the screen.

- **Edges**: walls and doors sit on tile edges, floors on tiles; the 2 m terrain grid is unchanged.
  `LocalEdge{x, y, axis}` is the North edge (corner (x, y) to (x + 1, y)) or West edge (to (x, y + 1)) of a tile,
  so each edge has exactly one form (the south edge of a tile is the North edge of the tile below).
  `edgeTiles`, `edgeEnd`, `tileEdges` convert. Walls need ground on both sides, so map-border edges are rejected.
- **Catalogue** (`Construction.h`): `StructureKind` (Wall, Door, Floor), `StructureMaterial` (timber, wattle and
  daub, rough stone, packed earth, wood plank, thatch for roofs), `ConstructionStage` (Planned → AwaitingMaterials → ReadyToBuild →
  UnderConstruction → Completed) and integer `StructureProperties` (integrity, insulation, blocks movement /
  sight, builder work). Enums are append-only. Placement limits are a plain `ConstructionConfig`.
- **State** (`ConstructionState`): an overlay like `ResourceState`. Structures are kept by `StructureId` in an
  ordered map, with an edge index (key `y << 17 | x << 1 | axis`) and a floor index (`y << 16 | x`). Keys are
  row-major, so a rectangle query is one ordered range per row: O(rows · log n + k). Memory follows the number
  of structures (~100 B each including map nodes), never the 9.4 M map tiles. Ids are assigned in order and
  never reused (no slot map needed: stale ids simply resolve to nothing). `hash()` and a versioned
  `serialize` / `deserialize` blob bound to world seed and site are tested; writing it to a settlement save
  file is Phase 4B.7.
- **Validation** returns the most severe `PlacementError`. Hard: outside the map, wrong material, occupied,
  water, too steep (largest elevation difference to the 4 neighbours for floors, across the edge for walls;
  computed on demand from the elevation layer, no slope layer is stored). Clearable (least severe): a standing
  tree, or a shrub / bush / fallen log / rock; grass, ferns and flowers are trampled and forage or minerals never
  block. A door may replace a wall; floors and walls never conflict.
- **Clearing** goes through `ResourceState` (felling yields wood, `clearUnderstory` records cleared objects; its
  blob is version 2 and still reads version 1), so views, sprites and the inspector show stumps and cleared
  ground. Clearing bumps the resource revision, which rebuilds the view texture like a test harvest.
- **Blueprints and orders** (4B.2): a `ConstructionOrder` groups the structures planned together (one house).
  Its stage is not stored but derived as the least advanced stage of its structures, so it cannot drift. Plans
  are ghosts: they hold their edges and tiles (conflicting with other placements) but change neither the map
  nor `ResourceState`; they may sit on trees and shrubs, which are cleared when a structure begins
  construction (a natural "clear the site" task for Phase 4C workers). Steps move only structures in the
  expected stage, so repeating one is a no-op: `confirmOrder` (Planned → AwaitingMaterials), `debugDeliver`
  (→ ReadyToBuild; replaced by stockpile delivery in 4B.5), `beginConstruction` (→ UnderConstruction, clears
  the site), `addWork(units)` (integer builder work up to `workRequired` → Completed), `debugCompleteOrder`.
  A fully completed order closes; `cancelOrder` removes unfinished structures and keeps completed ones.
  Unfinished orders can be edited (new plans join them; removing a planned part drops it from its order); a
  planned door may replace only an unfinished wall (replacing finished work is renovation, 4B.6).
- **Rooms** (4B.3, `Rooms.h`): a room is the tiles connected without crossing a completed wall or door, at most
  `maxRoomTiles` (1024, 4096 m²); anything larger, or reaching the map border, is open ground. Planned and
  unfinished parts are openings; a completed door seals a room (it stays a passage for later pathfinding).
  `RoomIndex` is derived data owned by `ConstructionState` and current after every public edit: a completed
  wall or door that appears or vanishes marks its two tiles, and the update takes apart only the rooms holding
  those tiles and floods again from them. Floods stop at the size limit, and an escaped flood marks its tiles
  as open ground for the rest of the update, so an edit costs O(maxRoomTiles · log n) whatever the map size
  (a wall in open ground: ~0.3 ms Release, ~3 ms Debug; a whole order completed in one update: ~0.35 ms).
  Batch operations (`removeInRect`, `debugCompleteOrder`) update once; tool drags still update per part.
- **Identity**: a re-detected room takes the id of the old room it overlaps most (largest overlaps first, ties
  to the lower id, each id once), so after a split the larger part keeps the id and the smaller one is new but
  inherits the roof settings, and after a merge the larger room survives. An extension keeps the room's
  identity when it is walled in before the old wall comes down (removing the wall first opens the room).
- **Buildings**: rooms joined by completed doors (one dwelling); rooms that only share a wall, like terraced
  houses, are separate buildings. Building ids pass on the same way, weighted by room tiles.
- **Roofs** are inferred per room, never placed tile by tile. `RoofMode::Auto` roofs a room when every tile is
  within `maxRoofSupportDistance` (3) steps of a tile along a wall (doors carry nothing): rooms up to 8 tiles
  (16 m) across. `Enabled` always roofs (the span check only guides inference), `Disabled` never does. The
  material is one of `roofMaterials()` (thatch by default, timber). Settings are player data and survive
  renovations. A roof is paid for (4B.5): `roofBuiltTiles` counts the tiles built, `roofComplete()` needs all
  of them, and `buildRoof` takes the missing tiles' cost from unreserved stockpile goods (a developer stand-in
  for roofers; no stages of its own). Built tiles pass to re-detected rooms in proportion to their overlap, so
  an extension needs roofing only over its new tiles.
- **Furniture** (4B.4, `Furniture.h`): `StructureKind::Furniture` structures share orders, stages, costs and
  removal with walls (templates in 4B.6 can therefore mix them freely). They sit on a sparse grid of 0.5 m cells
  (`kCellsPerTile` = 4 on 2 m tiles) with a `FurnitureKind`, `Facing` (east / west swap the footprint) and the
  north-west cell in `Structure::tile`; no cell layer exists, only an index from each touched tile to its pieces.
  Pieces never overlap, never cross a wall or door of any stage (and walls never cut through them), stand on
  ground like floors, and stay off stockpiles. The catalogue (bedroll, bed, double bed, table, stool, chest,
  shelf, hearth, workbench, altar) gives each piece a footprint, materials and what it contributes: sleepers,
  seats, work places, storage, comfort.
- **Room functions** (4B.4, `RoomFunctions.h`): the player designates a room (`Room::function`, persisted and
  inherited through renovations like roofs); furniture never changes it. A data table, after RimWorld
  (furniture makes the room), Songs of Syx (rooms as workplaces) and Civilization (each building yields one
  service), lists required furniture, the `RoomService` it yields (housing, storage, food, production, amenity,
  faith, health), housing kind (private bedroom, communal dormitory), sleeper limit and base privacy:
  Bedroom, Dormitory, Storeroom, Kitchen, Workshop, Dining hall, Shrine, Infirmary. `evaluateRoom` computes on
  demand (no cache) whether it works, what is missing, a suggested function, capacities, and separately
  privacy, furnishing, wall condition and a comfort potential from space, furnishing, privacy and roof.
  `summarizeBuilding` names a building from its rooms (House, Dormitory, Storehouse, ...) and counts its
  private and communal beds. No households or occupants yet (Phase 4C).
- **Materials** (4B.5, `Goods.h`, `ConstructionCosts.h`, `Stockpiles.h`): goods (wood, stone, clay, thatch,
  food) are whole kilograms whose arithmetic never goes below zero. `ConstructionCosts` is plain data (per
  kind and material, per furniture piece and material, per roof tile) passed to `ConstructionState`; direct
  developer building is free. Goods exist only in stockpiles: tile rectangles on open ground, owned by
  `ConstructionState` so reservations, deliveries and refunds stay consistent with orders in one place (they may
  move to a settlement-level owner once population systems use them). A confirmed order reserves what its
  parts awaiting materials still need (`allocateMaterials`, nearest stockpiles first); reserved goods are
  unavailable to other orders and to roofs. `debugDeliver` (hauling stand-in) takes the reservations out of
  storage into the order's `onSite` pool and makes each part whose cost is on site ReadyToBuild, in placement
  order; repeating it moves nothing. A removed part that was supplied returns its cost to the pool; cancelling,
  or an order closing with leftovers, returns the pool to the stockpile it came from and releases its
  reservations. Removing a stockpile drops its goods and reservations. A founding stockpile (3 x 3, nearest
  open ground to the map centre) holds `defaultFoundingGoods()`.
- **Persistence**: blob version 4 (versions 1-3 still load). Per room it stores the id, building id, anchor
  (first tile), roof settings, function and built roof tiles; tiles are not saved. Loading detects rooms from
  the structures again and maps the saved ids onto them, rejecting a blob whose rooms do not match. Structures
  add furniture kind and facing, orders their goods on site and source, and stockpiles and reservations close
  the blob (a reservation larger than its stock is rejected).
- **Drawing**: structures are drawn per frame for the visible rectangle (`collectStructureQuads`, filled
  rectangles in tile units), so edits never rebuild the 3072² texture. Unfinished stages fade towards
  blueprint blue. The room overlay (roofed rooms in their translucent roof colour, or each room in its own
  colour) is drawn as row runs between floors and walls. `ConstructionTool` (SDL-free, tested) turns mouse
  positions into snapped corners / edges / tile rectangles / furniture cells (the footprint centred on the
  cursor), previews each part with its error and commits only the valid parts. Furniture is drawn as its body
  plus one detail per kind (pillow, fire, altar top); stockpiles as a framed area with a heap per stored good;
  unbuilt roofs faintly in blueprint blue.
- **HUD** (`SettlementHud.h`, SDL-free): a resource bar (free goods, beds, rooms, buildings; totals recomputed
  only when the construction revision changes), a bottom bar (Structure, Furniture, Stockpile, Remove,
  blueprint / instant, overlay), build menus with costs and room uses in tooltips, and panels for the selected
  room (designation, status, roof), blueprint (cost, reserved, on site, missing, lifecycle steps) and stockpile.
  It only returns `HudCommand`s; the app carries them out exactly like the matching keys, and ignores map clicks
  the HUD takes. Text is the 8 x 8 debug font for now.

## World ownership

`World` owns **data**: `WorldConfig`, the world seed, persistent layers, and world-level entities
(regions, rivers, lakes; later settlements, kingdoms, armies, trade routes, resource deposits).

`World` does **not** own: generators, renderers, debug views, pathfinding, simulation systems, RNG state,
temporary generation buffers, or local settlement maps. Those systems receive a `World` and operate on it.

Rule of thumb: if saving and reloading the world requires the data, `World` owns it.

## Storage

- **Struct-of-arrays layers** (`Layer<T>`), grouped into `TerrainData`, `ClimateData`, `HydrologyData`,
  `GeographyData`, `ResourceData` (and `CivilizationData` later). `std::vector<WorldTile>` is never the canonical
  storage.
- `TileView` is a lightweight accessor (`world.tile(coord)`) that owns nothing.
- A static layer descriptor table (name, type, units, hash) lets debug views, hashing and saving enumerate layers.
- **No persistent layer exists before the pass that produces it.**
- Persistent layers use the smallest sensible integer representation (see WORLD_GENERATION.md).
- Saving (`world/WorldIO`) writes config, seed, every present layer (by `LayerId`) and the entity lists, followed
  by the world hash; loading validates all of it and returns errors for bad files. Entity serialization is shared
  with the world hash, so anything saved is also hashed.

## IDs and entities

- Strongly typed IDs (`StrongId<Tag>`, `uint32_t`), `0` = invalid. Persistent references use IDs, not pointers.
- Generation-created entities are stored in dense vectors indexed by ID (IDs never reused).
  Generational slot maps will be introduced for entities created/destroyed during play when profiling shows
  the need; construction (Phase 4B) keeps structures in an ordered map keyed by never-reused ids instead.

## Errors and assertions

- Programmer errors (broken invariants, invalid coordinates passed internally) -> `OLAM_ASSERT` (Debug builds).
- External/runtime failures (bad config, command line, files) -> returned errors, never asserts.
- `Layer<T>::at()` is bounds-checked in Debug; `operator[]` is unchecked for hot, validated loops.
- Coordinates are never silently clamped unless an algorithm explicitly requires it.

## Scale targets (design targets, not limits)

- Detailed (individually simulated) settlement: ~50,000 citizens; stretch ~100,000+.
- Total world population: ~5-20 million, mostly aggregated.
- Pre-game history: none in V0.1; later ~200-500 years of generated history.

## Applications

A single `Olam` executable for now (world viewer). Split into `olam_game` and `olam_world_viewer` once gameplay
and world tooling diverge.
