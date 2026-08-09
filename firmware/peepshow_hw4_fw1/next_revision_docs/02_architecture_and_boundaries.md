# Architecture and Boundaries

This is the core architecture contract for this project.

---

## Four-Layer Model

1. PeepOS Core Platform
2. PeepOS UI and Shared UX Layer
3. Runtime Hosts
4. Packages

Only layers 1-3 are firmware-owned. Layer 4 is package-owned content and logic.

---

## Layer Responsibilities

### 1) PeepOS Core Platform

Owns:
- RTOS object graph and thread ownership
- low-power policy and mode transitions
- wake source arming and wake classification
- peripheral policy (display/audio/input/sensors/storage)
- package mount/install plumbing

### 2) PeepOS UI and Shared UX Layer

Owns:
- shell UI
- settings and calibration flows
- reusable widgets/services (text input, numeric input, dialogs, pickers)
- standard navigation and system SFX behavior

### 3) Runtime Hosts

Owns:
- host lifecycle (`mount -> start -> suspend -> resume -> unmount`)
- package execution context and scheduling contract
- intent-to-policy requests (never direct hardware control)

### 4) Packages

Owns:
- content
- state graphs
- package variables and scripted logic under host contract

Must not own:
- direct peripheral handles
- raw thread creation
- clock tree changes
- low-power entry

---

## Ownership Rules (Non-Negotiable)

- Every peripheral and shared datapath has exactly one owner thread.
- Other threads use queue/event requests only.
- Requests must be bounded, deterministic, and typed.
- No package path may bypass owner threads.

---

## Runtime Contract Boundary

Packages can request:
- cadence hints
- wake intents
- invalidation or presentation hints
- audio cues by symbolic ID
- public knob changes via validated public-knob API only

Platform decides:
- sleep depth
- clock profile
- IRQ/poll mode
- DMA/transfer method
- whether requested public knob changes are allowed and applied

---

## Forbidden Coupling Patterns

- UI pages directly invoking package internals.
- Power manager containing package/game state logic.
- Storage API exposing game-scene-specific operations.
- Runtime host implementation hardcoded to one game mode.

---

## Required Artifacts For Any New Subsystem

Before implementation starts, each subsystem must have:
1. owner thread declaration
2. public request schema
3. explicit state machine
4. mode behavior table (`SHELL`, `LP_GRAPH`, `LP_TEMPLATE`, `RT_SCENE`, `INSTALLER`)
5. failure and recovery policy
