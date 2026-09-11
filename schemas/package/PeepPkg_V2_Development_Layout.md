# PeepPkg V2 Development Layout

Status: host encoder/parser and restricted firmware installation/activation;
NOT a shipping capability or universal device acceptance claim. Authoring API 42
enables ordinary build/export and `parse_egg` for a conservative subset of the
restricted firmware profile. Studio integration remains pending. See
[[Peep_Studio_Restricted_V2_Export_Handoff]] for capabilities and readiness.
The explicit development encoder remains broader for hardware/negative fixtures;
its output is not automatically eligible for ordinary export.
Use `build_development_egg_v2` / `parse_development_egg_v2` for host fixtures;
the C development entry is `PS_EggStateLoader_DecodeDevelopmentScene`.
Authority: [[Scene_Object_Executable_Design]], [[Package_Blob_Format_Contract]].

## Isolated Firmware Profile Preflight

`PS_EggStateLoader_ValidateV2Profile` is a thRuntime-only, non-publishing check,
not an installer entry point. It reuses the candidate loader's whole-package
integrity and every-scene descriptor checks, then requires:

- One scene, execution model 2, continuous interaction.
- A fully resident package within the existing 65,536-byte bridge capacity.
  This is not the product package-size limit.
- Local input and scoped timer bindings, guards and variables, scene-owned
  object operations, variable writes and start/restart/cancel timer actions.
- No audio chunks, cross-scene transitions or exit-to-shell actions.

The result distinguishes argument/capacity/integrity failures from unsupported
scene count, model, interaction, audio, event, scene target or action. It retains
the loader reason for malformed packages and a zero-based offending item index
for binding/transition/action rejection. No candidate pointer survives return;
active catalogs, scene banks and active probes are unchanged.
Callers must use the returned status and profile result: the separate candidate
loader probe describes structural decoding, which can succeed while the profile
rejects otherwise valid but unsupported content.

**A profile pass does not authorize installation, export or STOP2.** It does not
compose display frames or prove LPBAM budgets across mutable object states.
The pure loader `ValidatePackage` API still rejects V2. The API 42 host export
subset adds conservative graph/timing/payload checks. The owner-level `PS_HW6_RTOS_ValidatePackage`
now dispatches V2 to the restricted profile plus exact display admission below.
The storage follow-up now implements the single-active-slot transaction journal
and ignores legacy A/B installation records. Its native interruption tests pass;
device install, PLAY, same-slot reinstallation and normal reboot passed with the
321,480-byte embedded V1 artifact (generations 2 then 4). Physical power-cut and
broader V2 profiles remain unproven on hardware. The 2196-byte GUI installation
fixture subsequently passed V2 boot, reinstall/PLAY, A/B animation continuity
and sleep accounting; see [[V2_Installed_Package_Test_Runbook]]. This is not
general V2 or shipping acceptance.
See [[Storage_and_Installer_Contract]].

Native coverage: `test_firmware_v2_profile.py` runs the real C loader against
the GUI numbered/timer fixture and OS structured, dual-animation and scoped-timer
variants. It checks legacy-format rejection, audio and shell-action rejection,
timeout/multiscene rejection, malformed non-entry scenes, digest/header/chunk
failures, object/control corruption, and the inclusive 64 KiB boundary. Every
candidate is exercised with both a live V1 package and a live V2 object bank;
successful profile checks still fail ordinary install validation. These are
host-native checks, not installed-package hardware acceptance.

## Isolated Candidate Display Admission

The next implemented boundary checks a **single frozen object snapshot and its
complete repeating animation program**, not every reachable package state.
The restricted installation increment connects this check to installation and
boot loading. API 42 separately advertises the conservative host export subset;
this exact device check remains mandatory.

1. `thRuntime` calls `PS_EggStateLoader_DecodeV2Candidate` into private descriptor
   and catalog outputs. This applies the same profile/integrity checks above,
   clears outputs on failure, and never publishes the active package catalog.
   Unlike the result-only validator, successful outputs deliberately borrow
   immutable candidate bytes. The caller must keep the candidate storage leased
   until all consumers finish, including a timed-out display request that has
   not acknowledged completion. No FileX/runtime FAT reads are introduced.
2. From a private candidate object bank, `PS_ObjectWaiting_Build` constructs the
   pointer-free combined program. Incompatible timing or more than 12 combined
   steps rejects before raster work. No approximation or discarded frames.
3. `thDisplay` calls `PS_ObjectDisplay_CheckWaiting` with that program, the
   explicit candidate catalog and a dedicated caller-owned workspace. It
   projects and composes each full frame, including static objects, and checks
   every transition including the wrap back to the first frame.
4. The result separates projection, raster and payload failures, gives the
   failing target step and number of composed frames, and reports exact sequence,
   chunk, wire-byte and allocated-payload-byte use. Success has payload status
   and reason zero. The checker does not present, mark ready, build/start DMA,
   change clocks, or replace active display data.

`DisplayRenderer_CopyCandidateSceneFrame` uses only the supplied catalog and
never substitutes an active-package sprite with the same ID. Its synchronous
thDisplay-only scope restores the framebuffer, rotation and asset resolver on
success or failure. `PS_LpbamDisplay_CheckFullSceneAnimation` uses separate
ordinary-RAM frames and payload storage, shared production row/wire helpers,
and exact byte comparisons for same-band payload reuse. All production slots
have equal capacity, so private slot numbering does not affect the budget.
Limits remain **12 steps, 18 chunks, 10,512 payload bytes**. An unchanged step
uses the same band-zero refresh as the full-scene production packer.

The workspace is allocated deterministically outside owner thread stacks;
it must not alias live framebuffers, payloads or descriptors. The development
owner-queue integration below now reserves private ordinary-RAM scratch for
the descriptor, graph, schedule and exact display check. No SRAM4 arena or
production payload capacity is changed.
Only validated immutable catalog views are accepted inputs; they are not a
second parser for untrusted package bytes.

**Do not cache this result as admission for the entire package.** Visibility,
position, frame masks, state overrides and object actions can change composed
bands, combined timing and resource use. Initial admission and each subsequent
candidate transaction need to check their actual resulting snapshot before
publication. An entry-state pass alone is not permission to label an installed
package fully supported. Timer/variable/graph reachability is not exhaustively
enumerated by this component.

Native coverage in `test_firmware_object_display_admission.py` compares exact
results with the existing production packer for structured/dual fixtures, both
A/B states and both timer visibility states. It exercises same-ID/different-pixel
active and candidate catalogs, missing candidate graphics, static hold, payload
deduplication, exact 18-chunk/10,512-byte capacity, chunk/step overflow and a
composition failure at wrap. Active catalog, object bank, framebuffer and all
live payload/compiler storage remain unchanged. The structured fixture reports
4/8/4,672 steps/chunks/bytes; dual reports 8/16/9,344. These are native checks;
the separate owner-queue hardware result is recorded below.

### Development Owner-Queue Integration

`PS_HW6_RTOS_CandidateService` runs in the existing bounded runtime service.
It checks the immutable embedded development egg, not the installed blob.
`thRuntime` validates and constructs a private entry-state program, then sends
a four-word `qDisplayCmd` envelope: candidate magic `0x43414e32`, display owner,
monotonic nonzero request token, bitwise-complement token. Tokens do not wrap.
The display owner checks the matching lease, performs exact raster/payload
admission, restores its clock request, and publishes the completion token after
its final candidate access. Debug event-group bit 10 is the candidate ACK;
it is separate from render, storage-validation and clock acknowledgements.

- Queue send is nonblocking. Failure before ownership transfer releases the
  private lease and reports failure without drawing or installing anything.
- The acknowledgement wait uses the existing owner ACK timeout. A timeout or
  unrelated/stale notification does **not** release or overwrite the candidate.
  Status remains NOT_RUN while ownership is unresolved; `wait_status` records
  the wait failure. Later ordinary runtime service releases only a matching
  completed token. No busy loop, hidden retry or second concurrent candidate.
- A late rejection releases memory exactly like a late success. An ACK alone,
  an old message, or a duplicate message cannot authorize reuse. Borrowed blob,
  descriptor and catalog pointers are cleared on release.
- Pending requests and leases inhibit automatic and controlled STOP2 admission.
  Other owners retain their normal responsibilities. At execution, display busy
  or autonomous prearmed/active state returns status 2 without raster work or
  changing its clock intent. No abort/replacement of live presentation is used
  to make the check succeed.
- The wrapper uses existing runtime reactive and display transfer clock intents;
  the pure checker itself still has no clock/peripheral operations. No clock
  profile, tuning value, queue allocation or thread topology is changed.

The `ps_hw6_object_candidate` probe is independently versioned at API 1. Existing
render/runtime probe layouts and Studio/service capabilities are unchanged.
The standalone diagnostic still borrows linked immutable ROM. Restricted
installation checks use a dedicated fixed 64 KiB copy, retained through the
same completion boundary even after timeout. Neither path borrows a mutable
transport buffer after returning to its caller.

Hardware sequence:

1. Launch the existing dual-animation development scene from HOME/shell with
   `__fw0_object_scene_lpbam_enable.gdb`. Observe its normal cadence first.
2. Wake normally with A/B, halt, and source `__fw0_object_candidate_enable.gdb`.
   Resume for two seconds, then halt and source `__fw0_object_candidate_prints.gdb`.
   If already asleep when requested, wake normally to let runtime consume it.
3. Expect profile/graph/schedule/queue/wait/display/status all zero, request token
   equal to display completion, leased zero, all four clock statuses zero,
   8 steps at 400 ms, 9 composed frames (including wrap), 16 chunks/9,344 bytes.
4. Source `__fw0_object_candidate_reject_enable.gdb`, resume and print again.
   This deliberately sets **only the private candidate catalog's frame count**
   to zero after valid decoding. Expect profile/graph/schedule zero but
   display/raster/status 1 and payload reason BUILD (5), completed token and
   released lease. It tests raster rejection, not a malformed exported egg or
   a resource-overflow package. It must never borrow active sprites to pass.
5. After both cases, check that A/B still moves the marker once, neither
   animation restarts, the timer reveal stays correct, and normal low-power
   playback returns. No new panel content is expected from the checker itself.

Native queue coverage runs the real candidate decoder, graph, scheduler and
renderer with deterministic ThreadX queue/event stubs. It checks normal and
immediate completion, queue-full failure, timeout reservation, refusal while
leased, late success/rejection, stale notification/message, duplicate delivery,
token exhaustion, busy display, clock failures and structural rejection before
enqueue. It compares active object/catalog/framebuffer state around candidate
success and rejection. This is not hardware timing or installed-package proof.

### Candidate Owner-Queue Hardware Pass (2026-09-11)

Initial acceptance and rejection checks succeeded from the shell. The user then
launched the dual-animation scene with `__fw0_object_scene_lpbam_enable.gdb`,
repeated both checks, and confirmed "yes animations as before". The candidate
helper itself does not launch or replace a scene.

- Normal request 1: profile/graph/schedule and queue/wait/display/status all zero;
  8 steps at 400 ms, 9 frames composed, 16 chunks and 9,344 payload bytes.
- Missing-sprite request 2: profile/graph/schedule zero, raster/display/status 1;
  failed step 0, zero frames composed, payload status/reason 1/5 (BUILD), and
  zero chunks/bytes. This is the deliberately injected private-catalog rejection.
- Both requests completed with matching tokens, lease zero, all four runtime
  and display clock request/release statuses zero, and no refusals or late
  completions. These counters show completed checker work, not just dispatch.

This passes normal candidate acceptance and deliberate raster rejection through
the real owner queues with observed animation continuity. No new current,
precise cadence or STOP2 residency measurement accompanied this capture; the
earlier dual-animation sleep/wake evidence remains separate. Timeout/stale-token
behaviour remains native-tested, not hardware fault-injected. This does not
enable ordinary V2 export, installation, replacement or reboot loading.

### Shared-Path Hardware Regression Pass (2026-09-11)

After the candidate-checker changes, the user repeated the existing dual-animation
development LPBAM test and confirmed unchanged behaviour. The capture reported:

- Enabled/active/development = 1/1/1; fault and launch status = 0/0.
- Combined schedule = 8 steps at 400 ms; publish count/status = 8/0.
- Five WFI returns; sleep measured/reconciled = 5/5, status 0, total reconciled
  missing time 12,129 ms. Wake snapshot/render/map/resume statuses all zero.
- Timer due/applied/error = 1/1/0, with one RTC selection; reveal visible = 1.
- Display request/complete = 8/8, result/fault = 0/0; sleep barrier = 0.

This closes the shared renderer/wire-packer regression check, combining observed
behaviour with completed display and sleep/wake work. Current payload/commit
status was NOT_RUN after redraw, so this capture does not independently report
the prepared chunk/byte totals. No new current or precise cadence measurement
was supplied. It does not validate candidate admission through hardware queues,
installation, or reboot loading; those remain separate work.

## Restricted Installed V2 Increment (Hardware Pending)

The owner-level package validation entry selects the restricted V2 checker for
header version 2. Scan and install both require full integrity/profile checks,
entry graph initialization, complete-cycle scheduling and exact display-owner
raster/payload admission before the storage owner can publish VALID or erase.
The existing single-slot journal is unchanged; its format is independent of
the egg's version. V1 validation and installation remain supported.

PLAY and boot resolve the installed resident bytes (source 3), repeat the V2
profile and exact entry admission, then activate the object runtime. They do not
substitute the linked ROM fixture. Missing admission callback, non-installed
source, partial residency or any rejection fails closed to package error/shell.
Successful activation initializes the existing reconciled object clock, scoped
timers and autonomous LPBAM publication instead of the legacy state renderer.

Before committing each input/timer transaction, the runtime checks its actual
staged object bank through the same owner queue. Failure aborts object, state,
variable and action changes. Existing error handling remains: input rejection
reports an error without committing; timer dispatch failure returns to shell.
There is no automatic retry or claim that entry admission proves all states fit.

`PS_HW6_RTOS_InstalledObjectCheck` copies at most 65,536 package bytes into fixed
ordinary RAM and validates its own immutable catalog. The bank is consumed
synchronously into the pointer-free waiting program before enqueue. A timeout
retains this copy and all private workspaces until matching display completion;
it never commits a late transaction or installs after a timed-out check. Further
checks refuse while leased. The wrapper restores an enclosing runtime clock
claim after candidate work. No new thread, queue, heap, SRAM4 capacity or clock
configuration is introduced. Integrity is currently rechecked per transaction;
hardware responsiveness still needs verification before optimizing that work.

Native coverage includes exact GUI fixture entry through `EnterStateScene`,
missing-callback rejection, input rollback, phase preservation, fresh entry,
private-buffer survival after timeout and source reuse, plus the existing
candidate resource and integrity rejection tests. These are not USB or physical
boot results. See [[V2_Installed_Package_Test_Runbook]] for the hardware sequence.

## Container

All integers are little-endian. Container magic remains `PKG1`, header version
is **2**. The 64-byte header, 40-byte directory entries, alignment, CRCs and
`END1` integrity footer retain V1 layouts. Package/chunk flags and capability
hashes must be zero in this development revision. An old reader must reject
header version 2 before interpreting scenes. API 39 and the device target
profile are unchanged; source validity does not grant device/export support.

## Scene Discrimination

The scene-table payload is `SCN2`, version 2, with the existing 12-byte header
and 20-byte `<8HI` record. Directory format version remains 1, as with the
existing versioned graph/render payloads. Record fields are:

`scene_id:u16, display_name:u16, scene_type:u16=1, entry_state:u16,
graph_chunk:u16, visual_chunk:u16, control_chunk:u16,
execution_model:u16, reserved:u32=0`.

- Model 1: legacy graph/render/wait chunks (types 4/5/6), unchanged semantics.
- Model 2: graph/object/control chunks (types 4/13/14).
- Scene IDs are unique; every scene owns three distinct chunks, none shared or
  orphaned. At least one model-2 scene is required in a V2 development package.
- String, manifest, sprite/animation and audio chunks retain V1 encodings.

## Objects (Type 13)

Header `<4s6H`: `OBJ2`, payload version 1, header size 16, object count,
record size 20, two zero reserved fields.

Record `<HBBBBHHhhHHH` (20 bytes): string-table object ID, kind, layer, z-order,
flags, width, height, default X, default Y, fallback frame index, clip index,
zero reserved. Kind uses existing retained values 1..8; layer is 0..2.
Flags: visible=1, up-right line=2, focus metadata=4; other bits invalid.
Absent frame/clip is `0xffff`. References index the package asset/animation
tables, not strings. IDs are unique within the scene; defaults fit whole bounds.
Sprite frames/looping clip frames must match object dimensions. Primitives
cannot reference frames/clips. Round bounds are odd, >=3; circles are square.

## Controls (Type 14)

Header `<4s8H`: `OCT2`, payload version 1, header size 20, state count,
override count, object-operation count, range size 4, override size 16,
operation size 16. Arrays follow contiguously in that order, with no tail.

State range `<HH`: first override, count, indexed by graph state order.
Ranges partition the override array in order. Each state controls an object
at most once. Override `<HHiiHH`: object index, property mask, X, Y, frame
index, visibility. Mask bits X=1/Y=2/frame=4/visible=8. A nonzero known mask
is mandatory. Omitted coordinates/visibility are zero, omitted frame is
`0xffff`. Visible is 0/1. Present positions and frames pass object-bound checks.

Operation `<BBHiiHH`: opcode, axis mask, object index, X/dx, Y/dy, frame
index, visibility. Opcodes: 1=set-position, 2=move-by, 3=set-visibility,
4=set-frame, 5=clear-frame. Position opcodes require axis mask 1/2/3;
others require zero. Unused coordinates/visibility must be zero, frame must
be `0xffff` except for set-frame. Position operands are signed int32, not
pre-clamped: runtime clamps after each ordered write. Relative positive dy
means up, absolute positive Y means down. Frame operations require a sprite.

## Object Graph Revision

Model-2 graph payload uses `STG1` revision **7**. It keeps revision-6 record
sizes, bindings, routes, guards, timers and policies. The old state render/wait
indexes and default-waiting index are all `0xffff`; object controls supply the
presentation. Legacy element-operation kinds 3..6 are forbidden.

Graph operation kind **12** references one control operation. In existing
`<BBHHHi` layout: kind=12, modifier=0, control-operation index, 0, 0, 0.
References follow control-operation order and cover every control operation
exactly once. This preserves interleaving with variable, timer, render and
audio actions. Direct scene replacement remains SFX-only.

Common graph ranges must partition their source/guard/operation arrays; guards
and records require zero reserved fields. IDs, references, bounds and types are
validated independently of the source encoder. No decoded scene is returned
until the complete package passes validation.

## Development Limits and Tests

Structural host ceilings: 32 objects, 64 states, 128 routes plus 16 handlers,
32 variables, 32 input bindings plus 16 timers, 8 guards/actions per route,
2048 overrides and 1152 control operations per scene. These are format test
bounds, NOT admitted HW6 object-bank or LPBAM capacities. Ordinary API 42 export
enforces the smaller limits in [[Peep_Studio_Restricted_V2_Export_Handoff]];
the broad explicit development encoder/parser retain these structural bounds.

Fixtures cover deterministic encoding, mixed legacy/object scenes, explicit
version rejection, all object operations, timer handlers, asset/audio reuse,
invalid masks/references/counts/reserved fields and recomputed-integrity
corruption. No source JSON or host preview annotations are stored in the blob.

## C Record Decoder

`firmware/peepshow_hw6_fw0/Core/Src/ps_egg_object_decoder.c` implements bounded
OBJ2/OCT2 decoding without allocation or global state. It reads unaligned bytes
explicitly and returns typed definitions/overrides/operations through getters
over an immutable borrowed view. Any decode failure clears that view.

The parent loader must still validate container integrity, shared catalogs and
pixel payloads, SCN2/STG1-7 cross-references and HW6 admission. The record decoder
checks referenced stable IDs, dimensions and looping clip frame/duration ranges;
it does not replace those whole-package checks. Restricted installed loading now
uses this decoder through the parent loader and owner-level admission described
above. The low-level decoder alone does not authorize launch.

Native tests compare decoded fields and accept/reject results with the Python
record reader, including unaligned payloads, signed-int32 extrema, every
object/control byte with three mutation masks, every truncated prefix, sparse
override masks, primitives and wire capacity boundaries. No hardware pass is
claimed by this stage.
