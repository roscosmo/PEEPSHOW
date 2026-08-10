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
- `SHELL_SHUTDOWN`

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

## HW6 FW0 Router Bring-Up Status

On HW6 unit 001, FW0 has a first shell router integrated with `thUI`,
`thInput`, and `thDisplay`:

- `thUI` owns page transitions for BOOT, HOME, MENU, SETTINGS, CALIBRATION,
  PACKAGES, RUNTIME, ERROR, and SHUTDOWN pages
- `thInput` supplies generic button events (`BTN_A`, `BTN_B`, `BTN_L`,
  `BTN_R`); it does not publish universal enter/back actions
- `thUI` maps those generic events contextually: A selects/advances, B backs
  out, and L/R move focus when joystick navigation is unavailable
- display presentation is routed through `thDisplay`; the UI does not touch the
  display peripheral directly
- HOME dispatch is gated until normal boot power work is complete

The measured UI router prints showed HOME reached from BOOT, display render
requests completed, and later button navigation reached the calibration page
with nonzero generic button counts. The user also physically confirmed A/B/L/R
navigation on the display.

This is an FW0 shell scaffold, not the final renderer or final menu system.
The joystick calibration page can be entered, but its multi-step calibration
flow is not complete yet.

Current FW0 also has a validated START shutdown scaffold: `thPower` accepts input-owned prep/warning/imminent/cancel lifecycle events, `thUI` maps those into a modal `SHUTDOWN` page, and `thDisplay` renders plain text for prep and warning states. HW6 unit 001 showed `PREPARING`, `POWER OFF IN 3/2/1`, then release before shipment returned to HOME with UI/display page `1` and shutdown state `4` (`CANCELLED`). This page is scaffolding only; final animation, sprites, copy, save progress, and product power-off policy remain open.

## Validation Cases

1. page stack and back behavior remains deterministic
2. modal preemption blocks unintended page actions
3. text/numeric entry does not leak navigation actions
4. runtime handoff/return restores shell state cleanly
5. invalid transitions are rejected and logged
6. START shutdown overlay blocks normal A/B/L/R page navigation while active
7. START release before shipment cancels the shutdown overlay and restores the prior page
