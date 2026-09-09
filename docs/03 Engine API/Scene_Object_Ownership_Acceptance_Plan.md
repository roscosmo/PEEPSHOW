# Scene Object Ownership Acceptance Plan

Status: acceptance specification with host/native subsets implemented; device
ownership and awake/STOP2 handoff acceptance remain NOT RUN. Incremental results
are recorded in [[Peep_Studio_Scene_Object_Ownership_Handoff]].

Authority: [[Scene_Object_Lifetime_and_Control_Contract]]. Coordination:
[[Peep_Studio_Scene_Object_Ownership_Handoff]]. This plan defines expected
behavior, not executable helper commands, wire IDs, or implemented features.

## Scope and Fixtures

Use bounded, deterministic fixtures shared by compiler/loader tests, host
preview, and device tests. Retain the exact source, built egg, target profile,
firmware/host commits, and relevant capability versions with every result.

The base fixture has one scene, menu states A/B/C, one marker, and two distinct
sprite objects P/Q referencing the same immutable four-frame looping clip.
Frames are visibly numbered 0/1/2/3 with a 250 ms duration per frame. Both clips
start on frame 0 at scene-active time zero. P has authored position (10,20).
States move the marker but do not control P/Q unless the case explicitly says so.
Use target-valid placement and frame data within the existing display budget.

For exact host expectations, the intervals are [0,250), [250,500), [500,750),
[750,1000), then repeat. At t=375 ms, playback is frame 1 with 125 ms remaining.
Times below refer to scene-active time except where a suspension test explicitly
uses wall time. Frame numbers are test labels, not executable enum allocations.
Each case starts from a fresh fixture unless it explicitly continues a sequence.

Host and byte-backed native fixtures now exercise subsets of these semantics.
The device fixture with visibly numbered frames and complete runtime/display
integration remains to be delivered. Do not treat native frame-index assertions
as evidence that a panel animated or that firmware dispatched a scene timer.

## Increment 1: Ownership and Continuity

### O01: Selection Does Not Restart Playback

At t=375 ms, transition A to B; then transition B to C at t=425 ms. The marker
changes as authored. P/Q remain frame 1 with respectively 125 ms and 75 ms
remaining, and reach frame 2 at t=500 ms. Object IDs and instance counts do not
change. Repeated render-snapshot publication must not reload defaults.

Evidence: host exact-time assertions and device frame/deadline observations.
Seeing a nonzero animation counter is not sufficient: the correct phase and
deadline must survive both transitions.

### O02: Persistent Position Beneath Temporary Overrides

P starts with underlying X=10. State B temporarily overrides X=100. While B is
active, apply relative X movement +5, then +7 in one ordered action list.
Underlying X becomes 22, displayed X stays 100, and Y stays 20. Exit to A:
displayed X becomes 22. Transition to C without an override: X remains 22.
Recreate the scene: X returns to authored 10.

The move must not use displayed X=100 as its starting value. Earlier ordered
actions must feed later relative actions; neither action independently reads 10.

### O03: Property-Level Override Removal

B overrides only P visibility to false. Move underlying P to (30,40) while B is
active; P stays hidden. Leaving B shows P at (30,40), not (10,20). Add a separate
case with only X overridden: Y changes remain visible while X remains masked.
Removing one controller must not clear a different property's valid override.

### O04: Add to Selected States Means One Object

Author one base-hidden object visible in A and C only. Visit A/B/C/A. The same
object is visible/hidden/visible/visible respectively, with no duplicate IDs or
allocation per state. A persistent property change made in A remains in C.
Compiled/source-derived ownership information must describe one object.

### O05: Ordered Transition Publication

Start in B with underlying X=10 and override X=100. A route moves X by +5 and
transitions to C, whose override is X=120. The committed result has underlying
X=15, effective X=120, and state C. No intermediate frame with X=15 is published
between removing B's override and applying C's override. Guards read the
pre-transaction snapshot. A rejected guard changes neither object nor state.

### O06: Conflicting Controllers

Build a reachable parent/child or parallel-controller configuration where two
active overrides set P.X. Reject the new-model build, including equal-value
overrides; identify the scene, object, property, and conflicting controllers.
Do not use "deepest state wins." A controls-X/B-controls-Y case is valid when
simultaneously active and the controller topology is supported. Mutually exclusive
states may both override X. Unsupported hierarchy itself must be rejected, not
accepted merely because properties do not conflict. These restrictions must not
silently change a retained legacy fixture's behavior.

### O07: Hidden and Statically Masked Playback

Hide P at t=375 ms and show it at t=875 ms. It reappears on frame 3 with 125 ms
remaining, not frame 1 or frame 0. Q remains unaffected. Repeat using a static
frame override instead of visibility: the override is shown while active, then
its removal reveals the same current underlying playback position.

Also test with every animated object hidden: invisible playback alone must not
cause periodic CPU rendering or wakes. Logical time can be reconstructed when
shown. Other admitted wake causes must be recorded separately.

### O08: Scene Suspension Versus Recreation

At scene-active t=375 ms, suspend the scene for 1000 ms of wall time. Resume:
P/Q remain frame 1 with 125 ms remaining; persistent object values are unchanged.
The next phase occurs after 125 ms of resumed scene-active time. Calendar time
continues advancing. Separately destroy/recreate the scene: defaults and initial
playback return. Do not treat resume as scene activation.

### O09: Scene Timer Without a State Transition

Arm a scene-owned one-shot duration timer for 5000 ms and change menu selections
before expiry. Its handler changes one underlying object property without a
transition target. It fires once, preserves the current logical state and other
objects' playback, and does not rearm merely because selection changed. If an
override masks the target property, verify the changed underlying value on exit.
Repeat across STOP2 using the existing RTC-backed timer path.

### O10: Awake/Autonomous Continuity

Repeat O01 around awake-to-autonomous and autonomous-to-awake handoffs. STOP2
does not pause scene-active playback. After 1000 ms of actual elapsed playback,
the four-frame clip has advanced one full cycle without restarting its epoch.
Observe intermediate phases as well; the same frame one cycle later alone
could be a frozen display. Change selection after a movement wake and verify
the correct remaining time, not just a correct-looking first frame.

Use the actual admitted backend/profile. An awake-only result cannot be recorded
as an autonomous pass. Do not change target budgets or enable a periodic CPU wake
just to make the fixture appear animated.

## Increment 2: Explicit Playback Actions

### P01: Idempotent Assignment and Target-Only Restart

At t=375 ms, assign P its already-selected clip. P/Q remain frame 1 with 125 ms
remaining. Explicitly restart P at t=400 ms: P becomes frame 0 with 250 ms
remaining, while Q stays frame 1 with 100 ms remaining. At t=500 ms, P is still
frame 0 with 150 ms remaining and Q becomes frame 2. These are exact host logical
time expectations. A shared-schedule limitation must produce a supported fallback
or actionable rejection, not reset Q silently; record any target restriction
before exposing the action/profile combination as supported.

### P02: Different Clip and Pause/Resume

Assign a different target-valid clip to P: only P starts that clip's declared
initial phase. In a separate run, pause P at t=375 ms and resume after 1000 ms
of elapsed running scene time. P resumes frame 1 with 125 ms remaining while Q
has continued normally. A visibility toggle does not remove the explicit pause.

## Increment 3: Groups and Prefabs

### G01: Group Operations

Declare groups G={P,Q} and H={Q}. A single bulk relative move +5 targeting their
union moves P and Q by +5, not Q by +10. Two separately ordered actions, one per
group, may intentionally move Q twice. Membership and execution order are
deterministic and bounded. Grouping itself does not reset playback, reparent
objects into a coordinate space, or change visibility.

### G02: Distinct Prefab Instances

Instantiate the same definition twice. Corresponding objects have distinct IDs
and mutable values despite sharing assets. Moving or restarting one instance
does not change the other. Test instance-owned timers only when that separate
capability exists; otherwise the compiler must report unsupported timer scope,
not silently attach both instances to one scene timer.

## Compatibility and Validation: Every Increment

| Case | Expected result |
|---|---|
| C01: Legacy destination-binding actions | Same observable legacy behavior before/after the backend change; no automatic persistent-object conversion |
| C02: Legacy different animations per state | Legacy compilation remains available; requested new-model conversion requires explicit supported choices or reports why it is unavailable |
| C03: Ambiguous animation migration | No asset-based identity merging, discarded tracks, or automatic restart-policy guesses; no successful export claiming an incomplete conversion |
| C04: Unsupported executable capability | Shared validator and firmware loader reject before runnable-install success; the shell remains usable |
| C05: Target budget exceeded | Structured error naming the incompatible scene/objects and target limit; no hidden memory/clock increase or reset of other objects |
| C06: Draft preview versus export | Incomplete projects remain editable/previewable where currently supported; `build_issues` block invalid export without destroying source |
| C07: Stable baseline features | Existing placement commands, later service API behavior, diagonal lines and filled circle/ellipse backend support survive compatibility integration |
| C08: Stable addressing | Unknown object IDs, duplicate instance IDs, invalid clip references, and unsupported action targets are rejected consistently by compiler and loader |

These cases require positive and negative fixtures. Preserve legacy sources and
known-good eggs rather than regenerating every baseline through the new compiler.
Compare observable behavior and decoded semantics; byte-for-byte identity is
required only where the existing format explicitly promises it.

## Recording Results

Every case is initially NOT RUN. Record PASS, FAIL, BLOCKED, or NOT RUN separately
for shared backend/loader, host preview, and device where applicable. Record N/A
only with a reason; later-increment tests cannot justify exposing an unimplemented
capability in an earlier increment.

Include case ID, fixture/source and egg identity, commits, profile/capabilities,
exact actions, expected/observed values, and test method. Record current phase,
remaining phase time, object identity, underlying/effective properties, and
state/timer results as needed. Define any hardware timing tolerance from the
target's actual timer resolution before recording a pass; host tests use exact
deterministic times.

For device tests, report whether debug-in-low-power was enabled. It changes the
power/debug environment; a pass with it enabled is not a power measurement.
Do not count debugger-halted time as real autonomous playback. Prefer bounded
runtime records read after the test, plus visible frame progression and actual
STOP2/backend evidence. Thread scheduling counters alone prove neither drawing
nor timer actions. No new debugger helper names are prescribed by this plan.

The first increment is complete only when O01-O10 and applicable C cases have
recorded evidence, with host-only and device results distinguished. Playback
actions and group/prefab support become available only after their own cases
and capability checks pass.
