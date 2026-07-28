# Architecture and Boundaries

This is the core architecture contract for PeepShow across hardware targets. HW6 is active; target-specific hardware facts are selected through [[Hardware_Index]].

PeepShow is split into three conceptual layers:

1. Platform
2. Engine
3. Reference Game

The device, repository, tools, and documentation must remain game-agnostic below the Reference Game layer.

---

## Layer Model

```text
Reference Game
    consumes Engine APIs

Engine
    consumes Platform capabilities

Platform
    owns hardware behavior
```

The Reference Game is the proof-of-capability game being built for PeepShow. It is a real game, but it is not the source of truth for Platform or Engine behavior.

---

## 1) Platform

The Platform is the console layer.

It owns:

- hardware bring-up
- RTOS object graph and thread ownership
- power policy and mode transitions
- wake source arming and wake classification
- display, audio, input, sensor, storage, USB, communication, and debug ownership
- low-level rendering infrastructure
- storage and installer ownership
- shell, settings, calibration, and device-management flows
- hardware-facing validation contracts

The Platform exposes capabilities and abstractions only.

Examples:

- logical input events
- framebuffer and present APIs
- storage/package read APIs
- audio cue/stream requests
- sensor snapshots
- mode-transition requests
- wake-intent handling

The Platform must not contain assumptions about:

- creatures
- encounters
- game mechanics
- lore
- world logic
- balancing
- Reference Game state

---

## 2) Engine

The Engine sits between Platform and Game.

It owns reusable game-development infrastructure:

- runtime host lifecycle
- `REACTIVE` and `REALTIME` execution semantics
- scene and state-machine runtime abstractions
- reactive wait, waiting-visual, meaningful-activity, and optional input-lock intent contracts
- rendering abstractions above the display owner
- input focus and routing abstractions
- package/content contracts
- asset loading/runtime contracts
- game SDK and authoring contracts
- package manager and activation logic

The Engine may understand gameplay concepts abstractly.

It must not depend on the Reference Game.

Examples of valid Engine concepts:

- scene
- actor/entity
- animation
- input action
- asset ID
- symbolic audio cue
- capability requirement
- save schema
- runtime class
- reactive wait
- waiting visual
- input-lock policy

Examples of invalid Engine coupling:

- hardcoding a PeepShow creature type
- assuming a specific Reference Game map
- embedding Reference Game encounter rules
- changing Platform sleep policy for a game mechanic

---

## 3) Reference Game

The Reference Game is the PeepShow proof-of-capability game.

It owns:

- creatures
- world design
- mechanics
- lore
- creature behavior design
- balancing
- encounters
- progression
- game-specific UI flow
- game-specific FSMs
- content and tuning

The Reference Game consumes Engine and Platform APIs.

It must not directly own hardware behavior.

If a game idea needs a capability, it should express intent and link to an Engine API requirement.

---

## Ownership Rules

- Every peripheral and shared datapath has exactly one Platform owner.
- Other layers use typed requests, events, or approved APIs only.
- Requests must be bounded, deterministic, and versioned where needed.
- No Game or Engine path may bypass Platform owners.
- No Reference Game note may redefine Platform policy.

---

## Intent Boundary

The Reference Game and Engine may request:

- cadence hints
- wake intents
- scene/frame rendering presentation
- symbolic audio cues
- asset reads
- save reads/writes through approved APIs
- logical input actions

The Platform decides:

- sleep depth
- clock profile
- wake-source arming
- IRQ versus poll behavior
- DMA and transfer method
- storage arbitration
- peripheral recovery behavior

---

## Forbidden Coupling Patterns

- Game code directly touching HAL/LL handles.
- Game notes defining pin ownership, DMA policy, or sleep policy.
- Platform owners containing creature, encounter, or world-state logic.
- Engine code hardcoded to one Reference Game scene or mechanic.
- Storage APIs exposing game-scene-specific operations.
- Power policy depending on Reference Game state instead of abstract intent.

---

## Required Artifacts For Any New Subsystem

Before implementation starts, each subsystem must have:

1. owner declaration
2. public request/API schema
3. explicit state machine
4. mode behavior table
5. failure and recovery policy
6. validation/evidence requirement

Subsystem notes should link back to [[Authority_and_Invariants]], relevant [[Hardware_Index]] notes, and any Engine API contracts they expose.
