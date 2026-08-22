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
- the FW0 input focus resolver keeps shell, installer, shutdown, and MSC overlay button events on `thUI`; package runtime classes receive the same generic logical presses through `thRuntime` stubs instead of shell navigation
- display presentation is routed through `thDisplay`; the UI does not touch the
  display peripheral directly
- HOME dispatch is gated until normal boot power work is complete
- bounded shell/router work publishes `REACTIVE_TRANSACTION_ACTIVE` through
  `thPower` and releases it after the UI transaction returns

The measured UI router prints showed HOME reached from BOOT, display render
requests completed, and later button navigation reached the calibration page
with nonzero generic button counts. The user also physically confirmed A/B/L/R
navigation on the display. FW0 evidence `EV-HW6-20260813-P1-UICLOCK-046`
adds target validation that HOME dispatch and a helper-queued `NAV_MENU`
transaction both requested and released UI reactive clock intent through
`thPower`, then settled with requester cap `UI=0x0` and `STOP2 ready=1`.

This is an FW0 shell scaffold, not the final renderer or final menu system. The package page is currently a temporary USB transfer scaffold: A requests the normal MSC enter service through `thUI` -> `thStorage`, and B requests MSC exit while an export is active. Package-page MSC reclaim asks `thStorage` to force a staging scan so an existing `.egg` can be recognized even when the bridge dirty flag is false. Valid package, install-stub, and error prompts return to MENU on B after clearing the prompt; successful install-stub completion also returns to MENU. This is a caller of the storage service, not a second USB ownership path. FW0 runtime evidence `EV-HW6-20260813-P1-RUNTIME-044` records this package-transfer flow as an `INSTALLER` runtime overlay that returns to `SHELL`; it is not final package launch or final installer UI.
FW0 now has the first input focus split for that handoff: generic A/B/L/R logical presses remain UI-owned while `SHELL` or `INSTALLER` is current, and while a system overlay such as shutdown or MSC is active. `LP_GRAPH`, `LP_MODULE`, and `RT_SCENE` route to a `thRuntime` input stub so package code can later interpret the same generic events contextually. HW6 validation confirmed normal menu navigation still targets `thUI`, and a reactive package stub press targets `thRuntime` without driving shell navigation.

When no validated `.egg` package is installed or selected, the Platform-owned
default package view is the `eggless` state. `eggless` is shell content, not a
package scene type and not a synthetic package. Installing or selecting an
`.egg` replaces that empty-package view through the normal package activation
path.

FW0 probe version 22 adds a matching system-action admission layer for shell actions that start system overlays. `thUI` still owns the page action, but before MSC enter or package-install-stub actions are queued, the admission layer checks whether another system overlay is active and whether a package runtime must be suspended first. Active package runtime classes are suspended through `thRuntime` with a bounded owner ACK before the action is allowed. MSC exit stays allowed while the MSC overlay is active so the user always has a local escape path. HW6 evidence `EV-HW6-20260814-P1-ADMISSION-053` validates the dry-run path: reactive package stub entered `LP_MODULE / REACTIVE / RUNNING`; MSC-enter admission reported action/result/reason/status `1 / 2 / 3 / 0x0`, counts `request/allow/deny/suspend = 1 / 1 / 0 / 1`, and `thRuntime` moved to `SUSPENDED` with suspend count `1` and zero runtime queue errors.

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
