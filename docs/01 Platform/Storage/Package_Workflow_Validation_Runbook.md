# Package Workflow Validation Runbook

Date: 2026-09-09.
Status: implemented; Debug build and native regressions pass. Hardware acceptance
and latency measurements are pending. Do not cite this as a device pass.

Authority: [[Storage_and_Installer_Contract]],
[[Shell_and_UI_Navigation_State_Machine]], [[Display_and_Rendering_Contract]].
GUI readiness work: [[Peep_Studio_Empty_Scene_Validation_Handoff]].

## What Changed

- Shell MSC enter/reclaim, scan/install and PLAY accept one asynchronous action.
  The initial `STARTING` notice must complete its display transfer before the
  runtime/storage owners start preparation. `thUI` does not wait for storage.
- Subsequent stage notices show actual read/validate/erase/write/verify/commit/
  load work. Fast stages may not be visible individually. Repeated input is
  discarded while busy; no cancellation is offered during flash commit.
- Scan requires full native preflight before `VALID`. Install rereads and
  preflights that exact source before flash writes. Container, digest, chunk
  CRCs, target resident-prefix constraints and all scenes/states are checked.
  Empty secondary scenes fail even when the entry scene is populated.
- Candidate storage and validator context are separate from installed RAM and
  the active scene. FileX closes before `thRuntime` performs HASH/semantic work.
  A preflight timeout keeps the buffer reserved until the owner completes it.
- Completion survives a full UI notification queue. Failed boot/PLAY loading
  restores SHELL/RUNNING and requests the recoverable package error prompt;
  absence of a package still uses EGGLESS. Both buttons and joystick remain
  shell-owned on shell pages, independently of the package lifecycle.

This does not change the A/B layout, calibration, flash geometry, clock profiles,
egg format or Peep Studio implementation. The staged MSC bridge still has a
65536-byte limit. Large embedded eggs retain the resident-prefix/raw-audio-reader
path. The SWD embedded installer receives the same preflight checks but retains
its existing diagnostic UI entry path; shell timing fields describe shell actions.

## Device Test

Use the rebuilt Debug ELF and normal device controls. Do not pause during work
or audio. Debugger halts can change timing and low-power behavior.

1. Export MSC, copy a known-good egg within the current staging limit, eject
   it on the host, then reclaim on-device. Confirm immediate busy feedback,
   followed by a responsive package browser. Reclaim must not auto-install.
2. Select PACKAGE INSTALL. Confirm a busy notice appears before reading and
   validation, then `VALID`. Press A once: stages advance to `INSTALLED`.
   Repeated presses during work must not start extra transactions.
3. Before PLAY, halt and print the helper below to preserve install timings.
   Resume and press A. Confirm the authored entry scene appears; inputs,
   secondary scenes and idle STOP2 still work. A normal reset must also launch.
4. Export the recorded bad empty-scene egg instead. After reclaim and scan,
   expect ERROR, not VALID or INSTALLED. Preflight reason must be `11` (RENDER)
   with the offending scene ID. The selected installed generation must remain
   unchanged. The previous good package must still launch after reset.
5. On a unit already holding the bad egg, reset with this firmware without
   replacing the egg first. Expect PACKAGE / PKG ERROR / B BACK, not a blank
   screen or SHELL FAULT. B must return to USB FLASH / PACKAGE INSTALL; START
   must reach the shell menu. Confirm both joystick navigation and A/B/L/R
   controls work. No shell navigation action may retry the failed egg.
   Correct the source visuals in the GUI, export a fresh egg, and install
   normally to recover. At idle after the load failure, expect runtime
   class/lifecycle=1/2 (SHELL/RUNNING), scene active=0 and resume_available=0.
6. With no installed egg, confirm EGGLESS and the same working shell controls.
   With a known-good running egg, confirm inputs still reach the game and
   START/suspend/RESUME still work. Busy install and critical power states
   must retain their navigation restrictions.

Print after the relevant action finishes, before starting the next action:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_package_workflow_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_usb_package_install_prints.gdb
```

The first helper uses standalone workflow probe API 1; the existing RTOS/owner
probe layouts are unchanged. Use symbols matching the flashed ELF. It prints
only target state and does not arm requests, breakpoints or low-power debug.

## Reading The Result

- `display_status=0` means the display owner reported the initial transfer
  complete, not just that a queue accepted the message. `displayed_tick` must
  precede or equal `work_start_tick`.
- `active=0`, `status=0`, phase `DONE=14` mean the workflow finished successfully.
  PLAY completion proves runtime activation/dispatch, not the final physical
  panel transfer; confirm the authored scene visually.
- `validation_count` counts completed validator calls. Status/reason `0/0` is
  the native preflight result; `RENDER=11` rejects empty visuals. The older
  minimum-envelope reason field is not full-validation proof.
- Each new shell action replaces the previous timing record. Phase durations
  are completed wall-time intervals in milliseconds, including scheduling,
  peripheral waits and phase cleanup. The currently open interval is excluded.
- HCLK is sampled at phase entry (validation samples after its runtime clock
  request); OSPI frequency is the clock-policy readback, not an oscilloscope
  measurement. Revisited phases accumulate time and retain their latest sample.
- `STARTING` includes time until runtime preparation starts. The separate
  accepted-to-notice value isolates the initial display response. Neither starts
  at the physical button edge, so report perceived input lag separately.
- A workflow timeout/error does not justify overwriting a still-reserved
  candidate. `preflight pending/reserved=1` means the runtime owner has not yet
  returned it. No new scan/install may reuse that memory.

The existing clock policy normally gives flash-only work 24 MHz HCLK and MSC
160 MHz. Other active requesters may raise the selected profile. Collect stage
times first; no speed or power improvement has been claimed from these changes.

## Local Coverage

```powershell
tools/.venv/Scripts/python.exe -m unittest discover -s tools/authoring/tests -p test_firmware_package_workflow.py -v
```

Tests compile production validator, workflow, activation and input-policy
functions with hardware/RTOS interfaces stubbed, linked to the real UI router.
Cases cover empty models across all fixture scenes/states,
bad header/digest, out-of-cache metadata, a valid 12-second streamed-audio egg,
live-context isolation, initial display failure, duplicate admission, stale
sequence messages, notice timeout, storage dispatch failure, queue-full
completion, candidate reservation and boot-error delivery. Shell regressions
cover MENU with NONE/ERROR/suspended runtimes, button and joystick navigation
through USB/scan/install/PLAY actions, boot and in-place load failures, missing
packages, render dispatch failure, terminal installer failure, healthy runtime
input, full input queues, operation locks and preserved power/fatal-error pages.
The earlier workflow test stubbed the router and proved error-event dispatch
only; it did not prove that the error page was navigable. These tests now check
actual focus/page/action changes. They do not emulate
ThreadX scheduling, physical flash power loss, panel timing or STOP2 hardware.

## Shell Lockout Evidence (2026-09-09)

The reported bad egg read all 33120 bytes with storage and HASH status zero,
but no scene activated. START opened MENU (page 2), nav FOCUS, modal NONE while
the runtime remained LP_GRAPH/ERROR (2/5). Nine presses and nine releases were
accepted by the button FSM, but all 18 logical events were suppressed with
RUNTIME_NOT_READY and thUI received none. This proves an input-routing lockout,
not missing button edges. The joystick delivery path contained the same
lifecycle restriction. The current fix has native test and Debug build coverage;
on-device acceptance is still pending. Debugger stack location alone does not
establish whether a thread was making forward progress.
