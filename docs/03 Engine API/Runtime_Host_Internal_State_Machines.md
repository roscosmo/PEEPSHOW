# Runtime Host Internal State Machines

This document defines internal FSM requirements for each runtime host class.

These FSMs sit under the external host lifecycle contract and keep host behavior explicit.

---

## Scope

Defines:
- host-internal state models for `SHELL`, `PACKAGE`, and `INSTALLER`
- package-host scene dispatch for `STATE_SCENE`, `SEQUENCE_SCENE`, and `PROGRAM_SCENE`
- mapping rules between internal states and external lifecycle

Does not define:
- package data schemas (see [[Package_Contract]])

---

## Common Rules

- Internal host state changes must be explicit event-driven transitions.
- Internal states must not bypass external lifecycle contract.
- Any host fault must map to lifecycle-safe error handling and return path.

Lifecycle mapping requirement:
- `mount/start/suspend/resume/stop/unmount` operations must be valid from internal state context and reject illegal calls.
- scene transitions must pass through the package scene manager and target declared package scenes.

---

## 1) SHELL Host Internal FSM

States:
- `SH_INT_BOOTSTRAP`
- `SH_INT_HOME`
- `SH_INT_MENU`
- `SH_INT_SETTINGS`
- `SH_INT_CALIBRATION`
- `SH_INT_PACKAGE_BROWSER`
- `SH_INT_MODAL`
- `SH_INT_HANDOFF`
- `SH_INT_ERROR`

Key events:
- navigation and modal events from UI state machines
- runtime launch request
- runtime return complete

---

## 2) PACKAGE Host Internal FSM

States:
- `PKG_INT_IDLE`
- `PKG_INT_LOAD_PACKAGE`
- `PKG_INT_ENTER_SCENE`
- `PKG_INT_STATE_WAIT`
- `PKG_INT_STATE_TRANSACTION`
- `PKG_INT_SEQUENCE_PREPARE`
- `PKG_INT_SEQUENCE_RUNNING`
- `PKG_INT_PROGRAM_PREPARE`
- `PKG_INT_PROGRAM_RUNNING`
- `PKG_INT_TRANSITION`
- `PKG_INT_SUSPENDED`
- `PKG_INT_ERROR`

Key events:
- package mount/start/suspend/resume/stop
- scene entry and declared transition requests
- input, schedule, sensor, lifecycle, and completion events
- sequence or program frame deadlines
- interaction-state changes
- scene, asset, sandbox, and capability faults

Rules:
- `STATE_SCENE` transactions run only in `PKG_INT_STATE_TRANSACTION` and return to `PKG_INT_STATE_WAIT` after settling.
- sequence frame work runs only in `PKG_INT_SEQUENCE_RUNNING`.
- programmable frame work runs only in `PKG_INT_PROGRAM_RUNNING`.
- inactivity leaves either realtime state through `PKG_INT_TRANSITION` before the inactive route is established.
- scene transitions are bounded and failure-visible.

---

## 3) INSTALLER Host Internal FSM

States:
- `INS_INT_IDLE`
- `INS_INT_WAIT_MEDIA`
- `INS_INT_SCAN`
- `INS_INT_STAGE`
- `INS_INT_VALIDATE`
- `INS_INT_COMMIT`
- `INS_INT_ROLLBACK`
- `INS_INT_REPORT`
- `INS_INT_DONE`
- `INS_INT_ERROR`

Key events:
- media attach/detach
- package detect/stage/validate/commit outcomes
- rollback outcomes
- report completion

---

## Validation Cases

1. each host rejects invalid lifecycle-to-internal-state calls
2. suspend/resume behavior preserves internal state consistency
3. host errors map cleanly to lifecycle-safe return path
4. host transition traces reconstruct full internal execution sequence
5. scene transition to an undeclared target is rejected
6. realtime scene without a declared inactivity route is rejected by package validation
