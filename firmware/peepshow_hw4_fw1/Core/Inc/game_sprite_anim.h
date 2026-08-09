#ifndef GAME_SPRITE_ANIM_H
#define GAME_SPRITE_ANIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  GAME_SPRITE_DIR_DOWN = 0U,
  GAME_SPRITE_DIR_UP = 1U,
  GAME_SPRITE_DIR_RIGHT = 2U,
  GAME_SPRITE_DIR_LEFT = 3U,
  GAME_SPRITE_DIR_DOWN_RIGHT = 4U,
  GAME_SPRITE_DIR_DOWN_LEFT = 5U,
  GAME_SPRITE_DIR_UP_RIGHT = 6U,
  GAME_SPRITE_DIR_UP_LEFT = 7U,
  GAME_SPRITE_DIR_COUNT = 8U
} game_sprite_dir_t;

typedef struct
{
  const uint8_t *color2bpp;
  const uint8_t *mask1bpp;
  uint16_t frame_ms;
} game_sprite_frame_t;

typedef struct
{
  const game_sprite_frame_t *frames;
  uint8_t frame_count;
} game_sprite_dir_track_t;

typedef struct
{
  const game_sprite_dir_track_t *dir_tracks; /* GAME_SPRITE_DIR_COUNT entries */
  uint8_t loop;
  uint8_t fallback_dir;
} game_sprite_clip_t;

typedef struct
{
  const game_sprite_clip_t *clips;
  uint8_t clip_count;
} game_sprite_set_t;

typedef struct
{
  uint8_t clip_index;
  uint8_t dir;
  uint8_t frame_index;
  uint16_t frame_elapsed_ms;
} game_sprite_anim_state_t;

void GameSpriteAnim_Init(game_sprite_anim_state_t *state, uint8_t clip_index, uint8_t dir);
void GameSpriteAnim_SetClip(game_sprite_anim_state_t *state, uint8_t clip_index);
void GameSpriteAnim_SetDirection(game_sprite_anim_state_t *state, uint8_t dir, uint8_t reset_frame_on_change);
uint8_t GameSpriteAnim_HasClip(const game_sprite_set_t *set, uint8_t clip_index);
void GameSpriteAnim_Tick(game_sprite_anim_state_t *state, const game_sprite_set_t *set, uint32_t dt_ms);
const game_sprite_frame_t *GameSpriteAnim_GetFrame(const game_sprite_anim_state_t *state, const game_sprite_set_t *set);

#ifdef __cplusplus
}
#endif

#endif /* GAME_SPRITE_ANIM_H */
