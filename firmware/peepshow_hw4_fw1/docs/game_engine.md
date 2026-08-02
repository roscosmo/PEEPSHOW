# Game Engine

Authoritative specification for the REALTIME runtime, scenario lifecycle,
input focus model, entity architecture, map metadata usage, and world render
contract in PeepShow V5.

This document defines how gameplay executes at 30 FPS,
how input is interpreted, how scenes are structured, and how world pixels
are produced prior to presentation.

If implementation differs from this document, the implementation is wrong.

---

## Scope

Defines:
- REALTIME runtime structure
- Scenario lifecycle contract
- Focus stack input model
- Action interpretation rules
- Entity update discipline
- Map metadata integration
- Lighting model (2bpp world surface)
- World viewport semantics for 1× and 2× modes

Does NOT define:
- Thread ownership (see rtos_architecture.md)
- Clock scaling (see power_management.md)
- Display DMA behavior (see display_and_rendering.md)
- Bitmap encoding (see asset_pipeline.md)

---

## Design Principles

- Deterministic 30 FPS runtime.
- Input interpreted only at intent level.
- No HAL calls inside gameplay logic.
- No dynamic allocation.
- Frame budget discipline is mandatory.
- Rendering and simulation are separated.
- Gameplay produces 2bpp world pixels, never panel-native bits.

---

## REALTIME Mode Overview

REALTIME is a fixed cadence mode:

Target frame rate:
30 FPS

Target frame time:
33.33 ms

The frame loop is driven externally (TIM2 ISR sets a frame flag).

thGame consumes the frame flag and executes one frame per tick.

---

## Frame Execution Discipline

Each frame:

1. Drain bounded ActionEvents from qGameEvents
2. Dispatch through focus stack
3. Update active scenario (simulation step)
4. Render world into world2bpp surface
5. Renderer presents world to 1bpp framebuffer
6. Compose UI
7. Request at most one present

Constraints:

- No blocking waits.
- No clock manipulation.
- No direct peripheral access.
- No RTOS object creation.
- No more than one present per frame.
- Gameplay must not write directly to 1bpp panel planes.

---

## Scenario Contract

Each scenario must implement:

- init()
- shutdown()
- update(delta_time)
- render()
- on_action(ActionEvent*)

Rules:

- update() performs simulation only.
- render() writes into world2bpp surface only.
- on_action() returns:
  - true  → consumed
  - false → not handled
- No HAL calls.
- No queue reads.
- No memory allocation.
- No peripheral control.
- No direct writes to panel framebuffer.

Scenarios are pure logic modules.

---

## Scenario Lifecycle

On REALTIME entry:

1. thGame selects scenario.
2. scenario.init() is called.
3. Focus stack is cleared.
4. Gameplay layer is pushed.

On REALTIME exit:

1. Focus stack cleared.
2. scenario.shutdown() called.
3. Frame loop disabled.
4. All gameplay state discarded.

No state leakage between modes is allowed.

---

## Focus Stack Model

Input interpretation in REALTIME uses a stack model.

Top layer receives ActionEvents first.

Dispatch order:

1. Deliver to top layer.
2. If consumed → stop.
3. Else → deliver to next layer.
4. Repeat until consumed or stack empty.

Typical stack (top to bottom):

- Pause menu
- Dialogue
- Inventory
- Gameplay scene

This prevents UI overlays from triggering gameplay logic.

---

## Action Mapping Responsibility

ActionEvents represent intent only.

Examples:

- ACT_CONFIRM
- ACT_CANCEL
- ACT_LEFT
- ACT_RIGHT
- ACT_UP
- ACT_DOWN

Mapping to gameplay behavior occurs inside the active scenario.

Mapping may depend on:

- Scenario type
- Focus layer
- Internal state

thInput must never change to accommodate gameplay behavior.

---

## Entity Model

Entities are owned by the scenario.

Each entity must:

- Have deterministic update()
- Write into world2bpp surface only during render()
- Avoid direct memory allocation
- Avoid peripheral interaction

Entity updates must be bounded in time.

No entity may block or spin.

---

# World Rendering Contract

Gameplay renders into a 2bpp world surface.

The renderer later converts this surface into the 1bpp panel framebuffer.

Gameplay must not be aware of:

- Dithering
- Panel bit ordering
- Dirty row mechanics
- SPI/DMA behavior

---

## World Pixel Semantics

World pixel levels:

- 0 = white
- 1 = light grey
- 2 = dark grey
- 3 = black

Transparency is handled via bitmap mask planes (see asset_pipeline.md).

Gameplay render must:

- Respect mask transparency
- Never overwrite pixels outside clip bounds
- Remain deterministic

---

## Viewport Semantics (1× vs 2× Mode)

Renderer supports two present modes.

### 1× Binary Clamp Mode

- World surface resolution equals panel resolution.
- Viewport size = DISPLAY_WIDTH × DISPLAY_HEIGHT.

### 2× Dither Zoom Mode

- World surface resolution equals:
  DISPLAY_WIDTH/2 × DISPLAY_HEIGHT/2.
- Each world pixel expands to 2×2 panel pixels.
- Visible world area is reduced accordingly.

Gameplay responsibilities:

- Camera coordinates are defined in world pixels.
- Camera must clamp differently depending on viewport size.
- Gameplay must not change logic based on present mode.
- Only viewport bounds differ.

Mode switching must not alter simulation behavior.

---

## Map Integration (Tiled Metadata)

Maps are authored in Tiled.

Runtime loads lightweight metadata:

Tile bitflags:
- solid
- water
- slow
- occluder
- roof
- emissive

Object types:
- Spawn
- Interact
- Exit
- CameraZone
- IndoorZone
- Light

Game engine responsibilities:

- Interpret tile bitflags for collision
- Spawn entities at Spawn objects
- Trigger transitions on Exit overlap
- Apply CameraZone overrides
- Apply IndoorZone lighting overrides

---

## Map Transitions

When player overlaps Exit:

1. Save current state if required.
2. Load target map.
3. Place player at target_spawn.
4. Set facing if specified.
5. Apply fade if requested.

Transition must be deterministic and non-blocking.

---

# Lighting Model (2bpp World Surface)

Lighting produces 2bpp intensity levels.

Lighting inputs:

- Global ambient brightness
- Light object contributions
- IndoorZone overrides

Lighting output:

- Final world pixel level 0–3.

Lighting must:

- Avoid floating point
- Use integer math
- Remain bounded per frame
- Never exceed 2bpp range

Renderer later:

- Clamps levels in 1× mode
- Applies ordered dithering in 2× mode

Gameplay does not perform dithering.

---

## Determinism Requirements

REALTIME must satisfy:

- Same inputs + same state → same outputs.
- No hidden asynchronous mutation.
- No direct hardware side effects.
- No race conditions.
- No uncontrolled memory growth.
- No dependence on present mode for logic correctness.

---

## Forbidden Patterns

- Calling HAL from gameplay logic.
- Checking raw button pins.
- Creating threads from scenarios.
- Blocking on queues inside scenario.
- Multiple presents per frame.
- Modifying clocks inside scenario.
- Writing directly to 1bpp framebuffer from gameplay.

---

## Integration Notes

- Frame timing feedback is provided to power governor.
- Display thread enforces single-flush invariant.
- Dirty rows are composed before present request.
- Game logic must tolerate dropped frames during clock scaling events.
- Renderer owns present mode selection.
- Gameplay always renders to world2bpp.

---

Last updated: 2026-02-27