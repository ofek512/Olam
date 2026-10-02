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

- [ ] Infrastructure: noise, math helpers, generation settings, layer descriptors, view framework
- [ ] 3A Tectonics + elevation (plates, rock type)
- [ ] 3B Ocean (+ distance to ocean)
- [ ] 3C Temperature
- [ ] 3D Rainfall / moisture
- [ ] 3E Hydrology (rivers, lakes)
- [ ] Soil
- [ ] 3F Biomes
- [ ] 3G Fertility
- [ ] 3H Vegetation
- [ ] 3I Resources
- [ ] Debug views, tile inspector with real data, generation statistics
- [ ] Save / load generated world
- [ ] Golden hashes, performance check (2048² ≤ ~3 s Release)

## Phase 4 — Civilization foundations (= Milestone 2)

- [ ] 4A Regions
- [ ] 4B Settlement suitability
- [ ] 4C Settlement placement
- [ ] 4D Roads
- [ ] 4E Initial factions / political seeds
