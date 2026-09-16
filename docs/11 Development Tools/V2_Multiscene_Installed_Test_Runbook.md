# V2 Multi-scene Installed Test

Status (2026-09-16): installed HOME/AWAY functional hardware PASS; native checks
and Debug build pass. Public multi-scene export remains disabled. This tests the
normal installed path, not the debugger-enabled development session.

## Hardware Result (2026-09-16)

The user confirmed the requested HOME/AWAY behavior, including automatic and
button-driven returns, after normal installation. The first capture showed
working HOME playback but no scene replacements; the follow-up establishes
actual cross-scene work:

- Installed source/model/bytes: 3/2/3440; two scenes, slot 0, generation 10.
- Replacement attempts/failures/status: 4/0/0; latest AWAY -> HOME.
- Admission token/completion/lease/status: 27/27/0/0; profile, schedule and
  display status zero, eight chunks and 4672 bytes.
- Display request/completion/result/fault: 22/22/0/0.
- WFI returns/measured/reconciled: 19/19/19, clock status zero.
- Timer due/applied/error/RTC selections: 4/4/0/4.
- Fresh HOME snapshot: digit phase 0, 400 ms remaining, marker x=32, reveal=0.

These completed operations support the observed functional pass, not merely
thread scheduling. The user reported the requested behavior after the sequence
that included deliberate reboot; the pasted capture retains replacement history
and is not separate post-reset evidence. No new current measurement or precise
timer pause/resume measurement was supplied. Saved stack margin 3140 bytes is
not a worst-case high-water measurement. Remaining timer acceptance cases and
Studio's actual connected-scene project are still separate work.

## Artifact And Setup

The existing labelled HOME/AWAY content is generated unchanged as a 3440-byte
egg, package ID `dev.peepshow.fresh_scene_exit`:

```powershell
python tools/authoring/build_object_development.py --scene-exits --egg-output firmware/peepshow_hw6_fw0/build/Debug/installed_scene_exits.egg
```

Use the newly built normal Debug firmware and its matching ELF. No battery test
configuration is required. Use the normal healthy battery/USB setup for MSC;
do not connect USB while retaining the isolated PPK2 Source Meter battery bench
wiring. No low-voltage test is requested here.

1. Flash the new normal Debug firmware. The existing installed single-scene egg
   should still boot. HOLD START opens the shell.
2. Enter USB FLASH, transfer `installed_scene_exits.egg` to staging, then eject
   and reclaim USB ownership normally. Keep only the intended test egg eligible
   for this scan; retain a known-good single-scene egg on the host for recovery.
3. Select PACKAGE INSTALL. Require VALID, install it, then require INSTALLED.
   Press A to PLAY. Installation replaces the current installed game; settings,
   calibration and other protected regions are unchanged.
4. Do not source any object-scene enable helper or embedded-install helper.
   Those would test a different source/path.

## Observe

- HOME initially shows digit 1, marker left and an empty bottom slot. Digits
  cycle 1-2-3-4 every 400 ms.
- L/R changes only the marker between its labelled slots. Animation continues;
  pressing the selected direction does not move it again.
- A in HOME enters AWAY. B in AWAY enters HOME. Each scene entry starts fresh:
  digit 1, marker left, empty bottom slot. HOME B and AWAY A do nothing.
- HOME fills its bottom slot once after two seconds. Leave HOME before that
  deadline at least once: its old timer must not fill AWAY's slot.
- AWAY stays empty and returns HOME six seconds after entry, including when
  L/R changes selection. HOME gets a fresh two-second reveal timer.
- Release controls to allow automatic STOP2. Confirm visible autonomous cadence,
  correct positions after wake and return to the expected low-current residency
  where measurement equipment permits. Counters alone are not current proof.
- HOLD START, then Resume: the package remains usable. A precisely timed timer
  pause/resume qualification still needs its dedicated fixture; this navigation
  check alone does not close that acceptance case.
- Deliberately reboot. Require HOME again from installed storage, with the same
  controls, timers and autonomous behavior. A debugger reconnect is not reboot.

## Read Results

Wake normally before halting. Reconnect without reset if STOP2 disconnected GDB:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_package_workflow_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_package_persistent_install_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_installed_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_state_scene_timer_prints.gdb
```

Before reboot, require successful install/preflight, COMPLETE stage 11 and zero
verify mismatches. While playing, require source 3, active 1, model 2,
installed/index size 3440, scene count 2, UI/class/lifecycle 6/2/2, successful
completed admission and no lease/display/LPBAM fault. After HOME/AWAY navigation,
replacement count must increase without failures. Expected presentation is four
steps at 400 ms, eight chunks and 4672 payload bytes per scene.

Timer due/applied work and observed changes must agree; sleep reconciliation
must complete after wake. Halted in-flight work may show NOT_RUN or unequal
request/completion counts: resume to completion before judging failure.
After reboot, install/workflow counters may be empty; installed source, byte
count, successful launch and observed HOME behavior are the relevant proof.

## Next Handoff

After this pass, consume Studio's connected-scene project and close remaining
timer hardware cases. Then widen shared export/parser/readiness bounds and
hello/per-scene capabilities together, with per-scene LPBAM budgets and unchanged
whole-egg residency. Do not combine animation cycles across different scenes.
Studio must not special-case multi-scene export before that capability handoff.
