# Peep Studio Scene Object Ownership Handoff

Status: GUI representation review accepted; full source envelope, migration/edit
transactions and host preview connected in API 39. API 40 adds native V2
project/scene creation and state management. API 41 adds native local graphs.
Development-only V2 binary
encoding/reading and C loader/graph cores are implemented. Normal export,
production firmware activation and autonomous display integration remain unavailable.

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

### C Object Bank Core Increment

After `dc8815196fd14fcd9a8bf2773d356bd41914e47f`, the OS adds
`ps_scene_objects.h/.c`: fixed-capacity underlying properties, transactional
object actions, sparse state overrides, effective snapshots and analytical clip
phase/residual time. Decoder getters now expose immutable clip steps. Native
tests consume actual V2 encoder bytes with two independent objects sharing a
four-step, 250 ms clip; they do not substitute host-preview data for C behavior.

Selection changes preserve phase and remaining time; persistent movement reads
underlying coordinates and clamps each ordered write safely. Temporary frame
masks beat persistent masks, while both leave playback advancing. Hidden clips
advance without a scheduler, and explicit scene suspension pauses elapsed time.
Failed, stale and double commits do not change the live bank. Scene recreation
uses fresh defaults and a new activation token.

The core is compiled but not connected to production loading, graph dispatch,
timer dispatch or the display owner yet. V2 installation/export remains disabled;
**API 39 and Studio commands/capabilities are unchanged**. No GUI worktree,
source schema, compiler semantics, clocks, drivers or power policy changed.

Verification: **218 authoring tests pass**, including six new native-core tests;
HW6 Debug build, target-profile and diff checks pass. ARM ABI bank/stage/snapshot
sizes are 216/240/408 bytes. Unoptimized local stack frames are 320 bytes for
Init, 128 for Apply and 504 for Snapshot, excluding callees/interrupt overhead.
No production banks are allocated yet, and no device pass is claimed.

Next OS integration: complete V2 container/scene/graph validation and lowering;
stage graph variables/object actions together; admit and publish the display
plan while preserving phase and residual deadlines across awake/STOP2 handoff.

### Development Loader and Graph Increment

After `ecf7797` (staged object-bank core), the shared C loader now has an explicit
development V2 decode entry. It validates the entire mixed-model package before
returning a selected scene, preserves live V1 loader state, and leaves output
unchanged on failure. Model-2 graph revision 7 lowers to scene objects and sparse
overrides, not per-state render bindings. Shared animation catalog validation
includes unreferenced clips. Object-operation references and route ranges must
be canonical, bounded and fully accounted for.

`ps_scene_object_graph` stages variables, object operations, state selection and
ordered side effects. One commit publishes variables/objects only after caller
admission; failed/aborted/stale transactions publish nothing. A scene-timer event
can mutate objects without state re-entry. Native checks retain frame 1 with
125 ms remaining across a transition at 375 ms. Replacement routes decode, but
commit is withheld until replacement admission/orchestration is implemented.

**Service API 39 and Studio capabilities are unchanged.** Runtime descriptor API
is 22; this is not an authoring API bump. No GUI worktree, source schema, Python
compiler/service, knobs, clocks, drivers or power modes changed. Do not enable
V2 export or claim on-device support. Normal V2 install/activation remains blocked.

Verification: **228 authoring tests pass**, including ten new native-loader/graph
tests; HW6 Debug build and target-profile/diff checks pass. ARM graph bank/stage/
effects sizes are 256/1096/784 bytes, caller-owned and not allocated in production
yet. Stack measurements and remaining limits are in the executable design.
No physical RTC, display, audio or STOP2 test is claimed for this increment.

Next: production owner/display admission, frame resolution and residual-deadline
handoff; then device continuity/recovery tests and an explicit capability/export
handoff for Studio. GUI placement/migration work can continue independently.

### Native Creation and States (API 40)

Following `323179d`, the agreed next priority is native scene-object authoring,
not migration. This increment changes only the shared Python authoring service,
tests and documentation. The GUI worktree and firmware are untouched. Existing
migration behavior remains available but no new migration work is included.

**Delivered:** native V2 project/scene creation, state create/add/delete/rename,
entry state selection, node/entry layout, and project entry scene selection.
**Not delivered:** V2 graph construction (inputs, triggers, routes, guards,
variables, independent handlers), scene-exit/connection authoring, runtime clip
controls, production V2 activation or ordinary egg export. Existing
`object_actions.set` still edits actions on already-existing routes/handlers.

Discover creation without loading a project:

```json
{
  "scene_creation": {
    "project_operation": "project.create",
    "scene_command": "scene.add",
    "entry_scene_command": "project.set_entry_scene",
    "version_parameter": "scene_schema_version",
    "supported_versions": [1, 2],
    "default_version": 1,
    "scene_add_inherits_version": false,
    "initial_state_id": "start",
    "empty_scene_is_editable_draft": true
  }
}
```

This is a fragment of `service.hello`, not a request. Create a new project using
`project.create` with these params (destination must not exist):

```json
{"path": "G:/PEEPSHOW/workbench/new-game.peepproj", "scene_schema_version": 2}
```

The new project is immediately saved and loaded with a new project revision.
Its manifest stays version 1. The initial `main` scene is version 2, has
`objects: []`, and one `start` state with `object_overrides: []`. No example,
migration or state-private waiting/render records are inserted. Blank V2 host
preview works; `build_issues` still reports `SCENE_OBJECT_EXECUTABLE_UNAVAILABLE`.
Adding objects does not remove that production-export restriction.

To add another scene, call `project.apply_commands` with the current
`project_revision` and a `commands` array containing:

```json
{"kind": "scene.add", "display_name": "Garden", "scene_schema_version": 2}
```

Use the returned `applied_commands` entry's `scene_id` and `source`, not a frontend-generated
slug; IDs and relative source paths are collision-safe. The file is created by
`project.save`, not by this command. Omission of `scene_schema_version` means
**V1 even in a wholly V2 project**. Studio must explicitly request its chosen
model. Invalid values, including strings, booleans and null, return
`SCENE_SCHEMA_VERSION_UNSUPPORTED` without partial changes.

Both hello's `scene_object_authoring` and each scene capability now list:

```json
{
  "state_management_commands": [
    "state.add", "state.create", "state.delete", "state.rename", "state.set_entry",
    "editor.state_graph.set_node_position", "editor.state_graph.set_entry_layout"
  ]
}
```

These commands also appear in the V2 scene's `supported_commands`, alongside
existing object commands, `scene.rename` and `project.set_entry_scene`.
`graph_construction_commands` remains false for V2 and true for V1. The broad
legacy graph catalog in hello must not override a V2 scene's command list.

Example command records, each used in `project.apply_commands`:

```json
[
  {"kind": "state.create", "scene_id": "main", "display_name": "Playing", "x": 200, "y": 100},
  {"kind": "state.add", "scene_id": "main", "state": {"state_id": "paused", "display_name": "Paused", "object_overrides": []}},
  {"kind": "state.rename", "scene_id": "main", "state_id": "paused", "display_name": "Resting"},
  {"kind": "state.set_entry", "scene_id": "main", "state_id": "paused"},
  {"kind": "editor.state_graph.set_node_position", "scene_id": "main", "state_id": "paused", "x": 240, "y": 120},
  {"kind": "editor.state_graph.set_entry_layout", "scene_id": "main", "target_handle": "entry-top-left", "target_side": "top"},
  {"kind": "state.delete", "scene_id": "main", "state_id": "start"},
  {"kind": "project.set_entry_scene", "scene_id": "garden"}
]
```

`state.create` allocates a stable collision-safe ID, empty overrides and graph
position. `state.add` accepts an explicit source record and runs shared validation.
The x/y above are editor graph coordinates, not object positions. New states do
not inherit entry-state overrides or join an existing exact visibility state set.
The 64-state limit remains enforced. Rename changes only the display name.

Deletion refuses entry/last states and route source/target or independent-handler
target references with `COMMAND_TARGET_IN_USE`; it does not silently remove
transitions. Unreferenced deletion removes that state's overrides and node layout,
not the scene-owned objects. Change the entry first when deleting the old entry.
Setting project/scene entry is not scene-exit wiring.

Edits retain revision checks, whole-batch rollback, bounded undo/redo, reference-safe
validation, draft preview/save and separate build readiness. A failed batch adds
no undo entry and writes no scene files. Existing V1 creation and editing remain
unchanged. GUI can now implement native project/scene creation and state management
from capabilities; OS's next shared increment is graph construction and connections.

Verification: **240 authoring tests pass**, including native C checks and 12 new
native-authoring service tests. Target-profile consistency and `git diff --check`
pass. No firmware build or device test was run: this increment changes no firmware
and does not claim production V2 execution or display/STOP2 continuity.

### Native Local Graphs (API 41)

After `9c5a42d`, native V2 scenes can author their local input/timer graph without
migration or source-file edits. This increment reuses shared command handlers,
validation, graph/timer host execution and the object-action editor. It changes
no source schema, wire values, GUI worktree or firmware.

**Capability handoff:** `service.hello.scene_object_authoring` and each valid
scene capability now contain `local_graph_commands`. For V2,
`graph_construction_commands: true`, `scene_connection_commands: false`, and
`route_destination_kinds: ["state", "system_exit"]`. Every delivered V2 command
is also in `commands` (hello) and `supported_commands` (per scene). V1 still
uses the legacy catalog and allows scene connections. Do not infer scene wiring
support from the graph boolean or API number alone.

Newly admitted V2 commands, with existing field names:

| Area | Commands | Payload after `kind` and `scene_id` |
| --- | --- | --- |
| Variables | `variable.add`, `variable.update`, `variable.delete` | Full `variable` record; delete takes `variable_id` |
| Inputs | `input_action.add`, `input_action.update`, `input_action.delete` | Full `input_action` record; delete takes `action_id` |
| Timers | `event_binding.add`, `event_binding.update`, `event_binding.delete` | Full `event_binding` record; delete takes `binding_id` |
| Independent handlers | `event_handler.add`, `event_handler.update`, `event_handler.delete` | Full `event_handler` record; delete takes `handler_id` |
| Trigger creation | `route.create_trigger` | `source_state`, `logical_source`, optional `event_kind`; exactly one `target_state` or `system_exit: true` |
| Trigger rebinding | `route.rebind_trigger` | `route_id`, `logical_source`; retains event kind |
| Routes | `route.add`, `route.delete` | Full `route` record; delete takes `route_id` |
| Route edits | `route.set_sources`, `route.set_action_ref`, `route.set_event_ref`, `route.set_target` | `route_id` plus `from_states`, `action_ref`, `event_ref` or local `target_state` respectively |
| Guard lists | `route.guard.add`, `route.guard.delete`, `route.guard.move` | `route_id`, `guard_index`; add takes `guard`, move takes `target_index` |
| Guard edit | `route.set_guard` | `route_id`, `guard_index`, `variable_ref`, `operator`, `value` |
| Policies | `scene.set_reactive_wait_default`, `scene.set_interaction_policy`, `scene.set_joystick_policy` | Full corresponding policy value, using the same field name without `scene.set_` |
| Route layout | `editor.state_graph.set_route_layout` | Existing `route_id`, `source_state`, `rails`, `target_handle`, `target_side`, optional `token_positions` |
| Shell terminal layout | `editor.state_graph.delete_system_exit` | No additional fields; refuses while shell-exit routes still reference it |

`object_actions.set` remains the complete ordered action-list editor for both
routes and handlers. It supports the existing validated mixture of object,
variable, timer and SFX actions. Do not use legacy `route.action.*`,
`route.set_action` or destination-element mutations on V2. Handler update accepts
the full handler, including its guards, optional local target and actions.

Create a native V2 project using the API 40 creation commands first. Example
`commands` array for `project.apply_commands` with the current `project_revision`:

```json
[
  {"kind": "state.create", "scene_id": "main", "display_name": "Other", "x": 200, "y": 0},
  {"kind": "object.add", "scene_id": "main", "object": {"object_id": "panel", "kind": "filled_rect", "width": 8, "height": 8, "z_order": 0, "layer": "SCENE", "defaults": {"x": 10, "y": 20, "visible": true}}},
  {"kind": "variable.add", "scene_id": "main", "variable": {"variable_id": "count", "value_type": "int32", "initial": 0, "minimum": 0, "maximum": 10}},
  {"kind": "input_action.add", "scene_id": "main", "input_action": {"action_id": "a", "logical_source": "BUTTON_A"}},
  {"kind": "route.add", "scene_id": "main", "route": {"route_id": "next", "action_ref": "a", "from_states": ["start"], "target_state": "other", "guards": [{"variable_ref": "count", "operator": "eq", "value": 0}], "actions": []}},
  {"kind": "object_actions.set", "scene_id": "main", "owner_kind": "route", "owner_id": "next", "actions": [{"kind": "object.move_by", "object_ref": "panel", "dx": 5}, {"kind": "set_variable", "variable_ref": "count", "operation": "add", "value": 1}]}
]
```

This example assumes a fresh project, so the generated state ID is `other`.
In general use IDs returned in `applied_commands`. `route.create_trigger` is the
convenience alternative: it creates/reuses a logical input, creates the route,
and registers its wait/meaningful-activity interests. Its optional entry socket
fields are `target_handle`/`target_side`. Explicit `input_action.add`/`route.add`
do not automatically edit those policies; use the policy commands where needed.
Full policy replacements must retain required fields, including interaction mode.

Scene timer creation is one batch containing both binding and handler:

```json
[
  {"kind": "event_binding.add", "scene_id": "main", "event_binding": {"binding_id": "tick", "event_type": "time.scene_elapsed", "configuration": {"delay_ms": 500, "start_policy": "scene_entry"}}},
  {"kind": "event_handler.add", "scene_id": "main", "event_handler": {"handler_id": "tick_handler", "event_ref": "tick", "guards": [], "actions": [{"kind": "object.move_by", "object_ref": "panel", "dx": 7}]}}
]
```

The handler above changes the underlying object without state re-entry. Scene
timers survive local state transitions. `start_policy: "action"` instead waits
for `start_timer` or `restart_timer`; `cancel_timer` disarms it. State-entry timers
use `time.state_entry_elapsed`, configuration `{ "delay_ms": 500 }`, and a route
with `event_ref` and `from_states`, not an independent handler. These are the
existing scoped timer semantics, not new OS event types. Unsupported sensor/SOC/
schedule triggers are still rejected.

Exactly one handler is required for each scene timer at batch completion. Delete
the handler first, then its binding, in the same batch after removing timer actions
and policy references. A lone binding addition or lone handler deletion is invalid.
Variable deletion refuses uses in handler guards/actions as well as routes.
Input rebinding retains the old input if another route uses its `event_ref` alias.
These operations never silently detach graph users.

Scene connections remain the next increment. V2 `scene_exit.*` and
`editor.scene_flow.*` commands are blocked. Writing `target_scene` or
`scene_exit_ref` through the newly exposed route/handler commands returns
`SCENE_OBJECT_CONNECTION_UNAVAILABLE`. Existing linked source can still load,
save and preview as before; this does not migrate or invalidate earlier documents.

Verification: **254 authoring tests pass**, including native C checks and 14 new
native V2 graph-authoring tests. Tests exercise real host input/timer dispatch,
not only command acceptance: guarded state transitions, underlying movement under
overrides, scene timers across state changes, one-shot state timers, action start/
restart/cancel, reference protection, undo/redo, save/reload and invalid-batch
rollback. V1 regression checks pass. No firmware build/device test was run because
firmware is unchanged. Ordinary V2 export remains blocked by
`SCENE_OBJECT_EXECUTABLE_UNAVAILABLE`; no hardware behavior is claimed here.

## First Awake Hardware Fixture Handoff

OS now has an explicit development-only V2 activation/display path. The original
awake fixture passed hardware animation, independent marker changes and shell
exit. Service API 41, target capabilities and ordinary export
remain unchanged. This is not permission to enable V2 egg export in Studio.

The first GUI-authored hardware fixture has one V2 scene with continuous
interaction, two local states, A/B button routes, one scene-owned looping sprite
and separate object overrides. It intentionally omits timers and SFX. The next
development increment admits scoped timers; SFX, timeout interaction and
scene-to-scene routes remain rejected. Object/variable actions and guards are
allowed; a system-exit route can return to the shell. No requirement for a
focus or cursor object is added.

OS generates the development egg from the supplied project using
`tools/authoring/build_object_development.py --project <path>` and builds the
firmware. GUI's source-only fixture from commit
`5c059bfce37770319cb49f09a14246202c42e039` is now imported unchanged at
`examples/authoring/native_v2_continuity.peepproj` and is the development builder's
default. Its 1,344-byte egg passes native C activation, A/B animation continuity
and exact raster checks. Its awake hardware test passed: the user saw the sprite
animate across A/B changes, and the dump recorded seven applied transitions,
30/30 successful display requests and no render/queue/wait errors or lease faults.
Exact frame timing remains native-tested rather than inferred from one snapshot.
A moves the marker right and B returns it left; neither exits to the shell.
The earlier synthetic fixture remains a native regression test. Details, commands
and evidence requirements are in
[[Scene_Object_Awake_Development_Test]]. GUI should continue capability-gated
authoring; no shared service or Studio files changed in this firmware increment.

## V2 Awake Timer Handoff

State-entry and scene-owned one-shots now connect to the existing `thRuntime`
scheduler in the explicit V2 development path. Scene handlers can change objects
without a destination state; state activation stays unchanged. Committed
Start/Restart/Cancel actions use the same bounded slots and ordering as V1.
State timers cancel/rearm with state activations; scene timers survive A/B state
changes. Relative timers pause in the shell and resume their remaining duration.

Native tests cover the real V2 runtime plus scheduler and rendering, not just
timer command acceptance. The OS timer fixture is generated with
`build_object_development.py --timers`; GUI's source fixture is unchanged.
The described awake hardware fixture passed: state/scene expiry, uninterrupted
animation during A/B changes, Restart and Cancel. Timer due/dispatch/applied were
2/2/2 with zero errors; all 50 display requests completed successfully. Broader
timer controls and suspension remain native-tested, not hardware-qualified.
Development probe API is now 2;
service API 41 and target/export capabilities do not change.

GUI may continue timer authoring with the existing commands and capabilities.
Create/delete each scene binding and its independent handler together. Supply
a source-only timer fixture for the next integration check; do not enable
ordinary V2 egg export or claim STOP2/LPBAM timer support from this increment.
