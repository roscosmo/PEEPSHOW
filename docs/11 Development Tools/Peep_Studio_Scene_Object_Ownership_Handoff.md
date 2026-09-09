# Peep Studio Scene Object Ownership Handoff

Status: GUI semantic review received and decisions recorded; compatibility merge and implementation pending.

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

1. Complete the current GUI compatibility merge, preserving Studio behavior and
   integrating implemented primitive support. At the time of the GUI review,
   nine conflicts were reported unresolved; this documentation does not claim
   to resolve or inspect those conflicts. Do not implement object migration in
   that merge. User performs git state-changing operations.
2. GUI provides the resolved commit and list of shared schema/authoring files
   changed. Establish a common shared-backend baseline before OS migration work;
   do not overwrite newer GUI backend changes with the older main copies. This
   does not require importing unfinished UI work into main. The exact integration
   route depends on the resolved diff and remains a user-run git operation.
3. Agree the source schema, executable discriminator, action addressing, and
   capability reporting together before either branch implements those fields.
4. The OS agent implements a bounded end-to-end backend/firmware increment and
   provides fixtures, validation results, and a capability handoff.
5. Merge that backend increment from main into GUI. The GUI agent implements
   editing/preview controls against the actual shared capability, then reports
   authoring-to-device acceptance results.

Do not begin competing schema implementations while compatibility integration
and the executable representation remain open. Documentation and acceptance
planning can proceed now; UI layout planning and unrelated GUI work can continue.

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

The GUI agent should mark its older hierarchical-precedence proposal deferred
when reconciling placement documentation. No matching "deepest state" wording
was found in main's Engine API or Development Tools docs during this update;
the GUI-only text has not been edited from this worktree.

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

## Remaining Representation Agreement

The GUI agent has agreed the ownership/lifecycle model and supplied the mapping
above. The next joint review is the executable representation, not another
request to approve those same semantics:
- Source version/migration selection and retention of legacy source semantics.
- Executable discriminator and target capability negotiation.
- Stable object/clip identity and action target addressing without a destination state.
- Bounded storage and shared animation scheduling representation.
- Service commands and structured diagnostics consumed by Studio.
- Resolved shared-file baseline and active ownership during implementation.

Do not allocate new wire IDs or implement a parallel runtime model in TypeScript.
The shared backend remains validation/compiler authority and the preview must
exercise the same semantics. Unsupported controls remain unavailable until the
backend capability is present.

## Completion Evidence

Record host tests and device results separately. A preview phase-continuity pass
does not prove autonomous playback or STOP2 behavior. Validate failed builds,
legacy packages, and unsupported-capability errors as well as the happy path.
Concrete acceptance cases are in [[Scene_Object_Ownership_Acceptance_Plan]],
under the ownership contract. No implementation or hardware pass is claimed
by this documentation checkpoint.
