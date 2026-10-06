# Project Coding Rules

Full vision: [agent.MD](agent.MD). Architecture: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
World generation: [docs/WORLD_GENERATION.md](docs/WORLD_GENERATION.md). Status: [docs/ROADMAP.md](docs/ROADMAP.md).

1. Simulation code must never depend on rendering code.
2. World generation must always be deterministic.
3. Never use global random state.
4. Do not introduce new dependencies without approval.
5. Do not redesign unrelated systems during a task.
6. Keep changes scoped to the requested milestone.
7. Compile after significant changes.
8. Run relevant tests after changes.
9. Every new WorldGen stage should include tests.
10. Do not replace working systems simply because another architecture is fashionable.
11. Prefer simple implementations first.
12. Do not optimize without evidence from profiling.
13. Do not hard-code gameplay rules into rendering systems.
14. Do not use external images without verifying their license.
15. Document major architectural decisions.
16. Ask for architectural guidance when a requested change conflicts with documented architecture.

## World & determinism rules

- `olam_core`, `olam_world` and `olam_worldgen` never depend on SDL (directly or transitively).
- World data is stored as struct-of-arrays `Layer<T>`; never `std::vector<WorldTile>` as canonical storage.
- No persistent world layer before the generation pass that produces it exists.
- `World` owns data only; generators, renderers, simulation systems and scratch buffers live elsewhere.
- No `std::` random engines/distributions, `std::hash` or `rand()` in deterministic code. Use `core/random`.
- Derive subsystem seeds with `deriveSeed(seed, fixedId)`; never share RNG state between subsystems.
- No fast-math, no float equality, no unordered reductions, no libm transcendental functions in generation.
- Programmer errors -> `OLAM_ASSERT`; bad external input (config, files, command line) -> returned errors.
- World and local settlement maps are separate grids and types.

## Layering

`platform/` (SDL3) -> `core/` + `render/` -> `world/` -> `worldgen/` -> `tools/`, `app/`.
Lower layers never include headers from higher layers.

- `olam_core`: pure C++20 (logging, time, input, files, math, camera, assert, IDs, `Layer<T>`, hash, random).
- `olam_world`: world data model. `olam_worldgen`: generation pipeline.
- `olam_engine`: SDL platform, renderer, textures, debug text panels.
- `olam_world_viewer`: world debug renderer, tile inspector. `Olam` (`src/app`): thin executable.
- `olam_settlement` (depends on `olam_world` only, no SDL, no worldgen): local settlement maps.
  `olam_settlement_views` (no SDL): local colouring + inspector. `olam_settlement_viewer` (SDL): local renderer.

## Build & test (Windows, MSVC)

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\Olam.exe --seed 12345 --size 1024x512
.\build\Debug\Olam.exe --load saves\12345.olamworld   # Ctrl+S saves, Ctrl+L reloads
# Enter founds a settlement on the pinned/hovered tile (local map), M switches world <-> local map
.\build\Release\olam_generate.exe --seed 12345 --size 2048x2048 --local river --png build\local.png
# --local <x,y | river|coast|lake|forest|plain|mountain|dry>, --local-crop <x>,<y> for a full-res 768^2 crop
```

Headless (no SDL, as in Linux CI): `cmake -S . -B build-headless -DOLAM_BUILD_ENGINE=OFF`.
