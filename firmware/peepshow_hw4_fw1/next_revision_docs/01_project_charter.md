# Project Charter (Next Hardware Revision)

This document defines what the project is, what it is not, and the stop point before game-specific work.

---

## Mission

Build PeepOS as a finished ultra-low-power handheld operating environment that remains useful with zero packages installed.

Built-in behavior must include:
- clock home screen
- settings
- input calibration
- package management and install entry
- shared UX primitives

---

## Scope

In scope:
- platform services (power, time, input, display, audio, sensors, storage)
- runtime hosting (`SHELL`, `LP_GRAPH`, `LP_TEMPLATE`, `RT_SCENE`, `INSTALLER`)
- package loading and metadata management
- deterministic ThreadX ownership model
- low-power policy and wake classification

Out of scope until platform freeze:
- map-engine design
- game-specific movement/scene logic
- package-specific gameplay loops in firmware core

---

## Product Principles

- Platform owns hardware policy.
- Packages own behavior/content.
- Every major subsystem has explicit states.
- All cross-thread operations are request-based.
- Determinism and low power are default, not optional.

---

## Platform Freeze Exit Criteria

Platform freeze is complete only when all conditions below are true:

1. Shell-only device works end-to-end without a package.
2. Runtime manager can mount, suspend, resume, and unmount hosts cleanly.
3. Storage/USB installer ownership transitions are safe and repeatable.
4. Shared UX services are usable by shell and runtimes.
5. STOP2 and wake behavior are verified with evidence.
6. State-machine docs and interface docs match implementation.

---

## Non-Goals

- Do not implement a specific game engine in core firmware.
- Do not encode package semantics into power, storage, or UI owners.
- Do not treat package content formats as firmware internals.

---

## Milestones

1. M0 - Docs lock and architecture sign-off.
2. M1 - Board bring-up and owner-thread skeleton.
3. M2 - Shell + shared UX primitives complete.
4. M3 - Runtime hosts + package contract complete.
5. M4 - Power and installer validation complete.
6. M5 - Platform freeze sign-off.

---

## Success Metrics

- Average STOP current and wake latency meet hardware targets.
- No cross-thread direct peripheral access violations.
- No package code path can call HAL/LL directly.
- Runtime host lifecycle passes full validation matrix.
- Shell remains fully functional after package install/uninstall cycles.

