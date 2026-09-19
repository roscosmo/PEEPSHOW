# HW6 Calendar Runtime Integration

## Status and First Scope

The wake and V2 handler benches passed on 2026-09-19: the square filled once,
A cleared it without replay, and shell suspension deferred the fill until return.
API 51 source/export integration is now implemented and native-tested. Its new
installed artifact still needs hardware confirmation; this is not shipping
qualification or measured-current evidence.

### API 51 Public Export

V2 supports one scene-owned `time.local_schedule` binding with exactly one
independent handler, using existing `event_binding.*` and `event_handler.*`
commands and handler layout. Configuration:

```json
{"mode": "daily", "time_of_day_seconds": 43200, "day_offset": 0}
```

Modes: `daily`, `today_offset`, `next_occurrence`. Seconds: 0..86399. Day offset:
0..32767, nonzero only for `today_offset`. Resolution outside 2000..2099 fails
rather than wrapping. Scene entry registers automatically; local state changes
preserve the schedule. Scene replacement cancels it and fresh entry registers
again. Unset time waits for an explicit clock change without repeated RTC reads.
No package-wide ownership, calendar timer-control actions, random windows,
time guards or saved-game scheduling are exposed.

Suspension does not shift deadlines. One retained daily occurrence covers the
wait for its handler; completion advances past additionally missed days without
a second catch-up burst. Clock edits skip crossed deadlines and invalidate
unclaimed work. False guards consume the occurrence. One-shot resolution is
retained across clock edits, not recalculated from the new date.

The hello calendar capability, per-scene capabilities and target profile expose
`available_pending_validation`. Restricted development export is enabled;
shipping remains unavailable. Native tests cover the public builder/parser,
actual installed C loader/handler/framebuffer, automatic registration, unset
clock recovery, retained queue messages, multi-day suspension and preview timing.

Preview adds `project.preview_set_local_time` with a local ISO `local_time`
string or null, plus `project.preview_suspend` and `project.preview_resume`.
These use normal project/preview revisions. `preview_advance` advances calendar
time while suspended but pauses scene-relative time. Resume returns deferred
`timer_events`; setting time never runs catch-up handlers. No host-clock polling
is used. This simulation command is not a package permission to set device time.

### Exported Artifact Hardware Test

Build: `python tools/authoring/build_calendar_export_fixture.py --output firmware/peepshow_hw6_fw0/build/calendar-export`.
The builder saves/reloads a `.peepproj` and invokes the normal public exporter.
Flash current Debug firmware; install `build/calendar-export/calendar_export.egg`.
Artifact: 1000 bytes; SHA-256
`56d58d58e12d557d7799c1131794951be993172ba90ae110e773d664aacceea3`.

Halt in the Calendar scene and source `__fw0_calendar_export_time_enable.gdb`.
It sets only system time to 2026-09-19 23:59:40, not calendar registration.
Resume untouched for 25 seconds: square fills once. A clears it without replay.
Collect `__fw0_calendar_runtime_prints.gdb`; require setup=0, one applied
increment, failures=0 and a calendar RTC expiry. Repeat shell suspension on a
later date: daily history intentionally prevents replay of an already delivered
date. Restore preferred time through shell TIME. Do not use the old bench enable
helper with this package or change low-power debug bits.

The sections below retain the original bench design and procedure for reference.

The first runtime increment connects a bounded calendar registration to an
existing V2 independent event handler. Start with scene-owned daily scheduling
and a targetless handler so calendar delivery can be distinguished from state
re-entry. Scene changes must invalidate the old registration. Package-session
ownership, random windows and persistent game schedules remain separate work.
In particular, a pet-wide daily reset that must survive every scene change will
need package-session ownership; a scene-owned bench does not provide that feature.

## Existing Integration Points

### Live Runtime Bench

`ps_hw6_calendar_runtime` connects thPower and thRuntime using the existing
four-word queues and a fixed immutable registration lease. Runtime debug requests
are DAILY=1, TODAY_OFFSET=2, NEXT_OCCURRENCE=3, CANCEL=4. Binding, time_of_day
(seconds since midnight) and day_offset are captured once. Setup requires valid
local time and an installed scene's action-started scene timer handler. No action
starts the fixture's relative timer; the calendar invokes that existing handler.
Public source/package calendar encoding is not introduced by this bench.

The adapter retains failed sends and attempts at most one outbound send per
owner service pass. Notifications and claims carry sequence/registration identity.
Runtime rechecks scene activation and suspension before the granted handler runs
through normal event admission and presentation completion. Scene replacement
cancels scheduling. Shell suspension retains the deadline and defers execution.
Clock changes invalidate unclaimed work; a claimed transaction finishes, unless
runtime defers it without execution, in which case stale clock work is discarded.

Pending work blocks another scheduler consumption. Work discovered during sleep
preparation aborts that sleep attempt, but does not force a zero receive wait
that would starve runtime. Suspended pending work can sleep. Battery fault
cancellation removes the calendar sleep hold. The older power-only debug mailbox
cannot overwrite a live runtime registration. Registration is nonpersistent and
must be cancelled before another explicit setup. See the ICD for lease/retry
limits; this does not introduce broken-owner recovery policy.

Native tests run both production endpoint implementations with bounded queue
stubs: failed notification/completion sends, stale clock events, scene replacement,
suspension before expiry and between claim/grant, ignored completion and a
day-offset one-shot. A separate test loads the actual egg via the production
installed/development loader, calls the production calendar handler adapter,
checks unchanged state activation and changed framebuffer pixels, then verifies
A restores the original pixels. Hardware remains pending.

#### Hardware Procedure

Flash normal Debug firmware and install through MSC:
`firmware/peepshow_hw6_fw0/build/calendar-runtime/calendar_dispatch.egg`.
Builder: `tools/authoring/build_calendar_runtime_fixture.py`.
Artifact: 1004 bytes; SHA-256
`9cca9f71493d8315c9bd7c093ffce77294bf1068e3540fe760051d8759dda51f`.
The native test verifies compiled handler binding index 1.

In the Calendar scene, wake normally/halt and source
`__fw0_calendar_runtime_enable.gdb`. This changes saved local time to
2026-09-19 23:59:40 and registers daily midnight. Resume untouched for 25 seconds:
the outlined square must fill once. A clears it without rearming; it stays empty.
Wake normally/halt and source `__fw0_calendar_runtime_prints.gdb`.
Require setup=0, one applied increment, failed=0, pending_ack=0, delivery_state=3,
a calendar RTC expiry and the observed fill. Counters do not prove visible pixels
or measured current.

Cancel with `set var g_ps_calendar_runtime_probe.request = 4` and resume before
another run. To test suspension, rearm then HOLD START before expiry, stay in
shell past midnight and resume: the square fills only upon return. Restore your
preferred time afterward. Do not change low-power debug bits. GUI capability
remains unavailable pending hardware evidence and source/export integration.

### Power Transport Endpoint

`ps_calendar_transport` now wraps the delivery record in a four-word CAL1
protocol: magic/version, operation, sequence, registration. Operations are
NOTIFY=1, CLAIM=2, GRANT=3, ACK_APPLIED=4, ACK_IGNORED=5, ACK_FAILED=6.
The message size is compile-time checked. The fixed record retains the full
identity and occurrence; queue payloads contain no pointers.

The endpoint supplies one pending outbound message at a time. Only a successful
enqueue may call Sent. Failed notification/grant sends leave the message
available; a successful send suppresses retransmission. Claims require a sent
notification; completion requires a sent grant. Old sequence/registration pairs
are rejected. Invalidation before claim rejects old notifications; after claim
the admitted transaction retains the slot until explicit completion.

This module is the power-side protocol, not the live HW6 queue adapter. The
runtime endpoint must retain unsent claims/acknowledgments, suppress duplicate
grants and revalidate scene lifetime and suspension before executing actions.
Registration must establish the matching immutable identity at both endpoints.
The adapter must define bounded retry and teardown/timeout behavior, including
rejection of claims after invalidation, before connecting these packets to the
owner loops. No unbounded wait or retry is implemented by the endpoint itself.

Native tests simulate full outgoing queues by withholding Sent; they verify
retained packets, ordering, duplicate rejection, stale registration/sequence,
clock invalidation and ignored completion. They do not exercise ThreadX queues
or prove hardware execution. The live adapter and hardware procedure above now
extend this endpoint; public capability promotion remains unavailable.

### Arm-Time Authoring Rules

Agreed author-facing forms are local time plus a nonnegative calendar-day offset,
next occurrence of a local time, and daily recurrence. Literal date entry is not
required for the initial editor experience. Midnight is a rollover test case,
not a special runtime trigger.

`PS_CalendarTimer_ResolveOnce` implements the first two forms in the core:

- TODAY_OFFSET resolves today's date plus N calendar days at the selected time.
  A past or equal deadline is skipped by the existing ONCE configuration path;
  it never silently rolls to tomorrow.
- NEXT_OCCURRENCE selects the strictly next occurrence: today if still ahead,
  otherwise tomorrow. At exactly the chosen second it selects tomorrow.
- DAILY remains a separate recurrence using the existing DAILY scheduler kind.

Resolution happens once on a new arm or explicit restart using a valid snapshot.
Start while already armed must not resolve again. Wake, local state changes,
resume and clock rebasing retain the resolved one-shot deadline. Invalid time
returns unavailable without inventing today's date or changing the output;
the future adapter must explicitly handle the unresolved registration.
Range exhaustion beyond 2099 is rejected, not wrapped. Day offsets are civil
calendar days in the local-only clock model, not relative elapsed countdowns.

Native coverage includes past-time consumption, exact/subsecond boundaries,
leap-day/month/year rollover, large-offset range rejection, unavailable time
and deadline preservation across rebasing. This core API does not expose a
Studio command or change executable package encoding. The live bench uses the
resolver; public capability advertisement remains outstanding.

### Delivery Record Implementation

`ps_calendar_delivery` now implements the single-owner record lifecycle:
EMPTY -> PENDING -> CLAIMED -> APPLIED/IGNORED/FAILED. Invalidation removes
unclaimed work; claimed work retains its slot until explicit completion. Offer
cannot overwrite pending/claimed work. Notification transport does not mutate
the record, so a failed send alone cannot discard it. Every claim/completion
checks the complete identity and a monotonically increasing sequence. Sequence
exhaustion rejects new work rather than wrapping. A terminal occurrence cannot
be offered again with the same identity and an equal or earlier deadline.

The live bench connects this to owner queues and a V2 handler. All mutation
must occur in one owner; it is not a concurrent mailbox. The adapter must supply
the current authorized identity, serialize clock changes/claims, prevent obsolete
registrations from being re-offered, and retain scheduler occurrences while the
delivery slot is busy. Cross-generation daily deduplication remains the calendar
scheduler's responsibility. Lifecycle suspension must also be checked again by
runtime before action admission, not inferred from an earlier claim request.

Native tests compile the production module and exercise retained pending work,
suspended claims, every identity component, stale sequences, invalidation before
and after claim, duplicate completion, false-guard/failed terminal outcomes,
fresh scene identities and sequence exhaustion. These are record-level tests,
not actual queue saturation, cross-thread race, handler or hardware evidence.
The normal Debug firmware builds; installed runtime behavior is unchanged.

- `ps_calendar_timer` owns peripheral-free deadline arithmetic and consumed
  occurrence history. The current HW6 adapter consumes into a diagnostic latch.
- `ps_hw6_calendar` and thPower own calendar snapshots and shared wake selection.
  They must not call gameplay handlers directly from the power thread or ISR.
- `PS_HW6_RTOS_RuntimeStateTimersService` already checks runtime lifecycle and
  dispatches independent handlers through `PS_SceneRuntime_HandleStateSceneEvent`.
  Its relative tick deadlines and pause accounting must remain unchanged.
- `ps_scene_runtime_event_binding_t` currently carries event class/kind/source
  and parameter. A new calendar meaning requires coordinated validation, encoding,
  decoding and capability work; do not reinterpret existing relative delays.

## Delivery Boundary

The next implementation should establish this boundary before adding authoring
or export controls:

1. Use fixed-capacity storage and existing owner queues, with no dynamic allocation.
   Runtime requests identify package session, scene activation, binding and arm
   generation. thPower retains a distinct registration identity and clock generation.
2. An expiry creates a retained pending occurrence containing those identities
   and the original local deadline. A successful queue send is notification only,
   not successful handler application. Do not discard the occurrence on queue full.
3. Runtime validates lifetime and generation before admitting the handler. A
   serialized claim/acknowledgment boundary must order clock edits against dispatch:
   edits invalidate unclaimed old-generation events; already admitted transactions
   finish normally. Checking an unsynchronized generation once is insufficient.
4. Runtime uses the existing transactional event/action path. Ordered actions,
   targetless behavior and explicit transitions retain their current semantics.
   A false guard consumes the occurrence without retry. Admission failure is
   explicit and must not silently rerun partially applied actions.
5. Cancellation, replacement, stop/unmount and restart invalidate old notifications
   even when the same binding index is reused. Local state changes alone do not
   invalidate scene-owned registrations.
6. Pending work needs bounded notification/retry behavior and a completion path;
   it must not turn an expired cached deadline into a zero-wait owner loop.
   Transport retries are separate from retrying gameplay actions.

## Suspension and Ordering

Calendar time does not pause in the shell. No suspended handler runs there.
Pending delivery prevents STOP2 until runtime explicitly acknowledges suspension
with DEFER, or claims and completes the occurrence. Successful notification send
alone is not permission to sleep: ThreadX can hand it directly to a waiting
receiver without leaving an enqueued message. DEFER retains the runtime notice
for a single claim on resume; a blocked DEFER reply continues to prevent sleep.
Claimed work remains awake until its completion acknowledgment reaches power.
On resume, an overdue registration supplies at most one occurrence; daily
recurrence advances to the next future day without a backlog burst. Relative
timers continue to preserve their remaining duration as before.

Clock edits are different from ordinary late wake: crossed occurrences are
skipped and old pending generations invalidated. Invalid time disarms scheduling
until an explicit valid-time notification, without a polling loop.

For multiple due calendar registrations, define stable ordering by original
deadline then compiled binding identity. Recheck owner identity after each handler,
because an earlier handler can cancel or replace a later event's owner. Ordering
against already queued input and relative timer events must be specified and
tested before enabling more than the single-binding runtime bench.

## Verification Before Export

- Queue full followed by recovery: no lost occurrence or duplicate action.
- Clock edit before notification, after notification and around admission.
- Scene replacement, cancellation and binding-slot reuse while delivery is pending.
- Shell suspension across midnight, resume once, and repeated resume without replay.
- False guard consumption and targetless ordered actions without state re-entry.
- Handler admission failure without partial effects or an automatic action retry loop.
- Shared battery/relative deadlines, normal STOP2 return and unchanged relative timers.
- Installed fixture with a visible variable/text change from a calendar handler.

Only after runtime evidence should the public source schema, package encoder,
host preview and target capability expose the same supported subset. Studio
must not infer readiness from the power-owned bench counters.
