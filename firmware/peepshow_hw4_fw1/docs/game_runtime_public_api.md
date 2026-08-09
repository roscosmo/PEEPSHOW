# Game Runtime Public API Contract

This document defines the supported API surface for game/runtime tuning work.

Goal:
- game-dev changes should use only public game APIs
- system internals (threads, HAL, power/storage ownership) must remain untouched

If this document conflicts with `docs/authority.md`, `docs/authority.md` wins.

---

## Ownership Boundary

Public game APIs are consumed from game/runtime code paths only.

System-private code remains owned by platform threads:
- `thPower`: mode/clock/STOP2 policy
- `thStorage`: flash/FileX ownership
- `thDisplay`: SPI panel ownership
- `thSensor`: sensor bus ownership
- `thAudio`: audio DMA ownership

Game code must not call HAL, peripheral drivers, or thread-entry internals directly.

---

## Public Headers

### `Core/Inc/game_runtime.h`

Mode/session lifecycle:
- `GameRuntime_Init()`
- `GameRuntime_Shutdown()`
- `GameRuntime_Update(...)`
- `GameRuntime_DrawFrame(...)`

Input dispatch:
- `GameRuntime_HandleControl(...)`

Active mode/query:
- `GameRuntime_GetActiveModeId()`
- `GameRuntime_GetActiveModeConfig()`
- `GameRuntime_GetActiveModeBaseConfig()`

Scene asset load/query:
- `GameRuntime_LoadSceneMapBlob(...)`
- `GameRuntime_ClearSceneMap()`
- `GameRuntime_GetSceneMap()`
- `GameRuntime_LoadSceneTilesetBlob(...)`
- `GameRuntime_ClearSceneTileset()`
- `GameRuntime_GetSceneTileset()`

Scene transition request path:
- `GameRuntime_RequestSceneTransition(...)`
- `GameRuntime_ConsumeSceneTransition(...)`

Live runtime tuning (no rebuild/reflash):
- `GameRuntime_ApplyActiveModeConfigRuntimeTune(...)`
- `GameRuntime_ResetActiveModeConfigRuntimeTune()`

### `Core/Inc/game_package.h`

Package routing/config selection:
- `GamePackage_RequestRuntimeModeById(...)`
- `GamePackage_RequestRuntimeModeByPetEntry(...)`
- `GamePackage_ConsumeRequestedRuntimeModeId()`
- `GamePackage_FindModeById(...)`
- `GamePackage_GetRuntimeConfigByModeId(...)`
- `GamePackage_GetActive()`

Manifest staging:
- `GamePackage_LoadManifestBlob(...)`
- `GamePackage_ClearLoadedManifest()`

---

## Runtime Tune API Semantics

`GameRuntime_ApplyActiveModeConfigRuntimeTune(...)` applies an overlay to the active mode config.

Important behavior:
- Base package config is immutable.
- Tune overlay is mode-scoped (`active_mode_id`).
- Overlay applies immediately to runtime consumers of `GameRuntime_GetActiveModeConfig()`.
- Overlay persists across REALTIME re-entry for the same mode until reset.
- `GameRuntime_ResetActiveModeConfigRuntimeTune()` clears overlay and restores base config view.

Field selection is explicit via `game_runtime_tune_patch_t.field_mask` and
`game_runtime_tune_field_t` bit flags.

This API is intended for:
- controller feel tuning
- movement/camera tuning
- topdown render/present tuning

It is not a replacement for package install/update workflows.

---

## Retained Session Snapshot API (Topdown)

`Core/Inc/game_mode_topdown_basic.h` exposes a compact snapshot contract:

- `GameModeTopdownBasic_SaveSnapshot(...)`
- `GameModeTopdownBasic_LoadSnapshot(...)`

Scope:
- Captures/restores topdown runtime state needed for fast resume (player/camera/spawn/anim).
- Does not include renderer cache pointers or peripheral-owned state.
- Scene selection continuity (`scene_map_id`/`scene_tileset_id`) is retained/restored by
  `thGame` orchestration, alongside snapshot load, before scene blob load requests.

Rules:
- Snapshot restore must run only from owner-thread orchestration (`thGame` lifecycle boundary).
- Snapshot blobs must be versioned/validated by the caller before restore.
- This mechanism is RAM-retained continuity, not durable save-game persistence.
- Scene transitions should be requested by gameplay code through
  `GameRuntime_RequestSceneTransition(...)`; only owner-thread orchestration may consume and
  execute the transition.

Live-debug safety:
- Do not place `game_runtime_tune_patch_t` scratch data in memory currently
  backing live scene views.
- Specifically, when a scene map is loaded, do not use
  `g_storage_scene_map_blob_buf` as scratch (map parser stores pointers into it).
- Use `g_storage_game_package_manifest_buf` or a dedicated scratch buffer.

Example (firmware-side):

```c
game_runtime_tune_patch_t patch = {0};
patch.field_mask = (uint32_t)(GAME_RT_TUNE_MOVE_SPEED_PX_S |
                              GAME_RT_TUNE_CAMERA_FOLLOW_PERMILLE |
                              GAME_RT_TUNE_CAMERA_LOOKAHEAD_X_PX);
patch.move_speed_px_s = 96U;
patch.camera_follow_permille = 380U;
patch.camera_lookahead_x_px = 28;
(void)GameRuntime_ApplyActiveModeConfigRuntimeTune(&patch);
```

---

## What Is Not Public

Do not call or modify from game-dev logic:
- `App_*` request functions in `app_threadx.c` unless explicitly documented as game API
- thread entry modules under `Core/Src/threads/*`
- HAL drivers, ISR handlers, or CubeMX-generated ownership plumbing
- power/clock transitions, STOP2 policy, storage layout policy

---

## Scope Discipline Rule

When adding new game-facing capabilities:
1. Add/extend API in `game_runtime.h` (or dedicated game API header).
2. Document it here.
3. Keep system ownership and mode policy unchanged unless explicitly requested.
