# HW6 System-Time Bring-Up

## Scope and Status

Fourth checkpoint: warm-reset retention added to the optional shell Time editor.
Native tests and firmware build pass. The user confirmed time continues after
NRST and does not survive full power-down on the corrected build.
NRST/software reset may preserve time while powered. Shipment/power loss returns
UNSET: the user confirmed the RTC domain is on the MCU rail and neither it nor
the external MEMS oscillator remains powered during shipment.

The earlier checkpoint below describes the original RAM-only adapter and its
historical reset-to-UNSET evidence; the retention section supersedes that reset
policy, not the earlier test results.
`thUI` submits one copied request to `thPower`, which samples the RTC and owns
the local-time mapping. The debugger mailbox exercises this same request path.
Firmware build and native tests pass. HW6 owner read/set, midnight rollover,
forward/backward edits and reset-to-UNSET passed the bench checks below. Debugger
cleanup caused a separate unresolved lockup; safe automated detach is not validated.
The shell editor builds and passes native tests. The user confirmed physical
entry, draft cancellation, SAVED feedback, advancing time on reopening, and that
the layout fits. No new wake source, calendar event, persistent record or public
capability is enabled.

Authority: [[Time_And_Power_Intent_API_Contract]],
[[Shell_Settings_Calibration_Contract]], [[Authority_and_Invariants]].
Platform owns time setup/validity and physical wake scheduling. Packages may
eventually read or schedule only through advertised Engine interfaces, not set
system time. A single owner must serialize core calls; it is not a shared-memory
cross-thread API. `thPower` owns RTC access; shell uses typed owner requests.

## Decisions

- Local date/time only, initially 2000-01-01 through 2099-12-31 inclusive.
  These are representation limits, not tunable gameplay settings.
- Shell time setting will be optional. Unset/lost time offers setup without
  forcing it or preventing ordinary package boot.
- Relative elapsed timing is independent of editable local time.
- Calendar gameplay events are scheduled, not polled. Explicit transition/handler
  time checks read a snapshot only when reached.
- Random scheduled events choose and retain one deadline. No reroll on polling,
  wakes, redraws or unrelated state changes.
- Clock adjustments skip crossed calendar events, without catch-up. Future
  deadlines must be rebuilt for the new calendar generation. Relative countdowns
  do not restart, expire early or extend as a consequence of a clock edit.

## Core Representation

The owner supplies a continuous elapsed-millisecond sample. A valid set records
that sample and a local-seconds anchor. Reads derive local time from elapsed
progress since the anchor; setting local time never writes the elapsed source.
Date conversion validates real month lengths and leap years in the supported
century. Reads include subsecond remainder, weekday (Monday=1) and generation.

Init starts UNSET. Invalid date/set arguments leave state unchanged. Successful
sets increment generation even when the requested date is unchanged. Detected
elapsed-source regression or local range exhaustion invalidates the mapping;
later samples cannot silently restore validity. Failed reads leave output
unchanged, so callers MUST inspect status rather than display stale output as
current time. Generation never wraps; exhausted generations reject further sets.

The owner must invalidate on source loss/reset or an unverified elapsed lifetime
change. A stopped source cannot be detected from arithmetic alone. Init must run
at each new lifetime until a validated persistence/retention adapter exists.
No assumptions about backup power, reboot or shipping retention are encoded.

## Existing Hardware Integration Constraints

Observed in current code:

- `MX_RTC_Init` in `main.c` unconditionally writes midnight, 1 January 2000.
  Calendar retention cannot be claimed while that startup policy remains.
- `PS_HW6_RTOS_ObjectRtcMilliseconds` derives sleep accounting from RTC calendar
  reads; changing that physical calendar directly could corrupt elapsed accounting.
- HW6 uses an external nominal 32.768 kHz input on PC14. Reviewed hardware docs
  do not establish backup-domain and oscillator supply retention through shipment.

The owner adapter avoids changes to those working paths. It reads time then date,
validates the sample, and invalidates local time on failed reads or detected
source regression. It never calls RTC setters. A stopped source still cannot be
detected from two identical samples alone.
Any later generated startup changes must stay inside USER CODE blocks. RTC wake,
display EXTCOMIN, battery monitoring and relative scene timers must remain intact.
Do not restore an old flash timestamp and pretend it advanced while power was off.

## Verification

`test_firmware_system_time.py` compiles the production C core with warnings as
errors and runs `native_system_time.c`. Coverage includes every date in the
supported century, leap-day/midnight/year rollover, invalid dates, forward and
backward local adjustments, subsecond progress, source regression, range/64-bit
overflow boundaries, generation exhaustion and unchanged failed outputs/state.
This is arithmetic/model evidence, not physical RTC, drift or retention proof.

`test_firmware_system_time_owner.py` compiles the production transaction block
with deterministic queue and RTC stubs. It checks owner identity, copied inputs,
busy/send failures, early/wrong-token consumption, stale/duplicate messages,
completion during queue send, source-read failures and invalid samples, midnight
and subsecond progress, clock edits, token exhaustion and explicit-only requests.
This does not simulate ThreadX scheduling or establish physical STOP2 continuity.

## Owner Transaction

`PS_HW6_SystemTime_Request` and `PS_HW6_SystemTime_Take` are thUI-only. The existing
four-word power queue carries magic, token, operation and encoded local seconds;
no transient pointers cross owners. Only one request may remain outstanding.
Send success means queued, not completed. `thPower` publishes result before its
completion token; thUI consumes only that token. Failed sends release the slot.
A timeout must not release a queued request: a late SET may still execute.
No new thread, queue, retry loop, periodic RTC read or clock policy is introduced.

READ returns an explicit local status plus raw-source status. A healthy RTC with
an unset local mapping is UNSET, not midnight presented as valid user time. SET
validates the date before queueing and starts local fractional time at zero when
the owner processes it. Reset deliberately initializes the mapping to UNSET.

## Bench Sequence

Use the normal Debug firmware build; no package replacement is needed. Wake
normally before requesting a debugger halt if the device is in STOP2. Helpers
only write a mailbox; GDB never calls peripheral functions. Resume between each
request and print, allowing the UI and power owner to complete real work.

1. After reset, source `__fw0_system_time_read_enable.gdb`, resume briefly, halt,
   then source `__fw0_system_time_prints.gdb`. Expect matching nonzero request and
   complete, pending=0 after consumption, send/source status=0 and local UNSET=1.
2. Set the test date fields below, then source `__fw0_system_time_set_enable.gdb`.
   Resume briefly and print. Expect local/source status=0, generation=1, and the
   requested date. This is a test timestamp, not the host's current time.

```gdb
set var g_ps_system_time_request_local.year = 2026
set var g_ps_system_time_request_local.month = 9
set var g_ps_system_time_request_local.day = 18
set var g_ps_system_time_request_local.hour = 23
set var g_ps_system_time_request_local.minute = 59
set var g_ps_system_time_request_local.second = 50
```

3. Resume for at least 15 seconds, allowing normal STOP2. Wake normally, request
   another READ, resume briefly, halt and print. Expect September 19 after midnight
   with unchanged generation. Elapsed time includes time spent preparing commands;
   compare local advancement with the raw source sample, not wall timing at a halt.
4. SET an earlier local hour/date and then a later one. Each completed SET advances
   generation; raw source milliseconds must continue naturally, not jump to match
   the edited date. Check the running scene, inputs, relative timers and normal
   sleep behavior remain intact. Do not halt during timed observations.
5. Reset and READ again: UNSET is expected. This is not a retention failure.

Print helpers show a completed snapshot, never automatically refresh it. The
snapshot cannot prove oscillator drift, current consumption or shipping retention.

## Next Checkpoints

1. Complete remaining shell edge-case checks: month/leap-day edits, idle/wake,
   package resume and reset through the shell. Basic editor checks passed below.
2. Bench relative timers across shell editing, pause/resume and STOP2. Confirm backup
   and oscillator supply facts before persistence promises.
3. Advertise validated clock reads and later bounded calendar schedules separately.
   Pet midnight/random events require their own scheduler and editor increment.

## Shell Time Editor

SYSTEM now offers TIME, CALIB and PACKAGES. TIME replaces the nonfunctional
SETTINGS placeholder row; calibration and package routes are unchanged. Setup
is optional and never prevents normal package boot. No automatic first-boot prompt
is introduced in this checkpoint. The router API is 19; existing page/event IDs
are preserved and TIME is appended as page 12.

- Opening requests one owner snapshot. Valid local time seeds the draft; unset,
  lost or out-of-range time shows TIME NOT SET and a draft of 2000-01-01 00:00:00.
  That draft is not committed until Save. Queue/transport failures show retry/back.
- Joystick left/right or L/R selects YEAR, MONTH, DAY, HOUR, MINUTE, SECOND,
  SAVE or CANCEL. Up/down changes a date/time field, wrapping its legal range.
  Changing month/year clamps an invalid day, including leap-day cases.
- A advances to the next field, or activates focused SAVE/CANCEL. B cancels.
  SAVE validates and submits once; SAVING blocks editing, duplicate Save and
  user cancellation until the matching owner result arrives. No success is shown
  merely because the request was queued. A failed Save preserves the draft.
- A successful Save shows SAVED and leaves the editor open. It is an editing
  snapshot, not a ticking clock. Reopening requests a fresh snapshot.
- A read can finish after Cancel without reopening the editor. Session identity
  prevents stale completion from changing a newer draft. A power overlay may
  interrupt an accepted Save; it cannot undo the owner transaction. Its completion
  updates only the matching session and never dismisses the power overlay.
- The footer states RESET CLEARS TIME. No flash, backup-domain persistence,
  raw RTC edit, calendar scheduling or public game-time API is added.

The UI service consumes the existing single-flight token without blocking or
introducing a new polling cadence. Only explicit entry/Save/retry requests sample
the RTC. Loading/saving or an outstanding time transaction temporarily blocks
ordinary automatic idle; settled editing is eligible for the normal held-display
STOP2 path. Battery fault/shutdown handling remains authoritative.

Display data is copied into a dedicated four-word message: TIME magic, encoded
civil seconds, focus and status. On thDisplay the existing RenderUI call uses
page TIME, calibration argument=encoded seconds, focus=field, shutdown=NONE,
countdown argument=editor status. The renderer does not read the mutable draft.
The page clears and redraws on input/completion, has no cursor-blink animation,
and invalidates the old list-focus cache. Joystick-state updates without a
dispatched direction do not redraw TIME.

Native evidence: `test_firmware_time_editor.py` exercises the actual router/time
core and production text drawing functions. It covers Cancel, field wrapping,
month/leap-day clamping, Save failure/retry, duplicate Save, abandoned reads,
stale sessions and shutdown overlays. All 56 focus/status rendering combinations
are nonblank with no out-of-bounds or overlapping black-pixel writes. This is not
a physical panel-transfer or joystick test. Owner tests also exercise the real
editor service with queue/RTC stubs; shell/package workflow tests remain passing.

### Shell Bench Check

User-confirmed hardware results for this checkpoint: entered through START/system
navigation; cancelled edits and reopened to a reset draft; saved and observed SAVED;
reopened after saving and observed advancing time; all text fitted on screen.
This is observed UI behaviour, not queue-counter inference. The report does not
separately establish month/leap-day handling on target, idle current, shell idle/wake,
or a new reset-through-editor test. The
earlier owner-level reset-to-UNSET evidence remains valid. No debugger cleanup
experiment was repeated for these shell checks.

### Relative-Timer Clock-Edit Regression (2026-09-19)

The user confirmed the installed TIMER 10S test passed after the shell waiting
timeline fix: start a guarded countdown, suspend through HOLD START, edit/save
local time, and RESUME the package. The requested procedure covered forward and
backward date edits and completion after the preserved remaining duration rather
than immediate expiry or a fresh ten seconds. This is user-observed functional
evidence, not a precision timing measurement. No further manual repetition is
required for this checkpoint.

SYSTEM navigation was usable after the fix. HOME (RESUME / SYSTEM / PACKAGES)
still has a static cursor while SYSTEM blinks; HOME is explicitly excluded by
the current blink eligibility. The user deferred that visual inconsistency to
the shell overhaul. It is not evidence that the fixed SYSTEM busy loop persists.

### Retention Boundary (NRST and Power-Down Confirmed)

The retention adapter now implements the following boundary:

- Distinguish continuously powered warm reset from loss of the RTC/oscillator
  supply. Current startup unconditionally resets the raw RTC calendar.
- Preserve the raw elapsed source on qualified warm resets; retain the local
  mapping separately rather than writing user edits into the raw RTC calendar.
- A retained mapping needs a version, integrity validation, commit ordering and
  explicit association with the retained source lifetime. Torn or invalid records
  must not produce an apparently valid clock.
- On unverified source continuity, report UNSET/SOURCE_LOST. Do not restore a
  flash timestamp and pretend it advanced during shutdown.
- Hardware clarification from the user: the RTC domain uses the MCU rail; the
  external MEMS oscillator runs while the device is powered, but nothing remains
  powered during shipment. Shipment retention is explicitly unsupported.

Startup changes are confined to main.c USER CODE blocks. After HAL RTC init,
qualified PIN/software resets with an initialized calendar and matching DR0
cookie bypass the generated calendar setters, while still enabling the existing
1 Hz calibration output. BOR, option-byte, low-power and watchdog reset flags,
unknown reset causes or a missing cookie select fresh startup. Reset flags are
captured in `g_ps_time_retention_probe.reset_flags` before being cleared so an old
power-on flag cannot poison every later warm reset. DR0 is invalidated before
fresh calendar writes and committed only after initialization completes.

### Year-Zero Hardware Finding and Correction

The first NRST bench attempt failed: reset flags were 0x04004400 (PIN reset,
no BOR), source_preserved/mapping_restored=0/0, and the completed READ was UNSET.
The RTC ICSR was 0x27, with INITS (bit 4) clear. DR0 read 0x52544301 and payload
words survived, while DR1 was cleared by the rejection path. This is not evidence
that backup power was lost.

The private raw RTC previously started at year 00 (2000). INITS depends on a
nonzero calendar year, so that valid running baseline fails both our retention
test and the HAL_RTC_Init initialized-calendar branch. The original native stub
incorrectly treated INITS as independent of the calendar year.

Cold startup now completes in RTC_Init 2 by setting and reading back the private
raw date 2001-01-01 (Monday), before committing source cookie 0x52544302. Failure
leaves the cookie invalid and enters the existing startup error handler. The
generated year-zero assignment remains untouched; it is superseded only on the
cold path before any runtime elapsed accounting starts. A qualified warm path
skips both calendar assignments. User local dates still support 2000..2099 and
never edit the physical RTC.

Old source cookies are rejected deliberately, requiring one new local-time set
after flashing this revision. Native tests now derive INITS from the fake raw
year, exercise date-write/readback failures and old-cookie rejection, and verify
warm preservation performs no date writes. Firmware build and four time tests
pass. The user subsequently confirmed that time continues after NRST and is
lost after full power-down. This is observed hardware behaviour, not inferred
from native tests or scheduling counters. Software-reset retention, precise
drift and interrupted-write behaviour on physical hardware were not separately
measured by this check.

DR1..DR8 are reserved for the local mapping: version/commit word, 64-bit raw
anchor, local-seconds anchor, generation, 64-bit last-observed raw time and CRC32.
thPower restores the record at startup and updates it after explicit time requests.
The commit word is invalidated first and written last after payload readback;
readback failure returns a non-OK result rather than SAVED. Interrupted writes
may lose the setting but must not accept a partial record. No heap, flash storage,
new thread, periodic poll or RTC calendar write is used for user edits.

Restore validates integrity, ranges and anchor ordering. The first requested
snapshot checks the live raw source against the retained last observation.
Failure invalidates the mapping. This does not detect every conceivable stopped
oscillator or hardware fault; retention is not a substitute for source monitoring.
Backup survival alone never grants shutdown retention.

Native tests cover NRST/software reset qualification, cold/unsafe lifetimes,
all single-bit record corruptions, interruption before each commit step, lost
calendar initialization, midnight advancement, generation preservation and raw
source regression. Existing owner/editor tests pass. No new calendar scheduling
or public game-time capability is advertised.

Short hardware check (no debugger register changes): set/save a recognizable
time in shell TIME, press NRST, reopen TIME and confirm it advanced instead of
returning UNSET. Separately use normal shipment and START: TIME must be unset.
Keep the installed egg; no package rebuild is required.

### Original Editor Procedure (RAM-Only Checkpoint)

The procedure below records the earlier editor bring-up. Its reset-to-UNSET
expectation is superseded by the warm-reset procedure above.

Reflash the normal Debug firmware; keep the installed egg unchanged. No GDB
time-setting helper or low-power-debug manipulation is needed for this test.

1. HOLD START to open the system root, choose SYSTEM, then TIME. After reboot
   expect TIME NOT SET and the editable default draft.
2. Change several fields, then B. Reopen TIME: it must still be unset. Try the
   focused CANCEL action as well.
3. Set a recognizable date/time, select SAVE and press A. Expect SAVED, then
   leave/reopen after several seconds. The newly read time must have advanced.
4. Try January 31 -> February and a leap-year change; the day must clamp to a
   valid date. Check left/right selection, up/down adjustment and all labels fit.
5. Leave the editor idle, then wake normally and continue editing. Resume the
   package and confirm its normal inputs, presentation and relative timers.
6. An intentional reset must return TIME to TIME NOT SET without blocking the egg.

Optional read-only debugger evidence: `p g_ps_ui_time_probe` shows draft/session,
editor status and pending request; `p g_ps_system_time_probe` shows the completed
owner transaction. Editor status LOADING/UNSET/EDIT/SAVING/SAVED/READ_ERROR/
SAVE_ERROR=0/1/2/3/4/5/6. These do not prove the panel was physically updated.

## HW6 Bench Results: 2026-09-18

Normal Debug firmware with the owner adapter and the existing installed test egg;
the adapter changes were uncommitted during testing. Cortex-Debug 1.12.1 and
ST-Link GDB server 7.7.0 were used. Agent-driven tests attached without flashing;
the reset check deliberately used the server's system-reset command.

| Check | Completed result |
| --- | --- |
| Initial READ | request/complete=1/1, pending=0, local UNSET=1, RTC status=0, source=19386 ms |
| Initial SET | request/complete=2/2, 2026-09-18 23:59:50.000, generation=1, source=138621 ms |
| Midnight READ | request/complete=3/3, 2026-09-19 00:01:08.828, generation=1, source=217449 ms |
| Backward SET | request/complete=4/4, 2026-09-18 12:00:00.000, generation=2, source=886847 ms |
| Forward SET | request/complete=5/5, 2026-09-20 12:00:00.000, generation=3, source=901546 ms |
| READ after reset | request/complete=1/1, pending=0, local UNSET=1, RTC status=0, source=8738 ms |

All completed transactions had send status=0 and rejected messages=0. Valid SET
and READ results had local/source status=0. Midnight local advancement exactly
matched the raw-source difference of 78828 ms. The two-day forward edit advanced
the raw source only 14699 ms. These are completed owner operations, not merely
thread scheduling or accepted queue requests.

The midnight result was initially obscured by malformed remote replies, including
a long response to a four-byte read and displaced probe fields. A clean attach
without reset recovered the coherent request-3 snapshot above without another SET.
Malformed debugger output must not be treated as proof of firmware memory corruption.

For the agent-driven edit/reset checks, the existing debug-low-power helper changed
DBGMCU CR from 0x0 to 0x6. Halts reached HAL_PWREx_EnterSTOP2Mode at __WFI(). These
observations do not establish normal STOP2 current, oscillator drift, shipping
retention, or complete relative-timer regression coverage.

### Unresolved Debugger Cleanup Lockup

After the successful reset READ, while halted at STOP2 __WFI(), the agent attempted
to restore DBGMCU CR to its original zero value. GDB reported that it could not
access 0xE0044004; readback and detach also failed. Subsequent attach attempts could
not find the target, including while the user held a button. The device was
unresponsive, NRST did not recover it, and the user needed the 12-second PMIC
shipment operation. START then booted the existing test egg normally.

The register write's final effect is unverified. A stranded low-power debug state
is a hypothesis, not an established root cause; the failure of NRST means a simple
CPU-halt explanation is insufficient. Successful time results precede this incident
and do not demonstrate safe debugger cleanup or complete platform robustness.

Do not repeat disabling low-power debug while halted inside STOP2. Any future
automated cleanup needs a separately verified awake point outside the sleep-entry
path and a recovery plan. No firmware workaround or further target manipulation
was made for this incident. Low-power debug state after physical recovery has not
been read back. The shell Time editor does not require automated low-power detach.
