# Runtime Logic and State API Contract

This document defines the package-facing runtime logic model used by PeepOS game-authoring tools, package data, Engine runtime hosts, and the digital twin.

Runtime logic is the layer that turns validated game content into bounded state, event, action, and frame behavior.

Related:

- [[Engine_API_Index]]
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
- runtime unit logic requirements
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
validated scenes / graphs / modules / scripts
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
  runtime_units[]
    scenes / modules / graphs
      states / substates
        transitions
          guards
          bounded action lists
```

Authoring tools may present richer editors, hierarchy, visual scripting, dialogue trees, map triggers, pet behavior trees, or scene timelines.

Compiled package output must reduce those forms to bounded PeepOS runtime logic primitives.

Rules:

- every runtime unit has one entry point
- every transition target is declared before package compilation/export
- every action list has a bounded maximum cost
- every event queue, timer table, and variable table has bounded size
- no runtime logic may create threads, tasks, RTOS queues, hardware timers, or direct callbacks
- no runtime logic may call Platform hardware APIs directly

---

## Runtime Classes

Runtime classes define execution and power shape, not game genre.

| Runtime Class | Logic Shape | Expected Use |
|---|---|---|
| `LP_GRAPH` | reactive event, schedule, and state transactions | long-running games, clocks, pets, idle toys, and Game & Watch logic that sleep between events |
| `LP_MODULE` | Engine-hosted bounded reactive block shape | menus, dialogue, map viewers, inventory, and structured modules that yield between interactions |
| `RT_SCENE` | frame-paced realtime scene | action scenes, microgames, realtime maps, higher-rate interaction |

`RT_SCENE` is more demanding than graph/module logic. It may request richer per-frame behavior, but it must declare more constraints before validation can accept it.

---

## Reactive Transaction And Block Contract

`REACTIVE` is the default execution semantic for `LP_GRAPH` and `LP_MODULE`. It is not a separate runtime class.

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
  input_lock_context_ref
```

Rules:

- `waiting_visual_ref` describes desired display behavior while the state waits; it does not select LPBAM or a sleep mode
- holding a frame and animating a waiting visual are both valid reactive waits
- autonomous display playback is cosmetic and cannot mutate package variables or advance graph state
- a designer-authored inactivity transition is a normal bounded schedule/transition in the state graph
- an `input_lock_context_ref` can activate only prevalidated meaningful-activity or bounded-deferral entries from the package lock policy
- hardware sleep, display backend selection, and wake-source arming remain Platform policy
- custom code handles an event and returns; it may not call sleep, start autonomous playback, or busy-wait for input
- if a waiting visual cannot be admitted, its declared reduced visual or hold fallback is used

---

## LP_GRAPH Requirements

`LP_GRAPH` is the preferred reactive primitive for long-running packages.

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
- declared transitions to other runtime units

Rules:

- no polling loop
- no high-rate sensor stream
- no active communication receive dependency on HW6 profiles while communication wake remains blocked
- no frame-paced realtime update requirement
- all timers and schedules must tolerate cadence clamp, missed wake, and bounded catch-up
- every settled state must declare or inherit a reactive wait contract: admitted events/schedules, waiting-visual intent, and fallback

`LP_GRAPH` should be powerful enough for complete games. Its restriction is power and boundedness, not style or genre.

---

## LP_MODULE Requirements

`LP_MODULE` is an Engine-hosted reactive module with a predefined bounded transaction shape.

Examples:

- dialogue module
- menu module
- map inspection module
- turn-based encounter module
- inventory/status module
- clock or schedule module

Rules:

- `module_type` must be approved by the Engine contract
- module config must be validated before package compilation/export
- host-defined update cadence and action limits apply
- module must declare reactive wait, suspend/resume, and failure-fallback behavior
- module must declare allowed runtime-unit transitions
- module may not contain arbitrary unbounded code

`LP_MODULE` exists to make common structured gameplay easier without exposing RTOS or hardware control.

---

## RT_SCENE Requirements

`RT_SCENE` is the package-facing primitive for active frame-paced scenes.

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
- bounded input-lock deferral declarations where used
- suspend behavior
- resume behavior
- reactive fallback runtime unit
- realtime cadence/power intent

Rules:

- an enabled PeepOS input-lock policy applies according to the package declaration and selected target profile
- realtime work must stop, suspend, or transition when no meaningful activity remains
- frame logic must not block on storage, communication, save writes, or hardware completion
- overruns must be observable through diagnostics where the active profile allows
- `RT_SCENE` must expose a declared reactive fallback and must leave frame-paced execution when an enabled input lock activates

`RT_SCENE` has no fixed maximum active duration at this contract level. It may remain active while meaningful user activity or Platform-approved active work continues.

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
  input_lock_policy_ref
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
- graph-local variables must declare type, size, reset behavior, and persistence behavior

---

## Event Model

Package-visible events are symbolic Engine events.

Allowed event classes:

| Event Class | Source Contract |
|---|---|
| lifecycle | runtime host mount/start/suspend/resume/lock/unlock/stop/unmount |
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
- event queue depth is bounded by runtime class and target profile
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
- push, pop, or replace runtime unit through declared edges
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
| unit-local | runtime unit state | survives within mounted unit lifecycle |
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

---

## Tick And Scheduling Semantics

Runtime logic receives work through events and approved ticks.

| Runtime Class | Tick Semantics |
|---|---|
| `LP_GRAPH` | no free-running tick; reactive event/schedule/wake transactions |
| `LP_MODULE` | host-defined bounded reactive transactions; yields between admitted events |
| `RT_SCENE` | frame tick while realtime activity remains valid |

Rules:

- reactive logic must not emulate an awake loop or display animation with high-frequency delayed events
- repeated timers must declare maximum cadence and catch-up behavior
- `RT_SCENE` frame delta comes from Engine time model, not hardware timer registers
- missed ticks must be handled through bounded catch-up or discard policy
- Platform may clamp, coalesce, delay, or suppress work according to power policy

---

## Runtime Unit Transitions

Runtime unit transitions use the package model defined in [[Package_Contract]].

Allowed transition forms:

```text
transition_to(unit_id)
push_unit(unit_id)
pop_unit()
exit_to_shell(reason)
```

Rules:

- transition targets must be declared
- transition stack depth is bounded
- recursive push loops are invalid unless statically bounded and approved by validation
- transition actions are bounded
- active contexts must be released, suspended, or transferred according to their contracts
- input focus must be released or transferred during transition
- realtime units must declare fallback routing before validation accepts them

---

## Fault And Failure Semantics

Package logic failures are separate from Platform hardware failures.

Package logic failures include:

- invalid transition target reached through corrupted package data
- action budget exceeded
- frame budget exceeded
- missing required asset after validation
- unhandled runtime unit failure
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
- same runtime unit transitions
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
- runtime unit declarations are valid
- runtime class requirements are satisfied
- action table length and cost are bounded
- expression cost is bounded
- timer cadence and catch-up policy are valid
- variable size and persistence class are valid
- event queue bounds are valid
- capability use is declared
- power compliance is satisfied
- `RT_SCENE` frame budget, meaningful-activity sources, bounded lock deferrals, suspend/resume behavior, and reactive fallback unit are declared
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

1. valid `LP_GRAPH` state graph validates and runs from entry node.
2. missing entry state fails package validation.
3. transition to undeclared state fails package validation.
4. transition to undeclared runtime unit fails package validation.
5. unbounded action loop fails validation in every build profile.
6. `LP_GRAPH` high-frequency polling timer fails validation.
7. `LP_GRAPH` with bounded calendar schedule and catch-up policy validates.
8. `LP_MODULE` without approved `module_type` fails validation.
9. `RT_SCENE` without frame budget fails validation.
10. `RT_SCENE` without a reactive fallback fails validation.
11. reactive state without a resolvable wait contract fails validation.
12. unbounded input-lock deferral fails validation.
13. `RT_SCENE` frame overrun emits diagnostics where profile allows and follows lifecycle policy.
14. suspend/resume during active runtime logic preserves or reconstructs package state according to declared persistence classes.
15. package logic cannot receive hardware owner faults as normal gameplay branches.
16. digital twin replay of a fixed input/time/sensor trace produces identical state and diagnostics output.

---

## Rule

PeepOS runtime logic is event, state, action, and frame behavior.

It is not RTOS ownership, hardware control, or Platform policy.
