# Scene Runtime and Interaction Model

This document defines the canonical package runtime model shared by PeepOS,
package tooling, the digital twin, and game-authoring tools.

It defines package-visible scene types, scene transitions, presentation timing,
runtime compositor layers, variable namespaces, capability derivation, and the
system interaction state. It does not expose hardware clocks, STOP modes, DMA,
LPBAM, SRAM placement, peripheral registers, or RTOS objects.

Related:

- [[Authority_and_Invariants]]
- [[Runtime_Host_Contract]]
- [[Runtime_Logic_State_API_Contract]]
- [[Rendering_API_Contract]]
- [[Time_And_Power_Intent_API_Contract]]
- [[Input_Focus_API_Contract]]
- [[Game_Authoring_API_Contract]]
- [[Authoring_Project_Schema_Contract]]

---

## Canonical Vocabulary

PeepOS separates three different concerns.

| Axis | Canonical Values | Meaning |
|---|---|---|
| System host | `SHELL`, `PACKAGE`, `INSTALLER` | Which PeepOS host owns the foreground experience |
| Package scene type | `STATE_SCENE`, `SEQUENCE_SCENE`, `PROGRAM_SCENE` | Which bounded package runtime primitive is active |
| Execution semantic | `REACTIVE`, `REALTIME` | Whether work settles and yields or remains frame-paced |

Mappings are fixed:

| Scene Type | Execution Semantic |
|---|---|
| `STATE_SCENE` | `REACTIVE` |
| `SEQUENCE_SCENE` | `REALTIME` |
| `PROGRAM_SCENE` | `REALTIME` |

`SHELL` and `INSTALLER` are Platform-owned hosts. A package is mounted once by
the `PACKAGE` host and may transition between declared package scenes without
remounting the package.

The retired `LP_GRAPH`, `LP_MODULE`, and `RT_SCENE` names are legacy FW0 and
historical-documentation terms. New contracts and package schemas must not use
them. Migration tooling may recognize them only to produce an explicit
compatibility error or a deterministic source migration.

---

## Package Scene Graph

A package scene graph contains:

```text
package
  scenes[]
    scene_type
    scene_properties
    child objects and authored constructs
    input routes
    event routes
    transition outputs
    inactivity route
    capability-derived service requirements
  transitions[]
    guards
    bounded actions
    target scene
```

Rules:

- every package declares one entry scene
- every scene and transition has a stable ID
- every transition target exists and is declared
- scene entry, exit, event, and transition work is bounded
- scene changes do not remount the package
- only package stop, package replacement, installer entry, or system recovery
  changes the package-host mount lifecycle
- tools may display scenes and transitions as a node graph
- compiled output uses deterministic bounded tables, not executable pointers

---

## STATE_SCENE

`STATE_SCENE` is the default and preferred package primitive. It represents a
static or looping low-rate scene that sleeps between admitted events.

A transaction is:

```text
wake or admitted event
  -> read a consistent Engine snapshot
  -> evaluate guards and bounded actions
  -> update scene/game state
  -> update dirty compositor content
  -> establish the settled presentation and waiting animation
  -> publish admitted events and schedules
  -> yield
```

Rules:

- package logic never remains awake waiting for input
- cosmetic waiting animation does not execute package logic or mutate variables
- PeepOS automatically chooses held display, autonomous playback, measured
  wake/update/return, or a declared fallback
- package scenes never select STOP2, LPBAM, clocks, DMA, or wake pins
- scene-level schedules are used only when logical state must change
- menu navigation may derive accepted directions and default bindings from the
  authored list layout
- joystick vector polling is disabled by default
- a scene may request cardinal joystick actions during an active interaction
  window; Platform decides whether wake-and-sleep threshold mode is admitted

Virtual pets, Game & Watch style games, clocks, status views, dialogue, menus,
choices, turn-based interactions, and ambient toys should normally compile to
`STATE_SCENE` graphs.

### State-Scene Presentation Cadence

The HW6 v1 target profile uses a `250 ms` presentation phase quantum.

Rules:

- waiting-animation phase durations are integer multiples of the target
  presentation quantum
- a two-frame blink with one quantum per phase has a `500 ms` complete cycle
- slower animation is represented by repeating or extending phases
- unsupported fractional cadence fails validation or uses a declared fallback
- the phase quantum drives presentation only; it is not a package logic tick
- entering a different scene or an incompatible presentation establishes a new
  settled presentation epoch immediately, so interaction does not wait for an
  unrelated presentation deadline
- an admitted event that updates content inside the same presentation preserves
  the current combined step and absolute deadline; the updated settled view is
  composed at that step rather than restarting its waiting animation
- scene content revision and presentation-timeline revision are separate:
  changing state data does not by itself restart animation, while a changed
  cadence, step topology, phase map, or explicit authored rebase policy does
- the HW6 v1 preferred waiting presentation permits at most four phases per
  element and twelve combined timeline steps
- elements with different phase counts share one combined timeline; for
  example, two-phase and three-phase elements repeat over six combined steps
- the HW6 v1 guaranteed autonomous presentation has three global steps; target
  reduction maps phase counts `1 -> 1/1/1`, `2 -> 1/2/1`, `3 -> 1/2/3`, and
  `4 -> 1/2/3`
- if the preferred combined timeline exceeds the target's autonomous budget,
  every element is reduced against the same three global steps; the runtime
  does not independently drop elements or run mismatched local cycles

---

## SEQUENCE_SCENE

`SEQUENCE_SCENE` is a bounded, data-driven frame timeline. It uses `REALTIME`
execution because the CPU remains awake and frame-paced while the sequence is
active, but it does not expose arbitrary per-frame program logic.

It may declare:

- target FPS and duration
- sprites, text, layers, and bounded scene objects
- frame animation and translation tracks
- music and SFX tracks
- general input routes
- timeline markers and scene-end route
- inactivity route
- suspend/resume behavior

Rules:

- the timeline and every track have fixed compiled bounds
- no arbitrary loop, recursion, or dynamic code is allowed
- changed compositor regions are presented incrementally where useful
- input routes may transition scenes or invoke bounded symbolic actions
- a scene-end route is required
- the default inactivity route is the scene-end route unless explicitly set to
  another declared `STATE_SCENE`
- Platform selects the lowest validated operating point that meets display,
  audio, input, and timeline deadlines
- sequence playback may defer inactivity only for a statically bounded segment

`SEQUENCE_SCENE` supports cinematics, animated interactions, timed prompts, and
small dynamic scenes without requiring the programmable runtime.

---

## PROGRAM_SCENE

`PROGRAM_SCENE` is the bounded programmable runtime primitive. It uses
`REALTIME` execution and may receive frame-by-frame control through the
PeepOS-approved instruction model.

It may request target-granted services such as:

- realtime display presentation
- active button, encoder, and joystick input
- bounded motion-sensor streams
- active audio
- bounded communication contexts
- frame delta and timeline services

Rules:

- instructions, stack, memory, event work, render work, and per-frame work have
  declared limits
- no direct native code, hardware access, RTOS access, or dynamic code loading
  is allowed
- every program declares suspend/resume behavior
- every program declares an inactivity route to a `STATE_SCENE` or shell
- Platform selects an admitted operating point from measured requirements
- frame overruns and sandbox faults are observable and follow declared failure
  routing

---

## TRANSITION

`TRANSITION` is an authoring and compiled-logic construct, not a scene and not a
display owner.

A transition may:

- evaluate bounded guards
- update `game.*`, `scene.*`, or `entity.*` state
- request a symbolic SFX, save, schedule, or service action
- select one of several declared output scenes
- remain in the current scene after bounded actions
- respond to input, time, sensor, lifecycle, or scene-completion events

Conditional transitions are compiled into bounded event, guard, and action
tables. A condition that reads a service-backed value causes tooling to derive
the corresponding capability and event interest.

---

## Authoring Constructs

Dialogue, Menu, Choice, Pet Room, Clock/Status, and Gameworld are authoring
constructs or prefabs, not runtime scene types.

They normally compile into `STATE_SCENE` data and transitions. Tools may expose
specialized editors and defaults while producing the same public scene schema.

Dialogue text is a rich property with:

- text and localization reference
- font and text region
- wrapping, line spacing, and clipping policy
- compiler-derived page boundaries
- optional explicit page breaks
- advance input
- optional speaker, sprite, and talking animation
- optional choices and transition outputs

Static dialogue should be laid out by tooling. Runtime layout is allowed only
for bounded dynamic substitutions covered by the target profile.

---

## Presentation Timeline

Every settled scene presentation has one backend-neutral timeline record:

```text
presentation_timeline:
  presentation_id
  epoch
  phase_quantum_ms
  phase_count
  current_phase
  next_phase_deadline
  cycle_policy
  rebase_policy
```

The awake renderer, held-frame path, measured wake/update/return path, and
autonomous display backend are consumers of the same timeline.

Rules:

- changing display backend never rebases the presentation epoch
- preparing or committing autonomous playback never repeats the current phase
- the autonomous queue begins with the phase after the committed physical frame
- wake/abort derives the current phase and remaining interval from the retained
  timeline and elapsed time before normal rendering resumes
- wake from a reduced three-step presentation anchors the preferred timeline to
  the same visible phase; after the remaining interval expires, preferred-only
  phases become available again without restarting at phase one
- a new settled scene state may deliberately establish a new epoch only when
  its authored presentation or rebase policy requires one; an ordinary content
  update inside a compatible presentation preserves the current epoch
- input may trigger an authored rebase policy, but the backend handoff may not
- a missed deadline follows the declared deterministic catch-up policy
- phase continuity must be testable in the digital twin and on target

This timeline is the contract that makes `REACTIVE <-> LPBAM` presentation
handoff visually continuous.

HW6 FW0 now exercises this boundary with a compiled-in `STATE_SCENE` vertical
slice. `thRuntime` owns its bounded state and actions, publishes content changes
without changing the compatible timeline identity, and requests rendering;
`thDisplay` owns composition, dirty rows, awake transfer, autonomous compilation,
and phase-preserving handoff. This is implementation evidence for the scene
transaction and ownership split, not yet evidence for package-defined scene
serialization or authoring-tool output.

---

## Runtime Compositor Layers

The runtime compositor has four retained logical layers, top to bottom:

1. `OVERLAY`
2. `UI`
3. `SCENE`
4. `BACKGROUND`

Rules:

- retained layer storage lives outside the autonomous-display SRAM arena
- `BACKGROUND` may use an opaque plane
- transparent layers retain pixel and ownership/mask information
- dirty state is tracked per layer and resolved to final changed panel rows by
  the Engine/Platform renderer
- cheap deterministic bitwise composition produces the committed 1bpp frame
- package tools never control native row packing or physical transfer regions
- `OVERLAY` is Engine/Platform controlled
- packages may provide validated overlay style assets, including an inactive
  border or icon, but cannot suppress mandatory system warnings
- target profiles publish memory and object limits without exposing physical
  SRAM addresses

The active autonomous-display program, descriptors, and transfer scratch use a
Platform-owned SRAM arena. Normal retained layers do not compete for that arena.

---

## Variable And Property Namespaces

Authoring tools expose typed names; compiled packages use stable numeric IDs.

| Namespace | Ownership | Examples |
|---|---|---|
| `system.*` | PeepOS, read-only | `system.time`, `system.uptime`, `system.soc`, `system.step_count` |
| `game.*` | package | `game.health`, `game.score`, `game.current_room` |
| `scene.*` | active scene | scene-local state, selected item, page index |
| `entity.*` | scene object | position, sprite/frame, animation state, visibility, integer scale |

Rules:

- every value has an explicit type, range, lifetime, and persistence policy
- `system.*` reads use consistent Engine snapshots or symbolic update events
- unavailable system values follow capability/fallback rules
- packages cannot write `system.*`
- property access compiles to bounded table operations, not string reflection
- save-backed `game.*` values use the package save schema
- object properties cannot contain pointers, hardware identities, or paths

---

## Capability Derivation

Normal authors select services and behavior, not capability flags.

The compiler derives required and optional capabilities from:

- scene type
- nodes, transitions, guards, and actions
- referenced system variables
- input bindings
- sensor, audio, communication, save, and display services
- cadence, FPS, and latency declarations
- preferred and fallback waiting presentations

The generated package manifest contains the resolved capability declarations.
Advanced tools may show the derivation and allow optional fallback design, but
must not require authors to understand hardware implementation details.

Examples:

- reading `system.step_count` derives `sensor.imu_steps`
- a joystick-vector binding derives `input.joystick_vector`
- a blinking settled presentation derives waiting-animation admission
- music in a sequence derives the corresponding audio capability and budget

---

## Interaction State

PeepOS has a system-owned interaction state independent of CPU sleep:

| State | Meaning |
|---|---|
| `ACTIVE` | Normal package focus and target-admitted controls are available |
| `INACTIVE` | Normal package focus is suppressed and the inactive route/overlay is established |

An active `STATE_SCENE` may spend nearly all of its time in STOP2. `ACTIVE`
does not mean the CPU must remain awake.

Rules:

- PeepOS owns the inactivity timeout and its RTC-backed enforcement
- meaningful admitted user activity refreshes the active interaction window
- passive animation, autonomous playback, and keepalives do not refresh it
- every `SEQUENCE_SCENE` and `PROGRAM_SCENE` declares an inactivity route to a
  `STATE_SCENE` or shell
- a `STATE_SCENE` declares the view preserved or selected when inactivity fires
- inactivity may update the `OVERLAY` and waiting presentation
- joystick vector polling is stopped in `INACTIVE`
- when an active state scene needs cardinal joystick actions, Platform may arm
  wake-and-sleep monitoring until inactivity expires
- inactivity expiry may wake the MCU only long enough to settle the inactive
  state, update presentation/autonomous payload, park inputs, and return to sleep
- only target/system-admitted activation gestures wake and restore normal
  interaction from `INACTIVE`
- the HW6 baseline activation gesture is `START`
- the contract allows future admitted buttons or chords such as `L+R`
- the physical activation gesture is consumed by PeepOS and is not replayed as a
  package action
- packages receive ordered `DEVICE_INACTIVE` and `DEVICE_ACTIVE` lifecycle
  events after scene, focus, and presentation state are valid
- bounded non-interruptible work may defer inactivity only through a validated
  completion bound

Packages may style their inactive presentation and choose among declared
inactive scene routes. They do not choose the timeout, physical wake wiring, or
raw activation-gesture detection.

---

## Validation Requirements

Tools must validate:

- all scene and transition IDs and routes
- scene-type-specific bounds
- `STATE_SCENE` reactive yield and waiting-presentation completeness
- `SEQUENCE_SCENE` timeline, FPS, duration, track, and scene-end bounds
- `PROGRAM_SCENE` instruction, stack, memory, frame, service, and fallback bounds
- inactivity routes for every realtime scene
- target-admitted activation and input behavior
- compositor layer and retained-memory budgets
- waiting-animation phase quantum, cycle, fallback, and autonomous admission
- derived capability closure and optional fallbacks
- variable/property types, ranges, lifetimes, and persistence
- deterministic output for a fixed project, toolchain, profile, and input trace

No package or authoring artifact may expose HAL, RTOS, raw hardware, clocks,
STOP mode, DMA, LPBAM, physical SRAM, panel rows, or filesystem runtime paths.
