# State Machine Architecture

Authoritative specification for finite state machines (FSM) used in
PeepShow V5, including the pet core loop and other deterministic
state-driven subsystems.

This document defines how FSMs are authored, generated, and integrated
into the runtime without dynamic allocation.

If implementation differs from this document, the implementation is wrong.

---

## Scope

Defines:
- FSM authoring workflow
- Code generation model
- Separation of generated and user logic
- Runtime integration rules
- Determinism and safety constraints

Does NOT define:
- Game scenario lifecycle (see game_engine.md)
- Thread model (see rtos_architecture.md)

---

## Design Principles

- FSM structure is generated, behavior is user-implemented.
- No dynamic memory allocation.
- Regeneration must be safe and idempotent.
- FSM execution must be deterministic.
- FSM transitions must be explicit and traceable.

---

## Authoring Workflow

FSMs are authored visually in yEd.

Node label format:
STATE:STATE_NAME

Edge label format:
EV:EVENT_NAME

Example:

STATE:PET_IDLE
STATE:PET_FEED

EV:FEED_SELECTED
EV:ANIM_DONE

The graph defines:
- State identifiers
- Event identifiers
- Valid transitions

No logic is authored in the graph.

---

## Code Generation Model

Input:
- GraphML exported from yEd

Generator produces:

1. Generated header
   - state enum
   - event enum
   - transition table struct

2. Generated C file
   - static transition table
   - dispatch function
   - weak default handlers

Generated files are disposable.

User files are permanent.

---

## Generated Structure Layer

Generated files contain:

- enum state_id
- enum event_id
- transition table array
- dispatch glue
- weak empty state handlers

Weak handlers prevent linker errors if user has not implemented a state.

Generated files must not be edited manually.

---

## User Behavior Layer

User file implements:

- enter_STATE()
- update_STATE()
- exit_STATE()

User file must:

- Contain only behavior logic.
- Not redefine enums.
- Not modify transition table.

User file must not be overwritten by generator.

---

## Runtime Integration

FSM context struct contains:

- current_state
- optional state-local data
- pointer to owning subsystem

Dispatch flow:

1. Event passed to fsm_dispatch(ctx, event).
2. Transition table searched.
3. If match:
   - call exit(old_state)
   - update current_state
   - call enter(new_state)

update() is called explicitly by owning subsystem.

FSM must not:
- Call HAL directly.
- Block on queues.
- Create RTOS objects.

---

## Runtime Integration (Scene Requests)

FSMs may emit high-level requests (including scene changes), but do not execute them directly.

Rule:
- FSM may emit `EVT_SCENE_LOAD_REQUEST` (and similar “request” events).
- The owning subsystem / engine consumes the request and performs the action outside the FSM.
- FSM remains deterministic: no HAL/RTOS calls, no blocking, no rendering effects.

---

## Pet Core FSM

The pet runtime (STOP mode) is driven by an FSM.

Tick source:
- RTC alarm (1 Hz)

On tick:
- pet_needs_catchup(now)        // apply elapsed-time decay once
- fsm_update(ctx)               // state behavior may emit events
- events may call pet_apply_event(...)

FSM must remain lightweight and bounded.

---

### Shared Pet Model Contract (STOP/STATIC <-> REALTIME)

The pet core FSM does NOT own pet stats.

There is a single authoritative pet model (pet_state_t) shared across:
- STOP/STATIC pet loop (this FSM)
- REALTIME gameplay entity (playable pet character)

FSM actions must update the pet model via the pet event API:

- pet_apply_event(type, value, source)

FSM must NOT:
- maintain a second copy of hunger/energy/mood/etc
- apply time-decay independently of the shared timebase

Time-based updates must use catch-up:

- pet_needs_catchup(now)

This prevents double-counting when switching between STOP/STATIC and REALTIME.

---

## Scene Changes + Visual Transitions

FSMs (including the pet core FSM) may **request** a scene change, but must NOT implement
visual effects (wipe/fade) directly.

Rule:
- FSM outputs a **request** only:
  - `next_scene_id`
  - `transition_type` (WIPE_* / FADE_*)
  - `duration_ms`

Responsibility split:
- FSM: decides *when* to change scenes (emits request/event)
- Engine (scene manager): owns transition timing/state + performs the scene swap
- Renderer: performs the pixel effect (clip wipes, 1-bit dither/noise dissolves)

Recommended integration:
- FSM emits `EVT_SCENE_LOAD_REQUEST` with payload:
  `{ scene_id, transition_type, duration_ms }`
- Engine consumes the event and runs the transition.
- FSM continues running deterministically (no effect rendering, no timing hacks).

---

## Mode Switch Requests (Including Zoom)

FSMs may request mode switches (REALTIME <-> STOP/STATIC), including a zoom transition,
but must NOT implement zoom rendering.

Rule:
- FSM outputs a REQUEST only:
  - mode target (ENTER_STOP / EXIT_STOP)
  - transition_type (ZOOM_IN_TO_PET / ZOOM_OUT_TO_GAME)
  - duration_ms

Focal point rule:
- FSM does not compute zoom geometry.
- Engine derives the focal point from the active pet companion entity at request time
  (screen-space position), and the renderer performs the zoom.

Recommended integration:
- FSM emits EVT_MODE_SWITCH_REQUEST with payload:
  { target_mode, transition_type, duration_ms }
- Engine consumes the request, captures focal point, and runs the zoom transition.

---

## Determinism Requirements

- Same event sequence must produce identical state sequence.
- No hidden side effects.
- No reliance on global mutable state outside context.
- No dynamic allocation.
- No recursion.

---

## Debugging Hooks

Optional:
- Emit SWO event on state transition.
- Optionally emit SWO markers for pet events (EAT/SLEEP/etc) at low rate
- Include state ID and event ID.

FSM must not spam logs.

---

## Invariants (Do Not Violate)

- Generated files are disposable.
- User files are permanent.
- FSMs must be deterministic.
- No dynamic allocation.
- No HAL inside FSM logic.
- All transitions defined explicitly in graph.

---

Last updated: 2026-02-18
