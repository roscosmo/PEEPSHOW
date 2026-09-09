# Scene Object Lifetime and Control Contract

Status: ownership direction approved; executable implementation pending.

This contract defines the target model for scene-owned objects. It does not
change the interpretation of existing eggs or advertise a new firmware
capability. The detailed defaults below are the shared design baseline for
OS/GUI review before freezing the executable schema and wire format.

Related:
- [[Authority_and_Invariants]]
- [[Scene_Runtime_and_Interaction_Model]]
- [[Runtime_Logic_State_API_Contract]]
- [[Authoring_Project_Schema_Contract]]
- [[State_Scene_World_Entity_and_Turn_Contract]]
- [[Peep_Studio_Scene_Object_Ownership_Handoff]]

## Ownership

**A scene instance owns its objects. States control objects; they do not own them.**

```text
Package
  Scene instance
    Objects: identity, mutable properties, animation playback
    Groups: named collections of object references
    Behaviors: state graphs, event handlers, scoped timers
    Prefab instances: reusable definitions instantiated as objects and behaviors
```

An asset is immutable reusable data. An object is a live instance referencing
that data. Two objects using the same asset have distinct identity and playback.
An object's identity remains stable for the lifetime of its scene instance.
Changing a menu selection or logical state does not recreate scene objects,
reload their defaults, or restart their animations.

The initial implementation can allocate all declared objects in fixed-capacity
storage at scene activation. This contract does not require dynamic creation,
a heap, a world/collision engine, or a thread per object. Object identity and
render-element identity must not be assumed permanently interchangeable: a
future entity can produce zero, one, or several render elements.

## Properties and State Control

Each object has authored defaults and mutable instance properties. Scene
activation initializes those properties once. Bounded actions change the
instance properties, which persist until another action changes them or the
scene instance is destroyed.

States may additionally declare temporary property overrides. These are not
copies of the object. The effective property is the active override, when one
exists, otherwise the object's current mutable property.

Examples:
- A pet animation placed on the scene continues while menu selection changes.
- A selected-menu-state override moves or shows a marker while that state is active.
- A scene timer action can change a label without selecting a synthetic state.
- A persistent action can swap a pet's animation without moving it to another group.

An action executed under an override changes the underlying instance property;
the override still controls the effective presentation until removed. Leaving
a state removes only its overrides, revealing the current underlying values,
not resetting the entire object to authored defaults.

Transactions have a deterministic order: evaluate guards against the current
snapshot, execute the ordered action list, change logical state if requested,
replace the exiting/entering states' override sets, then publish one resulting
render snapshot. Existing package/scene replacement and audio-lifetime rules
remain separate; this contract does not extend package audio outside its package.

The first increment must reject simultaneously active overrides that write the
same object property from different controllers. Do not resolve these conflicts
using editor order, hash order, or whichever thread happens to run first.
Ordered actions within one transaction may write the same property; the last
action in that declared order wins. Hierarchical override priority is deferred
until explicitly specified.

## Animation Continuity

Playback belongs to the object, not the currently selected state or a generated
per-state render-model ID.

| Operation | Target behavior |
|---|---|
| Unrelated state/selection change | Preserve clip, phase, and remaining phase time |
| Position or visibility change | Preserve playback |
| Assign the already-selected clip | Preserve playback; assignment is idempotent |
| Assign a different clip | Start that object's new clip at its declared initial phase |
| Explicit restart | Restart only the addressed object's clip |
| Explicit pause/resume | Freeze/resume the addressed playback position |
| Destroy/recreate scene instance | Reinitialize objects from scene defaults |

These operation names describe semantics, not currently allocated API opcodes.

Hidden objects retain logical playback by default. The runtime may derive their
current phase from elapsed time when shown, without rendering hidden frames or
waking the CPU solely to advance invisible animation. Visibility is not pause.

A static visual override masks the object's underlying animation without
restarting it. Removing the override reveals its then-current phase. Temporary
overrides with their own animated playback tracks are outside the first
increment; they require an explicit lifecycle before being exposed.

Independent logical playback does not promise arbitrary independent hardware
cadences. The compiler/runtime still compose a bounded shared display schedule.
Target limits on phase quantum, combined steps, retained display data, and
autonomous playback remain in force. A compiler must reject an unsupported
combination or select an explicitly supported deterministic fallback. It must
not silently restart other objects to make a schedule fit.

Awake rendering and autonomous playback must consume equivalent object playback
state. Rebuilding a render snapshot or changing backend is not a playback reset.

## Scene Lifecycle and Timers

STOP2 is not scene suspension: admitted autonomous animation and RTC-backed
timer behavior continue under the existing contracts.

Temporary package/scene suspension preserves object values and pauses their
scene-active playback time, consistent with paused scene/state duration timers.
Resume does not recreate the scene. System calendar time remains unaffected.
Destroying a scene instance discards its object state; any persistence across
recreation requires a declared package/save-state mapping.

Timer ownership remains explicit: state activation, scene instance, prefab/
behavior instance, or package session as supported by the executable profile.
Scene-owned timers survive selection changes. Their handlers may target scene
objects without needing a state transition. This target does not imply that
all timer scopes or state-independent object actions are already implemented.

## Groups and Prefabs

Groups are optional named sets of object references. They are not owners, render
layers, independent coordinate spaces, or implicit animation controllers.
The first increment uses declared membership and bounded bulk operations with
stable object order. An object may belong to multiple groups; a single bulk
operation must not apply twice to the same object through overlapping membership.
Conflicting writes follow the transaction rules above.

Prefab definitions are reusable source data. Each placed instance receives
distinct object IDs, local behavior state, and supported instance-owned timers.
Sharing an asset or prefab definition must not alias mutable playback or state.
Compile-time expansion into bounded scene records is sufficient initially; a
runtime allocator and arbitrary spawning are not prerequisites.

## Current Implementation Boundary

As inspected at main commit `8823b83572cf80c60d0fc401bac5397ca30c5a68`:

- Authoring already defines one scene placement surface with state overrides.
- `compiler.py::_package_scene` clones that base render model for each state.
- `project.py::_waiting_visual_for_state_edit` creates state-specific waiting
  presentation identities when separating animation edits.
- `PS_SceneRuntime_StageActions` stages changes against a destination state's
  visual binding, not an independent shared scene-object instance table.
- Preview and firmware preserve compatible presentation timing, but changed
  presentation identity intentionally causes a rebase.

Consequently, existing "base placement" means shared starting definitions, not
the complete live-object semantics above. In the investigated menu egg, three
states had identical base sprite frames and timing but different waiting
presentation identities. Selection changes therefore restarted the sprite.
Sharing those identities preserved phase in a host reproduction, but changing
IDs alone would not implement persistent object mutation or independent playback.

## Compatibility and Acceptance

Existing eggs retain their declared executable semantics. Adopt an explicit
version/capability discriminator for the new model; exact identifiers and binary
layout must be agreed before implementation. Do not reinterpret old actions or
waiting records silently. Unsupported packages must be rejected before install
is advertised as runnable, with a useful authoring error and functioning shell.

Legacy source migration must be explicit and previewable. Source already using
base placement should keep object IDs. Per-state visual differences must be
classified as temporary overrides, persistent actions, or intentional playback
changes; ambiguous behavior must be reported, not guessed. Preserve legacy
compilation until the new profile is supported end to end.

Required acceptance cases:
- Menu movement preserves a base animation's phase and remaining time awake and across STOP2.
- Object mutations survive unrelated state transitions; override exit reveals the current underlying value.
- Same-clip assignment preserves playback; explicit restart affects only its target.
- Hidden playback, scene suspension/resume, and scene recreation follow their distinct lifecycle rules.
- Independent scene-timer actions change objects without synthetic state transitions.
- Two prefab instances sharing assets do not share mutable state.
- Group operations are bounded, ordered, and do not duplicate targets.
- Conflicting overrides and unsupported combined animation schedules produce actionable build errors.
- Compiler, host preview, loader validation, and device execution agree; legacy eggs retain compatibility.

These are acceptance requirements, not recorded test passes. Implement and
validate in increments without raising memory, clock, or autonomous-display
budgets implicitly.
