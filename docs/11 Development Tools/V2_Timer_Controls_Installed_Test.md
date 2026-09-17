# Installed V2 Timer Controls Test

Status: 2026-09-17, native verification complete; initial idle/Start,
active Restart, Cancel, false-guard consumption and shell pause/resume
observed on hardware.
This is OS test content, not a Studio export capability change. No firmware
logic, linked development fixture, GUI project or public compiler gate changed.

## Artifact

From the repository root:

```powershell
python tools/authoring/build_timer_controls_fixture.py --egg-output firmware/peepshow_hw6_fw0/build/Debug/installed_timer_controls.egg
```

The generated egg is 3356 bytes. Install it through the already-tested USB MSC
transfer, PACKAGE INSTALL and A PLAY flow. Use only this egg in staging. It
replaces the installed HOME/AWAY egg. No reflash or development-scene enable
helper is needed with the firmware that passed static HOLD and animated LPBAM.
Reboot deliberately to verify the new package starts with GUARD filled, DONE
empty and no running timer. Reconnecting the debugger is not a reboot.

SHA-256: `5d8f3f50d691e96c360544d1fd7a4cf07ecf30d6f3ecd3995a0c03dbe1ffdc27`.

## Display and Controls

The screen is static, headed TIMER 10S. Each indicator has a fixed outline.
Filled GUARD means the expiry handler is allowed to fill DONE.

| Button | Effect |
| --- | --- |
| A | Start the timer if inactive; preserve its deadline if already active. Clear DONE. |
| B | Restart a full ten seconds, even when active. Clear DONE. |
| L | Cancel the timer. Clear DONE. |
| R | Toggle GUARD and its corresponding local state without starting, restarting or cancelling the scene timer. |
| Hold START | Open the system menu and pause the package. Resume continues the remaining time. |

The two local states have identical layouts and no position overrides. Each
button has at most one route per state. A/B/L re-enter their current local state;
R switches states. The scene timer handler has no target state, and its guard
reads the underlying `allow` variable. The relative timer is action-started.
No animation, audio, scene exits, memory/resume-of-recreated-scenes or calendar
time is authored.

## Hardware Cases

Use an external stopwatch or video. Do not halt during the intervals being
measured. Release buttons between presses; do not force STOP2. If an expiry and
button press coincide, repeat with wider separation rather than infer ordering.

1. **Idle:** after PLAY or a deliberate reboot, leave it alone for at least
   twelve seconds. DONE must stay empty. Static HOLD should permit normal sleep.
2. **Start preserves:** with GUARD filled, press A. Press A again about four
   seconds later. DONE must fill about ten seconds after the first press, not
   ten seconds after the second. Repeat with R off then on before expiry: neither
   local state change should move the deadline.
3. **Restart replaces:** press B, wait about four seconds, then press B again.
   DONE clears and fills about ten seconds after the second B, not the first.
4. **Cancel:** press B, wait about three seconds, then L. Wait twelve seconds.
   DONE must stay empty, timer inactive, and no expiry caused by the cancelled run.
5. **False guard:** make GUARD empty with R, then press A. Wait twelve seconds:
   DONE stays empty; due and ignored each increase once, active becomes zero.
   Turn GUARD on with R and wait another twelve seconds. DONE must remain empty;
   changing the guard alone does not retry the consumed one-shot. Pressing A now
   explicitly starts a new run that may fill DONE.
6. **Shell pause:** with GUARD filled, press B and immediately hold START to open
   the system menu. Stay there at least fifteen seconds. If desired, halt in the
   menu and record `paused_remaining_ticks`; it must be positive and below 1000.
   Resume the debugger, select package Resume and time from the return to the
   scene. DONE must remain empty initially, then fill after approximately the
   saved remainder (ticks / 100), not instantly and not after a fresh ten seconds.
   Do not press A/B/L/R to wake for inspection during this measurement: they are
   authored timer/guard controls. Inspect after DONE is visibly filled.

Native tests establish exact tick boundaries; visual timings on hardware do not
establish sub-tick or physical-button latency. The optional paused snapshot gives
a concrete remainder, not a live countdown. A halted debugger changes running
time, so only measure the final interval after resuming execution and the package.

## Prints

After observations, wake normally if necessary, halt, and run:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_timer_controls_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_installed_prints.gdb
```

For the false-guard case, compare counters before and after it rather than
requiring boot-zero counts. The fixture helper reads timer binding index 4 and
objects 0/1 (GUARD/DONE), checked by the native fixture test. Its size/count check
is a convenience, not cryptographic package identification. Raw deadlines are
rebased across STOP2; comparing their numerical values across sleeps is invalid.
The paused remainder is meaningful only while `paused=1`.

Require installed source 3, execution model 2, completed admission/render without
errors, correct physical display behavior and normal return to sleep. Dispatch
counts alone do not prove expiry actions drew anything, and WFI counters do not
measure current. This fixture has static HOLD, not autonomous animation playback.

## Hardware Results (2026-09-17)

The user reported correct behavior for the initial idle and repeated-Start
instructions. Installed source/model/size were 3/2/3356, generation 16;
admission token/complete 29/29 and display request/complete 21/21 had status zero.
Static HOLD backend requested/selected/status was 1/1/0 with held-ready=1.
WFI/measured/reconciled were 4/4/4. This confirms the reported display behavior
and completed work, not measured current or exact button-to-expiry latency.

False-guard expiry advanced due/applied/ignored/errors from 4/4/0/0 to 5/4/1/0,
with timer inactive and DONE empty. The user then explicitly confirmed that R
filled GUARD while DONE remained empty after waiting. The consumed one-shot did
not retry merely because the guard became true.

In the shell, lifecycle=3 and paused=1 retained 647 ticks (6.47 seconds), with
GUARD=1, DONE=0 and counters still 5/4/1/0. After package Resume, the user observed
DONE filling about six seconds later. The subsequent print showed lifecycle=2,
paused=0, active=0, DONE=1 and counters 6/5/1/0: exactly one additional applied
expiry and no error. This supports remaining-duration resume, not a new
ten-second interval or immediate expiry from time spent in the shell.

The final snapshot had GUARD=0 and state activation 26 versus 25 while paused.
It is not an isolated expiry-boundary snapshot, so it does not independently
prove absence of state re-entry during expiry; that remains covered by native
assertions. Do not infer the sequence of intervening input from this print.

The user subsequently confirmed both active Restart (B, four seconds, B; DONE
fills ten seconds after the second press) and Cancel (B, three seconds, L;
DONE stays empty for twelve seconds). The supplied final print had
due/applied/ignored/errors=1/1/0/0, active=0, DONE=0 and rejected_pending=0.
These counters are lower than the preceding capture and belong to a restarted
counter baseline; do not subtract them from the earlier 6/5/1/0 result. The
print corroborates one applied expiry and an inactive final timer, while the
reported observation establishes the Restart timing and absence of a later
Cancel expiry. GUARD was off at the final snapshot; it does not establish the
guard value throughout the preceding sequence.

The R-off/on-while-active deadline check and deliberate reboot of this
particular egg still have no separate recorded confirmation. No public
capability label has been promoted from these results.

## Native Coverage

`test_firmware_object_timers` has 15 passing tests. Its new mode 20 runs the exact
egg through both development scene-set and installed single-scene entry using
the real decoder, graph, timer scheduler and renderer, with host clock/owner
substitutes. The installed harness registers both single-scene and scene-set
admission callbacks, matching the production entry split.

Assertions cover idle action policy, active Start preservation, local guard-state
changes, Restart, Cancel, consumed false-guard expiry, explicit rearming, a
twenty-second simulated suspension with seven seconds remaining, exact resumed
expiry and no state re-entry from the targetless handler. Five full framebuffer
states are compared to independently constructed expected pixels, including
unchanged labels/outlines and both indicators. These tests do not execute the
physical shell, RTC wake, flash installation or panel transfer.

The four `test_firmware_object_stop2` regression tests also pass (19 focused
tests total). The new GDB helper's timer/object symbols resolve against the
existing Debug ELF without connecting to hardware. Generated bitmap layout was
visually inspected; labels and indicators remain separate and within the panel.
No firmware rebuild was needed.

Do not promote `time.scene_elapsed` or `time.state_entry_elapsed` capability
labels from this preparation alone. Record individual hardware outcomes before
the shared bounded export/readiness handoff.
