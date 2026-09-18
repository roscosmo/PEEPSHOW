# HW6 Calendar Wake Bench

## Scope

One nonpersistent, thPower-owned bench registration exercises the calendar core
and the existing shared RTC wake timer. There is no package format, scene handler,
UI control, queue-based gameplay delivery or capability promotion in this step.
The installed package is unchanged. No peripheral ownership changes are made.

`g_ps_calendar_probe.request`: ONCE=1 (value is encoded local seconds), DAILY=2
(value is seconds since midnight), CANCEL=3. Requests are copied/consumed by
thPower. Registration count never wraps. Invalid requests leave the existing
registration intact. Battery fault service cancels the bench registration.
The mailbox is deferred while an explicit system-time transaction is outstanding.

The scheduler uses a cached ThreadX deadline while awake. The power owner wait
is shortened to it; ordinary owner-loop passes do not read the RTC. Registration,
clock changes, due service and sleep boundaries obtain explicit fresh snapshots.
Invalid time disarms without a retry polling loop; a successful time SET rebases.

Before sleep, the existing earliest-deadline selection compares calendar against
relative timers, interaction and battery. Existing candidates win equal deadlines.
The RTC ISR only records the shared expiry for calendar; it does not fabricate a
relative-timer command. On wake thPower samples local time again and reconciles
the deadline even after a different source woke first. A calendar IRQ alone is
not proof of a delivered occurrence. Early/chunked wakes do not deliver early.

Long horizons are limited to one day in cached tick arithmetic; the existing
30-minute battery deadline normally wins first. No new periodic RTC poll is
introduced. Calendar registration is bench-global, not paused with the game and
not restored on reset; these are not advertised package lifetime semantics.

Delivery is a synchronous bench latch: occurrence, registration and calendar
generation are recorded after the core consumes the deadline. It proves calendar
work, not a gameplay action or panel transfer. There is no queued gameplay event
whose backpressure/lifetime handling could be inferred as validated. Daily late
wakes deliver one occurrence and select the next future day, not a burst.

## Native Evidence

Tests compile the production adapter/core and actual RTC prepare/IRQ/finish
functions against deterministic stubs. Covered: midnight while kernel ticks are
frozen, awake delivery, no reads before an awake deadline, clock edits, invalid
time, cancellation, battery fault cancellation, calendar selection, battery ties,
no invented runtime command, and battery remaining-time preservation.
Time retention, owner requests and battery regression tests also run. These are
not hardware or current measurements.

## Hardware Procedure

Flash the rebuilt normal Debug firmware and keep the installed egg. Use a settled
scene known to enter STOP2, not an awake-only development session. No low-power
debug configuration changes are required.

1. Wake normally and halt. Source `__fw0_calendar_midnight_enable.gdb`.
2. Resume and leave controls alone for 25 seconds.
3. Wake normally if needed, halt and source `__fw0_calendar_prints.gdb`.

The helper deliberately changes saved local time to 2026-09-19 23:59:40. It queues
that SET through thUI/thPower and registers DAILY midnight. No GDB peripheral call
or forced STOP2 is performed. There should be no screen/sound change at midnight.

Require exactly one additional delivered occurrence for the current registration,
time_status=0, a matching delivered generation, and a new calendar RTC expiry.
Daily should remain armed for the following midnight. Compare cumulative counters;
later buttons and battery wakes are not additional calendar events. PPK2 current
evidence is needed to claim low-current residency. Ordinary debugger disconnect
does not justify a reset; reset discards the bench registration and its evidence.

To end the bench, queue `set var g_ps_calendar_probe.request = 3` and resume.
Restore preferred local time in the shell. The installed package is not modified.

## Hardware Result: 2026-09-19

User-run midnight bench returned:

```text
registration/status/configured/armed/generation = 1/1/1/1/1
time_status/delivered = 0/1
occurrence/deadline_seconds = 843177600/843264000
delivered_registration/delivered_generation = 1/1
rtc_selections/rtc_expiries = 1/1
sample_count = 5
STOP2 entries/RTC source/arm status = 3/3/0
retention writes/failures = 1/0
```

PASS for the single-registration calendar wake bench: an RTC calendar expiry
was recorded and the scheduler consumed one occurrence with matching registration
and clock generation. The next daily deadline is exactly 86400 seconds later.
The final RTC source is BATTERY; this is a subsequent selector snapshot, not a
contradiction of the retained calendar expiry. No exact wake latency, current,
game action, or rendering result is established by this capture. An ELF hash
and PPK2 trace were not supplied with it.

Package/calendar controls remain capability-gated. The next increment is the
runtime delivery boundary described in [[HW6_Calendar_Runtime_Integration]].
