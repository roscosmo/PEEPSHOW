# Peep Studio Time Node Design Handoff

Status: `approved_countdown_design_backend_extension_required`

Recorded: `2026-09-12`. This records the user's approved interpretation of the
TIME NODE and TIME NODE EXAMPLE in [[Peep Studio Design]]. It does not advertise
new service, preview, export or firmware capabilities.

## Visual Reference

The live Excalidraw drawing remains the visual authority. The countdown node has
the grey card, green corner entries, Xy/Obj indicators, timer name, COUNTDOWN
label, prominent time display and condition rows shown there. Preserve the
connecting line from the clock through successive rows: it represents sequential
evaluation, not independent or competing event triggers.

The clock has a default exit; each condition row has its own exit. Exit actions
remain ordered symbols along transition lines, with details in the inspector.
The example's numbered diamonds identify the illustrated action sequences;
they do not authorize replacing the existing individual action-symbol language.
Exact labels for adding condition rows can be reconciled with the drawing before
implementation; the drawn ADD NEW TRIGGER does not mean a second expiry event.

## Approved Countdown Behavior

- Expiry begins at the clock and evaluates the condition rows in visual order.
- A failed first check selects the clock's default exit.
- A passed check advances to the next row. If that next check fails, select the
  preceding successfully passed row's exit.
- Passing every check selects the last row's exit. With no rows, select the
  clock's default exit.
- Stop checking at the first failure. Exactly one exit is selected, and only
  that exit's ordered actions execute. Passing an intermediate row does not
  execute its actions. This is not first-match selection or parallel branching.
- Entering any countdown corner starts the full configured countdown again,
  including when the timer is already running. A return connection therefore
  means restart, not local-state re-entry.
- The editor displays the configured duration. During emulation, display the
  remaining time supplied by the shared preview, without a private scheduler.
- Timers remain one-shot. A loop returning to the timer explicitly restarts it;
  there is no implicit periodic capability.

For two checks C1 and C2:

| Evaluation | Selected exit |
| --- | --- |
| C1 fails; C2 is not evaluated | Clock default |
| C1 passes, C2 fails | C1 |
| C1 and C2 pass | C2 |

The example checks step count **at least 500** after 30 minutes. Below 500, the
default path resets the step count. At or above 500, the conditional path adds
one to Stamina and then resets the step count. Both drawn return paths restart
the timer. Reaching 500 before expiry does not itself fire this countdown.
Step-count access/reset remains dependent on supported OS operations; this
example is design intent, not a currently executable hardware fixture.

## Shared Backend Request

API 43 already supports scene timer binding/handler pairs, guards, ordered
actions, explicit timer start/restart/cancel and optional destinations. It
requires exactly one handler per scene timer. A guarded handler alone does not
represent the approved sequence of checks and fallback exits.

OS owns the shared representation and implementation. Please supply supported
increments for this concrete workflow:

1. Create a scene countdown node and persist its position and connection layout
   using a stable shared timer identity, not a generated state or GUI-only node.
2. Author/reorder/delete sequential checks and their associated exits, including
   a default exit, while preserving exit actions and references explicitly.
3. Execute one selected exit according to the table above in shared preview and
   runtime. Preserve existing action ordering and destination restrictions.
4. Connect a permitted path to a timer entry to restart it. Define the shared
   connection/action encoding; Studio must not invent timer-target semantics.
5. Expose enough preview status for remaining time and selected expiry path,
   plus advertised commands and validation/build issues for supported subsets.

This request does not require multiple independent handlers for one timer or a
parallel-region implementation. OS should choose the compatible bounded encoding
and settle limits, invalid-reference handling and failed-action behavior. Existing
input routes, single-handler timers and legacy authoring must remain intact.
Current V2 scene exits require empty action lists; a new timer visual must not
silently permit outgoing mutation/restart actions on a cross-scene exit.

## Deferred Time Modes

The node is intended to support more than countdown eventually. No time-of-day,
date/time, absolute scheduling or additional mode controls are approved for
implementation now. Clock-setting support is not yet available for this workflow.
The user identified **current time + 24 hours** as a useful future direction,
not a settled specification. Clock source, sleep, persistence, clock corrections
and re-entry behavior remain for later OS discussion. Do not infer that entering
a future mode changes the device clock, or define its arming policy now.

## Bring-Up Order and Checks

1. Reconcile this request with OS and its capability delivery; no backend edits
   or TypeScript ownership/execution workaround in Studio.
2. Implement the supported countdown-node slice against the drawing, retaining
   current timer inspector behavior where the new commands are unavailable.
3. Add sequential rows and timer-return wiring only when their shared semantics
   are advertised. Keep export readiness independent of editor/preview support.
4. Verify no-row default, first failure, later failure, all-pass and skipped
   later checks. Verify only one action sequence executes, in authored order.
5. Verify restart while active, explicit return restart, scene-timer continuity
   through local state changes, remaining-time display, layout, references,
   undo/redo and save/reload. Hardware evidence requires a later OS-coordinated
   fixture; existing passed fixtures stay unchanged.

No application, schema, compiler, firmware or fixture changes accompany this
document. Existing export restrictions and backend build issues remain intact.
