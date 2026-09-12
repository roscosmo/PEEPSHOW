# Studio V2 Scene Connection Reconciliation

GUI handoff for OS, 2026-09-12. This records current behavior and open decisions;
it does not extend the shared runtime contract or authorize multi-scene export.

Reviewed GUI baseline: `0be39ccb6f44ae32e97e39e9cb8df9cb1d0dcd1d`
(`Move animation authoring into Assets`). This handoff is a subsequent docs change.

## 1. Design

See [UX direction](Peep_Studio_UX_Direction.md), Scene-flow mode and the planned
hierarchical-state/history section. Scene Flow is a left-to-right storyboard;
Local Logic is the scene's state machine.

- A scene owns named exit records. Each appears as an output on its Scene Flow
  card and as a scene-exit endpoint in Local Logic: one link, two views.
- Creating an exit must not require an input or invent a button route. Local
  state routes/handlers reach it through their own triggers, guards and actions.
  A menu's A button can therefore reach a different exit from each selected state.
- Package Entry has one output connected to the starting scene. A scene's local
  entry node points to its default state.
- Go To cards are editor-only references to existing scenes, not new instances,
  entries, call/return operations or history commands. Deleting a visual alias
  retains the real connection. Layout and socket-side choice have no runtime meaning.
- Exits/entries are semantic graph links, not scene-owned drawable objects and
  not object-hierarchy children. No default exit-to-shell route is generated.

## 2. Commands and Blockers

Available for native V2: `project.create` / `scene.add` with
`scene_schema_version: 2`, `scene.rename`, `project.set_entry_scene`,
`state.create`, `state.add`, `state.delete`, `state.rename`, `state.set_entry`,
state-node/entry layout, local input/event/timer routes, guards and variables.
`object_actions.set` edits ordered actions on existing routes/handlers.

Existing legacy command paths to integrate, not a new GUI model:

- `scene_exit.add`: `scene_id`, `target_scene`; optional `display_name` and
  `scene_flow_reference_id`. Returns the generated `scene_exit` record.
- `scene_exit.set_target`: `scene_id`, `scene_exit_id`, `target_scene`.
- `scene_exit.delete`; route creation/retargeting with `scene_exit_ref` and
  `target_scene`. Legacy retargeting synchronizes referring routes.
- `editor.scene_flow.set_node_position`, `set_route_layout`,
  `set_package_entry_position`, `add_reference`, `set_reference_position`,
  `set_reference_target`, `set_exit_reference`, `delete_reference` (all names
  carry the `editor.scene_flow.` prefix).

These exit/editor commands remain blocked for V2. Local graph commands reject
`target_scene` or `scene_exit_ref` with `SCENE_OBJECT_CONNECTION_UNAVAILABLE`.
The handoff still advertises `scene_connection_commands: false`. Existing linked
source loading/preview is not permission to expose V2 construction commands.

Concrete blocked workflow: create two native scenes, add a Menu exit, wire a
state's A route or scene-timer handler into it, connect it to Settings (directly
or through a Go To card), retarget/delete it, then undo and save/reopen.
OS needs to own reference validation and atomic cleanup for routes AND handlers,
not merely lift the command guard. Advertise the supported commands and layout
subset through hello/per-scene capabilities; GUI will consume that increment.

Evidence: `tools/authoring/peepshow_authoring/scene_object_authoring.py`
(`COMMON_SCENE_COMMANDS`, `check_command_model`), `project.py`
(`_apply_scene_exit_add`, `_apply_scene_exit_set_target`), and Studio `App.tsx`
(`addSceneExit`, `setSceneExitTarget`, scene-flow reference handlers).

## 3. Destination and Entry Selection

Current exit record: `scene_exit_id`, `display_name`, `target_scene`. Its name
labels the source exit; it is NOT a named entry in the destination.
The connection selects a scene, and that scene's single `entry_state` chooses
the initial state. `state.set_entry` changes that scene-wide default, not one
connection's destination state. Package Entry similarly selects only a scene.

No per-connection entry-state override or multiple named-entry table/command is
currently exposed. State-card corner sockets are alternate drawing positions
for the same state, not named entries. Go To aliases also select only a scene.
Legacy exit creation rejects a same-scene destination and requires a destination
immediately; unconnected draft exits need an explicit supported model if desired.

If named entries are included, OS should define stable entry references and their
state mapping, default-entry fallback, retarget/delete validation, and payloads.
GUI must not infer entry identity from port geometry or change `entry_state` to
simulate a per-connection entry. Single default entry is a possible first subset,
subject to agreement, not a decision made by this handoff.

## 4. Lifetime and Re-entry

The [scene-object lifetime contract](../03%20Engine%20API/Scene_Object_Lifetime_and_Control_Contract.md)
guarantees continuity within a scene instance, not automatic retention after it
is destroyed. Within the same instance, local state changes preserve underlying
object properties and animation phase; temporary overrides change per property.
State timers restart on state entry; scene timers survive local state changes.

Observed host replacement in `preview.py`, `_activate_scene`:

| Runtime data | Current fresh scene activation/re-entry |
| --- | --- |
| Objects | Initialized again from authored defaults; previous mutable instance discarded |
| State | Destination's declared entry state; its overrides applied |
| Animation | New object playback, not remembered outgoing phase/residual time |
| Scene variables | Reset to declared initial values |
| Scene timers | Scene clock/deadlines rebuilt; scene-entry timers armed again |
| State timers | State elapsed/fired tracking reset for the initial state |
| Assets/clip definitions | Reusable authored data; not mutable retained scene state |

This is source-code evidence for host behavior, not a V2 multi-scene hardware pass.
No GUI snapshot cache provides scene resume. No scene-variable value transfer or
package/global variable persistence is assumed by this connection UI.

User-required future direction remains explicit: remember menu selection when
returning, and distinguish Go to (replace), Go to and resume, Open (retain current
context), and Return. These need typed backend semantics. Remembering a substate
alone does not establish preservation of object changes, variables, animation or
timer residuals. Suspended-time advancement, retained-instance limits, nesting,
history fallback, and cold-boot persistence remain separate decisions. Package
launch is fresh unless a later resume contract says otherwise.

## Reconcile Before Implementation

1. Is the first subset fresh replacement with one default entry, deferring named
   entries and resume? Keep future remembered-menu behavior explicitly deferred.
2. Which exit-transition actions are legal, and on which scene instance do they
   run? Current host `_dispatch_route` replacement logic only accepts play_sfx;
   object/variable actions cannot be assumed to work on scene exits. New V2
   support must define action order, source/destination scope and rejection.
3. Define cleanup for outgoing timers/events and held-input delivery across a
   transition; no stale callback should act on a replaced instance.
4. Confirm draft/unconnected exit handling and atomic rename/retarget/delete
   behavior for shared exits, entry references, routes, handlers and Go To aliases.

Keep ordinary multi-scene export blocked until the backend advertises a supported
profile and whole-project `export_ready`. Preserve `build_issues`, draft editing
and supported preview. GUI does not calculate resource budgets or bypass compiler
checks. No fixture or firmware change is made here; agree a two-scene source and
hardware acceptance criteria after reconciliation.

## OS Response: Proposed First Increment

OS agrees with the shared named-exit model and proposes:

- Fresh replacement into the destination scene's single default entry state.
- Reference-safe integration for both routes and event handlers, with explicit
  capability advertisement before GUI controls are enabled.
- Reject outgoing object, variable and timer mutations for this initial subset;
  never accept them and silently discard their effects on scene replacement.
  This does not establish support for other action kinds: those still require
  explicit backend admission.
- Scene resume and remembered menu selection remain planned, separate work.
- Multi-scene export stays disabled until backend and firmware support are
  delivered and tested. No new fixture is requested yet.

This records OS's proposal, not an implemented command/export capability.
GUI continues using the existing advertised command lists, export readiness and
build issues; it makes no backend or firmware workaround for the pending subset.
