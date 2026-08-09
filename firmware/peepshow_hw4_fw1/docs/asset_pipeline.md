# Assets Pipeline

Authoritative specification for asset authoring, conversion,
blob packaging, and runtime integration in PeepShow V5.

This document defines how maps, sprites, audio, and metadata
are transformed into installable game blobs and consumed by
the runtime engine.

If implementation differs from this document, the implementation is wrong.

---

## Scope

Defines:
- Authoring tools and expected export formats
- Map metadata schema (Tiled)
- Sprite packing format (2bpp + mask)
- Audio encoding format
- Game blob structure and chunk layout
- Build tooling and versioning rules
- Runtime loading expectations

Does NOT define:
- Runtime behavior semantics (see game_engine.md)
- Storage layout and installation (see storage_and_updates.md)
- Audio playback implementation (see audio.md)

---

## Design Principles

- Runtime must not depend on FileX.
- All gameplay assets must reside in installed blobs.
- Blobs must be self-contained or depend only on system/public resources.
- No dynamic allocation required to parse blobs.
- Blob format must be versioned and forward-safe.
- Asset lookup must be deterministic and bounded.
- Bitmap format must align with renderer world surface (see display_and_rendering.md).

---

## Authoring Tools

### Pixel Art

Tool:
- Aseprite

Export:
- PNG
- Fixed tile sizes (8x8 base grid recommended)
- 4-colour indexed PNG required for world assets
- Transparency preserved (alpha channel)

Color constraints:
- 5 visible shades + transparency
- Fully transparent pixels allowed

---

### Maps

Tool:
- Tiled Map Editor

Export format:
- JSON (recommended)
- Fixed tile size: 8x8
- Explicit layer naming required

Maps must define:

Tile properties:
- solid (bool)
- water (bool)
- slow (bool)
- occluder (bool)
- roof (bool)
- emissive (bool)

Object types:
- Spawn
- Interact
- Exit
- CameraZone
- IndoorZone
- Light

Map semantics are defined in game_engine.md.

---

### Audio

Encoding:
- IMA ADPCM
- Mono
- 16 kHz
- 4-bit blocks

All audio must be pre-encoded offline.

Runtime performs decoding only.

---

## Asset Conversion Stage

A host-side toolchain must:

1. Parse Tiled JSON.
2. Extract:
   - Tile grid
   - Tile property bitflags
   - Object metadata
3. Convert to compact binary structures.
4. Parse PNG sprites and tiles:
   - Quantize to 5 visible shades (+ transparent)
   - Extract transparency mask
   - Convert to 2bpp color plane
   - Convert to 1bpp mask plane
5. Package audio into aligned ADPCM blocks.
6. Emit a structured game blob (.psblob or equivalent).

No runtime JSON parsing is allowed.

---

# Bitmap Encoding Format (Authoritative)

All world-visible bitmaps (tiles, sprites, images) must use:

- 2bpp color plane
- 1bpp mask plane

This matches the renderer world surface format.

---

## Pixel Level Encoding

Colour levels:

- 0 = white
- 1 = light grey
- 2 = mid grey
- 3 = black

Runtime decode for masked 2bpp assets:
- mask=1, level=0 -> white (0/4 black)
- mask=1, level=1 -> light grey (1/4 black)
- mask=1, level=2 -> mid grey (2/4 black)
- mask=0, level=1 -> dark grey (3/4 black, extended tone)
- mask=1, level=3 -> black (4/4 black)
- mask=0, level=0 -> transparent

Reserved combinations (currently treated as transparent):
- mask=0, level=2
- mask=0, level=3

Generator contract:
- Transparent pixels must be encoded as mask=0, level=0.
- Dark-grey (3/4 black) pixels must be encoded as mask=0, level=1.

---

## Bit Packing Rules

### 2bpp Color Plane

Pixels packed 4 per byte:

- bits 7:6 = pixel 0
- bits 5:4 = pixel 1
- bits 3:2 = pixel 2
- bits 1:0 = pixel 3

Stride:
- Must be padded to 4-byte alignment.

---

### 1bpp Mask Plane

Pixels packed 8 per byte:

- bit 7 = pixel 0
- bit 6 = pixel 1
- ...
- bit 0 = pixel 7

Stride:
- Must be padded to 4-byte alignment.

Mask plane must be present for all sprites.
Tiles may omit mask if fully opaque (flag required).

---

## Blob Structure (Authoritative)

A blob represents a self-contained world or campaign.

High-level structure:

- BlobHeader
- ChunkTable
- ChunkData[]

---

### BlobHeader

Must contain:

- magic (fixed identifier)
- format_version
- blob_id
- blob_version
- total_size
- chunk_count
- header_crc

Header must be validated before use.

---

### ChunkTable

Array of fixed-size entries:

Each entry contains:

- chunk_type
- chunk_id
- offset
- size
- flags
- crc32

Chunk table must be located immediately after header.

Chunk entries must be aligned to 4 bytes.

---

### Chunk Types (Defined Set)

Examples:

- MAP_DATA
- TILESET_DATA
- SPRITE_DATA
- AUDIO_BANK
- STRING_TABLE
- SCRIPT_DATA
- METADATA
- GAME_PACKAGE_MANIFEST

Unknown chunk types must be safely ignored.

---

## Game Package Manifest Chunk (V1-V4)

`GAME_PACKAGE_MANIFEST` chunk provides package-owned mode descriptors and pet route
mapping for runtime launch selection.

Binary layout is defined by:

- `Core/Inc/game_package_manifest.h`

Manifest header must include:

- magic/version
- bounded table counts
- table offsets
- `total_size`
- CRC32

Manifest table content:

- mode descriptors:
  - v1: (`mode_id`, `runtime_kind`, `backend_id`)
  - v2/v3: v1 fields plus runtime config payload (scene addresses/sizes,
    controller profile, camera profile, movement/camera tuning)
  - v4: v2/v3 fields plus package reference IDs for map/tileset/audio
- pet routes (`pet_entry_id`, `mode_id`)

Runtime contract:

- parser must be bounded and deterministic
- no dynamic allocation
- invalid manifest must be rejected and system must fall back safely

Host-side generation tool (current):

- `tools/gen_game_package_manifest.py`
- input example: `Assets/game_package/manifest.example.json`
- v2 input example: `Assets/game_package/manifest.v2.example.json`
- output default: `Assets/game_package/manifest.bin`

JSON fields (required):

- `manifest_version` (`u16`, optional; defaults to `4`)
- `package_id` (`u32`)
- `package_version` (`u32`)
- `modes[]` objects with:
  - `mode_id` (`u32`)
  - `runtime_kind` (`u16`)
  - `backend_id` (`u16`)
  - authoring profile references:
    - `controller_profile_key` (from `Assets/game_project/controller_profiles.json`)
    - `camera_profile_key` (from `Assets/game_project/camera_profiles.json`)
    - `input_profile_key` (from `Assets/game_project/input_profiles.json`)
  - v2/v3/v4 optional runtime config fields:
    - `scene_map_addr`, `scene_map_size_bytes`
    - `scene_tileset_addr`, `scene_tileset_size_bytes`
    - profile-derived at build time (resolved from profile keys):
      - `topdown_render_scale`, `topdown_tile_present_mode`
      - `controller_profile_id`, `camera_profile_id`
      - `input_deadzone_permille`, `input_flags`
      - `move_speed_px_s`, `move_accel_px_s2`, `move_decel_px_s2`
      - `camera_deadzone_w_px`, `camera_deadzone_h_px`
      - `camera_follow_permille`, `camera_max_speed_px_s`
      - `camera_lookahead_x_px`, `camera_lookahead_y_px`
    - legacy inline mode tuning fields are still accepted for compatibility, but profile-owned values are preferred
  - v4 optional package reference ID fields:
    - `scene_map_id`, `scene_tileset_id`
    - `music_asset_id`
    - `sfx_interact_asset_id`, `sfx_confirm_asset_id`, `sfx_error_asset_id`
- `pet_routes[]` objects with:
  - `pet_entry_id` (`u16`)
  - `mode_id` (`u32`, must reference an entry in `modes[]`)

Example:

```bash
python tools/gen_game_package_manifest.py
```

---

## Map Chunk Format

Map chunk must contain:

- width (tiles)
- height (tiles)
- tile index array
- tile property bitmask array
- object count
- object records

Object record must contain:

- type ID
- position (tile coordinates or pixel)
- property fields

All values must be fixed-width integers.

No pointers inside blob.

Current baseline system map blob (`TMAP`, v1):

- header: `game_map_blob_header_t` (`Core/Inc/game_map.h`)
- tile flags plane: 1 byte per cell (`solid/water/slow/occluder/roof/emissive`)
- tile gid plane: 16-bit gid per cell
- object table: fixed-size `game_map_object_t` records (bounded, no pointers)

Host-side converter:

- `tools/gen_tiled_map_blob.py`

Example:

```bash
python tools/gen_tiled_map_blob.py --in-json Assets/maps/inside_building.json --out-bin Assets/maps/build/inside_building.tmap.bin
```

Runtime parser:

- `GameMap_Parse()` in `Core/Src/game_map.c`
- consumed via game runtime seam (`GameRuntime_LoadSceneMapBlob`)

---

## Tileset Chunk Format

Tiles must be stored as 2bpp + optional mask plane.

Per tile:

- width
- height
- color_stride
- mask_stride (0 if fully opaque)
- color_plane data
- mask_plane data (if present)

Tile dimensions must match map tile size.

---

## Sprite Chunk Format

Sprites must be stored as 2bpp + mask.

For each sprite:

- width
- height
- color_stride
- mask_stride
- color_plane data
- mask_plane data

No panel-native 1bpp conversion at build time.

All sprites are consumed into world2bpp surface at runtime.

Local player-sheet helper (firmware-side topdown placeholder path):

```bash
python tools/gen_player_sprites.py
```

Inputs/outputs:
- input sheet: `Assets/sprites/player_sheet.png`
- optional frame map: `Assets/sprites/player_sheet.json`
- output header: `Assets/sprites/player_sprites_autogen.h`

---

## Audio Chunk Format

Audio bank chunk contains:

- clip_count
- clip table:
  - clip_id
  - offset
  - size
  - loop flag
- ADPCM payload blocks

Clips must be block-aligned.

Runtime decodes ADPCM into 16-bit PCM in bounded buffer.

---

## System/Public Resources

System resources are not part of individual blobs.

Examples:

- System font
- UI icons
- Basic SFX (beep, click)
- Boot/loading graphics

System/UI bitmap format:

System/public UI assets may remain panel-native 1bpp.

Allowed formats for system/public resources:
- 1bpp ON plane + optional 1bpp OP plane (panel-native bit semantics; 1=white, 0=black)
- 2bpp + 1bpp mask (only if a system asset is intended to participate in world2bpp)

Rules:
- UI rendering is performed after world present and is never dithered or scaled.
- System/public assets are not required to use the blob world bitmap format.

Blobs may depend only on these public assets.

Cross-blob asset dependencies are forbidden.

---

## Versioning Rules

Blob format must include:

- format_version (structure version)
- blob_version (content revision)

Runtime must:

- Reject incompatible format_version.
- Optionally accept older compatible versions.
- Validate CRC before install.

Forward compatibility:
- Unknown chunk types ignored.
- Known chunk types validated.

---

## Installation Flow (High-Level)

1. Blob file copied to FAT transport volume.
2. User selects game/world.
3. thStorage reads blob file.
4. Header and CRC validated.
5. Blob copied into raw installed region.
6. Installed index updated atomically.
7. Runtime loads from raw installed region only.

Blob must not be used until fully validated.

---

## Runtime Loading Model

At runtime:

- Loader reads header + chunk table.
- Builds lightweight RAM metadata table.
- Maps chunk offsets for fast lookup.
- Loads specific chunks into RAM as required.

No dynamic memory required:
- Metadata arrays can be static with compile-time limits.
- Chunk count must be bounded.

---

## Determinism Requirements

- No JSON parsing at runtime.
- No FAT access during gameplay.
- All chunk reads must be bounded.
- No heap allocation.
- No recursion in blob parsing.

---

## Build Toolchain Responsibilities

Host-side tool must:

- Validate input assets.
- Enforce chunk alignment.
- Enforce maximum sizes.
- Compute CRC32.
- Emit versioned blob file.
- Quantize PNG assets to 4 colour levels.
- Generate mask plane from PNG alpha channel.

Firmware must not attempt to repair malformed blobs.

---

## Forbidden Patterns

- Embedding large map/sprite data in firmware image.
- Streaming assets from FAT at runtime.
- Using dynamic allocation to parse chunks.
- Allowing blobs to depend on other blobs for assets.
- Mixing raw installed region and FAT volume access simultaneously.

---

## Integration Notes

- Game engine reads metadata defined here (see game_engine.md).
- Storage installs blobs (see storage_and_updates.md).
- Audio subsystem reads from AUDIO_BANK chunk (see audio.md).
- Renderer consumes 2bpp + mask bitmaps and presents to 1bpp panel (see display_and_rendering.md).

---

Last updated: 2026-03-16
