# Shell and UI Navigation State Machine

This document defines explicit state machines for shell flow, focus/navigation behavior, and modal interaction.

Shell settings, first setup, calibration, and package-management ownership are defined in [[Shell_Settings_Calibration_Contract]].

---

## Scope

Defines:
- shell page-flow state machine
- navigation/focus state machine
- modal dialog and input-entry state machine

Does not define:
- runtime host lifecycle handoff rules (see [[Runtime_Host_Contract]])

---

## 1) Shell Flow FSM

States:
- `SHELL_BOOTSTRAP`
- `SHELL_HOME`
- `SHELL_MENU`
- `SHELL_SETTINGS`
- `SHELL_CALIBRATION`
- `SHELL_PACKAGE_BROWSER`
- `SHELL_RUNTIME_HANDOFF`
- `SHELL_ERROR`

Key events:
- `EV_BOOT_COMPLETE`
- `EV_NAV_HOME`
- `EV_NAV_MENU`
- `EV_NAV_SETTINGS`
- `EV_NAV_CALIBRATION`
- `EV_NAV_PACKAGES`
- `EV_LAUNCH_RUNTIME`
- `EV_RUNTIME_RETURNED`
- `EV_SHELL_FAULT`
- `EV_RECOVER_OK`

Rules:
- Runtime launch requests are legal only from `SHELL_PACKAGE_BROWSER` or `SHELL_HOME`.
- Returning from runtime always routes through `SHELL_RUNTIME_HANDOFF` before `SHELL_HOME`.

---

## 2) Navigation and Focus FSM

States:
- `NAV_IDLE`
- `NAV_FOCUS_ACTIVE`
- `NAV_MODAL_ACTIVE`
- `NAV_TEXT_INPUT`
- `NAV_NUMERIC_INPUT`
- `NAV_TRANSITION_LOCK`

Key events:
- `EV_INPUT_ACTION`
- `EV_FOCUS_MOVED`
- `EV_OPEN_MODAL`
- `EV_CLOSE_MODAL`
- `EV_OPEN_TEXT_INPUT`
- `EV_OPEN_NUMERIC_INPUT`
- `EV_SUBMIT_INPUT`
- `EV_CANCEL_INPUT`
- `EV_PAGE_TRANSITION_BEGIN`
- `EV_PAGE_TRANSITION_END`

Rules:
- Focus moves are blocked in `NAV_TRANSITION_LOCK`.
- Page navigation events are blocked while in text/numeric entry states unless explicitly allowed.

---

## 3) Modal and Confirmation FSM

States:
- `MODAL_NONE`
- `MODAL_CONFIRM`
- `MODAL_ALERT`
- `MODAL_ERROR`
- `MODAL_DISMISSING`

Key events:
- `EV_MODAL_SHOW_CONFIRM`
- `EV_MODAL_SHOW_ALERT`
- `EV_MODAL_SHOW_ERROR`
- `EV_MODAL_ACCEPT`
- `EV_MODAL_CANCEL`
- `EV_MODAL_TIMEOUT`
- `EV_MODAL_DISMISSED`

Rules:
- Modal states preempt normal page focus handling.
- Dismiss completion must be explicit (`EV_MODAL_DISMISSED`) before input routing returns to page context.

---

## Required Integration

- `thUI` owns transitions for these FSMs.
- `thInput` supplies actions only; it does not decide page semantics.
- Transition logging must include state and triggering action/event.

---

## Validation Cases

1. page stack and back behavior remains deterministic
2. modal preemption blocks unintended page actions
3. text/numeric entry does not leak navigation actions
4. runtime handoff/return restores shell state cleanly
5. invalid transitions are rejected and logged
