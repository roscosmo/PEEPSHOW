#include "game_sprite_anim.h"

#define GAME_SPRITE_FRAME_MS_FALLBACK (120U)
#define GAME_SPRITE_TICK_STEP_LIMIT   (64U)

static const game_sprite_clip_t *GameSpriteAnim_GetClip(const game_sprite_set_t *set, uint8_t clip_index)
{
  if ((set == (const game_sprite_set_t *)0) ||
      (set->clips == (const game_sprite_clip_t *)0) ||
      (clip_index >= set->clip_count))
  {
    return (const game_sprite_clip_t *)0;
  }
  return &set->clips[clip_index];
}

static const game_sprite_dir_track_t *GameSpriteAnim_GetTrack(const game_sprite_clip_t *clip, uint8_t dir)
{
  if ((clip == (const game_sprite_clip_t *)0) ||
      (clip->dir_tracks == (const game_sprite_dir_track_t *)0) ||
      (dir >= (uint8_t)GAME_SPRITE_DIR_COUNT))
  {
    return (const game_sprite_dir_track_t *)0;
  }
  return &clip->dir_tracks[dir];
}

static uint8_t GameSpriteAnim_TrackReady(const game_sprite_dir_track_t *track)
{
  if ((track == (const game_sprite_dir_track_t *)0) ||
      (track->frames == (const game_sprite_frame_t *)0) ||
      (track->frame_count == 0U))
  {
    return 0U;
  }
  return 1U;
}

static uint8_t GameSpriteAnim_ResolveDirection(const game_sprite_clip_t *clip, uint8_t requested_dir)
{
  static const uint8_t kDiagFallback[4][2] =
  {
    {(uint8_t)GAME_SPRITE_DIR_DOWN, (uint8_t)GAME_SPRITE_DIR_RIGHT}, /* down-right */
    {(uint8_t)GAME_SPRITE_DIR_DOWN, (uint8_t)GAME_SPRITE_DIR_LEFT},  /* down-left */
    {(uint8_t)GAME_SPRITE_DIR_UP, (uint8_t)GAME_SPRITE_DIR_RIGHT},   /* up-right */
    {(uint8_t)GAME_SPRITE_DIR_UP, (uint8_t)GAME_SPRITE_DIR_LEFT}     /* up-left */
  };
  const game_sprite_dir_track_t *track;
  uint8_t dir = requested_dir;
  uint8_t i;

  if (dir >= (uint8_t)GAME_SPRITE_DIR_COUNT)
  {
    dir = (uint8_t)GAME_SPRITE_DIR_DOWN;
  }

  track = GameSpriteAnim_GetTrack(clip, dir);
  if (GameSpriteAnim_TrackReady(track) != 0U)
  {
    return dir;
  }

  if ((dir >= (uint8_t)GAME_SPRITE_DIR_DOWN_RIGHT) && (dir <= (uint8_t)GAME_SPRITE_DIR_UP_LEFT))
  {
    uint8_t diag_idx = (uint8_t)(dir - (uint8_t)GAME_SPRITE_DIR_DOWN_RIGHT);
    for (i = 0U; i < 2U; i++)
    {
      uint8_t cand = kDiagFallback[diag_idx][i];
      track = GameSpriteAnim_GetTrack(clip, cand);
      if (GameSpriteAnim_TrackReady(track) != 0U)
      {
        return cand;
      }
    }
  }

  track = GameSpriteAnim_GetTrack(clip, clip->fallback_dir);
  if (GameSpriteAnim_TrackReady(track) != 0U)
  {
    return clip->fallback_dir;
  }

  for (i = 0U; i < (uint8_t)GAME_SPRITE_DIR_COUNT; i++)
  {
    track = GameSpriteAnim_GetTrack(clip, i);
    if (GameSpriteAnim_TrackReady(track) != 0U)
    {
      return i;
    }
  }

  return (uint8_t)GAME_SPRITE_DIR_DOWN;
}

void GameSpriteAnim_Init(game_sprite_anim_state_t *state, uint8_t clip_index, uint8_t dir)
{
  if (state == (game_sprite_anim_state_t *)0)
  {
    return;
  }
  state->clip_index = clip_index;
  state->dir = dir;
  state->frame_index = 0U;
  state->frame_elapsed_ms = 0U;
}

void GameSpriteAnim_SetClip(game_sprite_anim_state_t *state, uint8_t clip_index)
{
  if (state == (game_sprite_anim_state_t *)0)
  {
    return;
  }
  if (state->clip_index == clip_index)
  {
    return;
  }
  state->clip_index = clip_index;
  state->frame_index = 0U;
  state->frame_elapsed_ms = 0U;
}

void GameSpriteAnim_SetDirection(game_sprite_anim_state_t *state, uint8_t dir, uint8_t reset_frame_on_change)
{
  if (state == (game_sprite_anim_state_t *)0)
  {
    return;
  }
  if (state->dir == dir)
  {
    return;
  }
  state->dir = dir;
  if (reset_frame_on_change != 0U)
  {
    state->frame_index = 0U;
    state->frame_elapsed_ms = 0U;
  }
}

uint8_t GameSpriteAnim_HasClip(const game_sprite_set_t *set, uint8_t clip_index)
{
  return (GameSpriteAnim_GetClip(set, clip_index) != (const game_sprite_clip_t *)0) ? 1U : 0U;
}

const game_sprite_frame_t *GameSpriteAnim_GetFrame(const game_sprite_anim_state_t *state, const game_sprite_set_t *set)
{
  const game_sprite_clip_t *clip;
  const game_sprite_dir_track_t *track;
  uint8_t resolved_dir;
  uint8_t frame_idx;

  if (state == (const game_sprite_anim_state_t *)0)
  {
    return (const game_sprite_frame_t *)0;
  }

  clip = GameSpriteAnim_GetClip(set, state->clip_index);
  if (clip == (const game_sprite_clip_t *)0)
  {
    return (const game_sprite_frame_t *)0;
  }

  resolved_dir = GameSpriteAnim_ResolveDirection(clip, state->dir);
  track = GameSpriteAnim_GetTrack(clip, resolved_dir);
  if (GameSpriteAnim_TrackReady(track) == 0U)
  {
    return (const game_sprite_frame_t *)0;
  }

  frame_idx = state->frame_index;
  if (frame_idx >= track->frame_count)
  {
    frame_idx = (uint8_t)(frame_idx % track->frame_count);
  }
  return &track->frames[frame_idx];
}

void GameSpriteAnim_Tick(game_sprite_anim_state_t *state, const game_sprite_set_t *set, uint32_t dt_ms)
{
  const game_sprite_clip_t *clip;
  const game_sprite_dir_track_t *track;
  uint8_t resolved_dir;
  uint32_t elapsed;
  uint32_t steps = 0U;

  if ((state == (game_sprite_anim_state_t *)0) || (dt_ms == 0U))
  {
    return;
  }

  clip = GameSpriteAnim_GetClip(set, state->clip_index);
  if (clip == (const game_sprite_clip_t *)0)
  {
    return;
  }

  resolved_dir = GameSpriteAnim_ResolveDirection(clip, state->dir);
  track = GameSpriteAnim_GetTrack(clip, resolved_dir);
  if (GameSpriteAnim_TrackReady(track) == 0U)
  {
    return;
  }

  if (state->frame_index >= track->frame_count)
  {
    state->frame_index = (uint8_t)(state->frame_index % track->frame_count);
    state->frame_elapsed_ms = 0U;
  }

  if (track->frame_count <= 1U)
  {
    state->frame_index = 0U;
    state->frame_elapsed_ms = 0U;
    return;
  }

  elapsed = (uint32_t)state->frame_elapsed_ms + dt_ms;
  while (steps < GAME_SPRITE_TICK_STEP_LIMIT)
  {
    uint32_t frame_ms = (uint32_t)track->frames[state->frame_index].frame_ms;
    if (frame_ms == 0U)
    {
      frame_ms = GAME_SPRITE_FRAME_MS_FALLBACK;
    }
    if (elapsed < frame_ms)
    {
      break;
    }

    elapsed -= frame_ms;
    if (state->frame_index + 1U < track->frame_count)
    {
      state->frame_index++;
    }
    else
    {
      if (clip->loop != 0U)
      {
        state->frame_index = 0U;
      }
      else
      {
        state->frame_index = (uint8_t)(track->frame_count - 1U);
        elapsed = 0U;
        break;
      }
    }

    steps++;
  }

  if (steps >= GAME_SPRITE_TICK_STEP_LIMIT)
  {
    elapsed = 0U;
  }
  state->frame_elapsed_ms = (uint16_t)elapsed;
}
