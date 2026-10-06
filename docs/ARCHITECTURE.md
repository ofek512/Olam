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
  math, camera, assertions, strong IDs, `Layer<T>`, hashing, deterministic RNG.
- `olam_world`: strategic world data model. No SDL, no rendering, no generation algorithms.
- `olam_worldgen`: generation pipeline and passes. No SDL.
- `olam_engine`: SDL platform layer, renderer, textures, debug text panels.
- `olam_world_viewer`: world debug visualization and tile inspector.
- `olam_settlement`: local settlement maps (`SettlementMap`, site checks, `generateLocalMap`). Depends on
  `olam_world` only: it reads a generated `World`, never runs world generation, and has no SDL.
- `olam_settlement_views`: SDL-free local map colouring and tile inspector (viewer and headless PNG output).
- `olam_settlement_viewer`: SDL texture renderer for local maps.
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
  fertility, ground cover, resource and a per-tile tree index. Trees are individual entities in a compact list.
- Sites: any land tile far enough from the world edge (`siteError`); harsh sites only produce warnings
  (`siteWarnings`). The app holds one active settlement; founding elsewhere replaces it, a new world drops it.
- Generation details: [WORLD_GENERATION.md](WORLD_GENERATION.md#local-maps-phase-4a).

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
  Generational slot maps will be introduced for entities created/destroyed during play.

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
