# Peep Studio Scene Object Ownership Handoff

Status: shared baseline committed; GUI representation review accepted;
development-only object/migration primitives host-tested. Service, full scene
schema, executable and firmware integration remain pending.

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

Development source fields/operations are implemented only in the isolated object
module and fragment schema. There are no newly available service commands,
capabilities or numeric wire IDs. The first increment does not include groups,
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

These pure-function tests do not complete O01-O10, integrate a real scene timer,
or prove preview/LPBAM scheduling. Production source loading/save/preview and
transactional migration service commands are next, followed by binary/firmware
integration and the recorded device acceptance cases. Existing example projects
stay legacy fixtures until that path is complete; no additional GUI design
approval is required to start the directly scoped next implementation work.

Record host tests and device results separately. A preview phase-continuity pass
does not prove autonomous playback or STOP2 behavior. Validate failed builds,
legacy packages, and unsupported-capability errors as well as the happy path.
Concrete acceptance cases are in [[Scene_Object_Ownership_Acceptance_Plan]],
under the ownership contract. Only the development host evidence above is
claimed here; no end-to-end scene-object or hardware pass is claimed.
