# Studio Scoped Variables API 52 Handoff

API 53 follow-up: [[Peep_Studio_Scene_Entry_API53_Handoff]] adds host entry graphs
and changes entry_graphs to true. The API 52 description below is historical;
firmware/export for scoped_v1 remain blocked.

Status: shared source editing and host preview implemented. Firmware execution,
egg export and scene-entry graphs are NOT enabled for this model. This implements
the variable foundation of [[Variable_Scope_and_Scene_Entry_Design]], not an
interim remembered-selection mode.

## Capability Boundary

Read `service.hello.scoped_variables` and the matching V2 per-scene capability.
It advertises `host_editing=true`, `host_preview=true`, `runtime=false`,
`export=false`, `entry_graphs=false`, types, operations, commands and limits.
The current host projection admits at most 32 combined scene/package variables
in each scene; this is not a firmware RAM-budget advertisement.

Existing projects retain their old semantics and export readiness. Opting in
blocks normal and development egg export with
`SCOPED_VARIABLE_RUNTIME_UNAVAILABLE`, even when declarations are empty. Do not
remove the model marker to bypass that block. Export profile revision remains 6;
no package encoding changed. Calendar and SFX fixtures need not opt in.

## Persisted Records and Commands

`project.variables.enable` explicitly sets project `variable_model` to
`scoped_v1` and creates `package_variables` if absent. It has no fields besides
`kind` and optional `command_id`. It participates in normal undo/redo; it does
not silently opt in existing documents. Shared validation requires V2 scenes.

Package declarations live in project `package_variables`; scene declarations
remain in each scene's `variables`. Both use these forms:

```json
{"variable_id":"selection","value_type":"int32","initial":0,"minimum":0,"maximum":3}
```

```json
{"variable_id":"visited","value_type":"bool","initial":false}
```

Integers must fit signed int32 and satisfy minimum <= initial <= maximum.
Booleans use actual JSON booleans, without minimum/maximum. IDs are stable and
unique within scope; the same ID in scene and package scope is permitted.

Commands in `project.apply_commands`:
- `package_variable.add` / `package_variable.update`: `variable` declaration.
- `package_variable.delete`: `variable_id`; rejects referenced values.
- Existing `variable.add/update/delete`: `scene_id` and existing payload fields.
- Existing `object_actions.set`: ordered V2 route/handler actions.
- Existing guard and independent-handler commands accept scoped references.

Package-variable commands have no `scene_id`. Variable update changes the
definition with the same stable ID; no rename-by-replacement or implicit scope
transfer is provided. Normal atomic batch validation, save/reopen and undo/redo
apply. A change that invalidates an existing reference or typed operand fails.

## References and Operations

Guards and `set_variable` actions have optional `variable_scope`, either `scene`
or `package`; omission means scene only, never fallback to package lookup.

```json
{"kind":"set_variable","variable_scope":"package","variable_ref":"selection","operation":"add","value":1}
```

```json
{"kind":"set_variable","variable_scope":"package","variable_ref":"selection","operation":"reset"}
```

`assign` takes a typed literal; `add` takes a signed integer delta (negative for
decrement); `reset` has no `value` and restores the declared initial value.
Arithmetic clamps to bounds. Assignment literals outside declared bounds fail
validation. Boolean actions support assign/reset only, and boolean guards support
eq/ne with a boolean operand. No implicit bool/integer coercion or expressions.

The existing SFX-only scene-exit restriction remains: this increment does NOT
allow variable mutations on scene exits. Those require destination-entry atomic
execution support. There are no state-entry actions or entry decisions yet.

## Host Preview Lifetime

- Local state changes preserve scene/package values.
- Fresh scene replacement resets scene values, preserving package values.
- Preview suspend/resume preserves both scopes; world time continues under the
  existing calendar rules, while relative timers pause.
- `project.preview_reset` starts a fresh preview session and resets both scopes.
- Snapshot `variables` contains scene values; `package_variables` contains package
  values. `scoped_variables` additionally lists scope, stable variable ID and
  typed value. Internal preview aliases are not author-facing IDs.

The host compiler uses a private graph projection for execution of this model;
it is not an egg encoder or a compatibility workaround for current firmware.

## Verification and Next Work

Public service tests cover save/reopen, both lifetimes, duplicate IDs in distinct
scopes, boolean typing, clamping, reset, scoped guards, reference-safe deletion,
undo/redo, combined limits and export rejection. Existing scene, audio, calendar
and service regressions remain required. No hardware claim is made here.

Studio can now integrate variable editing and preview, clearly indicating the
export restriction. Next work is the explicit entry graph and coordinated
firmware/package support; no remembered-state control should be introduced.
