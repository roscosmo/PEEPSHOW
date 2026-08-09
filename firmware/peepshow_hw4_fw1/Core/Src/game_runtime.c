#include "game_runtime.h"

#include <string.h>

#include "game_package.h"
#include "game_mode_topdown_basic.h"
#include "knobs_autogen.h"
#include "render_demo.h"
#include "render_demo_3d_walk.h"
#include "render_demo_title_anim.h"

typedef struct
{
  uint32_t backend_id;
  void (*init)(void);
  void (*shutdown)(void);
  uint8_t (*handle_control)(const game_runtime_input_t *input,
                            uint8_t *request_exit_to_static,
                            game_runtime_audio_cue_t *audio_cue_out);
  void (*update)(const app_sensor_snapshot_t *sensor_snapshot, uint32_t dt_ms);
  void (*draw_frame)(const app_sensor_snapshot_t *sensor_snapshot);
} game_runtime_ops_t;

static uint8_t GameRuntime_RenderDemoHandleControl(const game_runtime_input_t *input,
                                                   uint8_t *request_exit_to_static,
                                                   game_runtime_audio_cue_t *audio_cue_out)
{
  return RenderDemo_HandleControl(input, request_exit_to_static, audio_cue_out);
}

static uint8_t GameRuntime_RenderDemo3dWalkHandleControl(const game_runtime_input_t *input,
                                                         uint8_t *request_exit_to_static,
                                                         game_runtime_audio_cue_t *audio_cue_out)
{
  return RenderDemo3dWalk_HandleControl(input, request_exit_to_static, audio_cue_out);
}

static uint8_t GameRuntime_TopdownBasicHandleControl(const game_runtime_input_t *input,
                                                     uint8_t *request_exit_to_static,
                                                     game_runtime_audio_cue_t *audio_cue_out)
{
  return GameModeTopdownBasic_HandleControl(input, request_exit_to_static, audio_cue_out);
}

static uint8_t GameRuntime_RenderDemoTitleAnimHandleControl(const game_runtime_input_t *input,
                                                            uint8_t *request_exit_to_static,
                                                            game_runtime_audio_cue_t *audio_cue_out)
{
  return RenderDemoTitleAnim_HandleControl(input, request_exit_to_static, audio_cue_out);
}

static const game_runtime_ops_t g_game_runtime_render_demo_ops =
{
  (uint32_t)GAME_RUNTIME_BACKEND_RENDER_DEMO,
  RenderDemo_Reset,
  RenderDemo_Reset,
  GameRuntime_RenderDemoHandleControl,
  RenderDemo_Update,
  RenderDemo_DrawFrame
};

static const game_runtime_ops_t g_game_runtime_render_demo_3d_walk_ops =
{
  (uint32_t)GAME_RUNTIME_BACKEND_RENDER_DEMO_3D_WALK,
  RenderDemo3dWalk_Reset,
  RenderDemo3dWalk_Reset,
  GameRuntime_RenderDemo3dWalkHandleControl,
  RenderDemo3dWalk_Update,
  RenderDemo3dWalk_DrawFrame
};

static const game_runtime_ops_t g_game_runtime_topdown_basic_ops =
{
  (uint32_t)GAME_RUNTIME_BACKEND_TOPDOWN_BASIC,
  GameModeTopdownBasic_Reset,
  GameModeTopdownBasic_Reset,
  GameRuntime_TopdownBasicHandleControl,
  GameModeTopdownBasic_Update,
  GameModeTopdownBasic_DrawFrame
};

static const game_runtime_ops_t g_game_runtime_render_demo_title_anim_ops =
{
  (uint32_t)GAME_RUNTIME_BACKEND_RENDER_DEMO_TITLE_ANIM,
  RenderDemoTitleAnim_Reset,
  RenderDemoTitleAnim_Reset,
  GameRuntime_RenderDemoTitleAnimHandleControl,
  RenderDemoTitleAnim_Update,
  RenderDemoTitleAnim_DrawFrame
};

static const game_runtime_ops_t *g_game_runtime_ops_list[] =
{
  &g_game_runtime_render_demo_ops,
  &g_game_runtime_render_demo_3d_walk_ops,
  &g_game_runtime_topdown_basic_ops,
  &g_game_runtime_render_demo_title_anim_ops
};

static const game_runtime_ops_t *g_game_runtime_active = (const game_runtime_ops_t *)0;
static uint32_t g_game_runtime_active_mode_id = 0u;
static const game_package_runtime_config_t *g_game_runtime_active_mode_cfg_base =
    (const game_package_runtime_config_t *)0;
static game_package_runtime_config_t g_game_runtime_active_mode_cfg_resolved;
static uint8_t g_game_runtime_active_mode_cfg_resolved_valid = 0U;
static game_runtime_tune_patch_t g_game_runtime_tune_values;
static uint32_t g_game_runtime_tune_mask = 0U;
static uint32_t g_game_runtime_tune_mode_id = 0U;
static game_runtime_scene_transition_req_t g_game_runtime_scene_transition_req;
static uint8_t g_game_runtime_scene_transition_pending = 0U;
game_runtime_tune_patch_t g_game_runtime_dbg_tune_patch;
volatile ULONG g_game_runtime_dbg_tune_apply_pending = 0UL;
volatile ULONG g_game_runtime_dbg_tune_reset_pending = 0UL;
volatile ULONG g_game_runtime_dbg_tune_apply_ok_count = 0UL;
volatile ULONG g_game_runtime_dbg_tune_apply_fail_count = 0UL;
volatile ULONG g_game_runtime_dbg_tune_last_status = (ULONG)TX_NOT_DONE;
volatile ULONG g_game_runtime_dbg_tune_last_mode_id = 0UL;
static game_map_view_t g_game_runtime_scene_map;
static uint8_t g_game_runtime_scene_map_loaded = 0U;
static game_tileset_view_t g_game_runtime_scene_tileset;
static uint8_t g_game_runtime_scene_tileset_loaded = 0U;

/*
 * Keep runtime tune API symbols referenced from always-linked code so they
 * remain callable from debugger sessions with --gc-sections enabled.
 */
static void GameRuntimeKeepTuneApiSymbols(void)
{
  volatile UINT (*apply_fn)(const game_runtime_tune_patch_t *) =
      GameRuntime_ApplyActiveModeConfigRuntimeTune;
  volatile void (*reset_fn)(void) = GameRuntime_ResetActiveModeConfigRuntimeTune;
  volatile game_runtime_tune_patch_t *dbg_patch = &g_game_runtime_dbg_tune_patch;
  if ((apply_fn == (UINT (*)(const game_runtime_tune_patch_t *))0) ||
      (reset_fn == (void (*)(void))0) ||
      (dbg_patch == (volatile game_runtime_tune_patch_t *)0))
  {
    return;
  }
}

static void GameRuntimeApplyTuneToConfig(game_package_runtime_config_t *cfg,
                                         const game_runtime_tune_patch_t *tune,
                                         uint32_t field_mask)
{
  if ((cfg == (game_package_runtime_config_t *)0) || (tune == (const game_runtime_tune_patch_t *)0))
  {
    return;
  }

  if ((field_mask & (uint32_t)GAME_RT_TUNE_TOPDOWN_RENDER_SCALE) != 0UL)
  {
    cfg->topdown_render_scale = tune->topdown_render_scale;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_TOPDOWN_TILE_PRESENT_MODE) != 0UL)
  {
    cfg->topdown_tile_present_mode = tune->topdown_tile_present_mode;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CONTROLLER_PROFILE_ID) != 0UL)
  {
    cfg->controller_profile_id = tune->controller_profile_id;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CAMERA_PROFILE_ID) != 0UL)
  {
    cfg->camera_profile_id = tune->camera_profile_id;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_INPUT_DEADZONE_PERMILLE) != 0UL)
  {
    cfg->input_deadzone_permille = tune->input_deadzone_permille;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_INPUT_FLAGS) != 0UL)
  {
    cfg->input_flags = tune->input_flags;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_MOVE_SPEED_PX_S) != 0UL)
  {
    cfg->move_speed_px_s = tune->move_speed_px_s;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_MOVE_ACCEL_PX_S2) != 0UL)
  {
    cfg->move_accel_px_s2 = tune->move_accel_px_s2;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_MOVE_DECEL_PX_S2) != 0UL)
  {
    cfg->move_decel_px_s2 = tune->move_decel_px_s2;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CAMERA_DEADZONE_W_PX) != 0UL)
  {
    cfg->camera_deadzone_w_px = tune->camera_deadzone_w_px;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CAMERA_DEADZONE_H_PX) != 0UL)
  {
    cfg->camera_deadzone_h_px = tune->camera_deadzone_h_px;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CAMERA_FOLLOW_PERMILLE) != 0UL)
  {
    cfg->camera_follow_permille = tune->camera_follow_permille;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CAMERA_MAX_SPEED_PX_S) != 0UL)
  {
    cfg->camera_max_speed_px_s = tune->camera_max_speed_px_s;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CAMERA_LOOKAHEAD_X_PX) != 0UL)
  {
    cfg->camera_lookahead_x_px = tune->camera_lookahead_x_px;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CAMERA_LOOKAHEAD_Y_PX) != 0UL)
  {
    cfg->camera_lookahead_y_px = tune->camera_lookahead_y_px;
  }
}

static void GameRuntimeRefreshResolvedModeConfig(void)
{
  if (g_game_runtime_active_mode_cfg_base == (const game_package_runtime_config_t *)0)
  {
    (void)memset(&g_game_runtime_active_mode_cfg_resolved, 0, sizeof(g_game_runtime_active_mode_cfg_resolved));
    g_game_runtime_active_mode_cfg_resolved_valid = 0U;
    return;
  }

  g_game_runtime_active_mode_cfg_resolved = *g_game_runtime_active_mode_cfg_base;
  g_game_runtime_active_mode_cfg_resolved_valid = 1U;
  if ((g_game_runtime_tune_mask != 0UL) &&
      (g_game_runtime_tune_mode_id == g_game_runtime_active_mode_id))
  {
    GameRuntimeApplyTuneToConfig(&g_game_runtime_active_mode_cfg_resolved,
                                 &g_game_runtime_tune_values,
                                 g_game_runtime_tune_mask);
  }
}

static void GameRuntimeMergeTuneValues(game_runtime_tune_patch_t *dst,
                                       const game_runtime_tune_patch_t *src,
                                       uint32_t field_mask)
{
  if ((dst == (game_runtime_tune_patch_t *)0) || (src == (const game_runtime_tune_patch_t *)0))
  {
    return;
  }

  if ((field_mask & (uint32_t)GAME_RT_TUNE_TOPDOWN_RENDER_SCALE) != 0UL)
  {
    dst->topdown_render_scale = src->topdown_render_scale;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_TOPDOWN_TILE_PRESENT_MODE) != 0UL)
  {
    dst->topdown_tile_present_mode = src->topdown_tile_present_mode;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CONTROLLER_PROFILE_ID) != 0UL)
  {
    dst->controller_profile_id = src->controller_profile_id;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CAMERA_PROFILE_ID) != 0UL)
  {
    dst->camera_profile_id = src->camera_profile_id;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_INPUT_DEADZONE_PERMILLE) != 0UL)
  {
    dst->input_deadzone_permille = src->input_deadzone_permille;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_INPUT_FLAGS) != 0UL)
  {
    dst->input_flags = src->input_flags;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_MOVE_SPEED_PX_S) != 0UL)
  {
    dst->move_speed_px_s = src->move_speed_px_s;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_MOVE_ACCEL_PX_S2) != 0UL)
  {
    dst->move_accel_px_s2 = src->move_accel_px_s2;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_MOVE_DECEL_PX_S2) != 0UL)
  {
    dst->move_decel_px_s2 = src->move_decel_px_s2;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CAMERA_DEADZONE_W_PX) != 0UL)
  {
    dst->camera_deadzone_w_px = src->camera_deadzone_w_px;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CAMERA_DEADZONE_H_PX) != 0UL)
  {
    dst->camera_deadzone_h_px = src->camera_deadzone_h_px;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CAMERA_FOLLOW_PERMILLE) != 0UL)
  {
    dst->camera_follow_permille = src->camera_follow_permille;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CAMERA_MAX_SPEED_PX_S) != 0UL)
  {
    dst->camera_max_speed_px_s = src->camera_max_speed_px_s;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CAMERA_LOOKAHEAD_X_PX) != 0UL)
  {
    dst->camera_lookahead_x_px = src->camera_lookahead_x_px;
  }
  if ((field_mask & (uint32_t)GAME_RT_TUNE_CAMERA_LOOKAHEAD_Y_PX) != 0UL)
  {
    dst->camera_lookahead_y_px = src->camera_lookahead_y_px;
  }
}

static const game_runtime_ops_t *GameRuntime_FindOpsByBackendId(uint32_t backend_id)
{
  ULONG i;
  ULONG count = (ULONG)(sizeof(g_game_runtime_ops_list) / sizeof(g_game_runtime_ops_list[0]));

  for (i = 0UL; i < count; i++)
  {
    const game_runtime_ops_t *ops = g_game_runtime_ops_list[i];
    if ((ops != (const game_runtime_ops_t *)0) && (ops->backend_id == backend_id))
    {
      return ops;
    }
  }

  return (const game_runtime_ops_t *)0;
}

static const game_runtime_ops_t *GameRuntime_SelectOps(void)
{
  uint32_t requested_mode_id = GamePackage_ConsumeRequestedRuntimeModeId();
  const game_package_mode_desc_t *mode_desc;
  ULONG scenario = (ULONG)KNOB_GAME_RUNTIME_DEFAULT_SCENARIO;
  const ULONG count = (ULONG)(sizeof(g_game_runtime_ops_list) / sizeof(g_game_runtime_ops_list[0]));
  const game_package_desc_t *active_pkg = GamePackage_GetActive();

  g_game_runtime_active_mode_id = 0u;
  g_game_runtime_active_mode_cfg_base = (const game_package_runtime_config_t *)0;

  if (requested_mode_id != 0u)
  {
    mode_desc = GamePackage_FindModeById(requested_mode_id);
    if (mode_desc != (const game_package_mode_desc_t *)0)
    {
      const game_runtime_ops_t *selected = GameRuntime_FindOpsByBackendId(mode_desc->backend_id);
      if (selected != (const game_runtime_ops_t *)0)
      {
        g_game_runtime_active_mode_id = requested_mode_id;
        g_game_runtime_active_mode_cfg_base = &mode_desc->runtime_config;
        GameRuntimeRefreshResolvedModeConfig();
        return selected;
      }
    }
  }

  if (scenario >= count)
  {
    scenario = 0UL;
  }

  if ((active_pkg != (const game_package_desc_t *)0) &&
      (active_pkg->modes != (const game_package_mode_desc_t *)0))
  {
    ULONG i;
    uint32_t fallback_backend = g_game_runtime_ops_list[scenario]->backend_id;
    for (i = 0UL; i < active_pkg->mode_count; i++)
    {
      if (active_pkg->modes[i].backend_id == fallback_backend)
      {
        g_game_runtime_active_mode_id = active_pkg->modes[i].mode_id;
        g_game_runtime_active_mode_cfg_base = &active_pkg->modes[i].runtime_config;
        break;
      }
    }
  }

  GameRuntimeRefreshResolvedModeConfig();
  return g_game_runtime_ops_list[scenario];
}

void GameRuntime_Init(void)
{
  GameRuntimeKeepTuneApiSymbols();
  (void)memset(&g_game_runtime_dbg_tune_patch, 0, sizeof(g_game_runtime_dbg_tune_patch));
  g_game_runtime_dbg_tune_apply_pending = 0UL;
  g_game_runtime_dbg_tune_reset_pending = 0UL;
  g_game_runtime_dbg_tune_last_status = (ULONG)TX_NOT_DONE;
  g_game_runtime_dbg_tune_last_mode_id = 0UL;
  (void)memset(&g_game_runtime_scene_transition_req, 0, sizeof(g_game_runtime_scene_transition_req));
  g_game_runtime_scene_transition_pending = 0U;
  GameRuntime_ClearSceneMap();
  GameRuntime_ClearSceneTileset();
  g_game_runtime_active = GameRuntime_SelectOps();

  if ((g_game_runtime_active != (const game_runtime_ops_t *)0) &&
      (g_game_runtime_active->init != (void (*)(void))0))
  {
    g_game_runtime_active->init();
  }
}

void GameRuntime_Shutdown(void)
{
  if ((g_game_runtime_active != (const game_runtime_ops_t *)0) &&
      (g_game_runtime_active->shutdown != (void (*)(void))0))
  {
    g_game_runtime_active->shutdown();
  }
  (void)memset(&g_game_runtime_scene_transition_req, 0, sizeof(g_game_runtime_scene_transition_req));
  g_game_runtime_scene_transition_pending = 0U;
}

uint8_t GameRuntime_HandleControl(const game_runtime_input_t *input,
                                  uint8_t *request_exit_to_static,
                                  game_runtime_audio_cue_t *audio_cue_out)
{
  if ((g_game_runtime_active != (const game_runtime_ops_t *)0) &&
      (g_game_runtime_active->handle_control != (uint8_t (*)(const game_runtime_input_t *,
                                                            uint8_t *,
                                                            game_runtime_audio_cue_t *))0))
  {
    return g_game_runtime_active->handle_control(input, request_exit_to_static, audio_cue_out);
  }

  if (request_exit_to_static != (uint8_t *)0)
  {
    *request_exit_to_static = 0U;
  }
  if (audio_cue_out != (game_runtime_audio_cue_t *)0)
  {
    *audio_cue_out = GAME_RT_AUDIO_CUE_NONE;
  }

  return 0U;
}

void GameRuntime_Update(const app_sensor_snapshot_t *sensor_snapshot, uint32_t dt_ms)
{
  if ((g_game_runtime_active != (const game_runtime_ops_t *)0) &&
      (g_game_runtime_active->update != (void (*)(const app_sensor_snapshot_t *, uint32_t))0))
  {
    g_game_runtime_active->update(sensor_snapshot, dt_ms);
  }
}

void GameRuntime_DrawFrame(const app_sensor_snapshot_t *sensor_snapshot)
{
  if ((g_game_runtime_active != (const game_runtime_ops_t *)0) &&
      (g_game_runtime_active->draw_frame != (void (*)(const app_sensor_snapshot_t *))0))
  {
    g_game_runtime_active->draw_frame(sensor_snapshot);
  }
}

UINT GameRuntime_LoadSceneMapBlob(const void *blob_data, uint32_t blob_size)
{
  UINT status = GameMap_Parse(blob_data, blob_size, &g_game_runtime_scene_map);
  if (status == TX_SUCCESS)
  {
    g_game_runtime_scene_map_loaded = 1U;
  }
  else
  {
    GameRuntime_ClearSceneMap();
  }
  return status;
}

UINT GameRuntime_LoadSceneTilesetBlob(const void *blob_data, uint32_t blob_size)
{
  UINT status = GameTileset_Parse(blob_data, blob_size, &g_game_runtime_scene_tileset);
  if (status == TX_SUCCESS)
  {
    g_game_runtime_scene_tileset_loaded = 1U;
  }
  else
  {
    GameRuntime_ClearSceneTileset();
  }
  return status;
}

void GameRuntime_ClearSceneMap(void)
{
  (void)memset(&g_game_runtime_scene_map, 0, sizeof(g_game_runtime_scene_map));
  g_game_runtime_scene_map_loaded = 0U;
}

void GameRuntime_ClearSceneTileset(void)
{
  (void)memset(&g_game_runtime_scene_tileset, 0, sizeof(g_game_runtime_scene_tileset));
  g_game_runtime_scene_tileset_loaded = 0U;
}

const game_map_view_t *GameRuntime_GetSceneMap(void)
{
  if (g_game_runtime_scene_map_loaded == 0U)
  {
    return (const game_map_view_t *)0;
  }
  return &g_game_runtime_scene_map;
}

const game_tileset_view_t *GameRuntime_GetSceneTileset(void)
{
  if (g_game_runtime_scene_tileset_loaded == 0U)
  {
    return (const game_tileset_view_t *)0;
  }
  return &g_game_runtime_scene_tileset;
}

uint32_t GameRuntime_GetActiveModeId(void)
{
  return g_game_runtime_active_mode_id;
}

uint32_t GameRuntime_GetActiveBackendId(void)
{
  if (g_game_runtime_active == (const game_runtime_ops_t *)0)
  {
    return 0UL;
  }
  return g_game_runtime_active->backend_id;
}

const game_package_runtime_config_t *GameRuntime_GetActiveModeConfig(void)
{
  if (g_game_runtime_active_mode_cfg_resolved_valid == 0U)
  {
    return (const game_package_runtime_config_t *)0;
  }
  return &g_game_runtime_active_mode_cfg_resolved;
}

const game_package_runtime_config_t *GameRuntime_GetActiveModeBaseConfig(void)
{
  return g_game_runtime_active_mode_cfg_base;
}

UINT GameRuntime_ApplyActiveModeConfigRuntimeTune(const game_runtime_tune_patch_t *patch)
{
  if (patch == (const game_runtime_tune_patch_t *)0)
  {
    return TX_NOT_DONE;
  }
  if ((g_game_runtime_active_mode_id == 0UL) ||
      (g_game_runtime_active_mode_cfg_base == (const game_package_runtime_config_t *)0))
  {
    return TX_NOT_DONE;
  }
  if (patch->field_mask == 0UL)
  {
    return TX_SUCCESS;
  }

  if (g_game_runtime_tune_mode_id != g_game_runtime_active_mode_id)
  {
    (void)memset(&g_game_runtime_tune_values, 0, sizeof(g_game_runtime_tune_values));
    g_game_runtime_tune_mask = 0UL;
    g_game_runtime_tune_mode_id = g_game_runtime_active_mode_id;
  }

  GameRuntimeMergeTuneValues(&g_game_runtime_tune_values, patch, patch->field_mask);
  g_game_runtime_tune_mask |= patch->field_mask;
  GameRuntimeRefreshResolvedModeConfig();
  return TX_SUCCESS;
}

void GameRuntime_ResetActiveModeConfigRuntimeTune(void)
{
  (void)memset(&g_game_runtime_tune_values, 0, sizeof(g_game_runtime_tune_values));
  g_game_runtime_tune_mask = 0UL;
  g_game_runtime_tune_mode_id = 0UL;
  GameRuntimeRefreshResolvedModeConfig();
}

void GameRuntime_ProcessPendingDebugTune(void)
{
  UINT status = TX_SUCCESS;

  if ((g_game_runtime_dbg_tune_reset_pending != 0UL) &&
      (g_game_runtime_active_mode_cfg_base != (const game_package_runtime_config_t *)0))
  {
    GameRuntime_ResetActiveModeConfigRuntimeTune();
    g_game_runtime_dbg_tune_reset_pending = 0UL;
    g_game_runtime_dbg_tune_last_status = (ULONG)TX_SUCCESS;
    g_game_runtime_dbg_tune_last_mode_id = (ULONG)g_game_runtime_active_mode_id;
    g_game_runtime_dbg_tune_apply_ok_count++;
  }

  if (g_game_runtime_dbg_tune_apply_pending == 0UL)
  {
    return;
  }

  g_game_runtime_dbg_tune_apply_pending = 0UL;
  status = GameRuntime_ApplyActiveModeConfigRuntimeTune(&g_game_runtime_dbg_tune_patch);
  g_game_runtime_dbg_tune_last_status = (ULONG)status;
  g_game_runtime_dbg_tune_last_mode_id = (ULONG)g_game_runtime_active_mode_id;
  if (status == TX_SUCCESS)
  {
    g_game_runtime_dbg_tune_apply_ok_count++;
  }
  else
  {
    g_game_runtime_dbg_tune_apply_fail_count++;
  }
}

void GameRuntime_RequestSceneTransition(uint32_t target_map_hash, uint32_t target_spawn_hash)
{
  if (target_map_hash == 0UL)
  {
    return;
  }

  g_game_runtime_scene_transition_req.target_map_hash = target_map_hash;
  g_game_runtime_scene_transition_req.target_spawn_hash = target_spawn_hash;
  g_game_runtime_scene_transition_pending = 1U;
}

uint8_t GameRuntime_ConsumeSceneTransition(game_runtime_scene_transition_req_t *request_out)
{
  if (g_game_runtime_scene_transition_pending == 0U)
  {
    return 0U;
  }

  if (request_out != (game_runtime_scene_transition_req_t *)0)
  {
    *request_out = g_game_runtime_scene_transition_req;
  }
  g_game_runtime_scene_transition_pending = 0U;
  return 1U;
}
