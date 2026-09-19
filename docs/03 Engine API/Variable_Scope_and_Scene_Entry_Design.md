# Variable Scope and Scene Entry Design

Status: agreed design, 2026-09-19. Not an implemented schema, command, package
format or advertised runtime capability. Existing exports retain their semantics.

This supersedes the special Remembered State entry-node proposal in
[[Scene_Memory_and_Parallel_Logic_Design]]. Remembering is authored data and entry
logic, not an implicit engine mode. Full scene resume and parallel regions remain
separate future work. No state-entry action lists are introduced.

Authority: [[Authority_and_Invariants]], [[Runtime_Logic_State_API_Contract]],
[[Scene_Object_Lifetime_and_Control_Contract]] and
[[Time_And_Power_Intent_API_Contract]].

## Variable Lifetimes

| Scope | Initialisation | Retained across | End of lifetime |
|---|---|---|---|
| Scene | Declared initial values on each fresh scene entry | Local state changes, STOP2 and shell pause/resume | Destruction/replacement of that scene instance |
| Package | Declared initial values when a package session starts | Scene replacement, local state changes, STOP2 and shell pause/resume | Package restart, reset or session destruction |

Resume is not a new session. An explicit Restart is analogous to restarting the
game: package and scene values initialise again. Hardware reset also begins a
new game session; retained system time does not imply retained game variables.
Shipment/power loss does not preserve these values. Durable save data is a
different future contract, never an implicit property of package scope.

A scene can access its own variables and package variables only. Cross-scene
communication uses package scope, not references to another scene's private
variables. Scope-qualified stable IDs determine references; display names and
editor layout do not. No implicit name shadowing or scope fallback is permitted.

Initial types are booleans and bounded integers. Integers declare minimum,
maximum and initial value; invalid ranges, types and initial values fail build.
Supported operations are set, increment/decrement by, and reset to the declared
initial value. Arithmetic clamps to the declared range without machine overflow;
booleans use boolean assignment/reset, not integer arithmetic. A constant set
outside its declared bounds is invalid authoring. Expression-based assignment
and its type/range rules are not implicitly added by this design.

No global reset-variable command is needed. Restart is a separate lifecycle
operation, not an ordinary action that clears a table during a transition.

## Explicit Entry Path

Each scene still has one entry root. Fresh entry follows a visible bounded path:

`Scene Entry -> actions -> ordered condition branches -> actions -> state`

Actions may appear before a decision, on its selected branch, and after branches
merge. Each decision chooses its first true branch in authored order, or its
mandatory default. Canvas position is never branch priority. Later checks and
actions see earlier staged variable writes on the selected path. Only one path
executes; merges do not mean parallel execution or executing sibling actions.

Every possible path must terminate in exactly one local state. Cycles, waits,
unresolved destinations and recursive scene navigation from entry are outside
this increment. Shared validation reports incomplete paths before export.
The selected state's existing overrides apply normally; reaching it does not
execute a hidden state-entry action list. Existing transition actions remain
the actions authored on those transitions.

An existing scene without an entry graph continues directly to its declared
entry state. Do not silently rewrite existing projects. Scene-entry actions run
on fresh entry, not local state changes or shell resume. Existing state-entry
timer bindings are not state-entry action lists and retain their own semantics.

Variable actions and admitted SFX are intended entry-path uses. The eventual
capability must enumerate the exact allowed actions; existing transition action
support does not automatically authorise every action on entry. Object/timer
action ordering at entry requires specification before those actions are exposed.

## Scene Replacement Transaction

This is an internal consistency safeguard, not a normal author-facing failed
entry workflow. Valid exported scenes should enter; predictable budget and
content failures must be reported during build/export.

1. Evaluate the outgoing guard against committed source values.
2. Stage the supported outgoing actions and package writes privately, in order.
3. Initialise destination scene variables and its fresh instance defaults.
4. Execute its entry path, reading the staged package values, and select a state.
5. Validate the complete destination and resource requirements, then commit the
   scene and package values together. Emit admitted external effects after commit.

If preparation fails, retain the source scene and committed package values. Do
not emit exit/entry sounds or partially apply variables. An entry-path reset
followed by a condition tests the reset value; it does not test an old snapshot.
Effect requests preserve selected-path order, but multiple SFX requests do not
imply sequential audio playback. Post-commit peripheral failure follows existing
owner recovery; the transaction does not promise to reverse emitted audio.

This does not widen today's SFX-only scene-exit action subset. Package-variable
exit writes require the coordinated implementation and explicit advertisement.

## Author Example: Menu Selection

Declare package integer `main_menu_selection`, initial 0, with a bounded range.
Selection transitions explicitly set it. The menu entry path checks it and
chooses a corresponding local state, with a visible default branch. No variable
is secretly created or updated by navigation. The same model supports progression
flags and shared game stats without inventing another remembering mode.

Objects, animation and relative timers still initialise freshly on scene entry.
Preserving their execution would require full scene resume. While shell-paused,
relative timers preserve remaining duration; world time continues and calendar
delivery follows the existing suspension/coalescing rules.

## Bounded Storage and Validation

The compiler knows all variable declarations and computes package-session and
scene-instance storage. Package variables have bounded session RAM allocation,
not permanent RAM across reset. Scene storage can be reused after destruction.
Budgets must also include simultaneous source/candidate storage and staged writes
needed for rollback. No runtime heap, hidden retained scene cache or allocation
based on visited states is allowed.

Before encoding, specify integer representation, variable/node/branch/action
limits, worst-case entry work, scope-qualified IDs and reference-safe deletion.
Publish those limits through shared profiles rather than GUI-local estimates.
Host preview and firmware must share ordering, lifetime and clamping semantics.

## Delivery and Acceptance

Implement coordinated increments, not an interim remembered-selection feature:

1. Define source records, stable references, commands, limits and capability
   distinctions for variables and entry graphs. Existing support remains unchanged.
2. Implement shared validation, persisted editing/undo/redo and host preview.
3. Implement package encoding, bounded firmware storage and atomic entry execution.
4. Verify normal Studio export/install, lifecycle behavior and STOP2; advertise
   each supported subset with its actual validation status.

Required cases include:
- Package values survive A/B scene visits; scene values reset on fresh entry.
- Both scopes survive shell pause/resume; Restart resets both.
- Local transitions preserve scene values and do not execute the entry path.
- First-match/default selection, pre-branch writes and merged actions agree in
  preview/runtime; each selected action executes once.
- Selection restoration uses explicit package data, not retained state IDs.
- Integer limits clamp arithmetic; invalid source values/types/references fail.
- Mixed supported/unsupported entry actions, cycles, missing fallbacks and
  over-budget paths fail export rather than creating partial execution.
- Failed destination admission leaves source/package values and SFX untouched.
- Save/reopen/undo/redo preserve scope, IDs, graph order and layout; display-name
  changes do not change references or execution.
- World time continues through pause independently of variable lifetimes.

No new API names or wire identifiers are allocated by this document.
