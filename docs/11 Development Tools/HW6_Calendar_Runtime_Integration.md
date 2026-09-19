# HW6 Calendar Runtime Integration

## Status and First Scope

Implementation plan following the passed single-registration midnight wake bench
on 2026-09-19. These are delivery requirements, not an advertisement of executable
package support. No schema, binary encoding or public command is introduced here.

The first runtime increment connects a bounded calendar registration to an
existing V2 independent event handler. Start with scene-owned daily scheduling
and a targetless handler so calendar delivery can be distinguished from state
re-entry. Scene changes must invalidate the old registration. Package-session
ownership, random windows and persistent game schedules remain separate work.
In particular, a pet-wide daily reset that must survive every scene change will
need package-session ownership; a scene-owned bench does not provide that feature.

## Existing Integration Points

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
Studio command or change executable package encoding. Owner delivery integration
and public capability advertisement remain outstanding.

### Delivery Record Implementation

`ps_calendar_delivery` now implements the single-owner record lifecycle:
EMPTY -> PENDING -> CLAIMED -> APPLIED/IGNORED/FAILED. Invalidation removes
unclaimed work; claimed work retains its slot until explicit completion. Offer
cannot overwrite pending/claimed work. Notification transport does not mutate
the record, so a failed send alone cannot discard it. Every claim/completion
checks the complete identity and a monotonically increasing sequence. Sequence
exhaustion rejects new work rather than wrapping. A terminal occurrence cannot
be offered again with the same identity and an equal or earlier deadline.

This is not yet connected to either owner queue or a V2 handler. All mutation
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
