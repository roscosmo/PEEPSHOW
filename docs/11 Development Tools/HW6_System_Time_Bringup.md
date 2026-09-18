# HW6 System-Time Bring-Up

## Scope and Status

Second checkpoint: local-time core plus a power-owned read/set transaction.
`thUI` submits one copied request to `thPower`, which samples the RTC and owns
the local-time mapping. The debugger mailbox exercises this same request path.
Firmware build and native tests pass. HW6 owner read/set, midnight rollover,
forward/backward edits and reset-to-UNSET passed the bench checks below. Debugger
cleanup caused a separate unresolved lockup; safe automated detach is not validated.
No user-visible Time page, new wake
source, calendar event, persistent record or public capability is enabled.

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

1. Shell Time entry/editor with explicit Save/Cancel, validation and an unset
   prompt. Opening/cancelling must not change time. Save completion must reflect
   owner acceptance rather than merely queued work.
2. Bench relative timers across shell editing, pause/resume and STOP2. Confirm backup
   and oscillator supply facts before persistence promises.
3. Advertise validated clock reads and later bounded calendar schedules separately.
   Pet midnight/random events require their own scheduler and editor increment.

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
