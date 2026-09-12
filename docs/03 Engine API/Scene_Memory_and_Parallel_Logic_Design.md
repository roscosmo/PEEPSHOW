# Scene Memory and Parallel Logic Design

Status: design direction agreed on 2026-09-12; not a schema, command, wire-format
or firmware capability increment. No new hardware result is recorded here.

Baseline: main `9fd07bbea8bad19f91ea87cf3480b7daa970be15` and the GUI scene
connection reconciliation supplied by the user. Existing restricted V2 export
remains governed by [[Peep_Studio_Restricted_V2_Export_Handoff]].

Related:
- [[Scene_Object_Lifetime_and_Control_Contract]]
- [[Runtime_Logic_State_API_Contract]]
- [[Scene_Object_Executable_Design]]
- [[Peep_Studio_Scene_Object_Ownership_Handoff]]

## Purpose and Delivery Boundary

Authors must be able to express independent behavior without code or duplicated
timer routes in every menu state. Scene memory, independent event handlers and
parallel state machines solve different problems and must remain explicit.

| Concept | Meaning | Delivery boundary |
|---|---|---|
| Scene timer handler | Executes independently of the selected local state | Existing backend primitives; GUI can integrate advertised commands |
| Remember selection | Recreate a scene, then enter its remembered local state | Agreed design; not implemented by this document |
| Resume scene | Retain and resume the scene instance | Future scene-navigation capability; not implied by existing shell resume |
| Parallel regions | Multiple independently active state machines in one scene | Agreed design direction; execution details still to be specified |

Neither disconnected graph placement nor a service API version increase enables
these features. Studio must use delivered hello/per-scene capabilities and
shared validation. Multi-scene export remains blocked at this baseline.

## Scene Entry and Memory

### Fresh Entry

Fresh replacement initializes destination objects, animation, variables and
timers and enters its default state. This remains distinct from remembering or
resuming. Each scene currently has one default `entry_state`; named destination
entries and per-connection state selection are not existing capabilities.

A named scene exit is one shared semantic connection displayed in Scene Flow
and Local Logic. Creating it does not invent a trigger. Go To cards are visual
aliases, not extra scene instances or retained snapshots.

### Remember Selection

Remember the destination scene's own last active state, not the state of the
other scene from which navigation occurred. Returning recreates the scene from
its authored defaults and enters that remembered state afresh.

This remembers selection only:
- Object mutations, animation phase and scene variables are not retained.
- The selected state's overrides are applied to the new scene instance.
- Scene-entry timers start for the new scene instance.
- State-entry timers start afresh for the selected state.
- Existing one-shot expiry records are not inherited from the old instance.

The agreed graph direction is an explicit **Remembered State** entry node with
a visible fallback to the default state. It represents a dynamic destination,
not a new entry port or generated route on every state. First use follows the
fallback. An explicit fresh-entry choice follows normal entry instead.

Preview should show the resolved state and whether fallback was used. An author
must not need to implement selection memory through hidden variables or code.
The eventual contract must specify invalid remembered-state handling, memory
update/reset points and reference-safe behavior when states are deleted.

### Resume Scene

Resume preserves a scene instance rather than reconstructing it from defaults.
Its retained state includes active local state(s), variables, mutable objects,
animation phase and remaining scene/state timers. Asset data remains immutable
and reusable; retaining an instance does not duplicate ownership of assets.

Returning to a retained instance is not fresh state entry. It must not reapply
defaults, replay entry behavior or restart state timers implicitly. Suspended
scene-active time pauses; STOP2 is not suspension and continues to use existing
autonomous playback and timer accounting.

This is not a promise to retain or replay audio. Package audio currently stops
and is discarded on shell entry; any navigation-related audio policy requires
its own supported lifecycle.

Remembered selection and scene resume do not imply persistence across reboot
or power loss. Persistent save data needs a separately declared contract.

Before exposing resume, define retained-instance capacity, navigation ownership,
whether nesting/call-return is supported, missing-instance behavior, explicit
discard/reset and what happens when capacity is exhausted. There must be no
unbounded scene stack, hidden allocation or silent eviction policy.

## Independent Timer Nodes

An independent timer is a scene-owned event binding and its handler. It is not
a synthetic state and requires no incoming edge from every local state.

Studio can expose a dedicated timer node with:
- Delay and supported start policy.
- Guards and an ordered action list.
- An optional local destination state.

With no destination, the handler changes objects or variables without state
re-entry. With a destination, it performs the supported local transition.
The editor must distinguish an action-only path from a state-transition path.
It must not expose every input-trigger option merely because state nodes do.

Existing backend primitives are `time.scene_elapsed`, `event_binding.*`,
`event_handler.*`, `object_actions.set` and Start/Restart/Cancel timer actions.
Create and delete each binding/handler pair atomically using the existing batch
and reference-protection rules. This is not permission to invent new commands.

Current timers are one-shot. A handler can explicitly restart its scene timer
to express recurrence; this is not a dedicated fixed-cadence periodic mode.
If a handler guard prevents its actions, a restart in those actions also does
not execute. Studio must not hide that behavior behind an unconditional
"repeat" promise. A future periodic mode needs explicit cadence, suspension,
missed-expiry and bounded catch-up semantics before it is advertised.

Scene timers survive local state changes. They stop with destruction of their
scene instance. Repeated logical work can cause CPU wakes; it is not cosmetic
LPBAM animation and does not create a private hardware timer or worker thread.

## Parallel State Regions

The agreed model is named state regions within a scene, each with its own entry
and exactly one active state while the scene is running. The existing graph is
the single-region case; legacy content must not be reinterpreted implicitly.

Example:

| Region | States |
|---|---|
| Navigation | Home, Inventory, Settings |
| Pet behavior | Idle, Eating, Sleeping |
| Challenge | Waiting, Running, Complete |

Changing Navigation must not re-enter Pet behavior or restart its state timers.
All regions refer to the scene's objects and variables; regions do not create
duplicate object instances. Object groups remain non-owning collections, not
parallel state machines.

These are logical regions, not RTOS threads. Their execution remains bounded,
deterministic and owned by the existing runtime. A disconnected state in the
editor is not automatically another active region.

### Agreed Constraints

- Transition destinations identify their region and state. Ordinary local
  transitions affect their own region; changing another region is explicit.
- A state timer belongs to one region's state activation. Leaving that state
  cancels its timers without cancelling other regions' or scene-owned timers.
- Scene handlers remain independent of region selection.
- Different active regions may control different properties of the same object.
  Simultaneously active overrides of the same property are rejected, even when
  their values agree. Editor position/order is never an implicit priority.
- Ordered actions within one transaction retain existing underlying-value
  semantics. Multi-region event ordering must be defined before implementation.
- Region changes do not restart unrelated object animation. All effective
  objects still share the existing bounded display/LPBAM admission budget.
- Remember selection can later retain the state IDs of explicitly selected
  regions. Full resume retains all active regions as part of the scene instance.

### Decisions Required Before Execution Support

The current single-state transaction contract does not decide these questions:

1. Whether an input/event targets one region or is explicitly broadcast, and how
   competing routes consume it.
2. Whether handlers/regions evaluate guards against a shared pre-event snapshot
   or results of earlier transactions, and the stable ordering of shared writes.
3. Whether one event can transition multiple regions atomically, including how
   admission failure rolls back mutations and external effects.
4. Priority between simultaneous timer expiries, local transitions and scene
   replacement; stale source events and held input must not leak into a new scene.
5. Maximum regions, active timers, transitions per event and retained instances,
   with actionable build errors rather than arbitrary resource expansion.
6. Initialization order, explicit cross-region destinations, and interaction
   with future hierarchy and prefabs. Hierarchical override precedence remains
   deferred; do not introduce "deepest state wins" through regions.

These are open design details, not authorization to choose incidental container
iteration order, add polling loops or silently change existing event dispatch.

## Acceptance Cases and Implementation Order

The following are requirements, not newly recorded test passes:

| Case | Required result |
|---|---|
| Scene timer fires while selection changes | Actions run without duplicated routes or unwanted state entry |
| Timer handler with a local destination | Ordered actions and destination commit with existing transaction semantics |
| Recurrence with a guarded restart | Preview shows when recurrence stops; no hidden unconditional restart |
| First remembered visit | Default fallback is used visibly |
| Remembered return after object/variable edits | Selection restored; other instance values initialized fresh |
| Full resume | Values, phases and remaining times retained; no implicit entry replay |
| Navigation changes while Pet behavior waits | Pet state and deadline remain unchanged |
| Conflicting active-region overrides | Shared validation rejects the conflict with both owners identified |
| One event writes shared data through multiple regions | Host and device follow the same specified ordering |
| Rejected destination or over-budget composition | No partially committed state/object change or abandoned usable source |
| Reboot | No selection/instance persistence implied without a supported save contract |

Delivery order:
1. GUI integrates existing independent timer-handler commands and preview;
   backend remains the shared source of validation and action semantics.
2. OS and GUI settle memory entry presentation and the open region execution
   rules before freezing new source/service contracts.
3. OS delivers explicit, bounded backend/runtime increments and advertises each
   capability; GUI integrates only those delivered subsets.
4. Validate representative authored fixtures in host tests and on hardware,
   including timer lifetime, state/animation continuity and STOP2 behavior.

Scene connections, remembered selection, full resume and parallel regions must
not be advertised as one indivisible capability. Existing restricted export and
passed installation fixtures remain unchanged until an explicit expansion.
