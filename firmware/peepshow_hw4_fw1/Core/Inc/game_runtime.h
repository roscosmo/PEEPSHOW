#ifndef GAME_RUNTIME_H
#define GAME_RUNTIME_H

#include <stdint.h>

#include "app_threadx.h"
#include "game_map.h"
#include "game_package.h"
#include "game_tileset.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  GAME_RT_INPUT_SRC_BTN_A = 1U,
  GAME_RT_INPUT_SRC_BTN_B = 2U,
  GAME_RT_INPUT_SRC_BTN_L = 3U,
  GAME_RT_INPUT_SRC_BTN_R = 4U,
  GAME_RT_INPUT_SRC_BTN_BOOT = 5U,
  GAME_RT_INPUT_SRC_JOY_UP = 6U,
  GAME_RT_INPUT_SRC_JOY_RIGHT = 7U,
  GAME_RT_INPUT_SRC_JOY_DOWN = 8U,
  GAME_RT_INPUT_SRC_JOY_LEFT = 9U
} game_runtime_input_source_t;

typedef enum
{
  GAME_RT_INPUT_EVENT_PRESS = 1U,
  GAME_RT_INPUT_EVENT_RELEASE = 2U,
  GAME_RT_INPUT_EVENT_REPEAT = 3U,
  GAME_RT_INPUT_EVENT_LONG = 4U
} game_runtime_input_event_t;

typedef struct
{
  ULONG source;
  ULONG event;
  ULONG tick;
  ULONG pressed_mask;
} game_runtime_input_t;

typedef enum
{
  GAME_RT_AUDIO_CUE_NONE = 0U,
  GAME_RT_AUDIO_CUE_MOVE = 1U,
  GAME_RT_AUDIO_CUE_PRIMARY = 2U,
  GAME_RT_AUDIO_CUE_SECONDARY = 3U,
  GAME_RT_AUDIO_CUE_BACK = 4U,
  GAME_RT_AUDIO_CUE_GAME_ACTION = 5U
} game_runtime_audio_cue_t;

typedef enum
{
  GAME_RT_TUNE_TOPDOWN_RENDER_SCALE = (1UL << 0),
  GAME_RT_TUNE_TOPDOWN_TILE_PRESENT_MODE = (1UL << 1),
  GAME_RT_TUNE_CONTROLLER_PROFILE_ID = (1UL << 2),
  GAME_RT_TUNE_CAMERA_PROFILE_ID = (1UL << 3),
  GAME_RT_TUNE_INPUT_DEADZONE_PERMILLE = (1UL << 4),
  GAME_RT_TUNE_INPUT_FLAGS = (1UL << 5),
  GAME_RT_TUNE_MOVE_SPEED_PX_S = (1UL << 6),
  GAME_RT_TUNE_MOVE_ACCEL_PX_S2 = (1UL << 7),
  GAME_RT_TUNE_MOVE_DECEL_PX_S2 = (1UL << 8),
  GAME_RT_TUNE_CAMERA_DEADZONE_W_PX = (1UL << 9),
  GAME_RT_TUNE_CAMERA_DEADZONE_H_PX = (1UL << 10),
  GAME_RT_TUNE_CAMERA_FOLLOW_PERMILLE = (1UL << 11),
  GAME_RT_TUNE_CAMERA_MAX_SPEED_PX_S = (1UL << 12),
  GAME_RT_TUNE_CAMERA_LOOKAHEAD_X_PX = (1UL << 13),
  GAME_RT_TUNE_CAMERA_LOOKAHEAD_Y_PX = (1UL << 14)
} game_runtime_tune_field_t;

typedef struct
{
  uint32_t field_mask;
  uint32_t topdown_render_scale;
  uint32_t topdown_tile_present_mode;
  uint32_t controller_profile_id;
  uint32_t camera_profile_id;
  uint32_t input_deadzone_permille;
  uint32_t input_flags;
  uint32_t move_speed_px_s;
  uint32_t move_accel_px_s2;
  uint32_t move_decel_px_s2;
  uint32_t camera_deadzone_w_px;
  uint32_t camera_deadzone_h_px;
  uint32_t camera_follow_permille;
  uint32_t camera_max_speed_px_s;
  int32_t camera_lookahead_x_px;
  int32_t camera_lookahead_y_px;
} game_runtime_tune_patch_t;

typedef struct
{
  uint32_t target_map_hash;
  uint32_t target_spawn_hash;
} game_runtime_scene_transition_req_t;

extern game_runtime_tune_patch_t g_game_runtime_dbg_tune_patch;
extern volatile ULONG g_game_runtime_dbg_tune_apply_pending;
extern volatile ULONG g_game_runtime_dbg_tune_reset_pending;
extern volatile ULONG g_game_runtime_dbg_tune_apply_ok_count;
extern volatile ULONG g_game_runtime_dbg_tune_apply_fail_count;
extern volatile ULONG g_game_runtime_dbg_tune_last_status;
extern volatile ULONG g_game_runtime_dbg_tune_last_mode_id;

void GameRuntime_Init(void);
void GameRuntime_Shutdown(void);
uint8_t GameRuntime_HandleControl(const game_runtime_input_t *input,
                                  uint8_t *request_exit_to_static,
                                  game_runtime_audio_cue_t *audio_cue_out);
void GameRuntime_Update(const app_sensor_snapshot_t *sensor_snapshot, uint32_t dt_ms);
void GameRuntime_DrawFrame(const app_sensor_snapshot_t *sensor_snapshot);
UINT GameRuntime_LoadSceneMapBlob(const void *blob_data, uint32_t blob_size);
void GameRuntime_ClearSceneMap(void);
const game_map_view_t *GameRuntime_GetSceneMap(void);
UINT GameRuntime_LoadSceneTilesetBlob(const void *blob_data, uint32_t blob_size);
void GameRuntime_ClearSceneTileset(void);
const game_tileset_view_t *GameRuntime_GetSceneTileset(void);
uint32_t GameRuntime_GetActiveModeId(void);
uint32_t GameRuntime_GetActiveBackendId(void);
const game_package_runtime_config_t *GameRuntime_GetActiveModeConfig(void);
const game_package_runtime_config_t *GameRuntime_GetActiveModeBaseConfig(void);
UINT GameRuntime_ApplyActiveModeConfigRuntimeTune(const game_runtime_tune_patch_t *patch);
void GameRuntime_ResetActiveModeConfigRuntimeTune(void);
void GameRuntime_ProcessPendingDebugTune(void);
void GameRuntime_RequestSceneTransition(uint32_t target_map_hash, uint32_t target_spawn_hash);
uint8_t GameRuntime_ConsumeSceneTransition(game_runtime_scene_transition_req_t *request_out);

#ifdef __cplusplus
}
#endif

#endif /* GAME_RUNTIME_H */
