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

The original workflow increment did not change the layout. The single-slot
follow-up below supersedes A/B storage without changing calibration, flash
geometry, clock profiles, egg format or Peep Studio. The staged MSC bridge still has a
65536-byte limit. Large embedded eggs retain the resident-prefix/raw-audio-reader
path. The SWD embedded installer receives the same preflight checks but retains
its existing diagnostic UI entry path; shell timing fields describe shell actions.

## Single-Slot Journal Checkpoint

2026-09-11: layout API 2 and index/install probe API 3 implement one 5 MiB
active package at `0x000C0000`, protected content reserve at `0x005C0000`, and
format-2 PENDING/VALID journal records. Debug build, target-profile check and
298 authoring/native tests pass. Device first install, PLAY, normal reboot,
same-slot replacement and subsequent PLAY/reboot passed with the embedded V1
fixture. Simulated NOR interruption/error tests remain distinct from physical
power-cut acceptance.

Recorded device evidence:

- First install: 321,480 bytes, pending record/generation 0/1, final
  record/slot/generation 1/0/2, package address `0xC0000`, index `0xB1000`.
  Preflight completed with status/reason 0/0 and released its reservation.
- Both installs: status/stage 0/11, pending/retire statuses 0/0, 79 erased
  sectors, 1,256 programmed pages, all 321,480 bytes read-verified with zero
  mismatches, commit marker `0x54494D43`, rescan success and availability 1.
- After an actual reset (not merely reconnecting after STOP2), boot selected
  generation 2, source 3, scene active 1 and loader status 0, with no install
  or replacement request. The 65,536-byte resident prefix was loaded. The
  printed boot HASH field was NOT_RUN; this is not fresh full-HASH proof.
- Replacement: source 1/0, pending 0/3, final 1/0/4 at the same `0xC0000`
  address. The user confirmed A PLAY and a subsequent normal reboot both
  entered the package. No post-replacement reboot probe dump was supplied.

Scope: the same embedded V1 artifact was reinstalled, not converted to V2.
Different-artifact replacement, physical interruption recovery, new MSC-path
acceptance, install latency measurement and a settings/calibration preservation
audit are not established by this result. V2 install/export remains disabled.

The first device attempt failed before reaching the writer: install count/size
remained zero and the validation reservation stayed active. The 321,480-byte
embedded candidate passed HASH in 131 ms, but validation had not reached scene
decoding. The saved runtime PC `0x08011de0` in that test ELF mapped to the
per-byte/per-chunk padding loop in `PS_EggValidateContainer`. This is evidence of
validation work overrunning the acknowledgement wait, not a flash-write failure
or a rejected package. Zero unexecuted erase/verify fields are not success.

The loader now walks validated physical chunk ranges without reordering chunk
indices and checks only the gaps for zero padding. Range selection is bounded
by `chunk_count * (chunk_count + 1)` comparisons (462 for this candidate),
instead of searching the chunk table again for every payload byte. CRC, SHA,
overlap, bounds and resident-prefix checks are unchanged, as are clock policy
and acknowledgement timeouts. Native preflight at `-O0` accepts the actual
embedded source, reordered physical payloads and minimal padding, and rejects
nonzero leading/intermediate/trailing gaps and overlapping chunks. The rebuilt
firmware subsequently passed the device sequence recorded above. Install latency
was not measured; host checks alone are not hardware timing evidence.

Old format-1 A/B records are intentionally ignored. Flashing this firmware on
an old installation should enter the shell/EGGLESS without erasing settings,
calibration, saves, USB staging or old slot-B bytes. Reinstall is required.
This checkpoint does not enable V2 install/export; use the existing embedded
V1 fixture to test storage independently of Studio:

1. Flash the matching Debug ELF, let boot finish, and confirm responsive shell
   controls and retained calibration. Do not arm a development V2 scene or MSC.
2. Halt and source the existing embedded installer, then continue normally:

   ```gdb
   source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_package_persistent_install_enable.gdb
   ```

3. Wait for `INSTALLED / A PLAY`, halt and print:

   ```gdb
   source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_package_persistent_install_prints.gdb
   source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_package_index_scan_prints.gdb
   ```

   Require status 0, stage 11, pending/retire statuses 0, byte verification with
   zero mismatches, selected slot 0/start `0xC0000`, and available 1. With only
   legacy/erased records initially, pending generation is 1 and final is 2.
4. Resume and press A to PLAY. Confirm actual scene drawing and working inputs;
   a journal commit alone does not prove activation or rendering. Reset normally,
   then confirm the same installed package launches. After boot, print the index
   helper and `__fw0_package_persistent_runtime_prints.gdb`.
5. Return to the shell and repeat the embedded install. The package address
   must remain `0xC0000`; final generation normally becomes 4. Repeat PLAY and
   reset. Two structurally valid journal records mean PENDING plus VALID, not
   two package slots. Calibration and shell access must remain intact.

Do not deliberately cut power during this first device check. The automated
test injects interruptions after every NOR mutation (partial and complete),
I/O failures and corrupt readback, then checks shell-or-complete-image selection,
reinstall recovery and protected-region preservation. It also covers generation
wrap/conflicts and the inclusive 5 MiB low-level writer boundary. Production
transport and V2 support remain separately bounded as described above.

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
5. On a unit holding a bad egg under a current format-2 VALID journal, reset
   without replacing the egg first. (Legacy A/B records instead produce EGGLESS
   under the migration above.) Expect PACKAGE / PKG ERROR / B BACK, not a blank
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
