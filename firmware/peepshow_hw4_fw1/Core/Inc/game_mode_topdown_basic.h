#ifndef GAME_MODE_TOPDOWN_BASIC_H
#define GAME_MODE_TOPDOWN_BASIC_H

#include <stdint.h>

#include "game_runtime.h"
#include "game_sprite_anim.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_MODE_TOPDOWN_BASIC_SNAPSHOT_VERSION (1UL)

typedef struct
{
  uint32_t version;
  uint8_t valid;
  uint8_t spawn_locked;
  uint8_t camera_initialized;
  uint8_t exit_zone_latched;
  int32_t player_x_px;
  int32_t player_y_px;
  float player_vx_px_s;
  float player_vy_px_s;
  float player_rem_x_px;
  float player_rem_y_px;
  float cam_x_px;
  float cam_y_px;
  uint32_t spawn_request_hash;
  game_sprite_anim_state_t player_anim;
} game_mode_topdown_basic_snapshot_t;

void GameModeTopdownBasic_Reset(void);
uint8_t GameModeTopdownBasic_SaveSnapshot(game_mode_topdown_basic_snapshot_t *snapshot_out);
uint8_t GameModeTopdownBasic_LoadSnapshot(const game_mode_topdown_basic_snapshot_t *snapshot);
uint8_t GameModeTopdownBasic_HandleControl(const game_runtime_input_t *input,
                                           uint8_t *request_exit_to_static,
                                           game_runtime_audio_cue_t *audio_cue_out);
void GameModeTopdownBasic_Update(const app_sensor_snapshot_t *sensor_snapshot, uint32_t dt_ms);
void GameModeTopdownBasic_DrawFrame(const app_sensor_snapshot_t *sensor_snapshot);
void GameModeTopdownBasic_RequestSpawn(uint32_t spawn_hash, uint8_t require_exact);
uint32_t GameModeTopdownBasic_LastInteractScriptHash(void);
uint32_t GameModeTopdownBasic_LastInteractDialogueHash(void);

#ifdef __cplusplus
}
#endif

#endif /* GAME_MODE_TOPDOWN_BASIC_H */
