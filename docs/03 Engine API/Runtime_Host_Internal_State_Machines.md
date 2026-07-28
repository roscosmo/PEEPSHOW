# Runtime Host Internal State Machines

This document defines internal FSM requirements for each runtime host class.

These FSMs sit under the external host lifecycle contract and keep host behavior explicit.

---

## Scope

Defines:
- host-internal state models for `SHELL`, `LP_GRAPH`, `LP_MODULE`, `RT_SCENE`, `INSTALLER`
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
- runtime unit transitions must pass through the runtime manager and target declared package runtime units.

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

## 2) LP_GRAPH Host Internal FSM

States:
- `LPG_INT_IDLE`
- `LPG_INT_LOAD_GRAPH`
- `LPG_INT_WAIT_EVENT`
- `LPG_INT_EVALUATE`
- `LPG_INT_APPLY_ACTIONS`
- `LPG_INT_RENDER_HINT`
- `LPG_INT_SLEEP_HINT`
- `LPG_INT_ERROR`

Key events:
- timer/input/sensor intents
- graph transition request
- action execution complete
- host fault and recover events

---

## 3) LP_MODULE Host Internal FSM

States:
- `LPM_INT_IDLE`
- `LPM_INT_LOAD_MODULE`
- `LPM_INT_PREPARE`
- `LPM_INT_RUNNING`
- `LPM_INT_PAUSED`
- `LPM_INT_COMPLETE`
- `LPM_INT_ERROR`

Key events:
- module load/prepare complete
- start/pause/resume/complete
- module fault

---

## 4) RT_SCENE Host Internal FSM

States:
- `RTS_INT_IDLE`
- `RTS_INT_LOAD_SCENE`
- `RTS_INT_PREPARE_FRAME`
- `RTS_INT_RUNNING`
- `RTS_INT_PAUSED`
- `RTS_INT_TRANSITION`
- `RTS_INT_UNLOAD`
- `RTS_INT_ERROR`

Key events:
- scene load/transition requests
- frame tick/update events
- pause/resume requests
- scene fault/asset fault events

Rules:
- frame update loop runs only in `RTS_INT_RUNNING`.
- scene transition path must be bounded and failure-visible.

---

## 5) INSTALLER Host Internal FSM

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
5. runtime unit transition to undeclared target is rejected
6. realtime unit without declared low-power fallback is rejected by package validation
