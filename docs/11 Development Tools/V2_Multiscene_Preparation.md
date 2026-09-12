# V2 Multi-scene Preparation

Status: private firmware candidate decoding, display-owner admission and
development-only fresh scene replacement
implemented on 2026-09-12. Decoder baseline is
`ac46c0e70e3f7aeab05ad22ae4b987d3e29c6436`, following authoring API 43.
Not multi-scene installation, export permission or a
hardware pass.

## Delivered Boundary

`PS_EggStateLoader_DecodeV2SceneCandidate` validates a resident scene set and
returns a requested scene's descriptor and borrowed sprite catalog. Scene ID 0
selects the package entry; explicit IDs use the existing one-based scene catalog.
Package entry need not be the first sorted scene. This is an internal C API,
not an authoring service command or a debugger helper.

It uses the existing limits: eight catalog scenes and 65536 resident bytes for
the entire egg. No per-scene asset copies, heap, extra scene banks or new resource
budgets are introduced. All scene graphs and descriptors are validated, followed
by every scene's feature profile, before any output descriptor/catalog is exposed.
An invalid non-selected or unreachable scene also rejects the whole candidate.

The feature subset remains V2 objects, continuous interaction, input/timer
bindings and supported object/variable/timer operations. Mixed execution models,
audio chunks, shell-exit/render-request actions and unsupported events remain
rejected. The only broadened feature is an action-free exit to another scene's
default entry, including timer-handler exits. Guards remain valid. Self-targets,
missing destinations and actions attached to an exit are rejected.

Fresh default entry is the only intended replacement semantic. Remembered state,
scene resume, named destination entries and parallel regions are not encoded or
implemented here. See [[Scene_Memory_and_Parallel_Logic_Design]].

## Ownership and Results

Only `thRuntime` may call this API, serialized with existing candidate validation
and HASH work. It reuses the private validation scratch and never publishes the
active loader context or catalog, changes live objects, or schedules effects.
No peripheral, flash, clock, display-transfer or STOP2 policy changes are made.

On success, `ps_egg_v2_profile_result_t.scene_count` gives the graph-validation
bound and `scene_id` identifies the returned scene. On failure, the count and
descriptor/catalog outputs are zeroed; `scene_id` identifies the failing/requested
scene when known, or is zero if failure preceded scene identification. Existing
profile reason, loader reason and zero-based item index conventions are retained.
This additional internal result field is not a probe or package wire change.

Successful descriptors and catalogs borrow immutable candidate bytes. The caller
must keep those bytes reserved through any subsequent display-owner work and its
matching completion. Clearing decoder scratch does not release a caller's lease.
Do not call with active descriptor/catalog storage as output scratch.

A decode success does not prove rasterization, payload fit, autonomous playback
or safe replacement. No display-owner admission is performed by this API.

## What Is Still Blocked

Existing `ValidateV2Profile` and `DecodeV2Candidate` retain the installed
single-scene restriction. Storage preflight, the installed launcher and existing
debugger requests continue using those entrypoints. `ValidatePackage` retains
its legacy-container contract. The new private owner-admission APIs use selected
decoding, but are not yet called by installed preflight or scene replacement.

Authoring service remains API 43. `multi_scene_export` stays false, and the public
`hw6_v2_resident_v1` profile is unchanged. Studio can continue its advertised
connection editing and host preview work; it must not enable export from this
internal firmware increment. The existing installed fixture remains the baseline.

## Verification

`test_firmware_v2_scene_candidate.py` compiles and runs the actual C loader, graph
initialization and sprite-catalog lookup with a live V1 or V2 runtime. It checks:

- Selection of every scene in an eight-scene package, including a package exactly
  at 65536 bytes; scene-count overflow, byte-limit overflow and invalid IDs reject.
- Input and timer exits, non-first package entry and distinct destination defaults.
- Rejection of non-entry malformed objects, forbidden actions, mixed models,
  missing/self destinations, exit actions and bad digest with repaired integrity
  where necessary to reach semantic checks.
- Zeroed failed outputs, cleared private scratch and unchanged live loader,
  descriptor, graph, snapshot and animation phase; success after failed checks.
- Existing installed candidate/profile and legacy package entrypoints still
  reject the newly accepted multi-scene candidates.

These tests use a host HASH oracle; they do not exercise hardware HASH, owner
queues, panel transfer, STOP2 or real scene replacement. No new on-device test
or reflash is requested for this preparation-only checkpoint.

Verification at the decoder checkpoint: **336 authoring tests pass**, including the
four new native scene-set tests; the Debug firmware build, target-profile
freshness check and `git diff --check` pass. The existing production-path stack
analysis passes at 2368 bytes including its 512-byte reserve, leaving 1728 bytes
of the 4096-byte runtime stack. This is static analysis, not a measured stack
high-water mark; future replacement call paths need their own analysis.

## Display-owner Admission Follow-up

`PS_HW6_ObjectCandidate_CheckScene(blob, size, scene_id)` admits the fresh initial
presentation of the requested scene; ID 0 selects package entry. Only `thRuntime`
may call it, serialized with existing candidate/validation work. It copies the
resident egg to the existing private owned buffer, decodes the selected scene,
initializes a private object graph, then builds a pointer-free waiting program.
`thDisplay` receives the existing token-only queue request and performs actual
projection, rasterization and exact payload accounting into private workspaces.
Neither committed framebuffer nor live LPBAM payloads/catalogs are replaced.

`PS_HW6_ObjectCandidate_CheckSceneSet` checks each scene in ascending catalog ID,
stopping on the first failure. It performs at most eight bounded child checks.
Each child validates package integrity/features and admits its initial display;
the batch does not combine different scenes' animation cycles or payload budgets.
It does not prove every possible state/variable/object mutation; runtime staged
transactions will still require their own exact admission.

Both functions return success only after a matching successful display completion
and successful clock-policy cleanup/restoration. They reuse the existing bounded
owner waits and clock requests, not new clock settings or peripheral access.
Busy/prearmed display, queue/clock failures, schedule/raster/payload rejection,
stale acknowledgements and timeout all return failure without changing the source.

After timeout, the private copy, catalog and waiting program remain leased until
the matching display token completes. The caller may release/reuse its original
buffer after return. Another check cannot overwrite leased work. Late completion
only releases the lease: it never commits a scene, retries a failed request or
continues an aborted batch. The batch result is a synchronous caller-owned value,
not an asynchronously updated probe or retained pointer. Its `checked` count is
completed successful scenes, `failed_scene` identifies the failure when known,
and `request_id` identifies its last started child (zero if none).

Candidate probe API is now **2**, adding requested scene, resolved/failed scene,
and validated scene count. Mode 3 identifies private destination admission.
Existing enable helpers deliberately remain single-scene modes 1 and 2; there is
no new debugger launch helper. Candidate and installed-object print helpers have
matching API guards. Use a matching flashed firmware/ELF when next testing; an
updated script alone does not update the running probe layout.

The actual C owner functions, decoder, scheduler, renderer and payload compiler
are exercised by `test_firmware_object_scene_admission.py` with deterministic
queue/clock/HASH substitutes. Five tests cover selected vs entry scene, all-scene
and eight-scene batches, static destinations, later-scene schedule/payload limits,
missing sprites, timeout and reuse of caller bytes, stale/duplicate completion,
refusal while leased, explicit retry, and unchanged live graph, phase, catalog,
framebuffer and LPBAM state. Existing single-scene installation/workflow tests
remain passing. This is real host raster work, not physical display proof.

Verification: **342 authoring tests pass**, Debug firmware build and target-profile
check pass, and `git diff --check` passes. Static stack analysis reports a worst
checked production path of 2408 bytes including the 512-byte reserve, leaving
1688 bytes of runtime headroom. The new private API roots are also analyzed, but
their future replacement caller chain is not yet present and must be added then.
No new hardware test is requested at this preparation checkpoint. Service API 43
and the restricted single-scene export capability remain unchanged.

## Development Fresh Replacement Follow-up

`PS_SceneRuntime_EnterDevelopmentSceneSet` now admits every scene's initial
presentation before publishing the package-wide immutable catalog. It retains
the bounded eight-scene/65536-byte, continuous V2, action-free-exit subset.
The runtime caller must retain immutable package bytes for the whole session.
Installed entry and public export remain single-scene.

On an input or timer exit, `thRuntime` decodes the destination into the inactive
descriptor slot and initializes one file-static staging graph. It then requests
exact display-owner admission for that fresh bank and selected scene. Only a
matching successful completion permits commit. The committed graph, descriptor,
authored variables, entry state, object defaults and playback become the fresh
destination together. The package-wide catalog is shared unchanged; there is no
cross-package replacement, per-scene asset copy, retained history or heap.

Local transactions after replacement also admit against the current scene ID,
not always the package entry. The triggering input is not replayed in the
destination. Returning to a scene recreates its defaults rather than resuming
the previous instance. Exit actions remain forbidden, so no outgoing variable,
object, sound or timer effect can accompany the replacement.

Failed decode, scheduling, raster, queue, clock or completion leaves the source
graph and activation intact. Late completion only releases candidate resources;
it cannot switch scenes. The existing scene-replacement counters record attempts,
failure, source and destination. `ObjectReplacementRejected` identifies the last
attempted event's rejected object exit; it is not an asynchronous completion.

An unsuccessful timer exit leaves an inspectable `rejected_pending` record in
the outgoing timer slot, with `active=0` and its original deadline retained.
It does not repeatedly retry or keep waking the device. Explicit timer
start/restart/cancel resolves that record; state-timer re-entry or successful
scene replacement clears it. Other source timers and input remain usable.
This pending state is not a successful or guard-ignored expiry. Ordinary local
timer error handling is unchanged.

On successful replacement, changed scene activation clears all outgoing timer
slots and arms the destination's own timers. Before first destination display,
the owner resets the animation time origin and consumes only the old instance's
already-accounted STOP2 time. Admission duration and old sleep duration do not
advance the new animation. Frame publication and LPBAM rebuilding still use
existing owner/token and recovery paths. Logical atomicity does not promise to
undo a physical panel/owner failure after commit.

Development probe API **4** adds explicit request modes 3 (awake scene set) and
4 (autonomous scene set). Existing enable helpers retain modes 1/2 and their
single-scene fixture instructions; their version guards now require API 4.
No new hardware run is requested with the currently embedded single-scene egg.
Use a matching new ELF/firmware and dedicated fixture instructions at the next
hardware checkpoint; do not force STOP2 or infer physical playback from dispatch.

Verification uses actual C runtime/loader/graph and display-owner candidate
functions with deterministic host queue/clock/HASH substitutes. It exercises:

- All-scene entry rejection, successful input exit, fresh return, unchanged
  shared catalog, default variables/playback and no input replay.
- Local state/variable changes in a non-entry scene with continuity and exact
  selected-scene admission.
- Missing-sprite, clock request/release, queue and timeout rejection without
  source mutation; refusal while leased and late success without replacement.
- Actual timer scheduler behavior for input/timer exits, fresh deadlines despite
  admission delay, discarded outgoing timers, rejected pending expiry without
  automatic retry, and explicit authored restart.
- Existing single-scene launch, owner handoff, STOP2 timing and installed/export
  restrictions remain covered by the regression suite.

The full suite passes **345 tests**, including development request/helper version
checks. Debug firmware build, target-profile freshness and `git diff --check`
pass. The expanded ARM static stack check includes
development launch, input and timer replacement callback chains: worst checked
path is 2424 bytes including the existing 512-byte reserve, leaving 1672 bytes
of the 4096-byte runtime stack. This is not a measured hardware high-water mark.
Physical multi-scene drawing, fresh-entry timing and STOP2 re-entry remain NOT RUN.

## Labelled HOME/AWAY Hardware Fixture Prepared

While Studio finishes its scene connections, OS supplies a development-only
fixture from `build_object_development.py --scene-exits`. No GUI project is
modified. The linked egg is 3440 bytes, package ID
`dev.peepshow.fresh_scene_exit`, with HOME (scene 1) and AWAY (scene 2). The
generator's internal AWAY scene ID is `visit`. Public multi-scene export remains
disabled; this fixture never replaces the installed package.

The scene label stays at the top, digits 1-2-3-4 advance every 400 ms below it,
and the middle marker has fixed L/R labelled slots. L/R changes local selection
once without restarting digits; selecting the current slot does nothing.
Every fresh scene entry starts at digit 1, marker left, empty bottom slot and
zero local move count. HOME A enters AWAY; AWAY B returns HOME. HOME B and AWAY A
have no authored binding. HOME's bottom slot fills once after two seconds.
AWAY's bottom slot stays empty and its six-second scene timer returns HOME,
independent of L/R changes. Leaving HOME early must cancel its outgoing timer.

Build and flash the matching firmware. Let boot finish, HOLD START for the shell
MENU, halt, then run:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_scene_exits_enable.gdb
continue
```

The helper checks APIs 4/22/2/1, linked size/package identity and shell/lease
conditions before requesting mode 4. The old single-scene awake/LPBAM enable
helpers refuse this linked fixture and point to the new helper. Reset ends the
development session; HOLD START suspends it in the shell, kept awake. Do not
force manual STOP2. A debugger disconnect is not a reset: reconnect without
resetting/reflashing to preserve the test.

Observe local continuity, input exits, automatic six-second return and the
fresh two-second HOME reveal. Include an early HOME exit before its reveal,
then leave controls released so automatic STOP2 and autonomous animation run.
Wake with L/R before halting and print:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_scene_exits_prints.gdb
```

The print reports scene/instance IDs, replacement failures, selected-scene
admission, local move count, timers, wake reconciliation and display completion.
Snapshots are not live DMA frames. Require visible transitions and continuity;
WFI/admission counters alone do not prove low-current physical playback.

Native tests cover this exact fixture through real private owner raster
admission (four steps, 400 ms, eight chunks, 4672 bytes) and nine complete panel
frames across local changes, input exits, cancelled outgoing timers, timed
return and fresh reveal. The production timer scheduler is exercised with a
deterministic host clock. All new print expressions are checked against the ARM
ELF types, including the 64-bit elapsed field.

Subsequent hardware result: **functional PASS, responsiveness unresolved**.
The user confirmed the HOME/AWAY behaviour, but reported an obvious delay from
button press to object movement. Replacement attempts/failures were 3/0;
candidate token/completion 18/18, lease/status 0/0; display request/completion
17/17 with no fault. Both timer expiries applied without errors; six WFI returns
were measured and reconciled. These counters do not measure input latency or
prove low-current residency. See `V2_Object_Input_Latency_Investigation.md`.

Verification: **347 authoring tests pass**, Debug firmware build and generated
target-profile freshness pass. ARM stack analysis remains 2424 bytes including
the 512-byte reserve, with 1672 bytes of runtime headroom. Only generated
development content, helper scripts and host tests changed; production runtime
behavior and service capabilities are unchanged.

## Next OS Work

1. Measure and resolve the reported HOME/AWAY response delay.
2. Repeat with Studio's actual two-scene project when delivered.
3. Only after those results widen and advertise the installed/export subset.
