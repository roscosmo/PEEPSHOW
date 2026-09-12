# V2 Multi-scene Preparation

Status: private firmware candidate decoding and display-owner admission
implemented on 2026-09-12. Decoder baseline is
`ac46c0e70e3f7aeab05ad22ae4b987d3e29c6436`, following authoring API 43.
Not multi-scene installation, runtime replacement, export permission or a
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

## Next OS Work

1. Stage and admit a fresh destination before replacing the usable source;
   commit catalogs, objects, variables, timer ownership and animation timeline
   coherently. Failed admission must leave the source usable with no effects.
2. Prove input/timer exits, fresh return, stale outgoing timer rejection and
   LPBAM wake/re-entry on hardware using a clearly labelled two-scene fixture.
3. Only then widen and advertise the installed/export subset to Studio.
