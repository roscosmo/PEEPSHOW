# Studio Scene Entry API 53 Handoff

Status: explicit fresh-entry graphs are implemented for shared source editing
and host preview. Firmware and egg export remain unsupported for scoped_v1.
This extends [[Peep_Studio_Scoped_Variables_API52_Handoff]] and implements the
host portion of [[Variable_Scope_and_Scene_Entry_Design]].

## Capability and Commands

Read `service.hello.entry_graph` and the matching per-scene `entry_graph`
capability. It advertises host editing/preview, runtime=false, export=false,
the required `scoped_v1` opt-in, supported nodes/actions and bounds.
`scoped_variables.entry_graphs=true` means host support, not firmware support.
Existing projects without entry graphs retain their direct `entry_state`.

Use normal `project.apply_commands`:

```json
{"kind":"scene.entry_graph.set","scene_id":"main","graph":{"root":"choose","nodes":[{"node_id":"choose","kind":"decision","branches":[{"guards":[{"variable_scope":"package","variable_ref":"selection","operator":"eq","value":1}],"next":"right"}],"default":"left"},{"node_id":"left","kind":"state","state_ref":"start"},{"node_id":"right","kind":"state","state_ref":"selected"}],"layout":{"choose":{"x":20,"y":40}}}}
```

The example assumes package integer `selection` and states `start`/`selected`
already exist. Set replaces the complete graph atomically. Optional command_id
is supported. Clear uses `scene.entry_graph.clear` with scene_id and restores
direct entry_state behavior. Both participate in undo/redo and save/reopen.
Studio may keep an incomplete editing draft locally, but shared source records
must pass complete-graph validation. Do not compile a substitute entry route.

The source scene's optional `entry_graph` holds the graph exactly. Stable
node_id values address edges and optional layout positions; moving a node does
not change execution. Branch array order, not layout, controls selection.
Schema: `schemas/authoring/scene-entry-v1.schema.json`.

## Nodes and Execution

- `actions`: node_id, kind, ordered actions, next. Actions are only scoped
  set_variable (assign/add/reset) or play_sfx. Empty action lists are permitted.
- `decision`: node_id, kind, ordered branches, default. Each branch has a
  nonempty guards array and next; all guards must pass. First matching branch
  wins, otherwise the mandatory default is taken.
- `state`: node_id, kind, state_ref. Terminates at one existing local state.

Actions can precede decisions, follow branches, and merge through shared nodes.
Conditions see preceding writes. Only the selected path executes. No state-entry
action lists, waits, loops, implicit remembered state, scene exits, object/timer
entry actions or parallel execution are supported.

Limits: 32 nodes; 8 actions per action node; 8 branches per decision; 8 guards
per branch. A path visits at most 32 nodes and has an advertised conservative
maximum of 256 actions. All nodes must be reachable; every edge must resolve;
the graph must be acyclic and end at a state. Layout x/y are integers within
-100000..100000. These are host bounds, not firmware RAM-budget claims.

Scene variables initialise fresh. Package variables retain session values.
Entry executes before selecting the destination's normal state overrides and
initialising its waiting/timer presentation. It runs only on fresh scene entry,
not local transitions or shell resume. Both variable scopes retain values during
pause; reset starts a fresh session. Calendar time retains its existing behavior.

Destination variables, state and effects are prepared privately. Failed preview
admission/rendering leaves the source scene and committed package values intact
and publishes no exit/entry cues. Successful replacement publishes exit cues,
then selected-path entry cues in order. Multiple cues do not imply sequential
audio playback. Scene exits themselves remain SFX-only.

## Preview Results

`project.preview_reset` returns `entry_events` for its newly admitted entry SFX.
Consume these once. Transition-driven scene entries append their cues to the
existing input/timer audio_events list, after outgoing cues. Ordinary snapshots,
advance, suspend and resume do not republish entry_events.

Snapshots include `entry_trace`, the last fresh entry's visited node IDs. This
is diagnostic history, not an effect queue; do not replay it. Explicit selected
state preview and scene-base placement preview bypass entry execution. Existing
scene/package value snapshots remain available.

## Export and Next Fixture

Normal and development export remain blocked by
`SCOPED_VARIABLE_RUNTIME_UNAVAILABLE`. Target profile/export encoding are
unchanged. Existing calendar, text and audio fixtures do not need to opt in.

GUI can now build the two-scene host fixture: local selection transitions write
an explicit package variable; menu fresh entry branches on that value with a
visible fallback. Include an action before the check, branch/shared actions,
and a scene-local value that resets on fresh entry. Verify pause/resume does not
rerun entry, while preview reset restores declaration initials. This is not
full scene resume and must remain clearly export-blocked until firmware delivery.
