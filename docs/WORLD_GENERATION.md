# World Generation

WorldGen-specific decisions. Project-wide architecture is in [ARCHITECTURE.md](ARCHITECTURE.md).

Goal for V0.1: **plausible geography with understandable causes**, not an Earth simulation.
Prefer causal relationships (mountains -> rain shadow -> dry interior; river valley -> water + soil -> settlement)
over independent random layers. Lite plate tectonics only; no ocean currents, long erosion or glaciation in V0.1.

Phase 3 (= Milestone 1, "what natural world exists?") ends at resources plus complete debug tooling.
Regions, settlement suitability, settlements, roads and factions belong to Phase 4.

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

| Pass            | Produces                                                         |
| --------------- | ---------------------------------------------------------------- |
| TectonicsPass   | `plateId`, `rockType`                                            |
| ElevationPass   | `elevation`                                                      |
| OceanPass       | `surfaceWater`, `distanceToOcean`                                |
| TemperaturePass | `meanAnnualTemperature`                                          |
| RainfallPass    | `annualRainfall`, `moisture`                                     |
| HydrologyPass   | `flowDirection`, `discharge`, `riverId`, `lakeId`, rivers, lakes |
| SoilPass        | `soil`                                                           |
| BiomePass       | `biome`                                                          |
| FertilityPass   | `fertility`                                                      |
| VegetationPass  | `vegetation`, `treeCover`                                        |
| ResourcePass    | `depositId`, deposits                                            |

All tuning values live in `WorldConfig::generation` (`WorldGenSettings`), have documented defaults, are
validated and are part of the world hash. Working layers may be shared between passes within one run
(e.g. plate boundary distance, floodplain strength) and are discarded afterwards.

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
| Plate id                         | —                | `uint8_t`                                         |
| Rock type                        | —                | `RockType` (`uint8_t`)                            |
| Elevation (ground / lake bed)    | normalised float | `int16_t` metres, sea level = 0 m (~-4500..+4500) |
| Surface water                    | —                | `SurfaceWater` (`uint8_t`: Land, Ocean, Lake)     |
| Distance to ocean                | —                | `uint16_t` km                                     |
| Mean annual temperature          | float            | `int16_t` tenths of °C (183 = 18.3 °C)            |
| Annual rainfall                  | float            | `uint16_t` mm/year                                |
| Moisture (derived, not rainfall) | float            | `uint8_t` 0-255                                   |
| Flow direction (D8)              | —                | `uint8_t` (`Direction8`, 255 = none)              |
| Discharge                        | double           | `uint32_t` hundredths of m³/s                     |
| River / lake / deposit           | —                | `RiverId` / `LakeId` / `DepositId` (`uint32_t`)   |
| Soil, biome, vegetation          | —                | `uint8_t` enums                                   |
| Fertility, tree cover            | float            | `uint8_t` 0-255 / 0-100 %                         |

Slope, terrain class, agricultural suitability (grain, livestock, orchard) and biological yields (wood, game,
fish) are **derived on demand** from stored layers (`world/queries`), not stored.

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

## Pass designs (Phase 3)

### Tectonics + elevation

- Own 2D gradient (Perlin-style) noise with fBm and ridged variants; gradients come from `coordinateHash`.
- ~16 Voronoi plates (domain-warped boundaries) placed with PCG; each is continental or oceanic and drifts.
  Converging boundaries raise mountain ranges / volcanic arcs, diverging boundaries form rifts and seas.
- `rockType` from plate context: sedimentary (basins, lowlands), igneous (arcs, rifts, ocean floor),
  metamorphic (old mountain belts, shields).
- Raw height = plate base + boundary uplift/rifting + noise detail, then a few iterations of thermal smoothing.
- An Earth-like hypsometric curve is applied via a histogram of raw heights; this also places sea level so the
  land share equals a configurable target ± a seed-dependent variation. Tiny islands are removed. Land may touch
  the map edges.

### Ocean

- Ocean = below-sea tiles connected to the map edge, or below-sea bodies large enough to be inland seas.
  Smaller isolated depressions stay land and are handled by hydrology.
- `distanceToOcean` via a chamfer distance transform.

### Temperature

- Piecewise-linear Earth-like latitude table, −6.5 °C per 1000 m above sea level, slightly colder
  interiors (distance to ocean), small low-frequency noise. Computed for every tile including ocean.

### Rainfall and moisture

- Prevailing winds by latitude band: easterlies below 30°, westerlies 30-60°, polar easterlies above 60°.
- Moisture is swept along the wind: picked up over ocean, rained out over land, extra rain on windward slopes
  (orographic), rain shadow behind mountains. Multiplied by a latitude rain-belt table (wet equator, dry ~30°,
  wet 50-60°, dry poles), lightly blurred across rows.
- `moisture` = aridity index = rainfall ÷ potential evaporation(temperature).

### Hydrology

- Priority-flood (ties broken by insertion order) from outlets (ocean-adjacent and map-edge tiles) over the true
  elevation finds depressions; the map edge drains like the sea.
- Filled depressions become lakes when large or deep enough; every lake spills over its lowest rim.
- Flow directions come from a second priority-flood over the filled surface plus low-amplitude noise (routing
  only, elevation is unchanged), so channels meander on smooth slopes instead of following grid lines.
- Discharge = runoff accumulated downstream. Runoff = rainfall − actual evapotranspiration (Turc-Pike:
  `AET = P / sqrt(1 + (P/PET)²)`).
- Tiles with discharge above a threshold carry a river; classes stream / river / major river (defaults 10 / 50 /
  300 m³/s, scaled to regional catchments). One `River` per main stem: at a confluence the smaller river ends as
  a tributary; ids are ordered by mouth discharge. No valley carving in V0.1.
- A floodplain working layer (strength by distance to the river, width growing with √discharge, fading with
  height above the river) is left for soil and fertility.

### Soil, biome, fertility, vegetation

- Soil (rocky, sandy, loam, clay, alluvial, peat, permafrost, laterite) from rock type, slope, climate and
  floodplains. Rules in priority order: permafrost (cold) -> rocky (steep / high / steep hard rock) -> alluvial
  (strong floodplain) -> peat (wet, flat, cool) -> laterite (hot and humid) -> sandy (arid) -> texture
  (sedimentary / wet -> clay, igneous / dry -> sand, plus regional noise) -> loam.
- Biome from a Whittaker-style temperature × moisture table (14 biomes); alpine by temperature/tree line,
  wetland on flat, very wet land near rivers and lakes (or on peat). Water is not a biome. The climate table
  (`climateBiome`) uses mean annual °C and the aridity index: ice ≤ −10 °C, tundra < −5 °C, deserts AI < 0.2,
  then cold / temperate / subtropical / tropical bands split by AI into grassland, shrubland, savanna and forests.
- Fertility = product of climate, terrain, water access and soil factors, with a floodplain bonus scaled by
  river discharge.
- Vegetation category + tree cover from biome, moisture, fertility, slope and small noise.

### Resources

- Minerals (iron, copper, tin, coal, gold, silver, stone, clay, salt): a probability field per mineral
  (geology × terrain × regional noise) is sampled with PCG and minimum spacing into `Deposit` clusters with a
  richness. Wood, game and fish are derived from existing layers.

## Statistics, saving and tests

- After generation a summary is logged (land/ocean/lake share, biomes, rivers, lakes, deposits) and shown in the
  viewer stats panel (I).
- Worlds are saved in an own versioned, uncompressed binary format (`.olamworld`): header, config, seed, layers
  via the descriptor table, entities, trailing world hash verified on load.
- Tests: invariants per pass, determinism, pinned golden hashes for a small world, save/load round trip.
- Budget: a 2048² world (all passes) should generate in about 3 s in a Release build.

## Debug viewer

- One texture per view, 1 texel = 1 tile, nearest filtering, rebuilt only when the world or view changes.
- Views: Tab / Shift+Tab cycle; F1 Terrain (atlas tints, H toggles hillshading), F2 Elevation,
  F3 Temperature, F4 Rainfall, F5 Moisture, F6 Biome, F7 Hydrology, F8 Soil, F9 Fertility, F10 Vegetation,
  F11 Resources, F12 Geology/Plates; Tab-only: distance to ocean, tree cover, hash debug.
- Rivers are baked into the textures and drawn as lines (width by class) when zoomed in.
- Tile inspector shows only fields of layers that actually exist; left-click pins a tile.
