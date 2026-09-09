# Scene Object Executable Design

Status: GUI representation review accepted; source loading, migration/editing
service and host scene-object preview implemented in service API 39. Executable
encoding, firmware execution and autonomous display integration remain pending.

Authority: [[Scene_Object_Lifetime_and_Control_Contract]]. Tests:
[[Scene_Object_Ownership_Acceptance_Plan]]. Coordination:
[[Peep_Studio_Scene_Object_Ownership_Handoff]]. The agreed lifetime semantics
remain authoritative. The source/service subset is documented in the handoff;
executable layouts below are still proposals, not allocated wire IDs.

Baseline: main `34c76bba4bcf59ab03d8668dca329f2338caf33f`, incorporating the
reviewed shared backend from GUI `57f7030cc65c09b434223e405c12b67e211b99a0`.
That baseline used service API 38. The 148-test baseline integration pass does
not test this model; subsequent host results are recorded in the handoff.

## GUI Review Clarifications

The GUI review found no architectural blocker. The following resolves its four
integration questions before the new service contract is frozen:

1. `defaults.visual_ref` is the object's authored static fallback, not a mask.
   When `animation_ref` is present, the animation supplies the displayed frame
   immediately. Persistent static selection is a separate nullable live property,
   initially unset. `object.set_frame` sets it; `object.clear_frame` clears it.
   A state's `visual_ref` override masks both. Removing that state override
   reveals the persistent selection if set, otherwise the current animation
   phase, otherwise the authored static fallback. Neither mask changes clip time.
2. Absolute placement remains top-left-origin pixels: positive X right, positive
   Y down. `object.set_position` uses those existing coordinates. Relative
   `object.move_by` uses `dx` positive right and `dy` positive up; apply
   `x += dx`, `y -= dy` to underlying coordinates, not displayed overrides.
   After each ordered position action, clamp X to `[0, canvas_width-width]` and
   Y to `[0, canvas_height-height]`. Subsequent actions read that clamped result.
   Use overflow-safe intermediate arithmetic. Defaults/overrides outside the
   canvas are authoring errors; do not silently clamp source placement edits.
3. Version-2 overrides and clearing address `x` and `y` independently. Omitting
   one axis preserves it. Remove a sparse override record only when it controls
   no properties. Legacy `state_placement.clear_override` with `position` retains
   its legacy paired behavior; it is not an implicit version-2 command adapter.
4. Each project document response declares per-scene execution model, source version and
   supported commands. Derived legacy-shaped views are read-only projections,
   not writable version-1 source. The backend checks the scene model as well as
   the project revision before applying commands, including every command in a
   mixed-scene transaction. A model mismatch rejects the transaction unchanged.
   Studio enables version-2 editing from delivered per-model capabilities, not
   from a higher service API number alone.

Authored object clip binding is needed in increment 1 and writes `animation_ref`
without creating per-state waiting records. This is distinct from deferred
runtime clip-assignment/restart actions. Preserve draft save/preview, build
issues, undo/redo and reference-safe asset edits during service integration.
Asset deletion/replacement must account for object defaults, static overrides,
frame actions and clip references, not only legacy render elements.

## Development Boundary

`tools/authoring/peepshow_authoring/scene_objects.py` implements object/state
fragment validation, pure object-action staging/resolution, independent-axis
clearing, and a non-writing migration plan/materialization API. Its schema is
`schemas/authoring/scene-object-model-v2.schema.json`, a definitions/placement
fragment. `state-scene-v2.schema.json` now describes the full source envelope.
Project loading validates the object model and shared graph rules. Service API
39 connects migration, transactional editing, save/reload and host preview,
including per-object phase/residual diagnostics. Target admission, binary
encoding and firmware integration remain pending.

The migration API consumes a validated legacy `ProjectBundle`, preserves IDs,
and returns an in-memory scene candidate plus new immutable catalog clip records.
It binds the preview to the normalized project's content hash and recomputes
materialization, rather than trusting modified plan contents. It writes no
project/catalog files. Static placement conversion needs no animation choice;
animated conversion explicitly requires acceptance of scene-continuous playback
starting at sequence step zero. Different state tracks and legacy destination
mutations remain blocking issues instead of being guessed or discarded.

Use `project.object_migration_preview` and `project.object_migration_apply` for
the connected workflow: converted scene and catalog clips form one undoable
in-memory transaction, then normal `project.save` persists them. Saving retains
the existing per-file write behavior, not a new whole-project disk transaction.
Opening/saving legacy source does not migrate it. Projects containing version 2
remain editable drafts and report `SCENE_OBJECT_EXECUTABLE_UNAVAILABLE` through
`build_issues`; no export path may encode them as legacy eggs.

Host preview reuses the graph/timer compiler and rasterizer through an internal
legacy-shaped graph projection, restores symbolic object actions, and maintains
live scene objects independently of that projection. It is explicitly labeled
`host_scene_objects_not_firmware`. This tests host semantics, not a future binary
decoder or STOP2/LPBAM execution. No new numeric wire IDs are allocated here.

## First Executable Increment

Implement one scene-owned object table, sparse state overrides, persistent
position/visibility/static-frame actions, and continuous object animation.
Keep existing state transitions, scene handlers, timers, primitives, and audio
behavior. A scene handler can change an object without selecting a new state.

Do not implement groups, prefab expansion, simultaneous hierarchical controllers,
temporary animated override tracks, or clip restart/replacement/pause commands
in this increment. They must not appear as available capabilities. Sharing an
asset is already allowed; sharing an object instance between scenes is not.

## Source Representation

Use `schema_id: peepshow.authoring.state_scene` with `schema_version: 2`.
Keep project and asset catalog versions unchanged unless their own structures
need changes. Scene version selects semantics explicitly; opening or saving
a version-1 project must not upgrade it implicitly.

Version 2 replaces per-state render models and waiting presentations with:

| Field | Meaning |
|---|---|
| Scene `objects` | One definition per placed object, including hidden objects |
| Object `object_id` | Stable scene-local authoring ID; preserve the old element ID during migration |
| Object `kind`, layer, geometry and primitive settings | Existing render properties with their existing constraints |
| Object `defaults` | Initial mutable `x`, `y`, `visible`, and fallback `visual_ref` where applicable; not a static animation mask |
| Object `animation_ref` | Optional immutable clip reference using the existing asset animation catalog |
| State `object_overrides` | Sparse records keyed by `object_ref`; only supplied properties are controlled |
| Route/handler object action `object_ref` | Object in the current scene, independent of any destination state |

Do not keep a second authoritative object list in `render_models`. The service
may derive a render-model view for existing editor consumers, but must label it
as derived and translate writes through the shared command implementation.
States no longer require `render_model_ref` or `waiting_visual_ref` in version 2.
Retain reactive-wait and interaction policy independently of object ownership.

Illustrative scene fragment, not a complete scene accepted by project loading:

```json
{
  "schema_version": 2,
  "objects": [
    {
      "object_id": "pet",
      "kind": "sprite",
      "layer": "SCENE",
      "defaults": {"x": 10, "y": 20, "visible": true, "visual_ref": "pet.frame0"},
      "animation_ref": "pet.idle"
    }
  ],
  "states": [
    {"state_id": "selected", "object_overrides": [{"object_ref": "pet", "x": 100}]}
  ]
}
```

X and Y are independently overridable. Omission removes that state's control;
it does not mean zero, false, or a reset to defaults. Reject duplicate object IDs,
duplicate property assignments within an override set, unknown references, and
properties invalid for the object's kind. Renaming an ID is an explicit backend
operation updating references, not a side effect of renaming its display label.

Reuse immutable catalog clips rather than creating an animation format per
object. Converting a legacy waiting sequence to a catalog clip requires exact
frame order, duration, looping/settling behavior, and initial phase equivalence.
If that cannot be represented within supported limits, migration needs a choice.

Development source operations are `object.set_position`, `object.move_by`,
`object.set_visibility`, `object.set_frame`, and `object.clear_frame`, each with
`object_ref` and typed arguments. They are not service commands or wire opcodes.
Exact service-command and opcode allocations belong to integration.
Position operations may address either axis independently. Relative operations
read underlying mutable coordinates, including earlier writes in the action list.
The frame operation is a persistent static visual selection, not an animation
restart: for an animated object it masks the continuing clip. Clearing that
selection reveals the clip's current phase. An explicit clear operation must
be representable; do not use an out-of-range frame as a sentinel in source.

Neither persistent nor temporary frame selection changes the clip clock.
Runtime clip assignment/restart controls remain increment 2. Authored object clip
binding belongs to increment 1. Existing legacy element
actions keep their destination-binding semantics on the legacy path.

## Executable Discrimination and Records

Propose a new egg container version for object-model packages. Retain the
existing envelope layout, directory, checksum/hash mechanisms, and immutable
asset/audio encodings where compatible. New firmware accepts both versions;
existing firmware must reject the new version during compatibility validation,
before reporting VALID or committing an install. Never hide changed semantics
inside an otherwise legacy version-1 egg.

Use explicit per-scene execution-model discrimination in the new scene table.
This permits legacy and object-model scenes in one migrated project without
reinterpreting either. A package containing any object-model scene requires the
new container and capability. Legacy-only export remains on the legacy format.

Logical record layout to implement after representation review:

| Record | Contents |
|---|---|
| Scene descriptor | Execution model, graph reference, object/override table references, counts |
| Object definition | Stable ID string reference, kind/layer/geometry, mutable defaults, optional catalog clip index |
| State override range | State index, first override record, record count |
| Object override | Scene-local object index, property mask, values for controlled properties |
| Object operation | Distinct operation kind, scene-local object index, typed arguments |
| Clip definition | Reused immutable frame/timing data; no instance phase or current frame |

Compile IDs deterministically into compact indices; serialized indices are local
to that egg, not persistent identity across builds. Runtime identity is scene
activation plus object index. References cannot target the next scene's objects.
All record sizes, offsets, counts, reserved bits and reference ranges are checked
before publication. Unknown operations, property bits, models or capabilities
are rejection reasons, not ignored extensions.

Do not allocate numeric chunk types, enum values, capability bits or packed C
structures in this document. Freeze them together with byte-level encoder/parser
fixtures. The loader and installation preflight must share semantic validation;
recognizing the container header is not proof that its scenes can run.

## Live Runtime and Display Ownership

Keep immutable decoded definitions separate from a fixed-capacity mutable object
bank. Each live object holds underlying properties, any persistent static-frame
selection, and playback position/epoch. Resolve the active state's sparse
overrides onto those properties to produce a render snapshot. No defaults are
reloaded on state change or snapshot publication.

Stage bounded variable/object writes and the prospective state selection before
committing a transaction. Validate arithmetic, references, effective properties
and display-plan capacity before committing. Failed staging changes neither
variables nor objects and must not dispatch partially validated side effects.
Guards read the pre-transaction view; ordered actions read prior staged writes.
Publish only the final effective snapshot, not intermediate action results.

Scene logic owns mutable object state under the existing runtime owner. Display
owns rasterization and autonomous display execution. Exchange bounded immutable
snapshots plus activation/content/timeline identities through existing ownership
boundaries; do not expose mutable banks for concurrent display reads. Storage
continues owning package reads. No gameplay FAT access, new per-object threads,
dynamic allocation, clock-tree changes or new peripheral ownership is required.

Separate content revision from playback identity. A marker move or selection
change can invalidate pixels without invalidating another object's clip epoch.
The display schedule is derived from object playback, never the authority that
resets it. During STOP2, derive elapsed playback from the existing low-power
time accounting and autonomous phase evidence; do not depend on sleeping CPU
ticks. During scene suspension, pause scene-active elapsed time.

## Bounded Playback Plan

Proposed first-increment ceilings retain the current renderer envelope:

- At most 12 scene objects, including hidden objects, rather than 12 per state.
- At most 8 simultaneously animated object instances; hidden instances still
  consume an instance slot even though they do not require rendering.
- At most 4 distinct phase visuals per object and 12 combined sequence steps.
- Existing state/action/timer limits remain unchanged. Sparse override capacity
  must be explicitly budgeted, with at most one record per object per state.

These are proposed version-2 admission limits, not claims about every existing
source-schema maximum. Declare them through the target-profile/knobs mechanisms;
do not scatter new tuning constants through runtime code. Measure static storage,
staging/snapshot capacity, stack use and compiled metadata before enabling the
capability. Do not silently increase pools or linker allocations.

Initially admit clips compatible with one common supported phase quantum and
a bounded combined cycle. Defaults start from scene-active time zero; explicit
independent restart/pause is deferred. Each object still has independent identity
and logical playback. Hidden clips advance analytically without display work or
periodic CPU wakes solely for hidden playback.

A content change midway through a frame must preserve the remaining interval:
at 375 ms in a 250 ms clip, the next frame is due in 125 ms, not 250 ms. Rebuilding
an LPBAM plan must preserve both phase and residual deadline, including the
first interval before the regular cadence resumes. Verify this on hardware;
sharing a presentation ID alone is not sufficient.

Reject unrepresentable timing/capacity combinations with a specific build issue.
Do not silently stretch cadence, reset playback, or substitute an always-awake
path. Explicit playback controls in increment 2 require additional scheduling
admission tests, not an assumption that every independently shifted clip fits.

## Migration and Studio Surface

Add a shared-backend migration preview followed by explicit application. Preview
reports retained IDs, converted defaults/overrides/clips, changed action meanings,
unresolved choices and target-readiness issues. Applying requires the same source
revision that was previewed. No partial project write on an unresolved choice.

Straightforward placement conversion preserves existing element IDs, turns the
base model into objects, and maps placement overrides to sparse object overrides.
An object added to selected states remains one base-hidden object, with true
visibility overrides in exactly those states. Do not change a legacy action's
meaning merely because its visual placement converted successfully.

Keep legacy compilation available for projects containing state-private animation
or destination-binding mutations until the author explicitly chooses a supported
conversion. Per-state animation restart becoming continuous playback is itself a
behavior change to show in migration preview, even when every clip is identical.

Studio continues using shared service commands and derived `placement_ownership`;
no separate TypeScript compiler or runtime semantics. Preserve existing commands
for legacy projects. Add version-aware object operations/projections rather than
silently changing legacy command behavior. Advance the service API from the
latest coordinated baseline when implementation changes that contract, never
restore an older service module to obtain a desired version number.

Preserve editable drafts, preview, `build_issues`, and export-readiness separation.
Version-2 source JSON Schema, Python validation, service and compiler need shared
parity fixtures: the existing version-1 JSON Schema does not fully describe the
current Python placement/primitive surface and is not a complete migration spec.
Report issues with stable codes, scene/object/state/action paths and actionable
messages. Host service capabilities distinguish this delivered subset from
unavailable executable/firmware support. The target profile does not yet
advertise a scene-object executable capability.

## Delivery and Review

1. OS/GUI review the source fields, version strategy and service boundary above.
   GUI may continue unrelated UI work; it does not independently implement this
   migration or edit its shared backend files in parallel.
2. OS implements explicit source migration, schema parity fixtures and host
   semantic tests. Keep export of the new format unavailable until its decoder
   and firmware validation exist; retain a separate legacy example fixture.
3. Freeze executable IDs/layouts in shared encoder/parser/native tests, implement
   the bounded object bank and display handoff, then build the firmware.
4. Run ownership cases O01-O10 and applicable compatibility cases from the
   acceptance plan. Report host, build and hardware evidence separately.
5. Publish the coherent backend/capability commit for GUI to merge from main.
   Studio exposes the new editing behavior through those implemented commands.

The first device proof is selection moving a marker without restarting a base
sprite, followed by a scene timer changing an object without a state transition.
Include awake/STOP2 continuity and invalid-package recovery. Explicit playback,
groups and prefabs remain later increments with their own acceptance evidence.
