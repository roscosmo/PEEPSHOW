# Package Blob Format Contract

This document defines the conceptual PeepOS package blob format produced by tooling and consumed by the installer, package manager, runtime hosts, and digital twin.

The package blob format is game/content-facing. It must not encode RTOS objects, HAL handles, filesystem paths, flash addresses, SRAM4 addresses, DMA descriptors, LPBAM descriptors, SPI payloads, or hardware row formats.

---

## Name And Version

The conceptual container name is `PeepPkg`.

The canonical filename extension for an installable PeepPkg blob is `.egg`.
The extension is user-facing identity only; the package is admitted by its
validated container header, schema versions, bounds, and integrity metadata,
not by its filename. Editable authoring projects remain `.peepproj`
directories and must never be accepted by the installer as package blobs.

Initial binary format examples use the magic/version family `PKG1`.

`PKG1` is a package-container format marker, not a package API version. Individual schemas and chunk formats still carry their own versions.

FW0 USB staging bring-up may use `PKG1` as a minimum-envelope read-only validator after MSC reclaim. That check only proves the staged file begins with the expected package magic; it does not freeze or validate the concrete binary header layout, chunk table, CRC/checksum, compatibility schema, signatures, or install commit policy.

The first concrete host-tool layout is frozen in
`schemas/package/PeepPkg_V1_Binary_Layout.md`. It defines the deterministic
header, chunk table, typed STATE chunks, CRCs, and integrity footer. Firmware
consumption and install commit remain unproven until separately implemented
and validated on target.

---

## Container Shape

```text
PeepPkg:
  package_header
  manifest_chunk
  chunk_table
  chunks[]
  integrity_footer
```

The blob must be deterministic from identical inputs.

Runtime code must not scan directories or parse editor-native source files. It resolves assets, schemas, and runtime data through the package manifest, asset table, and chunk table.

---

## Package Header

Conceptual fields:

```text
package_header:
  magic
  container_format_version
  header_size
  package_id
  package_version
  package_size_bytes
  manifest_chunk_id
  chunk_table_offset
  chunk_count
  alignment
  package_flags
  header_crc
```

Rules:

- `magic` must match the supported container family.
- `container_format_version` must be explicitly accepted by firmware/tooling.
- `package_size_bytes` must match the staged blob size.
- offsets and sizes must be bounded and inside the package blob.
- alignment must be explicit and compatible with the installed raw storage reader.
- header fields must not contain host paths or hardware addresses.

---

## Manifest Chunk

The manifest chunk contains the normalized package manifest described in [[Package_Contract]].

It includes:

- package identity and version
- build profile and target profile
- entry scene and scene table
- required and optional capabilities
- package power policy
- asset table reference
- save schema reference
- message schema reference
- compatibility constraints
- package checksum reference

The manifest is the authority for package admission. Runtime code may request less than the manifest declares, but it must not request undeclared capabilities or undeclared assets.

---

## Chunk Table

Every package payload entry is represented by a chunk table entry.

Conceptual fields:

```text
chunk_entry:
  chunk_id
  chunk_type
  format_version
  byte_offset
  byte_size
  alignment
  crc32
  required_capability
  scene_refs[]
  load_policy
  flags
```

Rules:

- chunk IDs are stable within the package.
- tooling may use symbolic IDs; runtime may use compact numeric indexes after validation.
- `chunk_type` must be known or explicitly skippable.
- `format_version` must be accepted for the selected runtime/profile.
- chunk byte ranges must not overlap unless a future schema explicitly allows shared data.
- chunk ranges must remain inside the package blob.
- every chunk has corruption-detection metadata.
- unknown required chunks reject the package.
- unknown optional chunks may be ignored only when the manifest declares a valid fallback.

---

## Stable IDs

Authoring and package manifests use stable symbolic asset IDs.

Firmware/runtime implementations may map those IDs to compact numeric indexes during validation or install.

Rules:

- duplicate symbolic IDs are invalid.
- unresolved asset references are invalid.
- asset IDs must not be filesystem paths.
- renaming source files must not silently change stable package IDs unless tooling reports it.

---

## Required Chunk Types

Initial chunk types:

| Chunk Type | Purpose |
|---|---|
| `manifest` | normalized package manifest |
| `asset_table` | asset IDs, metadata, and chunk references |
| `scene_table` | scene records, types, entries, transitions, and budgets |
| `time_power_profile` | calendar requirements, schedules, reactive waits, interaction policy, wake intents, cadence hints, and catch-up policy |
| `state_graph` | bounded state-scene logic, graph, event, action, and state data |
| `world_table` | bounded world descriptors, camera/turn policy, map references, and world budgets |
| `entity_definition_table` | immutable entity defaults, property schemas, tags, collision, inventory, visual, and behavior references |
| `entity_instance_table` | stable initial instances, world positions, and bounded property overrides |
| `world_behavior_table` | reusable bounded entity behaviors, collections, and deterministic turn iteration records |
| `scene_result_schema` | fixed-schema bounded results returned by pushed sequence/program scenes |
| `sequence_scene` | bounded realtime tracks, markers, FPS, and scene-end/inactive routes |
| `program_scene` | bounded sandbox program data and instruction/memory/frame budgets |
| `input_map` | logical input bindings and focus scopes |
| `audio_profile` | symbolic cue tables, BBB pattern/melody metadata, audio contexts, and timeline markers |
| `sensor_profile` | PeepOS sensor contexts, event interests, step sessions, and wake intents |
| `save_schema` | save records, package-owned settings, defaults, migration policy, reset policy, and write budget |
| `communication_profile` | session contexts, roles, message schema references, rate limits, and routing behavior |
| `message_schema` | communication message types, payload schemas, limits, and compatibility policy |
| `diagnostics_profile` | package marker, counter, timing, warning, and package fault code metadata |
| `masked_1bpp_sprite_bank` | crisp masked 1bpp sprite/image data |
| `tone5_sprite_bank` | tone5 masked sprite/image data |
| `tileset_bank` | bounded tile graphics and tile metadata |
| `tilemap` | compact map layers, regions, collision, and data tables |
| `animation_table` | frame references, timing, loop policy, and bounds |
| `font_bank` | package fonts and text layout metadata |
| `text_table` | localized/string table data |
| `audio_bank` | music/SFX audio payloads such as bounded ADPCM blocks |
| `bbb_pattern_bank` | bounded BBB tone/gap/sweep/repeat pattern data, including compiled RTTTL melody output |
| `waiting_visual_sequence` | portable bounded 1bpp waiting-visual sequence |
| `data_table` | bounded generic package data |
| `compat_report` | tooling compatibility report retained for diagnostics |

Chunk type names are conceptual. Final binary IDs live in the schema files.

---

## Time And Power Profile Chunk

The time/power chunk records portable package intent.

Conceptual fields:

```text
time_power_profile:
  calendar_requirements
  schedule_table_ref
  lifecycle_policy
  wake_intents[]
  catch_up_policy
  reactive_wait_table_ref
  interaction_policy:
    meaningful_activity_sources[]
    inactive_route             # preserve_scene, transition_to_scene, exit_to_shell
    inactive_target_scene
    inactive_waiting_visual_ref
    bounded_deferral_table_ref
  realtime_policy_ref
```

Rules:

- PeepOS inactivity handling is mandatory and cannot be disabled by this chunk.
- this chunk does not contain a package-authored inactivity timeout or physical activation gesture; those are target/system policy.
- interaction policy must resolve one admitted inactive route and every deferral must be statically bounded.
- activation-gesture consumption and `DEVICE_INACTIVE` / `DEVICE_ACTIVE` ordering are Engine invariants, not package options.
- reactive waits carry visual/event/schedule intent only and contain no STOP, LPBAM, DMA, SRAM4, clock, or wake-pin data.

---

## Asset Table Chunk

The asset table maps stable asset IDs to chunk entries and runtime metadata.

Conceptual fields:

```text
asset_record:
  asset_id
  asset_type
  chunk_id
  format_version
  byte_size
  bounds
  required_capability
  optional_capability_fallback
  scene_refs[]
  memory_budget
  decode_budget
  checksum
```

Rules:

- every runtime asset reference must resolve through the asset table.
- asset records must declare bounds before runtime use.
- asset records must declare capability requirements.
- asset records must declare memory/decode budget where applicable.
- assets must not reference editor-native source paths.

---

## Rendering Chunk Requirements

Rendering chunks must follow [[Rendering_API_Contract]].

Rules:

- `masked_1bpp_sprite_bank` stores black/white pixels with opacity masks.
- `tone5_sprite_bank` stores semantic tone5 content and opacity/ownership metadata.
- `tileset_bank` declares tile size, pixel model, allowed scale, and layout.
- `tilemap` declares dimensions, layers, collision/data tables, viewport assumptions, and tileset references.
- world/entity chunks reference tilemaps and visual assets by validated chunk or
  asset IDs; they contain no source-format paths, runtime strings, or pointers.
- `animation_table` declares frame references, frame timing, loop policy, and bounds.
- `font_bank` declares glyph metrics, supported codepoints or string-table bindings, and layout limits.
- no rendering chunk may encode panel-native framebuffer addresses, physical LCD row numbers, SRAM4 addresses, SPI bytes, DMA descriptors, or LPBAM descriptors.

### Masked-1bpp Sprite Bank V1

The first package-backed STATE slice uses one portable logical format:

```text
masked_1bpp_sprite_record:
  asset_id
  frame_id
  width
  height
  row_stride_bytes
  pivot_x
  pivot_y
  pixel_offset
  pixel_size
  mask_offset
  mask_size
  flags
```

Rules:

- logical pixel value `1` is black and `0` is white.
- logical mask value `1` owns/replaces the destination pixel and `0` is
  transparent.
- rows are stored top to bottom; bits within each byte are MSB first.
- `row_stride_bytes` is explicit and padding bits outside `width` are zero.
- an `OPAQUE` flag may omit mask bytes and means every in-bounds pixel is owned.
- pixel and mask payload offsets are relative to their containing sprite-bank
  chunk and must pass bounds and alignment validation before use.
- records are ordered deterministically by stable asset ID and frame ID before
  compact indexes are assigned.
- package data is logical-canvas data. Firmware may cache or transpose it, but
  the `.egg` never stores panel-native orientation or transfer payloads.

`animation_table` records for this slice contain an animation ID, ordered frame
indexes, positive integer frame durations, loop policy, and declared bounds.
Host preview and firmware must interpret the same records.

---

## Waiting-Visual Sequence Chunk

`waiting_visual_sequence` is portable package data.

Conceptual fields:

```text
waiting_visual_sequence:
  sequence_id
  logical_width
  logical_height
  pixel_model
  frames[]
  frame_duration_ms[]
  cycle_duration_ms
  loop_policy
  checksum
```

Rules:

- `pixel_model` must be `precomposed_1bpp` for v1 waiting-visual sequences.
- frames are logical PeepOS display content, not Sharp LCD row payloads.
- the chunk must not contain SRAM4 placement, SPI command bytes, LPBAM linked lists, or physical row addressing.
- tooling must validate frame count, total byte size, cadence, cycle duration, loop policy, and target-profile compiler admission.
- one sequence cycle is bounded, but an admitted loop may repeat for the full reactive wait; playback lifetime and wake/exit behavior belong to the state reactive-wait policy, not this asset.
- reduced-sequence and hold fallback bindings belong to package reactive-wait tables, not the sequence chunk.
- the digital twin may preview the sequence from this portable chunk.
- continued waiting-visual motion while reactive logic is yielded requires `display.waiting_visual_animation`; tools derive this requirement from the preferred/fallback contract.
- Platform may convert or cache the validated sequence as full frames, row deltas, repeated payloads, or another display-owner format.

---

## Compression And Packing

V1 runtime paths do not allow general-purpose compression.

Allowed packing is format-specific and bounded:

- bitplanes
- opacity masks
- tone planes
- fixed-layout tilemaps
- compiled BBB pattern steps from hand-authored patterns or RTTTL melody sources
- IMA ADPCM or other explicitly documented bounded audio payloads
- simple RLE only when the chunk format defines a bounded decoder, maximum expansion size, and decode budget

Rules:

- no unbounded decompression.
- no package-controlled heap allocation for decompression.
- no streaming decompressor whose worst-case time or output size is unknown.
- expansion size must be known before runtime decode.
- packages must remain usable when optional compressed assets fall back to uncompressed or lower-cost alternatives, where declared.
- RTTTL and other melody source text are authoring inputs only. Runtime packages must contain compiled BBB pattern data if the melody is required at runtime.

---

## Integrity Model

V1 integrity requirements:

- header CRC for early rejection of malformed packages
- per-chunk CRC32 or equivalent corruption check
- whole-package checksum over installable payload
- schema version list in the manifest or compatibility report
- optional signature/authentication placeholder for future policy

Rules:

- install-time validation must recompute and verify checksums.
- runtime must not mount or execute packages with failed required chunk integrity.
- failed optional chunks may be ignored only if the manifest declares fallback behavior.
- checksum/signature metadata is not a substitute for schema and bounds validation.

---

## Install And Runtime Loading

Installer flow:

1. host places package blob into USB staging/export storage.
2. `thStorage` reclaims and rescans staging/export after host release.
3. package manager validates the blob, manifest, chunk table, schemas, capabilities, bounds, and checksums.
4. validated package is written to the inactive `5 MiB` installed raw slot and
   read back without modifying the currently indexed slot.
5. the inactive index record is erased, written, and read-verified.
6. the new index commit marker is programmed last; only then is the new package
   generation installed.
7. the previous package slot and index record remain intact until a later
   replacement needs them.

The `.egg` bytes stored in a package slot are identical to the compiler output.
Slot addresses, index records, commit markers, and generations are Platform
metadata and must not be encoded into the package container or exposed to
authoring tools.

Runtime flow:

- runtime loads assets through [[Package_Asset_Loading_API_Contract]] by asset ID.
- runtime reads installed raw package storage or bounded RAM caches only.
- runtime does not read FAT/FileX staging paths.
- runtime does not parse JSON, PNG, Aseprite, Tiled, WAV, or other editor-native files.
- scene activation may load only assets declared for that scene or approved shared assets.

---

## Validation Requirements

Tooling and installer validation must reject:

- unsupported container format
- malformed header or chunk table
- chunk ranges outside the blob
- overlapping chunk ranges
- checksum or CRC mismatch
- duplicate asset IDs
- unresolved asset or scene references
- unknown required chunk types
- unsupported required chunk versions
- asset bounds that exceed target profile limits
- scene using assets not declared for that scene
- missing fallback for optional capabilities
- package requiring animated waiting visuals without `display.waiting_visual_animation` and without an admitted reduced/hold fallback
- interaction-policy record without exactly one admitted inactive route
- package-authored inactivity timeout or physical activation gesture
- inactivity deferral without a static completion bound or timeout
- package artifacts containing host paths, hardware addresses, RTOS symbols, filesystem API requirements, SRAM4 addresses, SPI payloads, DMA descriptors, or LPBAM descriptors
- nondeterministic rebuild output for identical inputs

---

## Compatibility Report

Every package build must produce a compatibility report.

The report schema, status model, required fields, waiver rules, staleness rules, and digital twin use are defined in [[Package_Compatibility_Report_Contract]].

The package blob may include a `compat_report` chunk for diagnostics, but installer validation must not trust that report as proof of package safety. Firmware install validation must still re-check package integrity, schema compatibility, scene type compatibility, and required capability compatibility.

---

Related:

- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[Package_Compatibility_Report_Contract]]
- [[Package_Contract]]
- [[Runtime_Logic_State_API_Contract]]
- [[Rendering_API_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Storage_and_Installer_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
