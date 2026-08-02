# PeepShow V5 (ThreadX)

PeepShow is a low-power, deterministic, blob-based handheld console
built on STM32U575 and ThreadX.

This repository contains the firmware and the **authoritative system
architecture documentation**.

The system is designed around:

- STOP2-first power policy
- Single-owner peripheral model
- Deterministic 30 FPS REALTIME runtime
- Blob-based game installation model
- No dynamic memory allocation
- Strict debugging discipline

**If implementation conflicts with documentation in `/docs`, the documentation is authoritative.**

---

## Architecture Overview

PeepShow is structured as a layered system:

1. Hardware layer (clock + peripheral topology)
2. Power governor and STOP2 control
3. ThreadX RTOS ownership model
4. Deterministic rendering pipeline
5. Blob-based asset and storage model
6. FSM-driven runtime behavior

Content is installed as discrete game blobs.
Runtime does not stream from FAT.
FileX is used only as a transport volume.

---

## Modes

The device operates in one of four modes:

| Mode      | Description |
|-----------|-------------|
| STOP      | Primary pet runtime (RTC-driven) |
| STATIC    | Menu/UI mode |
| REALTIME  | 30 FPS game runtime |
| FLASHING  | USB MSC transport mode (USB-only) |

Mode ownership belongs to **thPower**.

---

## Storage Model

External flash is partitioned into:

- Raw Settings Region (MCU-only, log-structured, power-fail safe)
- Raw Installed Blob Region (MCU-only, deterministic runtime reads)
- FAT Transport Volume (LevelX + FileX, exposed in FLASHING mode)

Install-on-selection policy:

- Blobs are copied from FAT to raw install region
- Validated before activation
- Runtime reads only from installed raw region

---

## Rendering Model

- Sharp Memory LCD
- Panel-native bitplane format
- Layered compositor (BG → GAME → UI)
- Dirty-row tracking
- Single flush in-flight
- DMA from SRAM4 only

---

## Audio Model

- SAI1, 16 kHz mono
- PLL2P master clock
- Double-buffer DMA
- Audio data sourced from installed blobs only
- No FAT streaming during runtime

---

## RTOS Model

- ThreadX
- Single-owner peripheral discipline
- All communication via queues and event flags
- ISRs signal only
- No dynamic allocation

---

## Documentation Authority & How To Use It

### Priority order (if anything conflicts)
1. `docs/authority.md` (cross-cutting invariants; **wins over everything**)
2. Domain specs in `/docs` (power/rendering/storage/etc.)
3. This `README.md`
4. Code comments

### How a code agent should approach work
1. Read `docs/authority.md` first.
2. Identify which domain doc governs the change.
3. Implement strictly within that doc’s constraints.
4. If a constraint is unclear, **fix the doc first** (don’t “guess in code”).

---

## Documentation Index (What each file controls)

All authoritative specs live in `/docs`.

| File | Domain authority | Use this when you are… |
|------|------------------|------------------------|
| `docs/authority.md` | Cross-cutting invariants (ownership, modes, timing, prohibitions) | Starting any work; resolving conflicts; reviewing PRs |
| `docs/hardware.md` | Board topology, peripherals, pins, rails, canonical parts | Wiring/peripheral assumptions; bring-up checks |
| `docs/boot_and_bringup.md` | Bring-up order + validation steps | Defining “what to test next”; staging STOP2 adoption |
| `docs/brought_up.md` | Current bring-up progress tracker vs planned phases | Seeing what is already validated and what is still pending |
| `docs/power_management.md` | STOP2 policy, wake sources, clock profiles, governor | Anything involving sleep/wake, clocks, battery behavior |
| `docs/peripheral_robustness.md` | Robust init/recovery rules for flaky peripherals | Hardening boot/wake reliability; retry state machines |
| `docs/rtos_architecture.md` | ThreadX structure, owner threads, queues/events, ISR rules | Creating tasks/queues; assigning peripheral ownership |
| `docs/debugging.md` | Debug channels, HardFault capture, logging discipline | Adding diagnostics; crash capture; SWO/RTT usage |
| `docs/display_and_rendering.md` | Sharp LCD rules, SPI oddities, buffer formats, flush discipline | Anything touching display config, renderer, DMA, buffers |
| `docs/game_engine.md` | Runtime architecture, focus stack, frame loop contracts | Implementing game systems; input routing; scene lifecycle |
| `docs/game_logic_architecture.md` | High-level gameplay layering, modes → gameplay responsibilities | Deciding where “game rules” live vs engine plumbing |
| `docs/state_machine.md` | FSM structure, generated/user split rules, integration | Adding pet/game FSMs; events/transitions; codegen workflow |
| `docs/Tiled_map_integration.md` | Tiled export conventions, layer/object semantics, map ingest | Importing maps; collision layers; spawn points; properties |
| `docs/storage_and_updates.md` | Flash layout, blob install/update rules, FileX boundaries | USB MSC behavior; installs; updates; raw region reads |
| `docs/asset_pipeline.md` | PC-side asset build → firmware-ready blobs/headers | Extending asset generator; adding asset types |
| `docs/audio.md` | Audio pipeline, formats, DMA buffering, constraints | Adding sounds/music/voices; preventing frame jitter |
| `docs/knobs.md` | Compile-time knob system + what is allowed to be tunable | Adding/configuring knobs; keeping builds deterministic |

**Rule:** if you add a new doc in `/docs`, you must update this table.

---

## Development Philosophy

- Hardware-first bring-up.
- STOP2 introduced only after stability.
- HardFault capture is mandatory.
- SWO/RTT is preferred over printf.
- USB MSC mode disables all other system activity.
- Determinism is prioritized over convenience.

---

## Build

Build via CMake using STM32CubeMX-generated base project.

All compile-time tunables are defined in:

- `config/knobs.json`

Never edit generated headers directly.

---

## Warning

Do not:

- Modify CubeMX-generated files outside USER CODE blocks.
- Introduce dynamic memory allocation.
- Access peripherals outside their owning thread.
- Stream assets from FAT during runtime.
- Enable STOP2 before system stability is proven.

---

Last updated: 2026-02-20
