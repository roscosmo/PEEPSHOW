# PeepPkg V2 Development Layout

Status: host encoder/parser and explicit C development decoder/graph increments;
NOT a shipping capability or device acceptance claim. Normal build/export,
`parse_egg` and production firmware installation continue to reject V2.
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
The production `ValidatePackage` path still rejects V2; service capabilities and
ordinary export remain unchanged. Further increments must connect candidate
display admission, installed activation and reboot loading, and reconcile the
FW0 A/B installer with the authoritative single-active-slot transaction contract
before claiming product installation support. This increment changes neither
flash layout nor storage behaviour. See [[Storage_and_Installer_Contract]].

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
It is not yet connected to installation, boot loading or a GUI capability.

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

The workspace must be allocated deterministically outside owner thread stacks;
it must not alias live framebuffers, payloads or descriptors. This increment
provides the caller-owned workspace types, not an additional runtime allocation
or an asynchronous request/lease protocol. Owner-queue integration is pending.
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
the new candidate path has not yet been exercised through hardware owner queues.

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
bounds, NOT admitted HW6 object-bank or LPBAM capacities. Firmware admission
and target-profile limits must be implemented before normal export is enabled.

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
it does not replace those whole-package checks. It is compiled into the build
source list but not connected to production loading. V2 remains rejected there.

Native tests compare decoded fields and accept/reject results with the Python
record reader, including unaligned payloads, signed-int32 extrema, every
object/control byte with three mutation masks, every truncated prefix, sparse
override masks, primitives and wire capacity boundaries. No hardware pass is
claimed by this stage.
