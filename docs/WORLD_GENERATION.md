# World Generation

WorldGen-specific decisions. Project-wide architecture is in [ARCHITECTURE.md](ARCHITECTURE.md).

Goal for V0.1: **plausible geography with understandable causes**, not an Earth simulation.
Prefer causal relationships (mountains -> rain shadow -> dry interior; river valley -> water + soil -> settlement)
over independent random layers. No plate tectonics, ocean currents, long erosion or glaciation in V0.1.

## Pipeline

- `WorldGenerator` runs an ordered list of `WorldGenerationPass`es, always the full pipeline.
- Each pass has a `name()`, a fixed `seedId()`, `validatePreconditions()` and `run(WorldGenContext&)`.
- `WorldGenContext` gives the pass the `World` being built, its derived seed and temporary float working layers.
  Working layers are discarded when generation ends.
- Generation is **synchronous and serial** in V0.1. Loops are written parallel-ready
  (`output[i] = f(inputs[i])`, no hidden shared mutable state). Later, after profiling:
  whole-world generation on a worker thread (swap the finished `World` on the main thread), then an own
  `parallel_for`. `std::execution::par` is not used (needs TBB on GCC, incomplete in libc++).
- Each pass adds its persistent layer(s) when it is implemented; no placeholder layers.

| Pass            | Produces                                           |
| --------------- | -------------------------------------------------- |
| ElevationPass   | `elevation`                                        |
| OceanPass       | `surfaceWater`                                     |
| TemperaturePass | `meanAnnualTemperature`                            |
| RainfallPass    | `annualRainfall`, `moisture`                       |
| HydrologyPass   | `flowDirection`, `flowAccumulation`, rivers, lakes |
| BiomePass       | `biome`                                            |
| FertilityPass   | `fertility`                                        |
| RegionPass      | `region`                                           |

## Determinism

Same seed + same config must produce a bit-identical world. Code is written **as if results must be identical
across MSVC, GCC and Clang**; cross-platform hash equality will be enforced in CI later.

- No global RNG state. No `std::` random engines/distributions, no `std::hash`, no `rand()`.
- RNG (all in `olam_core`, bit-exact, covered by fixed-output tests):
  - **SplitMix64**: seed mixing and derivation, RNG initialisation.
  - **PCG32**: stateful streams for intentionally sequential decisions (sampling, shuffles, weighted choice).
  - **Coordinate hash** `coordinateHash(seed, x, y)`: stateless per-tile randomness, order-independent.
- Subsystem seeds: `deriveSeed(seed, id) = splitMix64(seed ^ id)` with fixed 64-bit constant IDs
  (ASCII-packed names, e.g. `"TERRAIN"`). Derivation may be hierarchical (`resourceSeed -> ironSeed`).
  Changing one subsystem must never change another.
- **Hashing**: own xxHash64. World hash = per-layer hashes (little-endian element bytes, independent of padding)
  combined with config and seed.
- **Seeds**: `uint64_t`. Text seeds: a valid decimal `uint64` is used as-is; anything else is xxHash64 of its
  UTF-8 bytes. Non-deterministic sources (`std::random_device`) may only _pick_ a seed, in the app layer.

### Float-math policy

- `float` for normal generation math; `double` accumulators for large sums.
- Allowed in generation: `+ - * /`, `sqrt`, `floor`, comparisons. Avoid libm transcendental functions
  (`sin`, `exp`, `pow`, ...) in generation; use own deterministic approximations if needed.
- Compiled with `/fp:precise` (MSVC) or `-ffp-contract=off` (GCC/Clang). Never fast-math.
- No float equality checks, no floats for IDs/coordinates/enums/counters.
- No unordered reductions; deterministic iteration order everywhere.
- Normalised `0..1` floats during generation; **clamp then quantize once** at the end of the pass.

## Units and storage

| Field                            | Generation       | Persistent                                        |
| -------------------------------- | ---------------- | ------------------------------------------------- |
| Elevation (ground / lake bed)    | normalised float | `int16_t` metres, sea level = 0 m (~-4500..+4500) |
| Mean annual temperature          | float            | `int16_t` tenths of °C (183 = 18.3 °C)            |
| Annual rainfall                  | float            | `uint16_t` mm/year                                |
| Moisture (derived, not rainfall) | float            | `uint8_t` 0-255                                   |
| Fertility                        | float            | `uint8_t` 0-255                                   |
| Biome                            | —                | `uint8_t` id                                      |
| Flow direction (D8)              | —                | `uint8_t`                                         |
| Flow accumulation                | —                | `uint32_t`                                        |
| Region                           | —                | `RegionId` (`uint32_t`)                           |

Terrain height is continuous. Categories (plains, hills, mountains) are **derived** from elevation, slope and
relative relief, never stored as the height model.

## Latitude and climate

- The map is a section of a larger planet. `WorldConfig::latitudeNorth` / `latitudeSouth`
  (default 60° N -> 30° N); `latitude(y) = lerp(north, south, (y + 0.5) / height)`.
- V0.1 climate is **annual means only** (`meanAnnualTemperature`, `annualRainfall`, `moisture`).
  Seasonal fields (winter/summer temperature, growing season, seasonal rainfall) can be added later.

## Water

- `surfaceWater` layer: `Land`, `Ocean`, `Lake` (standing water). Rivers are **not** part of this enum:
  a tile can be `Land` and carry a river.
- Global sea level = 0 m. Ocean candidates have elevation below sea level; the persistent classification is stored.
- Elevation is the ground / lake-bed; `waterDepth = waterSurfaceElevation - terrainElevation`.
- **Rivers**: D8 `flowDirection` + `flowAccumulation` per tile; tiles above a configurable threshold carry a river.
  `River` entities (id, source, mouth, path, discharge, tributaries) are derived after hydrology. No edge rivers,
  no randomly drawn rivers.
- **Lakes** (simple in V0.1): depressions are filled to an outlet or, if large/deep enough, become lakes with a
  water surface elevation, tiles, inflows and an optional outflow. Endorheic lakes later. A stable drainage
  network matters more than lake realism.

## Persistent vs temporary

Kept in `World`: elevation, climate, biomes, fertility, hydrology, rivers, lakes, regions, resources.
Discarded after generation: noise buffers, erosion scratch data, flood-fill queues, masks, distance fields and
other intermediate layers.

## Debug viewer

- One texture per view, 1 texel = 1 tile, nearest filtering, rebuilt only when the world or view changes.
- Until `ElevationPass` exists the viewer shows a grayscale hash of `(seed, x, y)` computed on demand
  (never stored in `World`).
- Tile inspector shows only fields of layers that actually exist.
