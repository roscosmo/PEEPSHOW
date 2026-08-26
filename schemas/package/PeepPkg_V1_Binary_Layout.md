# PeepPkg V1 Binary Layout

This file freezes the first concrete `PKG1` container used by `.egg` packages.
All integers are little-endian. All offsets are package-relative. Reserved
fields must be zero when emitted and ignored only when the accepted format
version permits it.

## Container

```text
package_header[64]
chunk_table[chunk_count * 40]
alignment padding
chunks[]
alignment padding
integrity_footer[40]
```

Container alignment is four bytes. Padding bytes are zero.

### Header

| Offset | Type | Field |
|---:|---:|---|
| 0 | char[4] | `PKG1` |
| 4 | u16 | container format version, `1` |
| 6 | u16 | header size, `64` |
| 8 | u32 | complete package size |
| 12 | u32 | chunk-table offset, `64` |
| 16 | u32 | integrity-footer offset |
| 20 | u16 | chunk count |
| 22 | u16 | chunk-entry size, `40` |
| 24 | u16 | manifest chunk index |
| 26 | u16 | container alignment, `4` |
| 28 | u32 | package flags |
| 32 | u32 | reserved |
| 36 | u64 | FNV-1a hash of package ID |
| 44 | u32 | CRC32 of the 64-byte header with this field zero |
| 48 | byte[16] | reserved |

### Chunk Entry

| Offset | Type | Field |
|---:|---:|---|
| 0 | u16 | chunk type |
| 2 | u16 | chunk format version |
| 4 | u32 | flags |
| 8 | u64 | FNV-1a hash of stable chunk ID |
| 16 | u32 | payload offset |
| 20 | u32 | payload size |
| 24 | u32 | payload CRC32 |
| 28 | u16 | payload alignment |
| 30 | u16 | reserved |
| 32 | u64 | required-capability hash, zero when none |

V1 chunk types are `MANIFEST=1`, `STRING_TABLE=2`, `SCENE_TABLE=3`,
`STATE_GRAPH=4`, `RENDER_MODELS=5`, and `WAITING_VISUALS=6`.

### Integrity Footer

The footer is `END1`, version `1`, footer size `40`, followed by SHA-256 of
every byte before the footer. Header and chunk CRCs are checked before any
compiled records are exposed.

## String Table

`STR1`, version, string count, byte-payload size, then `count + 1` u32 offsets
and concatenated UTF-8 bytes. Strings are unique and lexicographically sorted.
Other chunks use u16 string indexes.

## Manifest

`MAN1` contains package ID, display name, target profile, semantic version,
entry scene, scene count, and flags. Text fields are string-table indexes.

## Scene Table

`SCN1` contains fixed records for scene ID, display name, scene type, entry
state index, and the chunk indexes for its graph, render models, and waiting
visuals. V1 emits `STATE_SCENE` only.

## STATE Graph

`STG1` contains fixed bounded arrays for variables, symbolic input actions,
states, routes, route source-state indexes, guards, operations, wait event
interests, and meaningful-activity actions. References are table indexes, not
pointers. Route order is authored priority order and therefore remains stable.

The container remains `PKG1` version 1 while the `STG1` chunk has its own
record version:

- `STG1` version 1 routes contain one local target-state index.
- `STG1` version 2 routes contain exactly one local target-state index or one
  target-scene string index. The unused target is `0xffff`.
- a version 2 scene target names another STATE scene in the same package and
  carries no scene-local operations in the initial direct-replacement subset.

Readers must continue to accept version 1 local routes. Current compilers emit
version 2.

## Render Models

`RND1` contains retained render-model records followed by bounded element
records. Elements carry symbolic visual string indexes and logical geometry.
They do not carry framebuffer, DMA, or hardware-display data.

## Waiting Visuals

`WAI1` contains waiting-visual records, element records, phase visual string
indexes, and combined-step phase indexes. The package stores portable visual
intent. Platform remains responsible for target admission and hardware payload
generation.

## Determinism

- scene chunks are ordered by stable scene ID
- strings are unique and sorted
- authored arrays with semantic priority retain source order
- all padding and reserved fields are zero
- identical validated semantic input produces identical package bytes

## HW6 Embedded Vertical-Slice Profile

Before external-flash installation is connected, FW0 embeds compiler output as
an immutable byte array and runs the same container checks before decoding it.
This is a transport substitution only: the bytes remain a complete `.egg` and
the firmware does not use a parallel C scene descriptor.

The first target profile deliberately accepts one entry `STATE_SCENE` and the
current fixed runtime capacities. It validates the header CRC, package SHA-256,
chunk bounds and CRCs, manifest, scene references, graph records, retained
render records, and waiting-visual records before exposing the decoded scene to
`thRuntime`. Unsupported records reject package admission; they are not partly
executed or silently approximated.

The current render adapter admits the named `cursor_outline`, `marker_outline`,
and `diamond` primitives only. Cursor and marker phase references must use the
corresponding V1 names emitted by the example project. A `request_render`
operation is consumed by the atomic scene-transition commit because that commit
already schedules the retained render model; variable mutations remain explicit
runtime actions.
