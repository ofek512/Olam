# OLAM — PHASE 4B

## Modular Construction, Buildings, Rooms and Founding Camp

**Status:** Planned  
**Prerequisite:** Phase 4A.5 — Local Ecology and Natural Resources  
**Next phase:** Phase 4C — First Population

## Vision

Olam is a hybrid settlement and strategy simulation inspired by:

- **RimWorld:** Flexible walls, floors, furniture, rooms and construction orders.
- **Songs of Syx:** Large-scale settlements, configurable workplaces, functional rooms and manageable construction at city scale.
- **Manor Lords:** A humble founding settlement, initial supplies and progression from homelessness to established housing.
- **Victoria 3:** Later connections between household wealth, goods consumption, comfort and standard of living.

The construction system must be satisfying when designing a single house while remaining usable in cities with thousands of inhabitants.

Do not implement a full economy or population simulation in Phase 4B.

---

# CORE DESIGN DECISIONS

## 1. Hybrid building system

Support both:

**Manual construction**

- Place walls.
- Place doors and windows.
- Place floors.
- Arrange furniture.
- Design custom room layouts.
- Extend or renovate structures.

**Premade templates**

- Select a predefined house, dormitory, workshop, etc.
- Place its complete blueprint.
- Modify its dimensions and layout.
- Move or replace equipment.
- Expand or renovate it later.

Templates must use the same underlying construction data as manually designed buildings.

Do not create a separate, incompatible prefab-building system.

## 2. Building, room and furniture separation

Use three conceptual levels:

    BUILDING
      |
      +-- Structural components
      |     Walls
      |     Doors
      |     Windows
      |     Floors
      |     Roof
      |
      +-- Room
      |     Function / designation
      |     Equipment
      |     Capacity
      |
      +-- Room
            Function / designation
            Equipment
            Capacity

A building may contain multiple rooms.

A room's purpose is determined by its designation and qualifying equipment.

Examples:

    Bedroom
      Required: Bed
      Optional: Wardrobe, furniture

    Kitchen
      Required: Cooking equipment
      Optional: Storage, tables

    Workshop
      Required: Suitable workbench
      Optional: Tools, storage

A qualifying item is necessary for a functional room, but placing one item must not automatically override the player's intended room designation.

## 3. Construction precision

Keep the existing 2-metre local terrain grid.

Walls occupy tile edges rather than entire terrain tiles.

Allow suitable furniture and smaller components to use finer placement precision, initially considering 0.5-metre increments.

Do not create an enormous permanent high-resolution construction grid across the entire local map.

Use sparse or spatially partitioned construction data.

## 4. Construction lifecycle

All construction follows:

    Planned
       ↓
    Awaiting Materials
       ↓
    Materials Delivered
       ↓
    Under Construction
       ↓
    Completed

Actual construction workers are introduced in Phase 4C.

During Phase 4B, support a developer/debug mechanism to deliver materials and complete construction.

## 5. Physical logistics

Resources belong to physical storage locations.

Building materials must eventually move from stockpiles to construction sites.

Do not assume a magical global inventory that instantly makes every resource available everywhere.

Phase 4B should define the material and construction-order mechanics.

Phase 4C will introduce actual workers who perform hauling and construction.

## 6. Starting technology and progression

Olam begins with a limited, low-technology founding settlement.

Initially available construction methods may include:

- Timber
- Wattle and daub
- Thatch
- Rough stone
- Simple earth and wooden floors
- Basic doors and furniture

Do not impose a fixed medieval technological level on the entire game.

The longer-term design should permit buildings to evolve through historical eras.

Examples:

    Thatch roof
        ↓
    Timber roof
        ↓
    Tile roof

    Simple timber home
        ↓
    Improved timber home
        ↓
    Stone residence

    Primitive workshop
        ↓
    Improved workshop
        ↓
    Specialized production facility

Upgrades should preserve existing structures when reasonably possible.

Do not implement a technology tree during 4B.

---

# PHASE 4B.1 — CONSTRUCTION GRID AND STRUCTURAL PLACEMENT

## Goal

Create the basic construction system for manually placing structures on the local map.

## Implement

- Construction coordinates and snapping
- Wall placement along tile edges
- Door placement
- Basic floor placement
- Placement preview
- Placement validation
- Component rotation where relevant
- Component removal

A structural element must know:

- Its position
- Orientation
- Material
- Construction state
- Basic physical properties

## Placement validation

Check:

- Terrain suitability
- Slope
- Water
- Existing structures
- Resource/vegetation obstructions
- Spatial conflicts

For now, allow a developer mode to clear suitable obstructions where necessary.

## Architecture

Use sparse structures or spatial indexing.

Do not allocate a heavyweight building object for all 9+ million local tiles.

Simulation data must not depend on SDL.

## Tests

- Walls align correctly with tile edges.
- Doors occupy valid openings.
- Floors occupy valid areas.
- Invalid placement is rejected.
- Overlapping incompatible components are rejected.
- Components can be removed.
- Identical placement commands give deterministic results.

## Stop condition

The player can manually draw walls, place doors and floors, remove them and inspect the results.

No room functionality is required yet.

---

# PHASE 4B.2 — BLUEPRINTS AND CONSTRUCTION ORDERS

## Goal

Separate player planning from completed buildings.

## Implement

- Place blueprint
- Preview before confirming
- Confirm planned construction
- Cancel blueprint
- Edit an unfinished blueprint
- Track construction state
- Debug instant-complete

Blueprints should not immediately modify the natural terrain except through explicitly committed construction actions.

Use data structures compatible with future construction tasks.

## Construction states

    Planned
    AwaitingMaterials
    ReadyToBuild
    UnderConstruction
    Completed

A blueprint can contain multiple structural components.

Future workers should be able to consume construction orders without requiring changes to the underlying data model.

## Tests

- Blueprint placement and cancellation
- Valid and invalid placement
- State transitions
- Component-level completion
- No duplicate resource consumption
- Deterministic command results

## Stop condition

The player can plan an entire structure and complete it through developer controls.

---

# PHASE 4B.3 — ROOMS, ENCLOSURE AND ROOFS

## Goal

Detect rooms from player-designed structures.

## Room detection

Implement geometric detection of enclosed areas.

Use:

- Walls
- Doors
- Building boundaries

A door counts as an enclosed boundary when closed/installed, while remaining a traversable opening for later pathfinding.

Detect changes when:

- A wall is added
- A wall is removed
- A door is added
- A building is extended
- Two rooms merge
- One room is divided

Avoid recomputing room connectivity across the entire 3072 × 3072 map for every edit.

## Roofs

Automatically infer roofing for structurally valid enclosed rooms.

The player should be able to override:

- Roof enabled
- Roof disabled
- Roofing material
- Later roof style

Automatic roofs should not require manual placement of every roof tile.

Do not build realistic structural physics or complex roof geometry yet.

## Building hierarchy

One structure may contain multiple rooms.

A building's identity should survive reasonable renovations and extensions.

## Tests

- Fully enclosed room detected
- Open room not incorrectly enclosed
- Door recognized
- Split room
- Merge rooms
- Roof added after enclosure
- Roof updated after structural changes
- Manual roof overrides respected

## Stop condition

The player can draw a two-room house, install doors and see both rooms and automatic roofing recognized correctly.

---

# PHASE 4B.4 — ROOM FUNCTIONS AND FURNITURE

## Goal

Give enclosed spaces meaningful purposes.

## Implement

Initial room functions:

- Unassigned
- Bedroom
- Dormitory
- Storage
- Kitchen
- Basic workshop

Do not implement full production systems yet.

## Functional equipment

Begin with a small equipment catalogue:

- Bed
- Simple table
- Chair
- Storage container
- Cooking hearth
- Workbench

Equipment has:

- Position
- Footprint
- Material
- Construction state
- Room-function compatibility
- Occupancy/capacity contribution where relevant

## Room identification

A room is functional when:

    valid enclosure
          +
    suitable designation
          +
    required equipment
          ↓
    functional room

Example:

    Bedroom designation
          +
    at least one usable bed
          ↓
    Bedroom capacity > 0

The room should report missing requirements.

## Housing model

Support two early housing categories.

### Communal housing

Examples:

- Dormitory
- Shared sleeping hall
- Temporary shelter

Characteristics:

- Higher capacity for area/material cost
- Low privacy
- Lower potential comfort
- Cheap early accommodation

### Private housing

Examples:

- Small house
- Family dwelling

Characteristics:

- Lower density
- Greater privacy
- More furniture opportunities
- Higher comfort potential
- Higher construction cost

Separate:

- Sleeping capacity
- Privacy
- Housing quality
- Material condition
- Furnishing

These properties will later influence standard of living.

Do NOT implement actual household assignment yet.

That belongs to Phase 4C or later.

## Tests

- Room function assignment
- Missing equipment validation
- Furniture footprint collision
- Bed capacity calculation
- Dormitory capacity
- Private-home capacity
- Furniture placement and removal
- Functional status updating after renovations

## Stop condition

The player can create a functional dormitory and a private home using the same construction tools.

Their capacity and potential comfort differ.

---

# PHASE 4B.5 — MATERIAL COSTS, STORAGE AND CONSTRUCTION LOGISTICS

## Goal

Connect construction to actual natural resources.

## Initial resources

Use existing resources where available:

- Wood
- Stone
- Clay
- Reeds/thatch where appropriate

Do not require iron for ordinary starting shelters.

## Construction materials

Example materials:

    Timber wall:
        Wood

    Rough stone wall:
        Stone

    Thatch roof:
        Reeds/Thatch + Wood

    Basic furniture:
        Wood

Exact costs should be configurable for future balancing.

## Stockpiles

Create basic physical stockpile entities/areas.

A founding stockpile contains initial resources.

Support:

- Location
- Resource type
- Available quantity
- Reserved quantity
- Material delivery state

Materials reserved for one blueprint must not be simultaneously available to unrelated construction orders.

## Important

Actual physical hauling will be added when citizens exist.

For Phase 4B, use a developer/debug delivery action.

Still represent:

    stockpile
       ↓
    delivery destination
       ↓
    construction site

Do not implement detailed logistics pathfinding yet.

## Tests

- Construction cost calculation
- Material availability
- Resource reservation
- Material release on cancellation
- Delivery bookkeeping
- Construction completion
- No negative inventories
- No duplicate deliveries

## Stop condition

The player can place a wooden house blueprint, inspect its cost, allocate supplies and complete construction.

---

# PHASE 4B.6 — TEMPLATES, EXPANSION AND RENOVATION

## Goal

Make the construction system scalable and approachable.

The player must not have to manually place every wall and piece of furniture in a city containing thousands of people.

## Templates

Initial templates:

- Temporary shelter
- Small timber house
- Communal dormitory
- Storage shed
- Basic workshop

A template contains the same components the player could place manually.

It is not a special building type.

## Customization

Player can:

- Resize or extend buildings
- Move doors
- Add new rooms
- Remove walls
- Change room designations
- Rearrange furniture
- Replace construction materials
- Renovate existing rooms

## Expansion example

    Small house
      |
      +-- Bedroom
      +-- Living space

After extension:

    Expanded house
      |
      +-- Bedroom 1
      +-- Bedroom 2
      +-- Kitchen
      +-- Living space

Unchanged components should remain intact.

Changes should not unnecessarily erase construction progress or completed furniture.

## Future-proofing

Support component replacement:

    old component
          ↓
    renovation blueprint
          ↓
    material delivery
          ↓
    new component

Technology unlocks may later expose new construction materials, equipment and templates.

Do not implement eras or research during this phase.

## Tests

- Template placement
- Template versus manual construction equivalence
- Building extension
- Room identity updates
- Renovation
- Valid resource accounting
- Preserving unchanged building components

## Stop condition

The player can place a premade house, customize it and extend it without demolishing the whole structure.

---

# PHASE 4B.7 — FOUNDING CAMP AND FIRST CONSTRUCTION EXPERIENCE

## Goal

Create the starting settlement experience inspired by Manor Lords.

## Starting situation

The player chooses a valid world location.

The settlement begins with:

- No permanent housing
- A small supply stockpile
- Basic food and materials
- A temporary camp location
- A future starting settler group

The initial settler population is not simulated yet.

Represent starting-population information only if needed for later integration.

## Initial resources

Use small configurable starting quantities of:

- Food
- Wood
- Stone
- Basic equipment where appropriate

The purpose is to provide enough resources for initial shelter construction, not unlimited wealth.

## Player experience

The player can:

1. Choose where the camp begins.
2. Inspect the available supplies.
3. Plan a temporary shelter.
4. Plan a communal dormitory or house.
5. Allocate building materials.
6. Complete construction through debug tools.
7. Inspect housing capacity and comfort.
8. Extend or renovate existing structures.

## Future Phase 4C integration

Later:

    homeless settlers
          ↓
    construction orders
          ↓
    resource hauling
          ↓
    building construction
          ↓
    housing becomes available
          ↓
    settlers move inside

No real settlers or hauling simulation in 4B.

## Stop condition

A settlement can progress visually from an empty founding camp to a small collection of completed structures.

---

# PHASE 4B — FINAL ACCEPTANCE TEST

The player should be able to:

    Generate natural world
            ↓
    Select settlement location
            ↓
    Enter local map
            ↓
    See founding stockpile
            ↓
    Design a building
            ↓
    Place blueprint
            ↓
    Allocate resources
            ↓
    Complete construction in debug mode
            ↓
    Create enclosed rooms
            ↓
    Automatically roof rooms
            ↓
    Install furniture
            ↓
    Recognize room functions
            ↓
    Create communal and private housing
            ↓
    Expand and renovate structures

At this point, Olam has a complete basic construction foundation.

It does not yet have a functioning population economy.

---

# IMPLEMENTATION ORDER

- [x] 4B.1 — Construction grid, edge walls and structural placement
- [x] 4B.2 — Blueprints and construction orders
- [x] 4B.3 — Room detection and automatic roofs
- [ ] 4B.4 — Room functions, furniture and housing
- [ ] 4B.5 — Material costs, stockpiles and construction bookkeeping
- [ ] 4B.6 — Templates, extensions and renovations
- [ ] 4B.7 — Founding camp and first construction experience

Each sub-phase must be independently testable.

Do not start Phase 4C until Phase 4B's acceptance tests pass.

---

# PHASE 4C — NEXT DEVELOPMENT MILESTONE

After 4B, introduce the first real citizens.

Initial focus:

- Small starting population
- Movement and pathfinding
- Construction workers
- Haulers
- Material transport
- Shelter occupancy
- Food consumption
- Basic daily routines

Future stages:

- Jobs
- Gathering
- Production
- Immigration
- Household comfort
- Standard of living

Do not implement those systems prematurely during 4B.

---

# INSTRUCTIONS FOR CLAUDE OPUS

Read the existing repository instructions and architecture before implementing Phase 4B.

Preserve:

- Determinism
- Headless simulation tests
- SDL-free settlement-domain code
- Existing World/local-map separation
- Efficient handling of large local maps
- Correct resource persistence and construction state
- Compatibility with future individual citizen simulation

Start with Phase 4B.1 only.

For each sub-phase:

1. Review the current implementation.
2. Identify existing reusable systems.
3. Propose a minimal architecture consistent with this plan.
4. Implement that sub-phase.
5. Add tests.
6. Verify the behavior in the local map.
7. Report performance and architectural implications.
8. Update the roadmap.

Do not implement all of Phase 4B in one enormous change.

Do not add a full ECS, physics engine, research system, global economy or population simulator unless a concrete requirement justifies it.

**Immediate implementation target: 4B.1 — Construction grid and structural placement.**
