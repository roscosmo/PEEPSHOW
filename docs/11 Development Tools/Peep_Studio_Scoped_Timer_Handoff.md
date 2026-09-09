# Peep Studio Scoped Timer Handoff

Status: `backend_ready_GUI_merge_pending`

Recorded: `2026-09-09`. OS baseline: `c982593` (`feat: add scene-owned timers
and independent expiry handlers`). This is a documentation-only handoff; no
GUI files, firmware, schemas, profile values, or generated artifacts are
changed by it. The separate GUI branch has not been inspected here.

## Start Here

Implement timer authoring using the shared service that already exists. Do
not implement another scheduler, compiler, or package representation in the
GUI. The first deliverable is a real authored timer egg that behaves the
same in preview and on HW6.

Authority and detailed semantics:

- [[Peep_Studio_PeepOS_Link_Contract]]: service and branch ownership.
- [[Authoring_Project_Schema_Contract]]: executable editable records.
- [[Time_And_Power_Intent_API_Contract]]: timer lifetime and recorded HW6 pass.
- [[Runtime_Logic_State_API_Contract]]: bounded handler/action execution.
- [[Package_Blob_Format_Contract]]: compiled STG1 v5/v6 compatibility.

Use the existing GUI design and graph conventions. A scene timer belongs in
scene logic, not on a particular menu-selection state. Its expiry branch can
perform actions without moving to a different state. Do not synthesize a
self-transition just to attach or execute that branch.

## Merge Sequence And Ownership

1. Preserve/checkpoint the GUI agent's current work, then merge the OS baseline
   and this documentation into the existing GUI branch. The user performs git
   state changes; no new branch or guessed branch name is required here.
2. Review shared authoring conflicts semantically. Keep both existing GUI
   features and the timer service/schema/compiler/parser/preview behavior.
   Do not resolve entire shared files wholesale by choosing one side.
3. Restart the Python sidecar from the merged checkout. Check `service.hello`
   before enabling controls; a still-running pre-merge process can advertise
   stale capabilities even when files on disk are correct.
4. Implement and test the editor work below. Produce one small saved timer
   test project with the final agreed schema, not a GUI-only mock timer.
5. Merge GUI back into OS after save/reopen, preview, export, and target
   acceptance pass. Later capabilities should arrive as separate coherent
   increments rather than holding this merge for the whole trigger list.

| Ownership | Paths and handling |
|---|---|
| GUI | `tools/peep-studio/`: renderer, Electron/preload integration, types and GUI tests; preserve the GUI branch's current design |
| Shared semantic implementation | `tools/authoring/peepshow_authoring/{project,service,compiler,egg_format,preview,target_profile}.py`, `schemas/authoring/state-scene-v1.schema.json`, authoring tests and contracts: merge carefully, preserving timer behavior and existing GUI extensions |
| Shared target description | `tools/authoring/peepshow_authoring/target_profiles/hw6_fw0_development.json`: preserve version/hash semantics; do not promote qualification or increase limits as a GUI workaround |
| Firmware | `firmware/peepshow_hw6_fw0/`, including GDB helpers, generated embedded egg, knobs and target-profile header: no incidental GUI edits |

No TypeScript mirror is authoritative for package validation or binary layout.
If a required operation is missing, record the concrete service gap rather
than directly editing normalized project data or exposing firmware controls.

## What Can Be Enabled

| Capability | This increment |
|---|---|
| Scene-owned one-shot | `time.scene_elapsed`; default for new scene timers |
| State-entry one-shot | `time.state_entry_elapsed`; explicit state-local choice, unchanged legacy semantics |
| Scene timer start policy | `scene_entry` (default) or `action` |
| Timer controls | `start_timer`, `restart_timer`, `cancel_timer`; local scene timer references only |
| Independent expiry branch | `event_handlers`; exactly one handler per scene timer |
| Handler destination | no destination, one `target_state`, or one same-package `target_scene` |
| Preview | compiled-package timer execution, variables/framebuffer, and expiry results including SFX/system actions |
| Hardware proof | one five-second scene timer became due, dispatched, and applied once through the RTC/STOP2 test |

Do not enable these placeholders yet:

- `lifecycle.device_active`, `lifecycle.device_inactive`, `lifecycle.resume`;
- `time.calendar_match`, `sensor.steps_milestone`;
- `device.battery.soc_percent`, `device.battery.soc_valid`,
  `device.battery.soc_threshold`;
- `timeline.animation_complete`, `timeline.audio_marker`;
- `peripheral.generic` (blocked, not an open-ended sensor input);
- repeat policies, package-session timers, prefab-instance timers,
  reset-persistent timers, or arbitrary wall-clock scheduling.

The existing package interaction settings are not lifecycle event bindings.
Battery SOC is a planned read-only game input; PMIC, charger, USB, shutdown,
RTC channels, clock speed and STOP2 configuration remain OS-only.

## Discover The Backend

At the OS baseline, transport protocol is `1`, service API is `23`, target
profile is `hw6_fw0_development` version `4`, and RTOS/scene probe APIs are
`82/21`. Probe versions are for target diagnostics, not GUI feature detection.

Call `service.hello` with empty params. In the response's `result`, use:

- `operations` for available service operations;
- `state_scene_graph.scene_timers` for event type, start policies, timer
  actions, handler collection, optional targets and element-action limits;
- `state_scene_graph.event_binding_commands` and `event_handler_commands`;
- `state_scene_graph.compiled_format_version` (`6`) and
  `load_compatible_versions` (`1..6`);
- the matching entry in `target_profiles.available`, then its
  `state_scene_events.sources` for status/configuration bounds and `values`
  for read-only value availability.

Both timer sources remain `available_pending_validation`: executable for
development authoring, with incomplete target qualification. This must not
be treated as `contracted_not_exposed` or as fully hardware-qualified.
Other listed sources are not enabled merely because they have a label and
configuration schema. Use returned metadata and authoritative validation,
not a blanket service-version comparison or hardcoded placeholder list.

Current delay bounds are integer `10..86400000` milliseconds. Input actions,
state timers and scene timers share **16 compiled event-binding slots**.
Independent handlers also consume transition/guard/action budgets; firmware
admits 16 expanded transitions, 16 guards and 32 stored actions per scene.
The service's larger authoring route/state limits are not the target's
expanded runtime budget. Use validation, package build and compatibility
results; do not infer installability from the source-record count alone.

The compiler emits STG1 v4 for input-only scenes, v5 for state-entry timers,
and v6 when a scene timer is present. Editable project/scene schema version
remains v1. The GUI must not select a binary graph version or rewrite old
state timers into scene timers on load.

## Timer Behavior To Preserve

| Operation or event | Meaning |
|---|---|
| Enter a scene with `scene_entry` timer | arm once for this live scene activation |
| Enter a scene with `action` timer | leave it idle until Start/Restart |
| Start idle, cancelled or consumed timer | arm its declared duration |
| Start already armed/due timer | leave the existing deadline unchanged |
| Restart | discard the old deadline and arm a fresh declared duration |
| Cancel | disarm; already idle is harmless |
| Change selection/state in the same scene | preserve scene timers; state-entry timers follow their own state lifetime |
| Render, animate, or enter STOP2 | do not restart timers; STOP2 time counts |
| Open system menu / explicitly suspend package | pause relative countdowns; resume the remaining duration |
| Logical INACTIVE alone | not a pause while the timer owner remains active |
| Replace/destroy scene, even with the same scene ID | cancel that activation's timers; a recreated scene has new timers |
| Stop/replace package or reset | end the session, not a persisted countdown |
| Expiry guard false | consume that one-shot without retrying the handler |

State-entry timers are armed only in states listed by their ordinary route's
`from_states`. Leaving cancels them; re-entering, including an explicit
self-transition, starts a new state activation. There is no separate
`start_policy` or Start/Restart/Cancel target for that event type today.

## Editable Records And Mutations

The examples below are source/command data, not additional schema fields.
Do not add a generic `scope`, `owner`, `repeat`, or `clock_basis` field to the
current executable records. The concrete `event_type` selects state versus
scene ownership in this increment.

A scene timer is declared in `event_bindings`; its handler is declared in
`event_handlers`. IDs are scene-local. Binding IDs cannot collide with
`input_actions` IDs; handler IDs cannot collide with route IDs. Each scene
timer needs exactly one independent handler and cannot also be routed from
ordinary states. A handler has no `from_states` or `action_ref`.

Omit both target fields for action-only execution. Do not supply `null`, an
empty string, or an implicit self-transition. `target_state` and
`target_scene` are mutually exclusive.

| Handler target | Supported actions |
|---|---|
| Omitted | `set_variable`, `request_render`, `play_sfx`, `exit_to_shell`, and scene timer controls |
| Explicit `target_state` | existing bounded state-route actions, including destination-state element mutations and timer controls |
| Explicit `target_scene` | existing same-package replacement rule: `play_sfx` actions only (or no actions) |

Guards use existing variable comparisons (`variable_ref`, `operator`,
`value`); no special current-state guard field is added. Handlers use the
existing maximum of eight guards/eight actions at the source level, subject
to the tighter compiled totals. Action-only execution does not create a
state re-entry merely because data or presentation changed.

### Add A Timer In One Transaction

This is a complete `project.apply_commands` request for the repository's
`state_demo` fixture. Load the project first and replace `project_revision`
with the returned revision. The example IDs must not already exist. For a
different project, use its scene ID and preserve its complete existing wait
policy and interests while adding the timer interest.

```json
{
  "protocol_version": 1,
  "id": "add-scene-timer",
  "operation": "project.apply_commands",
  "params": {
    "project_revision": 1,
    "commands": [
      {
        "kind": "variable.add",
        "scene_id": "state_demo",
        "variable": {
          "variable_id": "timer_hits",
          "value_type": "int32",
          "initial": 0,
          "minimum": 0,
          "maximum": 100
        }
      },
      {
        "kind": "event_binding.add",
        "scene_id": "state_demo",
        "event_binding": {
          "binding_id": "choice_timeout",
          "event_type": "time.scene_elapsed",
          "configuration": { "delay_ms": 5000, "start_policy": "scene_entry" }
        }
      },
      {
        "kind": "event_handler.add",
        "scene_id": "state_demo",
        "event_handler": {
          "handler_id": "choice_timeout_expired",
          "event_ref": "choice_timeout",
          "guards": [],
          "actions": [
            { "kind": "set_variable", "variable_ref": "timer_hits", "operation": "add", "value": 1 }
          ]
        }
      },
      {
        "kind": "scene.set_reactive_wait_default",
        "scene_id": "state_demo",
        "reactive_wait_default": {
          "policy_id": "state_wait_policy",
          "waiting_visual_ref": "state_wait",
          "hold_fallback_allowed": true,
          "event_interests": [
            "move_left", "move_right", "joy_move_left", "joy_move_right",
            "open_details", "choice_timeout"
          ]
        }
      }
    ]
  }
}
```

The whole batch validates and becomes one service undo step. Adding the
binding and handler in separate requests leaves an invalid intermediate
project and is rejected. Accepted edits increment `project_revision` and
invalidate the live preview. Replace local semantic data with the returned
document, then explicitly reset preview when appropriate; do not silently
replay an edit against a newer revision.

### Update, Control And Delete

`event_binding.update` uses `scene_id` plus the complete `event_binding`
record. `event_handler.update` uses `scene_id` plus the complete
`event_handler` record, including unchanged guards/actions/target. Omitting
both targets in that replacement makes the handler action-only. There are
no handler-specific `route.action.*` or `route.guard.*` commands: edit the
complete handler record instead. Keep stable IDs during ordinary edits;
there is no binding/handler rename command to repair references implicitly.

Timer controls are ordinary actions in a route or independent handler, not
new transport operations. A restart action record is:

```json
{ "kind": "restart_timer", "timer_ref": "choice_timeout" }
```

Replace `kind` with `start_timer` or `cancel_timer` for the other operations.
The referenced binding must be a scene timer in the same scene. Input routes
still need their normal explicit destination; adding a timer action does
not make an input route an independent handler.

For deletion, submit one batch in this order: remove all referring timer
actions from routes/handlers, delete the timer's handler with
`event_handler.delete` (`scene_id`, `handler_id`), remove its wait interest
using the complete wait-policy command, then `event_binding.delete`
(`scene_id`, `binding_id`). Preserve unrelated actions and at least one valid
wait interest. Removing a referenced binding reports `COMMAND_TARGET_IN_USE`.
The service allows at most 64 commands per batch.

Surface validation codes and field paths instead of retrying with weaker
rules. Relevant errors include `EVENT_HANDLER_REQUIRED`,
`EVENT_HANDLER_SCOPE_INVALID`, `ACTION_TIMER_UNKNOWN`,
`EVENT_TIMER_START_INVALID`, `EVENT_TIMER_DELAY_INVALID`,
`EVENT_BINDING_LIMIT_EXCEEDED`, and `EVENT_TYPE_UNAVAILABLE`.

## Preview Integration

1. Use `project.preview_reset` with `project_revision` and `scene_id` to
   create a live scene preview. Retain its `preview_revision`.
2. Send normal `project.preview_input` calls for selection changes. Do not
   reset/recreate the preview on each selection or rerender; that would
   restart scene timers artificially.
3. Drive time using `project.preview_advance` with `project_revision`,
   `preview_revision` and integer `elapsed_ms` (`0..600000` per request).
   Serialize time/input requests. Each advance is additive, not an absolute
   timestamp; retransmitting it advances time again.
4. Apply the returned snapshot's variables, scene and framebuffer. Consume
   every item in the new top-level `result.timer_events` exactly once.
5. Route timer audio through the existing cue audition/playback path and
   handle any returned `system_action`. Do not inspect only `result.input`,
   drop timer-triggered SFX, or replay previous timer results on every repaint.

Each `timer_events` item has `logical_source` (`time.scene_elapsed` or
`time.state_entry_elapsed`), `event_kind` (`elapsed`), `action_id` (binding
ID), `accepted`, `route_id` (handler ID for a matched independent handler),
`audio_events`, and singular `system_action`. A false guard produces an
unaccepted result and still consumes the one-shot. An empty array means this
advance delivered no timer expiry; a changing framebuffer alone is not
evidence that a handler ran.

`project.preview_state` and `project.scene_thumbnails` are side-effect-free
placement/inspection paths, not replacements for the live preview session.
Service edits/undo/redo invalidate live preview; handle the new revision and
restart intentionally. Saving alone does not reset it.

The Python preview has `suspend()`/`resume()` methods tested directly, but
there are **no `project.preview_suspend` or `project.preview_resume` service
operations** at this baseline. Do not invent them in the renderer. Pausing
the preview clock means sending no elapsed-time advances; it is not a full
simulation of OS suspension/lifecycle. Request a typed service extension if
an interactive system-menu simulation becomes necessary. No live timer
countdown/slot-inspection response is currently advertised either.

## Evidence And Acceptance

The production C scheduler/runtime harnesses and shared compiler/parser/
preview tests passed at the OS baseline: 115 authoring tests, including the
two native C harnesses. Debug firmware built successfully. The exact target
transcript and caveats are in [[Time_And_Power_Intent_API_Contract]], under
"Recorded scene-timer target pass".

The hardware report recorded `due/dispatch/applied = 1/1/1`, zero
ignored/errors, `configured/active = 1/0`, timer RTC selection and elapsed
accounting, and six STOP2 entries. It proves handler application, not merely
thread scheduling. It did not include the injected private variable's value,
a measured current trace, or a GUI-exported package. No whole-profile
qualification is implied.

Documentation verification on `2026-09-09` reran all 115 shared tests and
`gen_target_profile.py --check` successfully. The JSON command example above
was executed against the actual service, compiled/reparsed as STG1 v6, and
checked for expiry across a selection change, one-shot consumption,
undo/redo, and reference-safe deletion. This used in-memory project edits;
the fixture on disk was unchanged.

The GUI agent's completion checklist:

- Create a scene-entry timer and independent handler without a source-state
  edge. Save/reopen and undo/redo preserve the declaration and references.
- In live preview, change menu selections before expiry; the original
  deadline remains and the handler runs once. Further time does not refire it.
- Author an action-started timer. Start while idle arms it; Start while
  active preserves the deadline; Restart moves it; Cancel prevents expiry.
- A false guard consumes the expiry. A targetless handler updates a variable
  without state re-entry; a targeted handler changes the intended state.
- Leaving/recreating the scene discards its old countdown. An explicit
  state-entry timer still cancels on departure and rearms on re-entry.
- Timer-triggered audio/system results are consumed once; unknown/unsupported
  triggers and invalid references are rejected, not silently exported.
- Export and reparse STG1 v6 through the shared backend; old input-only and
  state-timer projects remain compatible. No new firmware build flags or
  asset/package budget increases are needed for the small timer test.
- On HW6, install the actual GUI-exported egg through the existing package
  pipeline. Use a visible state/presentation change as the expiry action,
  vary selections before expiry, allow STOP2, observe one action, and verify
  return to STOP2. Do not use GDB injection as proof of GUI export.
- Complete targeted hardware checks for Start/Restart/Cancel, explicit
  system-menu suspension/resume, scene replacement, and state-timer expiry.
  Keep host-only checks labelled until they have target evidence.

For target inspection, pause only after audio is silent and use the existing
read-only helper:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_state_scene_timer_prints.gdb
```

For the older RAM injection test, a STOP2 disconnect/new GDB session can lose
`__fw0_scene_timer_test_prints.gdb` bookkeeping. That is not a failed timer.
Reattach without reset and read firmware counters before rearming. Do not
change debug-in-low-power settings merely to make a result helper work;
debug/power measurement conditions must remain explicit. A reset or package
reload invalidates the RAM injection and its evidence.

Run these short shared checks from the repository root after merging:

```powershell
tools/.venv/Scripts/python.exe -m unittest discover -s tools/authoring/tests
tools/.venv/Scripts/python.exe tools/authoring/gen_target_profile.py --check
```

The native harness runner requires a host C compiler, not just the ARM
cross-compiler; any reported skips are not passes. Run the GUI branch's own
type, build and UI tests as well. The user's experiment project is
`workbench/peep-studio/test_bed.peepproj`; preserve it. Add a deliberate small
fixture/test case rather than replacing personal experiments or silently
changing the built-in example.

## Follow-On OS Work

After this merge, the proposed next capability is read-only battery SOC and
validity, then threshold events; lifecycle event bindings, package-session
steps/milestones, calendar scheduling, and completion/expanded timer scopes
follow as separate increments. These are priorities, not newly exposed APIs.
The GUI agent can prepare unavailable surfaces but must not encode these
events until service/schema/compiler/preview/firmware support agrees.
