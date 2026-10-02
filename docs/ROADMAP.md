# Roadmap

Short status checklist. Full vision and rationale: [agent.MD](../agent.MD).

## Phase 0 — Engine skeleton ✅
- [x] CMake + SDL3, application loop, timing (frame vs simulation ticks), input, logging, files, camera
- [x] Debug overlay, unit tests

## Phase 1 — World representation
- [x] Architecture and world-generation decisions documented
- [ ] Build restructure (headless option, determinism flags, per-library tests, `src/app`)
- [ ] Core primitives: assert, `StrongId`, `Layer<T>`, xxHash64
- [ ] World model: `WorldConfig`, `WorldCoord`, `World`, `TileView`, layer descriptors, world hash

## Phase 2 — Deterministic random generation
- [ ] SplitMix64, PCG32, coordinate hash, seed derivation, text seeds
- [ ] WorldGen pass skeleton (`WorldGenContext`, `WorldGenerationPass`, `WorldGenerator`)
- [ ] Viewer: world texture, hash debug view, tile inspector, seed controls (R / N, `--seed`, `--size`)
- [ ] CI: Linux GCC + Clang headless, Windows MSVC full

## Milestone 1 — Procedural world viewer
- [ ] Elevation
- [ ] Ocean
- [ ] Temperature
- [ ] Rainfall / moisture
- [ ] Hydrology (rivers, lakes)
- [ ] Biomes
- [ ] Fertility
- [ ] Vegetation
- [ ] Resources
- [ ] Debug map modes
- [ ] Tile inspector with real data
- [ ] Save generated world

## Milestone 2 — Geographic civilization foundations
- [ ] Regions, settlement suitability, settlements, roads, initial factions
