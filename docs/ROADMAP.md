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

## Phase 4 — Civilization foundations (= Milestone 2)

- [ ] 4A Regions
- [ ] 4B Settlement suitability
- [ ] 4C Settlement placement
- [ ] 4D Roads
- [ ] 4E Initial factions / political seeds
