# 2D World Simulation Engine — Development Roadmap

## 1. Project Goal

The goal of this project is to build a purpose-built 2D game engine for a large-scale world simulation game.

The game is inspired by games such as Songs of Syx in terms of:

- Large populations
- Settlement building
- Resource gathering
- Production chains
- Cities
- Kingdoms
- Factions
- Trade
- Migration
- Warfare
- Long-term world development
- Emergent simulation

The game is NOT intended to be a fantasy race simulator.

The world's population will primarily consist of humans.

Differences between populations should emerge from systems such as:

- Culture
- Language
- Religion
- Geography
- Economy
- Social class
- Political affiliation
- Kingdom
- City
- Family
- Profession
- Wealth
- Education
- Military service
- Historical events

The engine should be designed specifically around this type of simulation.

It is NOT intended to compete with general-purpose engines such as:

- Unity
- Unreal Engine
- Godot

Instead, it should contain only the systems needed by our game.

---

# 2. Core Design Philosophy

The most important architectural principle is:

> The simulation is the engine.

Graphics should represent simulation state.

Graphics should NOT define simulation state.

For example:

BAD:

```cpp
if (tile.texture == grassTexture)
{
    fertility += 10;
}
```

GOOD:

```cpp
if (tile.biome == Biome::TemperateGrassland)
{
    fertility += 10;
}
```

The renderer may later decide that:

```text
TemperateGrassland
```

is displayed using:

```text
grass_01.png
grass_02.png
grass_03.png
grass_flowers_01.png
```

But the simulation itself does not care.

---

# 3. Major Architectural Layers

The project should roughly follow this structure:

```text
Operating System
       │
       ▼
SDL / Platform Layer
       │
       ▼
Core Engine
       │
       ├── Rendering
       ├── Input
       ├── Camera
       ├── Files
       ├── Logging
       ├── Debugging
       └── Timing
       │
       ▼
World Simulation
       │
       ├── World Generation
       ├── Geography
       ├── Climate
       ├── Hydrology
       ├── Resources
       ├── Regions
       ├── Settlements
       ├── Population
       ├── Economy
       ├── Politics
       └── Warfare
       │
       ▼
Game Systems
       │
       ├── Player Interaction
       ├── Construction
       ├── Management
       ├── UI
       ├── Orders
       └── Gameplay Rules
```

The lower layers should NEVER depend on higher layers.

For example:

```text
World Generation
```

must not depend on:

```text
UI
Renderer
Player
Buildings
```

---

# 4. Initial Technology Direction

Tentative stack:

```text
Language:
C++20 / C++23

Platform / Windowing:
SDL3

Graphics:
SDL3 Renderer initially

Possible later:
SDL_GPU / OpenGL / Vulkan / DirectX abstraction

Build System:
CMake

Version Control:
Git + GitHub

AI Development:
GitHub Copilot
Claude Opus
Other coding agents where useful
```

The first version should deliberately avoid unnecessary complexity.

Do NOT initially create:

- Custom scripting language
- Full ECS framework
- Physics engine
- Visual shader editor
- Generic animation engine
- Unreal-style asset pipeline
- Generic scene graph
- Full game editor
- Multiplayer networking
- Modding system
- Complex particle engine

These can be considered later if the game actually needs them.

---

# 5. Repository Structure

Tentative structure:

```text
project/
│
├── CMakeLists.txt
├── README.md
├── AGENTS.md
│
├── docs/
│   ├── ARCHITECTURE.md
│   ├── WORLD_GENERATION.md
│   ├── SIMULATION.md
│   ├── POPULATION.md
│   ├── ECONOMY.md
│   ├── RENDERING.md
│   ├── SAVE_SYSTEM.md
│   └── ROADMAP.md
│
├── src/
│   │
│   ├── core/
│   │   ├── application/
│   │   ├── logging/
│   │   ├── random/
│   │   ├── math/
│   │   ├── time/
│   │   └── files/
│   │
│   ├── platform/
│   │   └── sdl/
│   │
│   ├── render/
│   │   ├── renderer/
│   │   ├── camera/
│   │   ├── texture/
│   │   └── debug_render/
│   │
│   ├── world/
│   │   ├── world/
│   │   ├── tile/
│   │   ├── region/
│   │   └── coordinates/
│   │
│   ├── worldgen/
│   │   ├── elevation/
│   │   ├── climate/
│   │   ├── hydrology/
│   │   ├── biome/
│   │   ├── resources/
│   │   ├── settlements/
│   │   └── factions/
│   │
│   ├── simulation/
│   │   ├── population/
│   │   ├── economy/
│   │   ├── jobs/
│   │   ├── trade/
│   │   ├── politics/
│   │   └── warfare/
│   │
│   ├── pathfinding/
│   │
│   ├── game/
│   │
│   └── ui/
│
├── tests/
│   ├── core/
│   ├── worldgen/
│   └── simulation/
│
├── assets/
│   ├── terrain/
│   ├── buildings/
│   ├── humans/
│   ├── vegetation/
│   ├── resources/
│   └── ui/
│
└── tools/
```

This is only an initial organizational idea.

Do not create every folder before it is needed.

---

# 6. Development Rule

The project should be developed vertically.

Instead of creating twenty unfinished systems:

```text
Renderer 10%
Population 10%
Economy 10%
Politics 10%
WorldGen 10%
UI 10%
```

we should create one complete working system at a time.

Example:

```text
Generate World
      ↓
Render World
      ↓
Inspect World
      ↓
Test World
      ↓
Save World
```

Only then move onward.

---

# 7. PHASE 0 — Engine Skeleton

## Goal

Create the smallest possible functioning engine.

At the end of Phase 0:

```text
Launch application
      ↓
SDL window opens
      ↓
Camera works
      ↓
Game loop runs
      ↓
Debug information displays
      ↓
Application closes correctly
```

---

## 7.1 Application

Create:

```text
Application
```

responsible for:

```text
initialize()
run()
shutdown()
```

Basic loop:

```cpp
while (running)
{
    processInput();
    update();
    render();
}
```

---

## 7.2 Timing

Create basic timing support.

Needed values:

```text
frameTime
deltaTime
simulationTime
FPS
```

Eventually simulation time should be separated from rendering time.

Example:

```text
Rendering:
60 FPS

Simulation:
10 / 20 / 30 ticks per second
```

This becomes important later.

---

# 8. PHASE 1 — World Representation

Before sophisticated generation, define how the world exists in memory.

---

# 8.1 World Coordinates

Define coordinate systems clearly.

Example:

```text
World coordinates

x = west/east
y = north/south
```

Possible representation:

```cpp
struct TileCoord
{
    int x;
    int y;
};
```

---

# 8.2 World

Example conceptual structure:

```cpp
class World
{
public:

    int width;
    int height;

    uint64_t seed;

    std::vector<WorldTile> tiles;
};
```

The exact representation can change later.

---

# 8.3 World Tile

Initial tile data might contain:

```cpp
struct WorldTile
{
    float elevation;

    float temperature;

    float moisture;

    float fertility;

    Biome biome;

    bool ocean;

    bool river;

    RegionId region;
};
```

Later additional information may include:

```text
soil
ground type
vegetation
resource deposits
river flow
water level
road
settlement
political owner
movement cost
```

Avoid putting every imaginable value into the tile immediately.

---

# 9. PHASE 2 — Deterministic Random Generation

Determinism is extremely important.

The following must hold:

```text
Seed 12345
```

must always produce:

```text
World 12345
```

---

# 9.1 Main World Seed

Example:

```cpp
World world(12345);
```

---

# 9.2 Independent Random Streams

Avoid one giant global RNG.

Instead derive subsystem seeds.

Example concept:

```text
worldSeed = 12345

terrainSeed =
hash(worldSeed, "terrain")

climateSeed =
hash(worldSeed, "climate")

resourceSeed =
hash(worldSeed, "resources")

settlementSeed =
hash(worldSeed, "settlements")
```

Advantages:

Changing resource generation should NOT completely change terrain generation.

Changing settlement generation should NOT move the mountains.

---

# 10. PHASE 3 — World Generation Pipeline

World generation should operate as a pipeline.

Tentative flow:

```text
World Seed
    │
    ▼
Base Noise
    │
    ▼
Elevation
    │
    ▼
Land / Ocean
    │
    ▼
Temperature
    │
    ▼
Moisture / Rainfall
    │
    ▼
Hydrology
    │
    ▼
Biomes
    │
    ▼
Fertility
    │
    ▼
Vegetation
    │
    ▼
Resources
    │
    ▼
Regions
    │
    ▼
Settlement Suitability
    │
    ▼
Settlements
    │
    ▼
Roads
    │
    ▼
Political Areas
```

Each stage should take existing world information and add information.

---

# 11. WorldGen Pass System

Generation should potentially use passes.

Example:

```cpp
class WorldGenerationPass
{
public:

    virtual void generate(World& world) = 0;
};
```

Possible passes:

```text
ElevationPass
TemperaturePass
RainfallPass
HydrologyPass
BiomePass
FertilityPass
ResourcePass
RegionPass
SettlementPass
RoadPass
FactionPass
```

This allows:

```text
Generate elevation
inspect result

Generate climate
inspect result

Generate rivers
inspect result
```

instead of having one giant:

```cpp
generateWorld()
```

function containing thousands of lines.

---

# 12. PHASE 3A — Elevation

Elevation is the foundation of geography.

Each tile receives approximately:

```text
elevation = 0.0 → 1.0
```

Example:

```text
0.00 deep ocean

0.25 shallow ocean

0.35 coast

0.40 plains

0.60 hills

0.80 mountains

1.00 highest peaks
```

These values are examples only.

---

# 12.1 Elevation Generation

Possible ingredients:

```text
large-scale continent noise

medium-scale terrain noise

small-scale detail noise

continental falloff

ridge noise

mountain masks
```

Possible conceptual equation:

```text
elevation =
    continentNoise
  + regionalNoise
  + mountainNoise
  + detailNoise
```

Then normalize.

---

# 12.2 Avoid Obvious Noise Maps

We should avoid worlds that look like:

```text
random blobs
random islands
noise texture
```

Eventually geography should produce meaningful structures:

```text
mountain ranges

river valleys

coastlines

plains

basins

peninsulas

islands
```

Do not overcomplicate this during the first implementation.

---

# 13. PHASE 3B — Ocean

Define sea level.

Example:

```text
seaLevel = 0.35
```

Then:

```cpp
tile.ocean =
    tile.elevation < seaLevel;
```

Later differentiate:

```text
deep ocean

shallow ocean

coast

beach

lake
```

---

# 14. PHASE 3C — Temperature

Temperature should depend on geography.

Basic factors:

```text
latitude

elevation

noise

distance from ocean

possibly prevailing climate systems later
```

Conceptually:

```text
temperature =
    latitudeTemperature
  - elevationCooling
  + temperatureNoise
```

---

# 14.1 Latitude

If the map represents a large part of the world:

```text
north

temperate

equator

temperate

south
```

temperature should follow latitude.

---

# 14.2 Elevation Cooling

Mountains should generally be colder.

Concept:

```text
temperature -= elevation * coolingFactor
```

---

# 15. PHASE 3D — Moisture / Rainfall

Initial version may use:

```text
noise

distance from water

elevation
```

Later versions can simulate more realistic rainfall.

Possible later factors:

```text
prevailing wind

mountain rain shadows

ocean moisture

river valleys

seasonality
```

---

# 16. PHASE 3E — Hydrology

Hydrology should eventually determine:

```text
water flow

watersheds

streams

rivers

lakes

river mouths
```

---

# 16.1 Water Flow

For every land tile:

```text
find lower neighboring tile
```

Water flows downhill.

Conceptually:

```text
Tile
 ↓
lower tile
 ↓
lower tile
 ↓
river
 ↓
ocean
```

---

# 16.2 Flow Accumulation

Each tile accumulates upstream water.

Example:

```text
Tile A contributes 1

Tile B receives A
flow = 2

Tile C receives multiple streams
flow = 15
```

Above a threshold:

```text
flow > riverThreshold
```

the tile becomes part of a river.

---

# 16.3 River Properties

Eventually rivers may contain:

```text
flow amount

width

depth

navigation ability

crossing difficulty

seasonal variation
```

Initially:

```text
river = true / false
```

is sufficient.

---

# 17. PHASE 3F — Biomes

Biome should NOT be random.

It should emerge from:

```text
temperature
+
moisture
+
elevation
```

Example:

```text
HOT + DRY
    ↓
desert

HOT + WET
    ↓
tropical forest

TEMPERATE + WET
    ↓
temperate forest

TEMPERATE + MEDIUM
    ↓
grassland

COLD + DRY
    ↓
tundra

HIGH ELEVATION
    ↓
mountain biome
```

---

# 18. PHASE 3G — Fertility

Fertility should influence future agriculture and settlement.

Possible inputs:

```text
biome

rainfall

temperature

river proximity

soil

elevation

flood plains
```

Output:

```text
fertility = 0.0 → 1.0
```

Example:

```text
0.1
almost useless

0.5
reasonable agriculture

0.9
extremely fertile
```

---

# 19. PHASE 3H — Vegetation

Vegetation should derive from climate.

Possible categories:

```text
none

grass

scrub

light forest

forest

dense forest

wetland
```

Later:

```text
tree species

plant species

wood quantity

regrowth

forest age
```

---

# 20. PHASE 3I — Resources

Resources should largely follow geography.

Do NOT simply do:

```cpp
if (random() < 0.02)
    spawnIron();
```

Resources should have geological/ecological rules.

---

# 20.1 Example Resources

Minerals:

```text
iron

copper

coal

tin

gold

silver

stone

clay

salt
```

Biological:

```text
wood

fish

game

wild plants
```

Agricultural potential:

```text
grain suitability

livestock suitability

orchard suitability
```

---

# 20.2 Resource Generation

Example concept:

```text
ironProbability =
    geologyFactor
  × mountainFactor
  × regionalNoise
```

Coal could use different conditions.

Clay might favor:

```text
river valleys
flood plains
lowlands
```

Fish might depend on:

```text
coast
river
lake
water productivity
```

---

# 21. PHASE 3J — Geographic Regions

World tiles should eventually form larger regions.

Example:

```text
River Valley

Mountain Basin

Coastal Plain

Highland

Island

Peninsula
```

Regions are useful because higher-level simulation should not operate entirely on individual tiles.

Example:

```text
WorldTile
    ↓
Region
    ↓
Province
    ↓
Kingdom
```

Exact hierarchy TBD.

---

# 22. PHASE 3K — Settlement Suitability

Before placing humans, calculate where humans WOULD naturally prefer to settle.

Each tile/region gets:

```text
settlementSuitability
```

Possible factors:

```text
fresh water

fertility

temperature

flat terrain

wood availability

stone availability

nearby minerals

river access

coast access

trade potential

defensibility
```

Concept:

```text
settlementScore =
    foodPotential
  + waterAccess
  + resourceAccess
  + tradeAccess
  + defensiveValue
  - terrainDifficulty
```

---

# 23. PHASE 3L — Settlement Generation

Once settlement suitability exists:

```text
place initial settlement
```

preferentially in good locations.

Avoid:

```text
cities every three tiles
```

Settlement placement should include spacing.

Possible categories later:

```text
hamlet

village

town

city

capital
```

Initially everything can simply be:

```text
Settlement
```

---

# 24. PHASE 3M — Roads

Settlements should eventually connect.

Road cost should depend on terrain.

Example:

```text
flat grass:
cost 1

forest:
cost 2

hill:
cost 3

mountain:
cost 10

river:
cost 8

bridge:
cost 2
```

Then pathfinding determines reasonable routes.

This naturally produces:

```text
passes through mountains

river crossings

regional road networks
```

---

# 25. PHASE 3N — Political Seeds

Once settlements exist, political entities may eventually emerge.

For initial generation:

```text
choose major settlements
```

as faction centers.

Then territory expands according to something similar to:

```text
travel distance

terrain

population

settlement importance
```

NOT simply:

```text
nearest point geometrically
```

---

# 26. World Generation Debugger

The first major application should basically be:

# World Generator Debugger

This should be extremely useful during development.

---

# 26.1 Camera

Controls:

```text
WASD / Arrow Keys
    move

Mouse Wheel
    zoom

Middle Mouse
    drag

R
    regenerate

N
    new random seed
```

Exact controls TBD.

---

# 26.2 Debug Views

Possible views:

```text
F1
Final Terrain

F2
Elevation

F3
Temperature

F4
Rainfall

F5
Biome

F6
Fertility

F7
Vegetation

F8
Resources

F9
Hydrology

F10
Settlement Suitability

F11
Regions

F12
Political Map
```

Could eventually move these into an ImGui debug panel.

---

# 26.3 Tile Inspector

Hover/click tile.

Display:

```text
Coordinate:
(528, 293)

Elevation:
0.63

Temperature:
14.2 C

Rainfall:
812 mm

Moisture:
0.61

Biome:
Temperate Forest

Fertility:
0.72

River:
No

Resources:
Iron
Stone

Region:
Northern Valley
```

This will be invaluable.

---

# 27. Generation Statistics

After generating a world, print information.

Example:

```text
WORLD GENERATION COMPLETE

Seed:
872638126

Map:
1024 x 1024

Land:
62.4%

Ocean:
37.6%

Biomes:

Temperate Forest:
21%

Grassland:
18%

Desert:
12%

Tundra:
8%

Mountains:
6%

etc.

Rivers:
327

Resource deposits:
1,832

Potential settlements:
743
```

This helps identify generation problems.

---

# 28. World Generation Tests

AI-generated code MUST be heavily tested.

Tests should include deterministic tests and invariant tests.

---

# 28.1 Determinism

Example:

```cpp
World a = generateWorld(12345);
World b = generateWorld(12345);

assert(hashWorld(a) == hashWorld(b));
```

---

# 28.2 Different Seeds

```cpp
World a = generateWorld(12345);
World b = generateWorld(54321);

assert(hashWorld(a) != hashWorld(b));
```

---

# 28.3 Geography Invariants

Examples:

```text
Elevation must remain within valid range.

Temperature must remain within valid range.

Ocean tiles cannot contain ordinary land settlements.

Rivers must follow legal flow paths.

Resource deposits must contain valid resource types.

Region IDs must exist.

No tile references nonexistent objects.
```

---

# 29. PHASE 4 — Rendering Real Terrain

Once generation works using debug colors, add actual pixel graphics.

IMPORTANT:

Do NOT introduce real art before WorldGen works.

Initially terrain should look like:

```text
████ green
████ blue
████ gray
████ yellow
```

That is GOOD.

Debug visualization makes problems obvious.

---

# 30. Asset System

Graphics should be data-driven.

Example:

```text
Biome::TemperateGrassland
```

may map to:

```text
grass_01.png
grass_02.png
grass_03.png
grass_04.png
```

The simulation does not know which texture is selected.

---

# 31. Asset Metadata

Every downloaded external asset should record:

```text
author

source

license

download date

asset type
```

Example:

```json
{
  "author": "Kenney",
  "source": "SOURCE_URL",
  "license": "CC0-1.0",
  "category": "terrain",
  "tags": ["grass", "temperate"]
}
```

---

# 32. Asset Licensing Rule

Coding agents must NEVER randomly download images from Google Images.

Approved licensing should preferably include:

```text
CC0

Public Domain
```

Possibly later:

```text
CC-BY
```

if we are comfortable maintaining attribution.

Every imported asset should have traceable licensing information.

---

# 33. Temporary Asset Sources

Potential sources include:

```text
Kenney

OpenGameArt
```

But assets must be checked individually.

"Free" does NOT automatically mean:

```text
free for commercial use
```

---

# 34. PHASE 5 — Separation Between World Simulation and Local Simulation

This is one of the most important future architectural concepts.

There should eventually be at least two simulation scales.

---

# 34.1 Strategic World Simulation

Example:

```text
World Map
```

Contains:

```text
kingdoms

cities

villages

population totals

trade

armies

resources

production

migration

politics
```

World simulation may operate statistically.

Example:

```text
City of Varna

population:
28,318

grain:
62,183

iron:
2,183

wealth:
8,392

soldiers:
1,120
```

We do NOT need to physically simulate all 28,318 citizens walking around while the player is 500 km away.

---

# 34.2 Local Settlement Simulation

When actively simulating a settlement:

```text
Local Map
```

may contain:

```text
individual humans

buildings

roads

warehouses

farms

workshops

resource piles

animals

construction sites
```

Individual humans can then have:

```text
position

home

job

workplace

family

inventory

needs

current task
```

---

# 35. Simulation Level Of Detail

Eventually simulation detail may depend on relevance.

Example:

```text
PLAYER CITY

individual simulation

      ↓

NEARBY CITY

partial simulation

      ↓

DISTANT CITY

statistical simulation

      ↓

DISTANT KINGDOM

high-level simulation
```

This allows enormous worlds.

---

# 36. PHASE 6 — Population

Population should be introduced only AFTER the world itself is functioning.

Initial population does NOT need:

```text
personality

relationships

politics

religion

family trees
```

Start simple.

---

# 36.1 Initial Person

Example:

```cpp
struct Person
{
    PersonId id;

    Position position;

    PersonState state;

    JobId job;

    BuildingId home;

    BuildingId workplace;
};
```

---

# 36.2 Initial Needs

Perhaps only:

```text
food

sleep
```

Later:

```text
safety

housing

social

wealth

religion

entertainment
```

---

# 37. PHASE 7 — Jobs

Initial jobs:

```text
Farmer

Woodcutter

Miner

Builder

Hauler
```

Example:

```text
Farmer

home
 ↓
farm
 ↓
work
 ↓
produce grain
 ↓
grain transported
 ↓
warehouse
```

---

# 38. Task System

Humans should probably perform tasks.

Example:

```text
Task:
Carry 10 wood

Source:
Stockpile A

Destination:
Construction Site B
```

Worker flow:

```text
find task

reserve task

walk to source

collect resource

walk to destination

deliver resource

complete task
```

---

# 39. PHASE 8 — Pathfinding

Pathfinding will eventually become extremely important.

Initial implementation:

```text
A*
```

Later optimization possibilities:

```text
hierarchical pathfinding

flow fields

region graphs

road graphs

path caching

shared routes
```

DO NOT optimize prematurely.

---

# 40. PHASE 9 — Resource Economy

Initial economy:

```text
Wood
Stone
Food
Iron
```

Example chain:

```text
Forest
 ↓
Woodcutter
 ↓
Logs
 ↓
Warehouse
 ↓
Construction
```

Another:

```text
Iron Deposit
 ↓
Mine
 ↓
Iron Ore
 ↓
Smelter
 ↓
Iron
 ↓
Blacksmith
 ↓
Tools
```

---

# 41. Production Chains

Production should become data-driven.

Example:

```text
Bakery

Inputs:

2 grain

Outputs:

1 bread

Workers:

3

Production time:

10
```

Instead of hardcoding every production chain directly into C++.

---

# 42. PHASE 10 — Settlement Economy

Eventually track:

```text
production

consumption

inventory

shortages

surplus

employment

wages

wealth

trade
```

A settlement should produce emergent economic conditions.

Example:

```text
Iron shortage

↓

tool production falls

↓

farm productivity falls

↓

food price rises

↓

migration decreases
```

---

# 43. PHASE 11 — Population Growth

Population dynamics:

```text
births

deaths

migration

food supply

housing

wealth

war

disease
```

Population should not simply be:

```text
population *= 1.01
```

Forever.

---

# 44. PHASE 12 — Culture

Culture should eventually be separate from race.

Example:

```cpp
CultureId culture;
```

Culture might influence:

```text
language

names

architecture

clothing

food

military traditions

laws

social norms

religion

relations with other cultures
```

Avoid arbitrary RPG-style bonuses unless they make sense.

---

# 45. PHASE 13 — Kingdoms

Kingdoms consist of:

```text
territory

settlements

population

government

military

resources

economy

diplomacy
```

Example:

```text
KINGDOM

    Capital

      │

 ┌────┼─────┐

City Town Village
```

---

# 46. Geography Should Affect Civilization

The world generator should eventually create different societal pressures naturally.

Example:

```text
Mountain Kingdom

little farmland

large iron deposits

isolated valleys

strong defensive terrain
```

This might naturally create:

```text
mining economy

smaller population

important metal production

strong fortifications
```

Another:

```text
River Kingdom

huge fertile flood plains

navigable river

few mineral resources
```

This may naturally create:

```text
large population

grain exports

river cities

dependence on imported metals
```

The simulation should create these differences whenever possible.

---

# 47. PHASE 14 — Trade

Trade starts between settlements.

Example:

```text
City A

surplus grain

needs iron

      ↕ TRADE

City B

surplus iron

needs grain
```

Trade routes should eventually depend on:

```text
distance

roads

rivers

sea routes

terrain

politics

war

transport capacity
```

---

# 48. PHASE 15 — Politics

Politics should come MUCH later.

Potential systems:

```text
government

social classes

factions

laws

nobility

merchants

religious institutions

military

local elites
```

Do not design this deeply until economic and population simulation already works.

---

# 49. PHASE 16 — Warfare

War should depend on the simulation rather than exist independently.

Military strength can depend on:

```text
population

food

weapons

iron

wealth

logistics

training

roads

terrain
```

For example:

```text
No iron

↓

fewer weapons

↓

military weakness
```

or:

```text
Poor road network

↓

bad army logistics
```

---

# 50. Time System

Eventually separate:

```text
render time

simulation ticks

game calendar
```

Possible hierarchy:

```text
1 tick

↓

hour

↓

day

↓

month

↓

year
```

Exact ratios TBD.

---

# 51. Simulation Frequency

Not every system needs to update every tick.

Example:

```text
Movement:
20 times/sec

Job tasks:
5 times/sec

Needs:
1 time/sec

Production:
1 time/sec

Settlement economy:
every game hour

Population demographics:
every game day

Kingdom politics:
every game week/month
```

This will become important for performance.

---

# 52. Performance Philosophy

Do not optimize everything immediately.

Correct order:

```text
make system work

↓

profile

↓

identify actual bottleneck

↓

optimize bottleneck
```

Avoid assumptions such as:

```text
"This might someday be slow"
```

being used to create unnecessarily complex architecture.

---

# 53. Important Performance Areas Later

Likely eventual bottlenecks:

```text
pathfinding

population AI

visibility

render batching

job searching

resource searching

collision/spatial queries

save/load

large arrays of entities
```

---

# 54. Data-Oriented Design

For extremely large populations, we may eventually prefer structures such as:

```cpp
std::vector<Position> positions;
std::vector<JobId> jobs;
std::vector<PersonState> states;
```

instead of:

```cpp
std::vector<Person*>
```

with many heap allocations.

However:

DO NOT prematurely redesign everything around data-oriented architecture.

First determine actual needs.

---

# 55. Entity IDs

Prefer stable IDs.

Example:

```cpp
using PersonId = uint32_t;
using BuildingId = uint32_t;
using SettlementId = uint32_t;
using KingdomId = uint32_t;
```

Objects should generally reference IDs rather than raw pointers when long-term persistence matters.

This will help:

```text
save/load

deletion

serialization

simulation references
```

---

# 56. Save System

Save/load should eventually serialize simulation state.

Important:

Save data should NOT contain rendering information unless needed.

Save:

```text
seed

world state

population

buildings

inventories

kingdoms

economy

simulation time
```

Not:

```text
GPU textures

renderer state
```

---

# 57. Development Debugging Tools

We should heavily invest in developer tools.

Potential tools:

```text
Tile inspector

Person inspector

Building inspector

Settlement inspector

Kingdom inspector

Economy graphs

Resource heatmaps

Path visualization

Job visualization

Population heatmap

Trade routes

AI state

Simulation speed

Pause

Single-step tick
```

Simulation games are almost impossible to debug properly without inspection tools.

---

# 58. Logging

Important events should support logging.

Example:

```text
[WORLDGEN]
Generated 103 rivers

[ECONOMY]
Settlement 12 entered grain shortage

[POPULATION]
Settlement 12 population fell below 1000

[TRADE]
Trade route established between 5 and 12
```

Logging categories should be configurable.

---

# 59. AI Coding Rules

The AI coding agent must follow strict rules.

Possible `AGENTS.md` instructions:

```text
# Project Coding Rules

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
```

---

# 60. AI Development Workflow

We should NOT ask:

```text
"Create the world generation system."
```

Instead:

```text
TASK 1

Create World and WorldTile.

Build.

Test.
```

Then:

```text
TASK 2

Create deterministic seeded RNG.

Build.

Test.
```

Then:

```text
TASK 3

Implement elevation generation.

Build.

Display elevation debug map.

Test.
```

Then:

```text
TASK 4

Implement sea-level classification.

Build.

Display land/ocean map.

Test.
```

Small tasks make AI-generated architecture much easier to control.

---

# 61. Git Strategy

Major milestones should be committed independently.

Example:

```text
engine: initial SDL application

world: add World and WorldTile

worldgen: deterministic RNG

worldgen: elevation generation

worldgen: ocean classification

worldgen: temperature model

worldgen: rainfall model

worldgen: hydrology

worldgen: biome classification
```

Avoid:

```text
"huge update"
```

containing 20 unrelated systems.

---

# 62. FIRST MAJOR MILESTONE

# MILESTONE 1 — Procedural World Viewer

The first real milestone is complete when:

- Application launches.
- SDL rendering works.
- Camera moves.
- Camera zooms.
- World has a configurable size.
- World uses a deterministic seed.
- Elevation is generated.
- Ocean is generated.
- Temperature is generated.
- Moisture/rainfall is generated.
- Biomes are generated.
- Rivers exist.
- Fertility exists.
- Basic resources exist.
- Debug map modes exist.
- Tile inspector exists.
- Same seed always generates same world.
- Different seeds produce different worlds.
- Tests pass.

NO:

- People
- Buildings
- Economy
- Kingdom simulation
- Combat

yet.

---

# 63. MILESTONE 2 — Geographic Civilization Foundations

Add:

```text
regions

resource distribution

settlement suitability

settlement locations

roads

basic faction territory
```

At this stage the map may look like a primitive strategy game.

Still no detailed population simulation.

---

# 64. MILESTONE 3 — Settlement Simulation Prototype

Create ONE settlement.

Add:

```text
local map

buildings

stockpiles

resources

workers

movement

jobs
```

Goal:

```text
10-100 humans can exist

↓

humans collect wood

↓

wood enters stockpile

↓

wood is used to build building
```

This is the first meaningful settlement gameplay loop.

---

# 65. MILESTONE 4 — Production Economy

Add:

```text
farms

mines

workshops

production chains

food consumption

basic employment
```

Goal:

```text
Settlement can function for multiple simulated years.
```

---

# 66. MILESTONE 5 — Large Population

Scale:

```text
100

↓

1,000

↓

10,000

↓

100,000
```

Profile at every stage.

Determine:

```text
CPU bottlenecks

memory usage

pathfinding cost

job-search cost

rendering cost
```

Optimize based on measurements.

---

# 67. MILESTONE 6 — Multiple Settlements

Add:

```text
City A

City B

Village C
```

Simulate:

```text
production

consumption

population

surplus

shortage
```

---

# 68. MILESTONE 7 — Trade

Create:

```text
trade routes

transport

imports

exports

regional specialization
```

Example:

```text
Mountain City
    IRON
      ↓

River City
    FOOD
```

---

# 69. MILESTONE 8 — World Population Simulation

World-level cities continue simulation when not locally loaded.

Create simulation LOD.

Example:

```text
Loaded settlement:
individual humans

Unloaded settlement:
aggregated simulation
```

---

# 70. MILESTONE 9 — Kingdom Simulation

Add:

```text
kingdoms

territories

taxation

diplomacy

armies

political relationships
```

---

# 71. MILESTONE 10 — Historical Simulation

Eventually the world may simulate history before the player starts.

Example:

```text
Year -200

first settlements

↓

Year -150

cities expand

↓

Year -100

kingdom formed

↓

Year -60

war

↓

Year -20

migration

↓

Year 0

PLAYER STARTS
```

This could produce:

```text
old cities

ruins

historic borders

mixed populations

old roads

regional grudges

wealth differences
```

This is a long-term feature, NOT an early requirement.

---

# 72. Possible World History Architecture

Future concept:

```text
World Generation
      ↓
Natural Geography
      ↓
Initial Population Seeds
      ↓
Settlement Growth
      ↓
Trade
      ↓
Migration
      ↓
Kingdom Formation
      ↓
Wars
      ↓
Cultural Change
      ↓
Current World
```

This can make every generated world have history rather than merely geography.

---

# 73. Emergent Simulation Goal

Ultimately we want:

```text
GEOGRAPHY

↓

RESOURCES

↓

ECONOMY

↓

SETTLEMENTS

↓

POPULATION

↓

TRADE

↓

CULTURE

↓

POLITICS

↓

WAR
```

with feedback loops.

Example:

```text
Mountains

↓

Iron

↓

Mining industry

↓

Metal exports

↓

Wealth

↓

Large city

↓

Political importance

↓

Enemy invasion

↓

Trade disruption

↓

Iron shortage elsewhere
```

This is the type of emergent behavior the engine should support.

---

# 74. Things We Explicitly Avoid Early

Do NOT spend early development time on:

```text
pretty main menus

character portraits

complex animations

multiplayer

Steam integration

achievements

mod support

advanced shaders

cinematics

complex sound systems

detailed politics

advanced military simulation

complex genetics

detailed family trees

hundreds of resources
```

The world simulation comes first.

---

# 75. Early Development Order

The immediate development order should approximately be:

```text
1. Repository setup

2. CMake

3. SDL window

4. Game loop

5. Camera

6. World class

7. Tile representation

8. Deterministic RNG

9. Elevation

10. Ocean

11. Temperature

12. Moisture

13. Hydrology

14. Biomes

15. Fertility

16. Vegetation

17. Resources

18. Debug map modes

19. Tile inspector

20. Save generated world

21. Regions

22. Settlement suitability

23. Settlements

24. Roads

25. Initial factions
```

Only after this should detailed citizens become a major focus.

---

# 76. First Development Target

The first target should look approximately like:

```text
+----------------------------------------+
| Seed: 837218721                        |
|                                        |
|       ^^^^^ Mountains                  |
|    ^^^^^^^                             |
|                ~~~~~ River             |
|         .........                      |
|       .........                        |
|                    ~~~~~~~~~ Ocean     |
|                                        |
| View: BIOME                            |
|                                        |
| Tile: 382,291                          |
| Elevation: 0.58                        |
| Temp: 18.3                             |
| Rainfall: 721                          |
| Biome: Temperate Forest                |
+----------------------------------------+
```

It does not need to be beautiful.

It needs to be inspectable.

---

# 77. Definition of Success for WorldGen V0.1

WorldGen V0.1 succeeds when we can repeatedly press:

```text
GENERATE
```

and receive worlds where we can say:

```text
"That geography makes sense."

"That river makes sense."

"That forest makes sense."

"That city would logically exist there."

"That mining region logically exists because of its geography."

"That kingdom would have reasons to interact with its neighbors."
```

The primary goal is not visual beauty.

The primary goal is:

# A world whose systems have understandable causes.

---

# 78. Future Questions To Decide

These should be discussed before their respective implementations.

## World

- Square grid vs hex grid?
- Tile size?
- World dimensions?
- Infinite vs finite world?
- Entire planet vs continent?
- Wrapping east/west?
- Seasons?
- Latitude model?

## Terrain

- Simple noise?
- Plate tectonics?
- Erosion?
- Real watershed simulation?
- Lakes?
- Oceans?
- Navigable rivers?

## Settlements

- How are settlements founded?
- Does population migrate automatically?
- Can cities die?
- Can villages merge?
- Why does one settlement become a city?

## Population

- Is every human individually simulated?
- At what distance do humans become aggregated?
- Families?
- Children?
- Aging?
- Education?
- Health?

## Culture

- Static cultures?
- Dynamic cultures?
- Language evolution?
- Cultural assimilation?
- Mixed identities?
- Religion?

## Economy

- Physical resources?
- Abstract markets?
- Currency?
- Prices?
- Wages?
- Private property?
- State property?
- Merchant simulation?

## Politics

- Monarchies?
- Republics?
- Nobility?
- Social classes?
- Revolts?
- Succession?
- Civil wars?

## Warfare

- Individual soldiers?
- Army entities?
- Strategic armies + tactical battles?
- Logistics?
- Fortifications?
- Occupation?

These decisions should NOT all be made immediately.

They should be decided when we reach the relevant subsystem.

---

# 79. Long-Term Vision

The final world should ideally allow situations such as:

```text
A fertile river valley develops several cities.

↓

Population grows rapidly.

↓

The region lacks iron.

↓

A nearby mountain kingdom has large iron deposits.

↓

Trade develops.

↓

The river kingdom becomes dependent on imported iron.

↓

Relations deteriorate.

↓

Trade stops.

↓

Tool prices rise.

↓

Farm productivity decreases.

↓

Food prices rise.

↓

Urban population becomes unhappy.

↓

Migration increases.

↓

Political unrest grows.

↓

War begins.
```

None of this should require a developer writing:

```cpp
triggerRiverKingdomIronCrisis();
```

It should emerge from interconnected systems.

That is the central long-term goal of the engine.

---

# 80. Project Principle

Whenever designing a new system, ask:

> Can this behavior emerge from existing simulation rules?

Prefer:

```text
general systems interacting
```

over:

```text
special scripted events
```

where practical.

Scripts can still exist for flavor and exceptional events, but the core world should be systemic.

---

# 81. Current Focus

For now, ignore:

```text
Population

Economy

Politics

War

Detailed settlements
```

Current focus:

# WORLD GENERATION

Specifically:

```text
World representation
        ↓
Deterministic RNG
        ↓
Elevation
        ↓
Ocean
        ↓
Temperature
        ↓
Rainfall
        ↓
Hydrology
        ↓
Biomes
        ↓
Fertility
        ↓
Resources
        ↓
Regions
        ↓
Settlement suitability
```

Once these systems work, we can review the results and decide how civilization should interact with the generated world.

---

# 82. Immediate Next Design Discussion

Before coding the full WorldGen system, decide:

```text
WORLD MODEL
```

Specifically:

1. World dimensions
2. Tile size / conceptual scale
3. Square vs hexagonal tiles
4. Finite vs infinite map
5. Continent vs whole-world simulation
6. Whether world tiles and local settlement tiles are separate
7. Approximate maximum settlement size
8. Approximate eventual world population
9. Whether rivers occupy tiles or run along tile edges
10. Whether terrain height is discrete or continuous
11. Desired level of geographical realism
12. How much world history should exist before game start

These decisions affect almost every later system.

Do not let an AI coding agent make these architectural decisions automatically.

They should be consciously decided before implementation.
