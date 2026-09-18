# HW6 System-Time Bring-Up

## Scope and Status

First checkpoint: peripheral-free local-time core and native tests only.
`ps_system_time.c` is compiled by the firmware build but is not yet instantiated
or called by an owner, shell, package or scheduler. No user-visible Time page,
new wake source, calendar event, persistent record or public capability is enabled.
No reflash or bench test is needed for this checkpoint.

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

The first core avoids changes to those working paths. The owner adapter must
handle source validity and continuity before exposing a time read/set service.
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

## Next Checkpoints

1. Owner-routed read/set and validity snapshots, using the retained RTC only as
   elapsed input; no game polling and no package access yet.
2. Shell Time entry/editor with explicit Save/Cancel, validation and an unset
   prompt. Opening/cancelling must not change time. Save completion must reflect
   owner acceptance rather than merely queued work.
3. Bench midnight rollover, forward/backward setting, relative timers across shell
   pause/resume and STOP2, then reset and shipment retention/loss. Confirm backup
   and oscillator supply facts before persistence promises.
4. Advertise validated clock reads and later bounded calendar schedules separately.
   Pet midnight/random events require their own scheduler and editor increment.
