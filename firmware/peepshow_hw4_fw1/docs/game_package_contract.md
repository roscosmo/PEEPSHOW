# Game Package Contract (V1)

Authoritative interface contract for package-provided game content routing between
STOP pet mode and REALTIME runtime modes.

This is a v1 scaffold for package-driven mode selection. It does not yet implement
full blob-backed package loading; it defines the in-firmware contract and hooks.

## Purpose

- Keep system runtime generic.
- Keep gameplay behavior/package routing out of hardcoded UI/system logic.
- Provide explicit route from pet entries to realtime mode descriptors.

## Ownership

- Package descriptor lookup: runtime code (`game_package.c`), no HAL access.
- Mode switch execution: `thPower` (unchanged).
- REALTIME mode execution: `thGame` via `game_runtime.c`.
- UI route request only posts a package runtime-mode request, then mode-set event.

## V1 Data Model

Defined in `Core/Inc/game_package.h`:

- `game_package_runtime_kind_t`
  - `TOPDOWN`
  - `SIDESCROLL`
- `game_runtime_backend_id_t`
  - Backend implementation identity used by `game_runtime.c`.
- `game_package_mode_desc_t`
  - `mode_id`
  - `runtime_kind`
  - `backend_id`
  - `name`
- `game_package_pet_route_t`
  - `pet_entry_id`
  - `mode_id`
- `game_package_desc_t`
  - package id/version
  - mode table
  - pet route table

## V1 APIs

- `GamePackage_GetActive()`
- `GamePackage_FindModeById(mode_id)`
- `GamePackage_RequestRuntimeModeById(mode_id)`
- `GamePackage_RequestRuntimeModeByPetEntry(pet_entry_id)`
- `GamePackage_ConsumeRequestedRuntimeModeId()`
- `GamePackage_LoadManifestBlob(manifest_data, manifest_size)`
- `GamePackage_ClearLoadedManifest()`

Request/consume behavior:

- UI sets requested runtime mode before switching to REALTIME.
- `GameRuntime_Init()` consumes request once at mode entry.
- If no request exists (or mapping is invalid), runtime falls back to existing
  `KNOB_GAME_RUNTIME_DEFAULT_SCENARIO` behavior.

## Current Wiring

- Pet START GAME action calls:
  - `GamePackage_RequestRuntimeModeByPetEntry(GAME_PET_ENTRY_START_GAME)`
  - then `App_SysEvent_ModeSet(APP_MODE_REALTIME)`
- `game_runtime.c` resolves active backend from requested package mode.
- `thStorage` exposes explicit raw-flash manifest load request:
  - `App_StorageReq_GamePackageManifestLoad(manifest_addr, manifest_size)`
  - request type: `APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_LOAD`
  - loader validates + calls `GamePackage_LoadManifestBlob(...)`
- `thStorage` also exposes default fixed-slot load request:
  - `App_StorageReq_GamePackageManifestLoadDefault()`
  - request type: `APP_STORAGE_REQ_GAME_PACKAGE_MANIFEST_LOAD_DEFAULT`
  - loader reads header from fixed raw slot (`KNOB_STORAGE_GAME_PKG_MANIFEST_ADDR`),
    validates `total_size <= KNOB_STORAGE_GAME_PKG_MANIFEST_MAX_BYTES`, then loads.
- If a validated manifest blob is loaded, `GamePackage_GetActive()` returns loaded
  descriptor data; otherwise built-in fallback descriptor is used.

## Blob Manifest Layout (V1/V2/V3/V4/V5)

Defined in `Core/Inc/game_package_manifest.h`.

### Header (`game_package_manifest_header_t`)

- `magic` (`0x4B50474D`, "MGPK")
- `version` (`1`, `2`, `3`, `4`, or `5`)
- `header_size`
- `total_size`
- `crc32`
- `package_id`
- `package_version`
- `mode_count`
- `pet_route_count`
- `modes_offset`
- `pet_routes_offset`

V3 (`game_package_manifest_header_v3_t`) extends the header with:

- `pet_menu_item_count`
- `pet_menu_items_offset`

CRC rule:

- CRC32 is computed over `total_size` bytes with the `crc32` field bytes treated as
  zero during calculation.

Bounds rules:

- `total_size <= GAME_PACKAGE_MANIFEST_MAX_BYTES`
- `mode_count <= GAME_PACKAGE_MANIFEST_MAX_MODE_COUNT`
- `pet_route_count <= GAME_PACKAGE_MANIFEST_MAX_PET_ROUTE_COUNT`
- `pet_menu_item_count <= GAME_PACKAGE_MANIFEST_MAX_PET_MENU_ITEM_COUNT` (V3+)
- all table regions must be in-bounds and 4-byte aligned

### Mode Entry V1 (`game_package_manifest_mode_t`)

- `mode_id`
- `runtime_kind` (`TOPDOWN`/`SIDESCROLL`)
- `backend_id` (system backend selector)
- `reserved0`

### Mode Entry V2 (`game_package_manifest_mode_v2_t`)

V2 extends mode descriptors with runtime configuration fields (data only;
engine/controller code remains firmware-owned):

- `scene_map_addr`, `scene_map_size_bytes`
- `scene_tileset_addr`, `scene_tileset_size_bytes`
- `topdown_render_scale`, `topdown_tile_present_mode`
- `controller_profile_id`, `camera_profile_id`
- `input_deadzone_permille`, `input_flags`
- `move_speed_px_s`, `move_accel_px_s2`, `move_decel_px_s2`
- `camera_deadzone_w_px`, `camera_deadzone_h_px`
- `camera_follow_permille`, `camera_max_speed_px_s`
- `camera_lookahead_x_px`, `camera_lookahead_y_px`

Authoring note:

- `manifest.example.json` should reference profile keys (`controller_profile_key`,
  `camera_profile_key`, `input_profile_key`).
- `tools/gen_game_package_manifest.py` resolves those keys against
  `Assets/game_project/*_profiles.json` and materializes the V2 runtime fields
  above for firmware consumption.

### Mode Entry V4 (`game_package_manifest_mode_v4_t`)

V4 extends V2 with scene/audio asset references:

- `scene_map_id`, `scene_tileset_id`
- `music_asset_id`
- `sfx_interact_asset_id`, `sfx_confirm_asset_id`, `sfx_error_asset_id`

### Mode Entry V5 (`game_package_manifest_mode_v5_t`)

V5 extends V4 with lifecycle policy fields:

- `scene_lifecycle`
  - `0` = `LEGACY` (fallback behavior)
  - `1` = `RESUMABLE` (retained save/restore enabled)
  - `2` = `TRANSIENT` (no retained save/restore)
- `resume_domain_id`
  - reserved for multi-domain resume routing (`0` for transient scenes)

### Pet Route Entry (`game_package_manifest_pet_route_t`)

- `pet_entry_id`
- `reserved0`
- `mode_id`

### Pet Menu Item Entry (`game_package_manifest_pet_menu_item_t`, V3)

- `slot_index`
- `icon_action_id`
- `select_kind`
- `status_kind`
- `arg0`
- `status_source_id`

Runtime semantics:

- `icon_action_id` selects which icon sprite to render for that slot.
- `select_kind` controls slot select behavior:
  - `NONE` (status/indicator slot, no launch action)
  - `FEED`, `PLAY`, `START_GAME`, `OPTIONS`
  - `LAUNCH_MODE` (request mode by `arg0`, then switch to REALTIME)
  - `OPEN_PAGE` (resolve page registry ID in `arg0`, open native page/tree in STATIC UI)
- `status_kind` + `status_source_id` define indicator behavior.
  - `status_source_id=BATTERY` + `status_kind=LEVEL4`:
    - `arg0` is the base icon action id.
    - runtime renders `arg0 + level`, where `level` is `0..3` from battery SOC.
  - `status_source_id=BATTERY` + `status_kind=BOOL`:
    - runtime renders `arg0` or `arg0 + 1` based on boolean status.
- If no V3 `pet_menu_items` are present, firmware falls back to built-in default pet slot map.
- For `LAUNCH_MODE`, `arg0` must reference a valid `mode_id` in the active package.
- For `OPEN_PAGE`, `arg0` must reference a non-zero page registry ID.

Authoring note:

- Source of truth for pet menu slots is `Assets/game_project/pet_menu_slots.json`.
- `tools/gen_game_package_manifest.py` compiles slots into `pet_menu_items` and resolves:
  - `mode_key -> mode_id` for `LAUNCH_MODE`
  - `page_key -> page_id` for `OPEN_PAGE`
- `Assets/game_project/pages.json` drives firmware page routing header generation:
  - `Core/Inc/ui/ui_page_registry_autogen.h`

Retained STOP2 resume policy:

- V5 manifests control this explicitly per mode using `scene_lifecycle`.
- `RESUMABLE` modes may save/restore retained topdown snapshot.
- `TRANSIENT` modes never save/restore retained snapshot and never clear
  retained snapshot written by resumable modes.
- Legacy manifests (without lifecycle fields) fall back to start-game route
  ownership behavior for backward compatibility.

## Non-Goals (V1)

- No automatic install-time package discovery/selection path yet (load is explicit request).
- No dynamic allocation.
- No runtime FAT access.
- No changes to thread ownership model.

## Next Steps (V2+)

- Replace built-in package descriptor with installed blob descriptor parsing in `thStorage`.
- Add package manifest chunk schema (mode descriptors + pet routes + pet menu items).
- Expose package-selected pet assets and realtime mode list from installed package metadata.
