# PeepPkg V2 Development Layout

Status: host encoder/parser and explicit C development decoder/graph increments;
NOT a shipping capability or device acceptance claim. Normal build/export,
`parse_egg` and production firmware installation continue to reject V2.
Use `build_development_egg_v2` / `parse_development_egg_v2` for host fixtures;
the C development entry is `PS_EggStateLoader_DecodeDevelopmentScene`.
Authority: [[Scene_Object_Executable_Design]], [[Package_Blob_Format_Contract]].

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
