# Platform Freeze Charter

This document defines what must be true before Reference Game implementation begins.

Game design documentation may continue during bring-up.

Game implementation must wait until Platform state machines, hardware abstractions, and Engine-facing contracts are stable enough to prevent the game from shaping hardware behavior.

---

## Mission

Build the active PeepShow HW6 Platform as a finished ultra-low-power handheld console layer that remains useful with no game installed.

Built-in behavior must include:

- clock or home screen
- settings
- input calibration
- package/content management entry points
- installer/transport mode
- shared UX primitives needed by Platform and Engine

---

## Scope

In scope for Platform freeze:

- Platform services: power, time, input, display, audio, sensors, storage, USB, debug
- deterministic ThreadX ownership model
- hardware state machines
- low-power policy and wake classification
- storage and installer arbitration
- Engine-facing runtime/package contracts
- shell/settings/calibration flows
- validation evidence and bring-up tracker
- development security/update seams sufficient to keep future Platform firmware update and production hardening possible

Shell/settings/calibration behavior is defined in [[Shell_Settings_Calibration_Contract]].

Out of scope until Platform freeze:

- Reference Game mechanics implementation
- Reference Game movement/scene logic
- creature-specific firmware behavior
- game-specific assumptions inside Platform owners
- production security lock-down, signature enforcement, final bootloader/update-slot layout, and watchdog release enablement

---

## Product Principles

- Platform owns hardware policy.
- Engine owns reusable game-development APIs.
- Reference Game owns its content and mechanics.
- Every major subsystem has explicit states.
- All cross-thread operations are request-based.
- Determinism and low power are defaults, not optional.

---

## Platform Freeze Exit Criteria

Platform freeze is complete only when all conditions below are true:

1. Shell-only device works end-to-end without Reference Game code.
2. Hardware owner threads are implemented and state-machine backed.
3. Display, audio, input, sensor, storage, USB, and power abstractions are in place.
4. Runtime/Engine lifecycle can mount, suspend, resume, stop, and unmount cleanly.
5. Storage/USB installer ownership transitions are safe and repeatable.
6. Shared UX services are usable by shell and Engine.
7. STOP and wake behavior are verified with evidence.
8. State-machine docs and interface docs match implementation.

---

## Non-Goals

- Do not implement Reference Game mechanics in Platform.
- Do not encode game semantics into power, storage, display, audio, input, or sensor owners.
- Do not treat Reference Game content formats as Platform internals.
- Do not let CubeMX defaults become architecture by accident.

---

## Milestones

1. M0 - Obsidian vault structure and architecture sign-off.
2. M1 - HW6 hardware contract and CubeMX checklist sign-off.
3. M2 - Board bring-up and owner-thread skeleton.
4. M3 - Shell, settings, calibration, and shared UX primitives.
5. M4 - Engine runtime/package contracts and lifecycle.
6. M5 - Power, storage, USB, and installer validation.
7. M6 - Platform freeze sign-off.
8. M7 - Reference Game implementation begins against frozen contracts.

---

## Success Metrics

- Average sleep current and wake latency meet evidence-backed HW6 targets.
- No cross-thread direct peripheral access violations.
- No Engine or Reference Game path can call HAL/LL directly.
- Runtime lifecycle passes the validation matrix.
- Shell remains fully functional without the Reference Game.
- Game notes map to Engine capability requests instead of Platform policy changes.
