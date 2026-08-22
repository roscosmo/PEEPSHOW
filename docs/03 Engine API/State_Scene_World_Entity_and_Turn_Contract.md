# STATE_SCENE World, Entity, and Turn Contract

## Purpose

This contract defines how one `STATE_SCENE` can own a complete low-rate game
world while preserving bounded `REACTIVE` execution and automatic return to
sleep.

A world-state change is not a scene transition. Moving an actor, damaging an
enemy, opening a chest, collecting an item, or advancing a turn updates objects
inside the active scene. A scene transition is reserved for logically distinct
content or a different execution primitive.

Example:

```text
STATE_SCENE: DUNGEON_FLOOR
  -> push PROGRAM_SCENE: TIMED_ATTACK
  -> receive bounded attack result
  -> resume DUNGEON_FLOOR
  -> finish the turn
  -> publish the settled view and wait contract
  -> yield
```

`REALTIME_SCENE` is obsolete terminology and is not a package scene type.
Interactive realtime content uses `PROGRAM_SCENE`. A fixed authored realtime
timeline uses `SEQUENCE_SCENE`.

## Core Model

A world-enabled `STATE_SCENE` may contain:

- one immutable world descriptor
- one or more compiled tilemap and collision assets
- a fixed-capacity mutable entity-instance pool
- typed world, scene, and entity properties
- a camera and viewport policy
- deterministic collections and tag masks
- bounded reusable behavior tables
- an optional bounded turn controller
- one settled render model and reactive wait contract

The scene remains active while these values change. PeepOS runs package logic
only for an admitted event, schedule, lifecycle event, or declared completion.
Cosmetic waiting animation never advances turns or mutates world state.

## Immutable Package Data

The following are compiled package data and do not require mutable instance RAM:

- entity definitions and defaults
- property schemas
- behavior, guard, and action tables
- tag definitions
- tile graphics and tilemaps
- collision and object layers
- spawn records and trigger records
- room, camera, and viewport metadata
- scene transition and result schemas

Runtime code must not parse Tiled JSON, authoring files, filesystem paths, or
dynamic dictionaries. Import tools convert source formats into bounded package
tables before installation.

## World Descriptor

Conceptual compiled shape:

```text
world_descriptor:
  world_id
  map_ref
  collision_ref
  width_tiles
  height_tiles
  tile_width
  tile_height
  camera_policy_ref
  entity_definition_refs[]
  initial_entity_records[]
  collection_records[]
  turn_controller_ref
  world_property_schema_ref
  budgets
```

All references are stable numeric IDs or validated package-relative table
indices. Installed runtime data contains no executable pointers.

## Entity Definitions And Instances

An entity definition describes shared immutable data:

```text
entity_definition:
  definition_id
  visual_ref
  property_schema_ref
  default_property_values[]
  tag_mask
  collision_shape_ref
  behavior_ref
  inventory_schema_ref
```

An entity instance occupies one slot in the active scene's fixed pool:

```text
entity_instance:
  instance_id
  definition_id
  active
  world_x
  world_y
  property_values[]
  animation_state
  visibility
  runtime_tag_mask
```

Rules:

- instance IDs are stable within the scene lifetime
- iteration order is deterministic by compiled instance order or stable ID
- mutable properties have fixed types, ranges, reset policy, and persistence
- custom properties are schema-declared slots, not runtime key/value maps
- inventories and status lists have fixed capacities
- entity records contain no raw pointers, hardware IDs, or host paths
- inactive pool slots do not imply heap allocation or compaction

## Collections And Tags

Author-facing tags such as `Enemy`, `Item`, `Blocking`, and `Collectable`
compile to fixed-width masks or bounded collection tables.

`FOR EACH entity tagged Enemy` is valid only when:

- the target profile admits the maximum candidate count
- iteration uses deterministic stable order
- the behavior invoked for each candidate has a validated operation bound
- the complete event transaction fits its action and world-operation budgets
- mutation during iteration follows a declared snapshot/deferred-commit rule

Runtime string matching and unbounded collection growth are forbidden.

## World Coordinates, Camera, And Rendering

World position is independent of display position:

```text
screen_x = world_x - camera_x
screen_y = world_y - camera_y
```

The exact transform may include tile dimensions, integer scale, clipping, and
camera policy, but it remains deterministic and bounded.

World entity count and render-element count are separate target limits. The
runtime culls entities outside the viewport and projects visible world content
into a bounded render model. A tilemap viewport is one bounded render command or
surface operation; it is not required to consume one retained element per tile.
One entity may produce zero, one, or several render elements according to its
validated visual definition.

If projected content exceeds the target render budget, compilation must reject
it or apply an explicitly declared deterministic visual fallback. Runtime
allocation order must not decide which gameplay objects disappear.

## Turn Controller

A world-enabled `STATE_SCENE` may declare a turn controller with bounded phases:

```text
1. resolve admitted player action
2. resolve entity behavior phase
3. resolve world effects
4. evaluate end-of-turn conditions
5. commit the settled view and wait contract
6. yield
```

One admitted input may therefore trigger a complete logical turn. The controller
does not create a free-running tick and does not remain awake waiting for the
next player action.

Required rules:

- phase count and phase order are declared
- every phase has a maximum candidate count and operation count
- entity iteration order is deterministic
- the runtime stages mutations in a bounded command/write journal
- validation failure or budget exhaustion preserves the previous committed
  world state and follows a declared failure route
- service requests use symbolic Engine actions and bounded completion events
- rendering occurs after a coherent world commit, not midway through an
  uncommitted turn
- the final pre-yield state publishes admitted events, schedules, waiting visual,
  and fallback

## World Operations

The Engine may expose bounded operations such as:

```text
CanMove(entity, direction)
Move(entity, direction)
MoveToward(entity, target)
IsAdjacent(entity_a, entity_b)
Distance(entity_a, entity_b)
GetEntityAt(world_x, world_y)
GetTileAt(world_x, world_y)
Damage(target, amount)
Heal(target, amount)
Spawn(definition, world_x, world_y)
Destroy(entity)
GiveItem(entity, item)
RemoveItem(entity, item)
```

These are symbolic authoring operations. Their compiled forms use stable IDs,
fixed records, and validated bounds.

Rules:

- movement and collision queries use compiled map/collision data
- occupancy conflicts have deterministic resolution
- spawn uses the fixed entity pool and has a declared `pool_full` result
- destroy marks or recycles a slot only at a transaction-safe point
- inventory overflow and missing-item behavior are declared
- damage, healing, and arithmetic obey declared range/overflow policy
- `MoveToward` uses an admitted bounded strategy; unrestricted path search is
  forbidden
- advanced pathfinding must declare search-node, memory, and operation budgets
  or use compiler-generated navigation data

## Reusable Behaviors

Entity definitions may reference reusable behavior graphs or macros such as
`CHASE_PLAYER`, `WANDER`, `GUARD_POSITION`, or `FLEE`.

Authoring reuse does not create dynamic runtime code. Tools inline or reference
validated bounded behavior tables. For a fixed event trace and world snapshot,
behavior selection and action order must be deterministic.

## PROGRAM_SCENE Integration

A `STATE_SCENE` may push a declared `PROGRAM_SCENE` for a timed interaction.
The package host remains mounted and the suspended state scene retains its
validated state according to target-profile limits.

The realtime scene returns a fixed-schema result record, for example:

```text
attack_result:
  outcome
  damage
  flags
```

Rules:

- result schemas are declared at compile time
- result payload size is bounded by the target profile
- only declared return routes may consume the result
- cancellation, inactivity, failure, and invalid-result routes are explicit
- resuming the state scene does not reset compatible presentation timelines
- remaining turn phases resume from declared transaction state; they are not
  inferred from display state

## Memory And Storage

Immutable definitions, maps, behavior tables, and visual assets remain package
data in admitted storage. Mutable active-world state uses deterministic runtime
RAM: entity slots, property values, camera state, turn state, bounded journals,
and renderer scratch.

The runtime must not load an entire package into RAM by default, stream from FAT,
allocate entities from a heap, or retain arbitrary authoring structures. Target
profiles publish abstract capacities and budgets rather than SRAM bank names.

## Power And Clock Policy

`STATE_SCENE` remains `REACTIVE` regardless of world complexity. Authors do not
select CPU clocks, STOP modes, DMA engines, or autonomous-display hardware.

The compiler derives a bounded workload class from entity counts, behavior and
query costs, render cost, service use, and latency requirements. PeepOS may
select the lowest validated clock profile that completes the transaction within
its admitted deadline. A temporary faster profile does not turn the scene into
`REALTIME`; after the transaction settles, the runtime yields and normal sleep
policy resumes.

## Target-Profile Limits

Each target profile must publish abstract limits for at least:

- world dimensions and tile size
- active and visible entity counts
- entity definitions, tags, collections, and custom property slots
- inventory/status capacities
- world operations and entity visits per event
- spawn/destroy operations per event
- turn phases and deferred mutation records
- pathfinding search nodes and scratch memory where supported
- projected render elements and tilemap viewport work
- suspended scene state and realtime result payload size

These limits are compatibility rules. They may be tuned as measured RAM,
latency, clock, and power evidence improves; they are not package-selected
hardware controls.

## Authoring And Validation

Tools should expose tilemaps, entity prefabs, instance placement, collections,
turn controllers, behaviors, collision, inventories, and conditions as authoring
features. They compile to the same bounded `STATE_SCENE` primitive.

Validation must reject:

- unbounded loops, recursion, or dynamic collections
- unknown entity/property/tag IDs
- property writes outside declared type or range
- behavior/query cost above the selected target profile
- missing collision, spawn, result, or overflow policy
- viewport output above render budgets without a deterministic fallback
- a realtime interaction without declared return/failure/inactivity routes
- map data that requires runtime parsing of its source format

The node graph represents meaningful scene structure. Ordinary world-state
changes remain data mutations inside the active scene.
