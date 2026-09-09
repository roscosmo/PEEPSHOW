# Peep Studio Scene Object Ownership Handoff

Status: GUI representation review accepted; full source envelope, migration/edit
transactions and host preview connected in API 39. Development-only V2 binary
encoding/reading is implemented. Normal export, firmware and autonomous display
integration remain unavailable.

Authority: [[Scene_Object_Lifetime_and_Control_Contract]]. This handoff coordinates
work; it does not allocate executable schema fields, capability IDs, or opcodes.

## Agreement to Carry Across Branches

Scene instances own objects. States, event handlers, and timers control them.
Groups are optional named sets; prefabs instantiate distinct objects/behaviors.
Changing menu selection must not reconstruct the scene or restart unrelated
animation. "Base placement" must eventually represent this live ownership,
not just a compiler copy made for each state.

The existing implementation does not yet satisfy the complete contract. Keep
legacy eggs working while the shared backend and firmware acquire the new model.
Do not expose the new semantics as available based only on an editor change.

## Coordination Now

### Pinned Compatibility Baseline

GUI merge commit: `57f7030cc65c09b434223e405c12b67e211b99a0`.
Parents: GUI `98f3cf0415fa4c8613a427e07b7cf3235c7fa627` and incoming main
`8f271532216216065f03a831bdb09cc07d8f2953`. The subsequent main design/acceptance
checkpoint is `018fc802234024d4df437cfb3be89da7c0bfae01` and must be retained.
The completed main integration is committed as
`34c76bba4bcf59ab03d8668dca329f2338caf33f`. Use that combined baseline for the
scene-object increment, not a fresh wholesale import of the GUI shared files.

The OS agent verified a full Debug build of the GUI merge in a separate build
directory: 653 build steps including link, with unused-function/parameter
warnings. RAM usage was 433992 bytes, ROM 522352 bytes, and SRAM4 15480 bytes.
This is build evidence, not a hardware pass. The GUI agent separately reported
148 tests, native checks, Studio typecheck, and target-profile checks passing;
the independent main integration results are recorded below.

The reviewed main integration boundary is shared authoring code/schema, its
internal fixtures, public `examples/authoring/state_slice.peepproj` example,
and matching tests, plus the updated authoring schema,
empty-scene validation handoff, and PeepOS link contract. Preserve service API
38 and the behaviors listed below. Exclude Studio UI, design artwork, workbench,
loose assets, generated embedded egg, and all GUI-side firmware differences.
Both branches already encode upward diagonal lines using the same egg flag;
main's loader translates it to its existing internal element type. Importing
the Python backend does not require adopting GUI's internal render-model flags.

Retain main's `test_authoring_model.py` embedded-package source expectation.
GUI's version changes that expectation to its own embedded demo. Bring across
its added audio-cue display-name regression separately during integration;
do not replace main's embedded egg merely to make GUI's fixture test pass.
Reconcile imported documentation with retained main design decisions and verify
the combined host/native suite and target profile before committing integration.
Object migration remains a separate increment after representation agreement.

### Main Integration Verification

Validated the working-tree integration on top of main
`018fc802234024d4df437cfb3be89da7c0bfae01`, importing the scoped shared files and
public example from GUI commit `57f7030cc65c09b434223e405c12b67e211b99a0`.
The first run had 146 passing tests and two public-example failures because the
initial import omitted that updated example. Importing its five changed source
files resolved both failures without firmware changes or weakened assertions.

- Full authoring suite: 148 tests passed, zero failures/errors/skips, including
  native package validation, shell recovery/input, workflow ordering, scene
  timers, and primitive pixel/validation checks against main's firmware.
- Target-profile generated-header check passed.
- Main's committed embedded-egg freshness test passed; its original source
  expectation was retained and the audio-cue display-name test was added.
- Service API 38, draft/readiness separation, placement commands, and updated
  scene-exit example behavior remain represented in the passing service suite.
- Firmware and workbench have no changes relative to main HEAD. No embedded
  egg regeneration, GUI firmware API adoption, or scene-object migration occurred.

Reproduction from the main workspace, with host GCC available:

```powershell
$env:HOST_CC = 'C:\msys64\ucrt64\bin\gcc.exe'
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
tools/.venv/Scripts/python.exe -m unittest discover -s tools/authoring/tests -p "test_*.py" -v
tools/.venv/Scripts/python.exe tools/authoring/gen_target_profile.py --check
```

No new full ARM build or hardware test was performed for this source-only main
integration. The earlier GUI ARM build is separate evidence, not a replacement
for device acceptance. Scene-object acceptance cases remain NOT RUN.

### Integration Sequence

1. Completed: the user committed the GUI compatibility merge identified above,
   preserving Studio behavior and integrating primitive support. Object migration
   was not implemented. User performs all git state-changing operations.
2. Completed: the reviewed shared-backend baseline and matching public example
   were imported, validated and committed on main at the integration commit
   above; no unrelated UI/firmware/workbench content was imported.
3. Reviewed: GUI found no architectural blocker in the executable proposal.
   Its four clarifications are recorded in the executable design. Full service
   and byte-level contracts are still to be frozen during implementation.
4. The OS agent implements a bounded end-to-end backend/firmware increment and
   provides fixtures, validation results, and a capability handoff.
5. Merge that backend increment from main into GUI. The GUI agent implements
   editing/preview controls against the actual shared capability, then reports
   authoring-to-device acceptance results.

Compatibility integration and GUI representation review are complete. OS owns
the shared implementation; do not begin competing schema implementations. GUI
layout planning and unrelated UI work can continue.

## GUI Review Decisions

| Existing feature or question | Agreed disposition |
|---|---|
| Scene Base placement | Preserve IDs; initialize scene-owned objects and authored defaults |
| State position/visibility/static frame overrides | Temporary per-property control, not object ownership |
| Add object to selected states | One base-hidden object, visible in exactly the chosen state set |
| State hierarchy object rows | References to controlled objects, not duplicate instances |
| Sprite assets and frames | Immutable shared data; playback belongs to each placed object |
| Relative movement during an override | Read/write underlying mutable position; override keeps masking it |
| Hidden playback and same-clip assignment | Continue playback; assignment does not restart |
| Override exit | Reveal current underlying values, not authored defaults |
| "Deepest state wins" | Deferred for the new model; reject conflicting simultaneous property overrides |
| Legacy destination-binding actions | Keep legacy meaning until explicitly migrated |
| Legacy per-state animated overrides | Keep legacy compilation or require an explicit supported conversion; no silent loss |

The imported placement documentation marks the older hierarchical-precedence
proposal deferred. It does not enable a "deepest state wins" runtime policy.

## File Ownership

| Area | Active implementation owner |
|---|---|
| Shared semantic contract and wire-format agreement | OS and GUI review together |
| Firmware object storage, action execution, renderer/STOP2 integration | OS agent |
| Shared schema/compiler/parser/preview/capability changes for this migration | OS agent, with GUI review |
| Peep Studio UI, inspector, selection, grouping and override authoring UX | GUI agent |
| Git commits, pushes and merges | User |

Shared files include `schemas/authoring/state-scene-v1.schema.json` and
`tools/authoring/peepshow_authoring/` modules such as `project.py`, `compiler.py`,
`egg_format.py`, `preview.py`, `service.py`, and `target_profile.py`. Exact files
will depend on the agreed representation. Assign one active writer for each
shared increment; do not replace whole shared files during a merge and lose
other GUI/backend work. Preserve newer service versions and unrelated features.

Specifically preserve draft-preview versus export-readiness separation,
`build_issues`, existing placement commands, and the later GUI service API
version. A project may remain editable/previewable while export is blocked;
ownership migration must not turn this into an unconditional project-open error
or let an invalid draft export as a runnable egg.

## Suggested Increments

### 1. Scene-Owned Objects and Continuity

Establish stable object identity and bounded live storage. Support scene defaults,
persistent object actions, temporary state overrides, and animation continuity
through selection changes. Include compiler, preview, loader validation, firmware,
legacy fixtures, and explicit capability reporting in the same coherent increment.

The minimum device proof uses one base animated sprite plus a state-controlled
menu marker. Changing selection must move the marker while preserving the
sprite's phase and remaining phase time, both awake and across STOP2. A second
proof changes an object from a scene timer without changing logical state.

Do not claim completion from just sharing presentation IDs: that only addresses
one trigger for rebasing and does not establish live object ownership.

### 2. Explicit Playback Control

Add only the agreed bounded playback actions. Test same-clip assignment,
target-only restart, clip replacement, pause/resume, and visibility. Validate
multiple logical playback positions against the existing shared display budget.
Any unsupported combination needs an author-facing validation error.

### 3. Groups and Prefab Instances

Expose flat groups and reusable prefab instances after object identity and
control are stable. Grouping must not change lifetime or playback. Prefab
expansion must provide distinct instance IDs and local state; instance timer
support requires its own advertised capability, not an assumption.

## Representation Review and Implementation

The reviewed proposal is [[Scene_Object_Executable_Design]]. GUI found no
architectural blocker and approved OS proceeding. Its design specifies:
- Explicit scene source version 2, stable object IDs, and sparse state overrides.
- Object-targeted persistent actions without destination-state addressing.
- A new executable version with explicit scene execution models, preserving
  legacy compilation and requiring full install-time semantic validation.
- Fixed-capacity live object storage and phase-preserving display snapshots
  within the existing renderer envelope; no per-object threads or hardware timers.
- Explicit migration preview/apply and version-aware service projections, while
  retaining API 38 baseline behavior, placement commands and `build_issues`.

The four GUI clarifications are now explicit: fallback frames do not mask clips;
static selections have separate set/clear semantics; relative dx/dy mean right/up
while absolute coordinates stay right/down and movement clamps whole bounds;
overrides clear individual axes; mixed-scene service operations must check model
and command capabilities. Authored clip binding must not create private state
waiting records and is distinct from deferred runtime clip assignment.

Source fields/operations are now connected to the host service as described
below. No numeric executable wire IDs are allocated. The first increment does not include groups,
prefabs or runtime playback-control commands. GUI enables editing based on
delivered per-model capabilities, never the service API number alone. Preserve
undo/redo and reference-safe asset edits along with existing draft/build behavior.

Do not allocate new wire IDs or implement a parallel runtime model in TypeScript.
The shared backend remains validation/compiler authority and the preview must
exercise the same semantics. Unsupported controls remain unavailable until the
backend capability is present.

## Completion Evidence

### Development Object Foundation

On top of main `abe38bc` (executable design checkpoint):
- Added `scene_objects.py`: object/state fragment validation; pure object action
  staging and visual resolution; independent-axis override clearing; migration
  preview/materialization with content-revision checks. No project writes.
- Added `scene-object-model-v2.schema.json`: fragment definitions, not the full
  scene schema or target admission contract. Structural field-set parity is
  tested; a general JSON Schema validator was not added or run.
- Added 24 host regressions covering masks, frame continuity at supplied times,
  underlying ordered movement/clamping, axis conventions, independent clearing,
  malformed input, shared-asset instance separation and migration restrictions.
- Full authoring suite: 172 tests passed, including existing native tests;
  target-profile generated-header check passed.
- Service API remains 38. Legacy egg output remains unchanged in the regression
  fixture. The current project loader/export path rejects a version-2 candidate.
- No firmware, compiler, service, preview, examples or workbench changed.
  No full ARM build or hardware test was needed/performed for this host-only work.

Those foundation tests alone did not complete O01-O10, integrate a real scene
timer, or prove preview/LPBAM scheduling. The connected host increment below
supersedes the source-loader/service limitations of that checkpoint. Binary/firmware
integration and device acceptance remain next. Existing example projects
stay legacy fixtures until that path is complete; no additional GUI design
approval is required to start the directly scoped next implementation work.

Record host tests and device results separately. A preview phase-continuity pass
does not prove autonomous playback or STOP2 behavior. Validate failed builds,
legacy packages, and unsupported-capability errors as well as the happy path.
Concrete acceptance cases are in [[Scene_Object_Ownership_Acceptance_Plan]],
under the ownership contract. No firmware scene-object or hardware pass is claimed.

### Connected Host Increment (API 39)

Built on main `aaad7c8` (the isolated foundation checkpoint). This increment
changes shared Python/source schema/docs only, not firmware, GUI UI, examples
or workbench content. It preserves legacy-only egg generation and the API 38
editing workflow. Existing projects are never migrated on open/save.

Implemented:
- Full source envelope `schemas/authoring/state-scene-v2.schema.json`, reusing
  object definitions and common graph schema definitions. Python performs
  reference, geometry, action and graph validation. Root/fragment structural
  parity is tested; a general JSON Schema validator was not added or run.
- Explicit migration preview/apply with project revision and source-content
  checks. Scene plus newly created immutable clips are one undoable in-memory
  edit. Undo/redo and save/reload preserve IDs, clips and mixed-version scenes.
- Object commands through `project.apply_commands`; the entire batch is
  rejected unchanged if any command or final reference validation fails.
- Host preview retains the existing input/guard/timer executor and rasterizer,
  with underlying object state, temporary masks and scene-active clip time.
  It exposes per-object phase and remaining interval. Scene changes recreate
  objects; state changes do not. Explicit scene suspension pauses playback.
- `build_issues` reports `SCENE_OBJECT_EXECUTABLE_UNAVAILABLE` for version 2.
  Strict build/export refuses any package containing such a scene, including
  mixed projects. This is an editable/previewable draft, not device support.

The host graph adapter temporarily represents common graph/render fields in
legacy-shaped records, then restores symbolic object actions before execution.
Those records are not a version-2 executable format or a semantic migration
back to legacy. Preview responses label it `host_scene_objects_not_firmware`.
No wire layout, device admission, LPBAM scheduling or hardware pass is implied.

### Studio Capability Checks

`service.hello.scene_object_authoring` reports `status: host_available`, source
version 2, execution model `scene_objects`, command names and axis conventions;
`egg_export` and `firmware_available` are both false. This is separate from the
unchanged device target profile. Do not enable a device/export option from it.
`clip_loop_policies` currently contains only `loop`; runtime playback controls
and version-2 graph-construction commands are explicitly unavailable.

Project document results (including normalization) expose `scene_capabilities[scene_id]`: source version,
execution model, host editing/preview, export-model availability,
`supported_commands` and `legacy_command_catalog`. For version 1 the existing
legacy catalog remains authoritative (`supported_commands: null`); for version
2 only the explicit command list applies. Export-model availability for a legacy
scene does not mean the whole project is build-ready; always use `build_issues`.

Version-2 `placement_ownership[scene_id]` contains `objects`, per-state `changes`
and `resolved_elements`, and `state_scoped_element_ids`. It is marked
`derived_read_only: true`. Each override's `local_properties` distinguishes X
and Y. These are entry-time derived values, not current preview playback.
Live preview responses supply `objects` with `underlying`, `effective`, and
`playback` (`animation_ref`, `phase_index`, `remaining_ms`). Their `timeline`
reports scene elapsed time/ownership, not a fabricated shared waiting phase.

Do not fabricate authoritative `render_models` or state waiting references for
version 2. Studio must branch on the model and use the object commands; legacy
placement/animation mutation commands targeting version 2 are rejected with
`COMMAND_EXECUTION_MODEL_MISMATCH`.

### Migration Operations

Both operations require `project_revision`, `scene_id` and the boolean
`accept_continuous_animation`. Preview returns `can_apply` and `plan` containing
`source_revision`, candidate scene, new clip records, changes and issues.

```json
{"operation":"project.object_migration_preview","params":{"project_revision":1,"scene_id":"main","accept_continuous_animation":true}}
```

Application additionally requires that preview's `source_revision` string:

```json
{"operation":"project.object_migration_apply","params":{"project_revision":1,"scene_id":"main","accept_continuous_animation":true,"source_revision":"<plan.source_revision>"}}
```

These examples show operation/params only; use the normal protocol envelope.
Application recomputes the plan rather than trusting edited candidate JSON.
Unresolved migration issues block application. It updates memory, increments
the project revision and invalidates preview; it does not write project files.
Normal `project.save` persists the result using the existing per-file writes.
Static migration can use `false`; animated migration requires the author's
explicit acceptance of continuous scene playback starting at sequence step zero.

### Object Command Payloads

All commands below use `kind` and `scene_id`, and may include `command_id`.
Submit them in the normal `project.apply_commands` batch with project revision.

| Kind | Additional fields |
|---|---|
| `object.add` | Full `object` definition; optional `visible_in_states` array of distinct existing state IDs |
| `object.delete` | `object_id`; removes overrides, but refuses while actions reference the object |
| `object.set_defaults` | `object_id`, nonempty `properties` containing any of `x`, `y`, `visible`, `visual_ref` |
| `object.bind_animation` | `object_id`, `animation_ref`; writes one authored clip reference, no state-private waiting records |
| `object.clear_animation` | `object_id`; removes the authored clip reference |
| `object_override.set` | `object_id`, `state_id`, nonempty sparse `properties` using `x`, `y`, `visible`, `visual_ref` |
| `object_override.clear` | `object_id`, `state_id`, nonempty `properties` array of individual property names; `position` is invalid |
| `object_actions.set` | `owner_kind: route` or `handler`, existing `owner_id`, complete ordered `actions` array |

`visible_in_states` explicitly makes the object base-hidden, visible only in
that exact state set. Omission leaves the supplied default visibility intact.
An empty array means hidden in every state. Null is not a substitute for omission.

Action records are distinct from edit commands: `object.set_position` with
`object_ref` and X and/or Y; `object.move_by` with `object_ref` and dx and/or dy;
`object.set_visibility` with `visible`; `object.set_frame` with `frame_ref`;
`object.clear_frame` with no value. Relative writes use underlying coordinates,
accumulate in order and clamp after each write. Positive dy is up; absolute Y
is down. Static masks do not stop/restart the clip, and clearing reveals its
current phase. These actions may coexist with supported variable/timer/SFX
actions. Direct scene-replacement routes still permit only `play_sfx`.

Existing `scene.rename`, `state.rename`, and `state.set_entry` also work for
version 2. Other legacy scene/graph/placement commands are deliberately not
advertised for version 2 yet. In mixed projects, catalog asset/animation/audio
upsert/delete, `scene.add` (creates a legacy scene), and
`project.set_entry_scene` remain allowed; final validation protects references
from either model. No object ID rename, geometry-edit command, version-2 state
creation/graph construction, groups or runtime playback controls are delivered
in this increment. GUI can integrate the declared object subset, not infer a
complete replacement for all existing editor tools. Keep examples legacy until
executable support is delivered.

### Host Verification

The connected regression suite covers migration choices/staleness, one-step
undo/redo including clip catalogs, save/reload, mixed-model rejection and batch
rollback, exact visibility sets, individual-axis clearing, reference-safe edits,
empty drafts, frame masks, hidden playback/suspension, scene recreation and
filled primitive rasterization. A scene timer fires through the shared executor
after a state change and changes an object without another state transition.
State changes at 375 ms preserve the remaining 125 ms of a 250 ms clip frame.
Late variable-action failure leaves earlier staged object writes uncommitted.

Verification: 190 authoring tests passed, including existing native checks and
18 connected scene-object integration tests. Target-profile generated-header
check and `git diff --check` passed. No ARM build or hardware test was performed:
the firmware is unchanged and cannot execute version-2 scenes yet.

### Development Binary Increment

On top of main `f2a7d9e`, the OS backend adds a separate development-only
`compiler.build_development_egg_v2(bundle)` and
`object_egg.parse_development_egg_v2(blob)`. Controlled source fixtures are
migrated and encoded in tests without Studio UI. The reader consumes actual
binary object/control records, not the API 39 host-preview annotations.

Container version 2 explicitly distinguishes legacy and object-model scenes.
New object/control tables and graph revision 7 preserve action ordering and
reuse asset, animation and audio payloads. The development layout is
`schemas/package/PeepPkg_V2_Development_Layout.md`; this records allocated
wire values but does not grant HW6 runtime or LPBAM admission.

GUI-facing API remains **39**. No service commands/capabilities, source schemas,
normal export paths, examples, workbench or firmware are changed in this
increment. Studio should continue its agreed capability-aware placement and
inspection work. Do not connect the development builder to the export button.
`SCENE_OBJECT_EXECUTABLE_UNAVAILABLE` remains the normal build result for V2.

Tests include deterministic mixed-scene round trips, all five object actions,
independent timer handlers, asset/audio reuse, default-reader rejection and
malformed records with recomputed checksums. A bounded byte-corruption sweep
checks that failures return format errors, not decoder crashes. These are host
format checks, not a C decoder or device execution pass. Next: firmware decode,
bounded object storage/actions, display handoff and hardware acceptance.

Verification: 206 authoring tests passed, including native checks and 16
development-format tests. Target-profile consistency and `git diff --check`
passed. No ARM build or hardware test was performed; firmware is unchanged.

### C Object Record Decoder Increment

Added `ps_egg_object_decoder.h/.c` to the HW6 source build. This bounded,
allocation-free module reads the new object/control chunks and validates their
references against shared catalog bytes. Six native tests compare its actual
decoded fields and malformed-input decisions with the independent Python
record reader. The existing native loader test also verifies that a V2 fixture
is still rejected without changing the active V1 context.

Verification: **212 authoring tests pass**, including native tests. HW6 Debug
build, target-profile consistency and `git diff --check` pass. No device test
was performed: the decoder is not yet called by production loading or runtime.
This does not prove full V2 package validation, live object execution or LPBAM.

API **39**, source/service commands, GUI capabilities and ordinary export are
unchanged. Keep V2 export unavailable. Remaining OS work: integrate complete
container/scene/graph validation with the loader, implement the bounded live
object bank and transactional actions, then display/awake/STOP2 continuity.
