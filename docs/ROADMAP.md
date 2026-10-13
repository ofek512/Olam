# Roadmap

Short status checklist. Full vision and rationale: [agent.MD](../agent.MD).

## Phase 0 — Engine skeleton ✅

- [x] CMake + SDL3, application loop, timing (frame vs simulation ticks), input, logging, files, camera
- [x] Debug overlay, unit tests

## Phase 1 — World representation ✅

- [x] Architecture and world-generation decisions documented
- [x] Build restructure (headless option, determinism flags, per-library tests, `src/app`)
- [x] Core primitives: assert, `StrongId`, `Layer<T>`, xxHash64
- [x] World model: `WorldConfig`, `WorldCoord`, `World`, `TileView`, layer descriptors, world hash

## Phase 2 — Deterministic random generation ✅

- [x] SplitMix64, PCG32, coordinate hash, seed derivation, text seeds
- [x] WorldGen pass skeleton (`WorldGenContext`, `WorldGenerationPass`, `WorldGenerator`)
- [x] Viewer: world texture, hash debug view, tile inspector, seed controls (R / N, `--seed`, `--size`)
- [x] CI: Linux GCC + Clang headless, Windows MSVC full (first run pending push)

## Phase 3 — Natural world generation (= Milestone 1: procedural natural world viewer)

- [x] Infrastructure: noise, math helpers, generation settings, layer descriptors, view framework
- [x] 3A Tectonics + elevation (plates, rock type)
- [x] 3B Ocean (+ distance to ocean)
- [x] 3C Temperature
- [x] 3D Rainfall / moisture
- [x] 3E Hydrology (rivers, lakes)
- [x] Soil
- [x] 3F Biomes
- [x] 3G Fertility
- [x] 3H Vegetation
- [x] 3I Resources
- [x] Debug views, tile inspector with real data, generation statistics
- [x] Save / load generated world
- [x] Golden hashes, performance check (2048² ≤ ~3 s Release)
- [x] 3J Geology & ore genesis: ancient orogens, geological provinces, deposit origins (veins, placers, bedded,
      evaporites, salt pans, bog iron), local stone / clay, natural map colours

## Phase 3.5 — World generation refinement

- [x] 3.5A Landmass analysis (landmasses, size classes, coastline, structure; `olam_generate --landmass-survey`)
- [x] 3.5B Macro landmass variety (continental plates grouped into 1-7 continents with ocean gaps)
- [x] 3.5C Watersheds / drainage basins
- [x] 3.5D River order, width class, navigability

## Phase 4 — First playable settlement

- [x] 4A World → local map (`olam_settlement`; Enter founds a settlement, M returns to the world map)
- [ ] 4A.5 Local ecology, natural resources and visual refinement
  - [x] 4A.5A Mineral accessibility (minor iron occurrences; `olam_generate --mineral-survey`)
  - [x] 4A.5B Hybrid visual rendering (zoom levels, sprite atlas, batched sprites; `olam_generate --local-zoom`)
  - [x] 4A.5C Forest ecology and vegetation diversity (tree kinds, stand structure, understory)
  - [x] 4A.5D Springs, brooks and small water features (brooks, seasonal beds, marshes, wet hollows, springs)
  - [x] 4A.5E Foraging resources (berries, mushrooms, greens, herbs, deadwood, reeds; sparse `ForageData`)
  - [x] 4A.5F Resource inspection and test harvesting (`ResourceState`, G / Shift+G / Y, Vegetation / Forage /
        Minerals views; river courses reworked)
  - [ ] 4A.5G Testing, performance and balancing
- [ ] 4B First buildings (plan: [4BPlan.md](../4BPlan.md))
  - [x] 4B.1 Construction grid and structural placement (edge walls, doors, floors; `ConstructionState`,
        `ConstructionTool`; B build mode, 1-4 tools, Q/E material, R flip door, Shift clears obstructions)
  - [x] 4B.2 Blueprints and construction orders (`ConstructionOrder`, derived order stage, debug deliver /
        complete; I instant build, Ctrl+LMB select order, Enter confirm, V deliver, C complete, X cancel)
- [ ] 4C First population
- [ ] 4D First economy
- [ ] 4E Immigration and growth
- [ ] 4F Standard of living V0

## Phase 5 — Deepen local gameplay

- [ ] 5A Population structure

## Phase 6 — Civilization foundations (= Milestone 2)

- [ ] 6A Regions
- [ ] 6B Settlement suitability
- [ ] 6C Generated settlements

## Later phases

- Phase 7 — Roads and trade
- Phase 8 — City states and kingdoms
- Phase 9 — Diplomacy and war
