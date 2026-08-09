# Power and Sleep Policy

This document defines how PeepOS translates runtime intent into power behavior.

---

## Core Policy

- STOP-first strategy for low-activity operation.
- Power thread is sole owner of clock and sleep transitions.
- Runtime hosts provide intent, not hardware commands.
- No transition is complete until timebases are verified.

---

## Wake Reason Classification

All wakes must be classified as one of:
- RTC cadence
- button or joystick input
- sensor event
- USB attach/detach
- watchdog or fault recovery
- unknown (requires investigation)

Unknown wake reasons are defects until explained.

---

## Intent To Policy Mapping

Inputs to power manager:
- runtime class
- requested cadence
- allowed latency
- required wake sources
- active audio/display flags

Outputs from power manager:
- sleep class
- clock profile
- sensor mode
- input mode
- display transfer mode

---

## Runtime Policy Matrix

`SHELL`
- default low-power interactive policy
- STOP2 eligible when idle

`LP_GRAPH`
- RTC and event-driven
- maximize STOP residency

`LP_TEMPLATE`
- low-power preferred, bounded awake windows

`RT_SCENE`
- frame paced active mode with explicit idle fallback

`INSTALLER`
- USB/staging ownership mode with strict subsystem isolation

---

## Transition Sequence (Required)

1. collect quiesce acknowledgements from owners
2. apply profile and sleep transition
3. validate HAL and RTOS timebases
4. resume owners and validate liveness
5. publish mode transition complete event

All waits in this sequence must have explicit timeout.

---

## Forbidden Patterns

- package code requesting direct STOP entry
- clock profile writes outside power owner
- polling loops used instead of wake events
- entering installer mode with active non-USB storage users

---

## Required Measurements

For each runtime class measure:
- average current in steady state
- wake latency
- resume latency
- failed wake/resume count

Track these in `docs/power_validation.md`.

