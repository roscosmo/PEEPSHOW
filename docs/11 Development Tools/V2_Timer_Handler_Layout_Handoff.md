# API 46 Timer-Handler Layout Handoff

Studio can integrate timer-handler path layout now. No firmware update, target
profile change, new timer compiler or export workaround is needed.
Restart the shared authoring service against this backend and confirm API 46
before using the new command.

## Discovery

- `service.hello.service_api_version`: 46.
- `service.hello.state_scene_graph.scene_timers.editor_layout`.
- `scene_capabilities[scene_id].timer_handler_layout` from project operations.
- `editor.state_graph.set_handler_layout` is advertised in the editor command
  catalog and V2 supported/local-graph command lists.

The capability reports `supported`, `editor_only`, `routing_version`, command,
addressing/storage, rail/token limits, endpoint support, replacement semantics
and null clearing. Do not enable later timer execution modes from this flag.

## Command

Send through the existing `project.apply_commands` operation with the current
`project_revision`:

```json
{
  "kind": "editor.state_graph.set_handler_layout",
  "scene_id": "garden",
  "handler_id": "reveal_handler",
  "layout": {
    "termination": {"x": 540, "y": 280},
    "rails": [{"axis": "x", "value": 360}, {"axis": "y", "value": 280}],
    "token_positions": {"condition": 0.3, "actions": [0.55, 0.75]}
  }
}
```

Scene and handler IDs above are illustrative: use the project's actual stable
IDs. The handler must already exist and reference `time.scene_elapsed`.
Address the handler directly, not a route ID or fabricated source state.

The returned/applied layout and saved project record include `routing_version: 1`.
Storage is `project.editor.state_graph.scenes[scene_id].handlers[handler_id]`.
The command replaces the record, so include any existing fields that should
remain. Use `layout: null` to clear it. An empty rail list alone retains other
fields supplied in the same layout.

`termination` is only for targetless handlers. For a local-state destination,
omit it and optionally supply a paired socket such as
`target_handle: "entry-bottom-right", target_side: "right"`. Destination state
and scene nodes retain their existing placement commands. A cross-scene handler
may have rails/tokens, but neither a termination nor local-state socket.

Use `token_positions.guards: [0.2, 0.4]` instead of `condition` when rendering
individual guard chips. Do not supply both. Guards precede action positions;
fractions must strictly increase within 0.02..0.98. Each array is bounded to
eight entries. These are placements, not guard branches or action ordering
commands. Positions use semantic list order, not new token IDs.

## Persistence and Cleanup

Normal revision checks, atomic batches, undo/redo and save/reload apply. Handler
deletion removes its layout; undo restores both. Full `event_handler.update`
preserves rails, clears endpoint geometry when the destination changes, and
clears token positions when guards/actions change. Studio should read the
returned document rather than assuming old geometry survived a semantic edit.

Project loading validates saved geometry and handler references. Invalid
commands fail without changing the project or revision. No runtime records,
timer deadlines, transition ordering or exported egg bytes change from layout
commands. Existing action-free V2 scene-exit and other export restrictions stay
in force.

## Verification

187 focused host tests passed across timer layouts, timers, authoring service,
V2 objects, creation/connections/local graphs and audio/export profiles. New
tests cover capabilities, targetless and targeted paths, normalization, malformed
and dangling layouts, atomic rejection, deletion/update cleanup, undo/redo,
save/reload, V1 compatibility, unchanged timer preview and byte-identical V2
exports through both compiler and public build operation.

Items 2-7 in the Studio request remain separate work. This increment does not
admit calendar timers, parallel execution, scene memory, runtime text, project
settings, names/tags or new exit actions.
