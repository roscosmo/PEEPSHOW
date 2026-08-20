# Runtime Logic and State API Contract

This document defines the package-facing runtime logic model used by PeepOS game-authoring tools, package data, Engine runtime hosts, and the digital twin.

Runtime logic is the layer that turns validated game content into bounded state, event, action, and frame behavior.

Related:

- [[Engine_API_Index]]
- [[Scene_Runtime_and_Interaction_Model]]
- [[Game_Authoring_API_Contract]]
- [[Runtime_Host_Contract]]
- [[Runtime_Host_Internal_State_Machines]]
- [[Package_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[Time_And_Power_Intent_API_Contract]]
- [[Input_Focus_API_Contract]]
- [[Rendering_API_Contract]]
- [[Diagnostics_API_Contract]]

---

## Scope

Defines:

- runtime logic primitives exposed to packages and authoring tools
- state graph and action table behavior
- package-visible event model
- package scene logic requirements
- validation rules for bounded execution
- digital twin replay expectations

Does not define:

- Platform hardware policy
- RTOS object ownership
- Platform owner-thread internals
- HAL, LL, CubeMX, DMA, interrupt, filesystem, or peripheral behavior
- Reference Game mechanics

---

## Core Principle

Packages do not own threads, RTOS objects, timers, queues, interrupts, or hardware loops.

Packages express:

- states
- events
- guards
- actions
- variables
- bounded ticks
- declared transitions
- lifecycle behavior

The Engine runs those primitives inside runtime hosts.

The Platform owns hardware behavior.

```text
game-dev tools
    |
validated scenes / graphs / timelines / programs
    |
runtime logic tables
    |
Engine runtime host
    |
Platform capability contracts
```

---

## Runtime Logic Model

Runtime logic is organized as:

```text
package
  scenes[]
    state graphs / sequence timelines / bounded programs
      states / substates
        transitions
          guards
          bounded action lists
```

Authoring tools may present richer editors, hierarchy, visual scripting, dialogue trees, map triggers, pet behavior trees, or scene timelines.

Compiled package output must reduce those forms to bounded PeepOS runtime logic primitives.

Rules:

- every package has one entry scene and every scene has one entry point
- every transition target is declared before package compilation/export
- every action list has a bounded maximum cost
- every event queue, timer table, and variable table has bounded size
- no runtime logic may create threads, tasks, RTOS queues, hardware timers, or direct callbacks
- no runtime logic may call Platform hardware APIs directly

---

## Scene Types And Execution

Scene type defines the package runtime primitive. Execution semantic defines
whether it yields or remains frame-paced.

| Scene Type | Execution | Logic Shape |
|---|---|---|
| `STATE_SCENE` | `REACTIVE` | event, schedule, state, and bounded action transactions |
| `SEQUENCE_SCENE` | `REALTIME` | validated data-driven frame and audio timeline |
| `PROGRAM_SCENE` | `REALTIME` | bounded sandboxed instruction program with per-frame control |

See [[Scene_Runtime_and_Interaction_Model]] for the canonical behavior.

---

## Reactive Transaction And Block Contract

`REACTIVE` is the fixed execution semantic for `STATE_SCENE`. It is not a
scene type.

A reactive transaction begins with one admitted symbolic event and performs bounded state, action, rendering, and owner requests. The transaction yields when its action chain is settled. PeepOS then sleeps immediately until the next admitted event; no package-visible low-power transition or inactivity delay is required.

Authoring templates and Authoring Kits such as Menu, Dialogue, Inventory, Turn-Based Encounter, Pet Idle, and Clock compile into reactive blocks. A compiled block/state provides:

```text
reactive_wait:
  waiting_visual_ref
  waiting_visual_fallback_ref
  event_interests[]
  delayed_or_calendar_schedules[]
  wake_intents[]
  logical_timeout_transitions[]
  interaction_context_ref
```

Rules:

- `waiting_visual_ref` describes desired display behavior while the state waits; it does not select LPBAM or a sleep mode
- holding a frame and animating a waiting visual are both valid reactive waits
- autonomous display playback is cosmetic and cannot mutate package variables or advance graph state
- a designer-authored inactivity transition is a normal bounded schedule/transition in the state graph
- an `interaction_context_ref` can select only prevalidated meaningful-activity, inactive-route, overlay-style, or bounded-deferral entries
- hardware sleep, display backend selection, and wake-source arming remain Platform policy
- custom code handles an event and returns; it may not call sleep, start autonomous playback, or busy-wait for input
- if a waiting visual cannot be admitted, its declared reduced visual or hold fallback is used

---

## STATE_SCENE Requirements

`STATE_SCENE` is the preferred reactive primitive for long-running packages.

It supports:

- state and substate graphs
- lifecycle events
- input action events
- delayed events
- local-calendar schedule events
- low-rate sensor events
- animation completion events
- audio timeline events where supported
- save/settings completion events
- bounded action tables
- declared transitions to other package scenes

Rules:

- no polling loop
- no high-rate sensor stream
- no active communication receive dependency on HW6 profiles while communication wake remains blocked
- no frame-paced realtime update requirement
- all timers and schedules must tolerate cadence clamp, missed wake, and bounded catch-up
- every settled state must declare or inherit a reactive wait contract: admitted events/schedules, waiting-visual intent, and fallback

`STATE_SCENE` should be powerful enough for complete low-rate games. Its
restriction is power and boundedness, not style or genre. Dialogue, Menu,
Choice, Inventory, and similar authoring constructs compile into this same
primitive rather than requiring a separate module class.

---

## SEQUENCE_SCENE Requirements

`SEQUENCE_SCENE` is a data-driven frame-paced primitive. It declares fixed
timeline tracks, FPS, duration or bounded loop policy, input routes, audio
tracks, scene-end route, inactivity route, and suspend/resume behavior. It may
not execute arbitrary per-frame instructions.

Rules:

- all tracks and loops are statically bounded
- frame and event work fit the selected target profile
- a scene-end route is required
- an inactivity route to `STATE_SCENE` or shell is required
- changed-region rendering is used where useful
- Platform selects the lowest measured operating point that meets deadlines

---

## PROGRAM_SCENE Requirements

`PROGRAM_SCENE` is the package-facing primitive for programmable active
frame-paced scenes.

It may use:

- frame tick
- active input focus
- realtime render commands
- bounded animation updates
- bounded high-rate sensor contexts where target profile grants them
- active audio contexts
- communication contexts where target profile grants them
- scene-local transient variables

Required declarations:

- target frame rate
- frame budget
- maximum update cost
- maximum render command count
- maximum event processing cost per frame
- asset preparation/preload requirements
- input focus scope
- sensor/audio/communication contexts
- meaningful-activity sources
- bounded inactivity-deferral declarations where used
- suspend behavior
- resume behavior
- inactive `STATE_SCENE` or shell route
- realtime cadence/power intent

Rules:

- PeepOS interaction-state policy always applies
- realtime work must stop, suspend, or transition when inactivity is admitted
- frame logic must not block on storage, communication, save writes, or hardware completion
- overruns must be observable through diagnostics where the active profile allows
- `PROGRAM_SCENE` must expose a declared inactivity route and leave frame-paced execution before `INACTIVE` is established

`PROGRAM_SCENE` has no fixed maximum active duration at this contract level. It
may remain active while meaningful user activity or Platform-approved work
continues.

---

## State And Substate Model

Authoring tools may expose nested states, substates, and grouped graph regions.

Compiled runtime data must provide:

```text
state_graph:
  graph_id
  entry_node
  states[]
  transitions[]
  timers[]
  local_variables[]
  action_tables[]
  reactive_wait_tables[]
  interaction_policy_ref
  bounds
```

Rules:

- every graph has one entry node
- every state ID is stable within the graph
- every transition target exists
- entry/exit actions are bounded
- transition actions are bounded
- hierarchical authoring must compile to deterministic runtime tables
- parallel state regions are allowed only if their scheduling and action cost are statically bounded
- every state that can settle without transitioning must resolve a reactive wait contract
- gameplay inactivity timers compile as normal delayed events and state transitions, not PeepOS lock settings
- graph-local variables and scene/entity properties must declare type, size, range, reset behavior, and persistence behavior

---

## Event Model

Package-visible events are symbolic Engine events.

Allowed event classes:

| Event Class | Source Contract |
|---|---|
| lifecycle | runtime host mount/start/suspend/resume/inactive/active/stop/unmount |
| input action | [[Input_Focus_API_Contract]] |
| delayed timer | [[Time_And_Power_Intent_API_Contract]] |
| local calendar schedule | [[Time_And_Power_Intent_API_Contract]] |
| wake/resume reason | [[Time_And_Power_Intent_API_Contract]] |
| render/animation completion | [[Rendering_API_Contract]] |
| audio timeline marker | [[Audio_API_Contract]] |
| sensor event/snapshot availability | [[Sensor_API_Contract]] |
| communication session/message | [[Communication_API_Contract]] |
| save/settings completion | [[Package_Save_Settings_API_Contract]] |
| package diagnostic/fault routing | [[Diagnostics_API_Contract]] |

Rules:

- event payloads are fixed-schema and bounded
- event queue depth is bounded by scene type and target profile
- event dispatch order must be deterministic for a fixed input trace
- overflow behavior must be declared and validated
- hardware faults are not ordinary gameplay events
- required Platform primitive failure routes through Engine lifecycle and Platform diagnostics

---

## Guards And Expressions

Guards are bounded expressions used to select transitions and actions.

Allowed inputs:

- graph variables
- package settings
- save-backed values read through schema
- event payload fields
- resolved sensor values
- local calendar/logical time
- capability state exposed by Engine contracts
- deterministic random source where a package seed policy is declared

Rules:

- expression cost is statically bounded
- types are explicit
- numeric ranges are explicit
- no recursion
- no unbounded loops
- no dynamic code loading
- no direct memory access
- no direct calls to Platform, HAL, RTOS, filesystem, or middleware APIs

---

## Actions

Runtime actions are symbolic requests to Engine contracts.

Allowed action categories:

- set or clear graph variable
- transition state
- transition package scene through declared edges
- request draw/update through [[Rendering_API_Contract]]
- request input focus change through [[Input_Focus_API_Contract]]
- request audio cue or BBB pattern through [[Audio_API_Contract]]
- request save/settings read or write through [[Package_Save_Settings_API_Contract]]
- request delayed event, calendar schedule, cadence, reactive waiting visual, or power intent through [[Time_And_Power_Intent_API_Contract]]
- request or release sensor context through [[Sensor_API_Contract]]
- request communication session/message behavior through [[Communication_API_Contract]]
- emit package diagnostics through [[Diagnostics_API_Contract]]
- raise package fault code

Rules:

- actions are bounded and non-blocking
- actions return package-visible completion events where the called contract defines them
- action tables must not contain hardware commands
- action tables must not contain host filesystem paths
- action tables must not contain function pointers or raw memory pointers
- action tables must not spin, sleep, busy-wait, retry forever, or block on I/O

---

## Variables And Persistence

Runtime logic may use several variable classes.

| Variable Class | Purpose | Durability |
|---|---|---|
| transient | current state/event calculation | lost on stop/unmount |
| scene-local | active scene state | survives while the scene remains mounted or retained by declared policy |
| fast-resume | small STOP-resume state where profile supports it | retained only across supported low-power resume |
| save-backed | durable package state | persisted through save schema |
| package setting | package-owned user preference | persisted through save/settings schema |

Rules:

- all variables have declared type, size, and bounds
- save-backed variables must map to [[Package_Save_Settings_API_Contract]]
- fast-resume state is not durable storage
- retained snapshots must be versioned and integrity checked where used
- packages must tolerate fast-resume loss by restoring from durable save/default state
- variables may not contain raw pointers, host paths, hardware addresses, or private struct layouts
- `system.*` values are read-only consistent Engine snapshots or symbolic event fields
- `game.*`, `scene.*`, and `entity.*` values compile to stable typed IDs, not runtime string reflection

---

## Tick And Scheduling Semantics

Runtime logic receives work through events and approved ticks.

| Scene Type | Tick Semantics |
|---|---|
| `STATE_SCENE` | no free-running logic tick; reactive event/schedule/wake transactions |
| `SEQUENCE_SCENE` | validated timeline frame tick while the scene remains active |
| `PROGRAM_SCENE` | sandboxed program frame tick while realtime activity remains valid |

Rules:

- reactive logic must not emulate an awake loop or display animation with high-frequency delayed events
- repeated timers must declare maximum cadence and catch-up behavior
- realtime frame delta comes from the Engine time model, not hardware timer registers
- missed ticks must be handled through bounded catch-up or discard policy
- Platform may clamp, coalesce, delay, or suppress work according to power policy

---

## Scene Transitions

Scene transitions use the package model defined in [[Package_Contract]].

Allowed transition forms:

```text
transition_to(scene_id)
push_scene(scene_id)
pop_scene()
exit_to_shell(reason)
```

Rules:

- transition targets must be declared
- transition stack depth is bounded
- recursive push loops are invalid unless statically bounded and approved by validation
- transition actions are bounded
- active contexts must be released, suspended, or transferred according to their contracts
- input focus must be released or transferred during transition
- realtime scenes must declare inactivity routing before validation accepts them

---

## Fault And Failure Semantics

Package logic failures are separate from Platform hardware failures.

Package logic failures include:

- invalid transition target reached through corrupted package data
- action budget exceeded
- frame budget exceeded
- missing required asset after validation
- unhandled scene failure
- package-declared fault code

Platform hardware failures include:

- sensor owner fault
- display owner fault
- storage owner fault
- audio owner fault
- communication owner fault
- wake/power fault

Rules:

- package logic faults route through Engine lifecycle policy and package diagnostics
- Platform hardware faults route through Platform diagnostics and lifecycle policy
- primitives granted by the selected target profile are assumed available after validation; if they fail, that is not ordinary game logic
- normal package tools should report failures in PeepOS authoring language

---

## Digital Twin Requirements

The digital twin must execute or faithfully mirror the same runtime logic contract.

Required behavior:

- same state graph data
- same action tables
- same scene transitions
- same event ordering for a fixed trace
- same cadence clamp behavior from the selected target profile
- same save/schema behavior
- same diagnostics behavior for a fixed trace
- deterministic replay support

The twin may expose richer inspection tools, but those tools must not become package runtime APIs.

Digital twin evidence can validate runtime logic and package behavior. It cannot validate HW6 electrical, timing, current, peripheral, or storage-media behavior.

---

## Validation Requirements

Tooling must validate runtime logic before package compilation/export.

Required checks:

- entry point exists
- every state/transition/action reference resolves
- scene declarations are valid
- scene-type requirements are satisfied
- action table length and cost are bounded
- expression cost is bounded
- timer cadence and catch-up policy are valid
- variable size and persistence class are valid
- event queue bounds are valid
- capability use is declared
- power compliance is satisfied
- realtime scene frame/timeline budget, meaningful-activity sources, bounded inactivity deferrals, suspend/resume behavior, and inactive route are declared
- every reactive state resolves a waiting visual, event interests, and fallback
- asset, save, input, sensor, audio, communication, time, power, and diagnostics references resolve
- generated logic contains no hardware, RTOS, filesystem, Platform-internal, or host-path references

Validation failures that affect runtime safety, determinism, storage integrity, or power policy block package compilation/export in every profile.

---

## Forbidden Runtime Logic Constructs

Normal package runtime logic must not contain:

- unbounded loops
- unbounded recursion
- dynamic code loading
- package-created threads
- RTOS object creation
- blocking waits without timeout
- busy-wait loops
- hardware interrupt handlers
- raw filesystem paths
- raw pointers or function pointers
- hardware registers
- HAL, LL, CubeMX, ThreadX, FileX, LevelX, USBX, or Platform-internal API references

Any generated artifact containing these constructs must fail internal safety verification.

---

## Validation Cases

1. valid `STATE_SCENE` graph validates and runs from its entry node.
2. missing entry state fails package validation.
3. transition to undeclared state fails package validation.
4. transition to undeclared scene fails package validation.
5. unbounded action loop fails validation in every build profile.
6. `STATE_SCENE` high-frequency polling timer fails validation.
7. `STATE_SCENE` with bounded calendar schedule and catch-up policy validates.
8. `SEQUENCE_SCENE` with an unbounded timeline loop fails validation.
9. `PROGRAM_SCENE` without frame budget fails validation.
10. realtime scene without an inactivity route fails validation.
11. reactive state without a resolvable wait contract fails validation.
12. unbounded inactivity deferral fails validation.
13. `PROGRAM_SCENE` frame overrun emits diagnostics where profile allows and follows lifecycle policy.
14. suspend/resume during active runtime logic preserves or reconstructs package state according to declared persistence classes.
15. package logic cannot receive hardware owner faults as normal gameplay branches.
16. digital twin replay of a fixed input/time/sensor trace produces identical state and diagnostics output.

---

## Rule

PeepOS runtime logic is event, state, action, and frame behavior.

It is not RTOS ownership, hardware control, or Platform policy.
