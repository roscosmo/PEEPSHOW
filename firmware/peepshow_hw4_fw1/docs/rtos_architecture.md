# RTOS Architecture

Authoritative specification for ThreadX architecture, ownership rules,
message passing, input routing, and execution discipline in PeepShow V5.

This document defines how threads interact, who owns peripherals,
how input flows, and what is forbidden.

If implementation differs from this document, the implementation is wrong.

---

## Scope

Defines:
- Thread responsibilities
- Peripheral ownership model
- Queue and event flag contracts
- ISR signaling discipline
- Mode routing
- Input layering and focus stack model
- REALTIME execution discipline

Does NOT define:
- Clock scaling policy (see power_management.md)
- Electrical or clock topology (see hardware.md)
- Game-specific behavior (see game_engine.md)

---

## Design Principles

- Single-owner peripheral model.
- No hidden concurrency on HAL handles.
- ISRs signal only.
- No blocking waits inside ISRs.
- Deterministic scheduling.
- All cross-thread interaction via ThreadX objects.
- STOP2 compatibility must always be preserved.

---

## Single-Owner Peripheral Model

Each time-sensitive peripheral datapath is owned by exactly one thread.

No thread may access a peripheral HAL handle unless it is the designated owner.

This prevents:
- Partial display flushes
- DMA race conditions
- Clock reconfiguration hazards
- Power coordination failures

---

## Thread Overview

| Thread     | Priority        | Owns |
|------------|----------------|------|
| thPower    | BelowNormal    | Clocks, STOP2, mode state |
| thDisplay  | High           | SPI3 + LPDMA |
| thAudio    | Realtime       | SAI1 + audio DMA |
| thRadio    | AboveNormal    | LPUART1 + LPBAM |
| thInput    | AboveNormal    | Raw → logical input routing |
| thUI       | Normal         | STOP runtime + STATIC UI |
| thGame     | Normal         | REALTIME runtime |
| thStorage  | Low            | OCTOSPI + FileX + LevelX |
| thSensor   | BelowNormal    | I2C sensors |

Exact priorities must prevent starvation while maintaining determinism.

---

## Queue Topology

All cross-thread interaction occurs through queues or event flags.

### qInputRaw
Producer: ISRs  
Consumer: thInput  
Contains raw input events (pin, edge, timestamp)

---

### qUIEvents
Producer: thInput  
Consumer: thUI  
Contains logical ActionEvents for STOP + STATIC

---

### qGameEvents
Producer: thInput  
Consumer: thGame  
Contains logical ActionEvents for REALTIME

---

### qSysEvents
Producers: multiple  
Consumer: thPower  
Used for:
- Mode transitions
- Clock requests
- Audio/stream on/off
- Inactivity timeout
- STOP entry requests

---

### qDisplayCmd
Producers: thUI, thGame  
Consumer: thDisplay  
Contains display invalidate/present commands

---

### qAudioCmd
Producers: thUI, thGame  
Consumer: thAudio  
Contains playback commands

---

### qStorageReq
Producers: thUI, thGame, thAudio  
Consumer: thStorage  
Contains filesystem and flash requests

---

### qSensorReq
Producers: thUI, thGame  
Consumer: thSensor  
Contains polling/configuration requests

---

### qRadioCmd (optional)
Producers: UI/Game/Power  
Consumer: thRadio  
Contains transmit/config requests

---

## Event Flag Groups

### egMode
Holds high-level mode state:
- STOP
- STATIC
- REALTIME
- FLASHING

Owned by thPower.

---

### egPower
Used for STOP2 quiesce coordination.

---

### egDebug
Indicates debug mode active.

---

## ISR Signaling Discipline

ISRs must:

- Post to queue
- Or set thread flag
- Return immediately

ISRs must NOT:

- Call HAL long operations
- Parse protocol data
- Perform filesystem operations
- Modify clocks
- Block

---

## Input Architecture (Layered)

Input handling is strictly layered.

There are three stages:

1. Raw input (hardware level)
2. Logical ActionEvents (intent level)
3. Contextual interpretation (REALTIME focus model)

The routing layer never decides gameplay meaning.

---

## Stage 1 — Raw Input

ISRs emit raw events only:

- source (BTN_A / BTN_B / BTN_L / BTN_R / BTN_BOOT / JOY_DIR)
- edge (PRESS / RELEASE)
- timestamp

No mode logic in ISR.

---

## Stage 2 — Logical Actions

thInput converts raw events into ActionEvents.

Examples:

- ACT_CONFIRM
- ACT_CANCEL
- ACT_LEFT
- ACT_RIGHT
- ACT_UP
- ACT_DOWN
- ACT_MENU

Responsibilities of thInput:

- Debounce
- Repeat policy
- Inactivity timer reset
- Mode-based routing:
  - STOP/STATIC → qUIEvents
  - REALTIME → qGameEvents
  - System override → qSysEvents

thInput must not interpret gameplay meaning.

---

## Stage 3 — REALTIME Focus Stack

Owned by thGame.

Only REALTIME uses contextual input.

Focus stack dispatch:

1. Deliver ActionEvent to top layer.
2. If consumed → stop.
3. Else propagate downward.
4. Repeat until consumed or stack exhausted.

Focus layers may include:

- Pause menu
- Dialogue
- Inventory
- Gameplay scene

This guarantees overlays do not trigger gameplay unintentionally.

---

## REALTIME Execution Model

REALTIME runs a fixed 30 FPS loop.

Each frame:

1. Drain ActionEvents (bounded)
2. Dispatch focus stack
3. Update scenario
4. Render to RAM buffers
5. Issue at most one present

Constraints:

- No blocking waits.
- No direct HAL calls.
- No RTOS object creation.
- No clock manipulation.

---

## Scenario Contract

Each REALTIME scenario must implement:

- init()
- shutdown()
- update(delta_time)
- render(framebuffer)
- on_action(ActionEvent*)

Rules:

- update() = simulation only
- render() = RAM writes only
- on_action() returns consumed/not consumed
- No HAL access
- No RTOS object creation
- No queue consumption

---

## STOP Runtime Model

STOP runtime is owned by thUI.

STOP uses RTC-driven ticks.

STOP may:

- Advance pet state machine
- Interpret STOP input
- Trigger mode transitions

STOP must not:

- Spin loops
- Poll continuously
- Block on display

---

## Forbidden Patterns

The following are not allowed:

- Accessing HAL handles outside owner thread
- Blocking inside ISR
- Checking raw button pins inside gameplay logic
- Modifying clocks outside thPower
- Creating RTOS objects inside game modules
- Calling display flush directly from game code
- Using dynamic memory

---

## Determinism Requirements

REALTIME must be deterministic:

- Same inputs + same state → same outputs
- No hidden async mutation
- No uncontrolled memory allocation
- Focus stack order must be explicit

---

## STOP2 Compatibility Requirement

All threads must:

- Respond quickly to quiesce request
- Avoid long blocking operations
- Keep datapaths compatible with SmartRun domain
- Avoid continuous polling

---

## Invariants (Do Not Violate)

- Exactly one owner per peripheral.
- All cross-thread communication via RTOS objects.
- ISRs signal only.
- No HAL access outside owner.
- No dynamic memory allocation.
- No RTOS object creation after init.
- Focus stack logic lives only in thGame.

---

Last updated: 2026-02-18
