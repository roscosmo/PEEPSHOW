#include "game_mode_topdown_basic.h"

#include <string.h>

#include "display_renderer.h"
#include "game_map.h"
#include "game_sprite_anim.h"
#include "game_tileset.h"
#include "stm32u5xx_hal.h"
#include "../../Assets/sprites/player_sprites_autogen.h"

#define TOPDOWN_PLAYER_HITBOX_W_PX    (12)
#define TOPDOWN_PLAYER_HITBOX_H_PX    (12)
#define TOPDOWN_PLAYER_SPR_W_PX       ((uint16_t)PLAYER_SPRITE_W_PX)
#define TOPDOWN_PLAYER_SPR_H_PX       ((uint16_t)PLAYER_SPRITE_H_PX)
#define TOPDOWN_PLAYER_SPR_COLOR_ROW_BYTES ((uint16_t)PLAYER_SPRITE_COLOR_ROW_BYTES)
#define TOPDOWN_PLAYER_SPR_MASK_ROW_BYTES  ((uint16_t)PLAYER_SPRITE_MASK_ROW_BYTES)
#define TOPDOWN_PLAYER_BASE_SPR_SCALE (1U)
#define TOPDOWN_PLAYER_FACING_SWITCH_MARGIN_PX (3)
#define TOPDOWN_TILE_CACHE_MAX_ENTRIES (192U)
#define TOPDOWN_TILE_CACHE_MAX_DIM_PX (32U)
#define TOPDOWN_TILE_CACHE_MAX_ROW_BYTES ((TOPDOWN_TILE_CACHE_MAX_DIM_PX + 7U) / 8U)
#define TOPDOWN_TILE_CACHE_MAX_PLANE_BYTES (TOPDOWN_TILE_CACHE_MAX_ROW_BYTES * TOPDOWN_TILE_CACHE_MAX_DIM_PX)
#define TOPDOWN_PLAYER_CACHE_MAX_ENTRIES (16U)
#define TOPDOWN_PLAYER_CACHE_MAX_DIM_PX (64U)
#define TOPDOWN_PLAYER_CACHE_MAX_ROW_BYTES ((TOPDOWN_PLAYER_CACHE_MAX_DIM_PX + 7U) / 8U)
#define TOPDOWN_PLAYER_CACHE_MAX_PLANE_BYTES (TOPDOWN_PLAYER_CACHE_MAX_ROW_BYTES * TOPDOWN_PLAYER_CACHE_MAX_DIM_PX)

typedef struct
{
  uint8_t initialized;
  uint8_t spawn_locked;
  uint8_t spawn_require_exact;
  uint8_t camera_initialized;
  uint8_t map_cache_valid;
  uint8_t clear_pending;
  uint8_t player_rect_valid;
  uint8_t hud_valid;
  int32_t player_x_px;
  int32_t player_y_px;
  float player_vx_px_s;
  float player_vy_px_s;
  float player_rem_x_px;
  float player_rem_y_px;
  float cam_x_px;
  float cam_y_px;
  uint32_t held_mask;
  uint16_t fps_last;
  uint16_t fps_frames_in_window;
  uint32_t fps_window_start_ms;
  uint32_t last_cam_x_px;
  uint32_t last_cam_y_px;
  uint32_t last_clk_mhz;
  uint16_t last_hud_fps;
  uint8_t last_present_mode;
  uint8_t last_blit_scale;
  uint8_t last_effective_scale;
  int16_t player_prev_sx;
  int16_t player_prev_sy;
  uint16_t player_prev_w;
  uint16_t player_prev_h;
  const uint8_t *player_prev_color2bpp;
  const uint8_t *player_prev_mask1bpp;
  const game_map_blob_header_t *map_cache_header;
  const game_tileset_blob_header_t *tileset_cache_header;
  uint32_t active_scene_crc32;
  uint32_t spawn_request_hash;
  uint8_t exit_zone_latched;
  uint32_t last_interact_script_hash;
  uint32_t last_interact_dialogue_hash;
  game_sprite_anim_state_t player_anim;
} topdown_state_t;

typedef struct
{
  uint8_t valid;
  uint8_t width_px;
  uint8_t height_px;
  uint8_t stride_bytes;
  uint16_t gid;
  uint8_t on_plane[TOPDOWN_TILE_CACHE_MAX_PLANE_BYTES];
  uint8_t mask_plane[TOPDOWN_TILE_CACHE_MAX_PLANE_BYTES];
} topdown_tile_cache_entry_t;

typedef struct
{
  uint8_t context_valid;
  uint8_t present_mode;
  uint8_t blit_scale;
  uint8_t pixel_scale;
  uint16_t tile_w;
  uint16_t tile_h;
  uint16_t next_victim;
  uint32_t tileset_crc32;
  uint32_t tileset_tile_count;
  const game_tileset_blob_header_t *tileset_header;
  topdown_tile_cache_entry_t entries[TOPDOWN_TILE_CACHE_MAX_ENTRIES];
} topdown_tile_cache_t;

typedef struct
{
  uint8_t valid;
  uint8_t width_px;
  uint8_t height_px;
  uint8_t stride_bytes;
  uint8_t present_mode;
  uint8_t blit_scale;
  const uint8_t *color_ptr;
  const uint8_t *mask_ptr;
  uint8_t on_plane[TOPDOWN_PLAYER_CACHE_MAX_PLANE_BYTES];
  uint8_t mask_plane[TOPDOWN_PLAYER_CACHE_MAX_PLANE_BYTES];
} topdown_player_cache_entry_t;

typedef struct
{
  uint16_t next_victim;
  topdown_player_cache_entry_t entries[TOPDOWN_PLAYER_CACHE_MAX_ENTRIES];
} topdown_player_cache_t;

static topdown_state_t s_topdown;
static topdown_tile_cache_t s_topdown_tile_cache;
static topdown_player_cache_t s_topdown_player_cache;

static int32_t TopdownClampI32(int32_t value, int32_t min_v, int32_t max_v);
static uint8_t TopdownCanOccupyPx(const game_map_view_t *map_view, int32_t cx_px, int32_t cy_px);
static uint8_t TopdownMapMetrics(const game_map_view_t *map_view,
                                 uint32_t *map_w_px_out,
                                 uint32_t *map_h_px_out,
                                 uint16_t *tile_w_out,
                                 uint16_t *tile_h_out);
static void TopdownTileCacheInvalidate(void);
static void TopdownTileCacheEnsureContext(const game_tileset_blob_header_t *tileset_header,
                                          uint16_t tile_w,
                                          uint16_t tile_h,
                                          uint8_t blit_scale,
                                          render_2bpp_present_t present_mode);
static uint8_t TopdownTileCacheTryDrawTile(uint16_t gid,
                                           int32_t dst_x,
                                           int32_t dst_y,
                                           uint16_t tile_w,
                                           uint16_t tile_h,
                                           const uint8_t *src_color2bpp,
                                           const uint8_t *src_mask1bpp,
                                           uint32_t src_color_stride_bytes,
                                           uint32_t src_mask_stride_bytes,
                                           uint8_t blit_scale,
                                           render_2bpp_present_t present_mode,
                                           render_layer_t layer,
                                           uint8_t fast_no_clip_ok);
static uint8_t TopdownPlayerCacheTryDraw(const uint8_t *color2bpp,
                                         const uint8_t *mask1bpp,
                                         int32_t dst_x,
                                         int32_t dst_y,
                                         uint8_t blit_scale,
                                         render_2bpp_present_t present_mode,
                                         render_layer_t layer);

static uint32_t TopdownFnv1a32(const char *text)
{
  uint32_t h = 0x811C9DC5UL;
  const uint8_t *p = (const uint8_t *)text;

  if (p == (const uint8_t *)0)
  {
    return 0UL;
  }

  while (*p != 0U)
  {
    h ^= (uint32_t)(*p++);
    h *= 0x01000193UL;
  }
  return h;
}

static uint8_t TopdownMapViewValid(const game_map_view_t *map_view)
{
  return ((map_view != (const game_map_view_t *)0) &&
          (map_view->header != (const game_map_blob_header_t *)0) &&
          (map_view->objects != (const game_map_object_t *)0) &&
          (map_view->object_count > 0UL))
             ? 1U
             : 0U;
}

static void TopdownPlayerHitboxRectPx(int32_t *left_px_out,
                                      int32_t *top_px_out,
                                      int32_t *right_px_out,
                                      int32_t *bottom_px_out)
{
  int32_t left_px = s_topdown.player_x_px - (TOPDOWN_PLAYER_HITBOX_W_PX / 2);
  int32_t right_px = s_topdown.player_x_px + ((TOPDOWN_PLAYER_HITBOX_W_PX - 1) / 2);
  int32_t top_px = s_topdown.player_y_px - (TOPDOWN_PLAYER_HITBOX_H_PX / 2);
  int32_t bottom_px = s_topdown.player_y_px + ((TOPDOWN_PLAYER_HITBOX_H_PX - 1) / 2);

  if (left_px_out != (int32_t *)0)
  {
    *left_px_out = left_px;
  }
  if (top_px_out != (int32_t *)0)
  {
    *top_px_out = top_px;
  }
  if (right_px_out != (int32_t *)0)
  {
    *right_px_out = right_px;
  }
  if (bottom_px_out != (int32_t *)0)
  {
    *bottom_px_out = bottom_px;
  }
}

static void TopdownObjectRectPx(const game_map_object_t *obj,
                                int32_t *left_px_out,
                                int32_t *top_px_out,
                                int32_t *right_px_out,
                                int32_t *bottom_px_out)
{
  int32_t left_px;
  int32_t top_px;
  int32_t w_px;
  int32_t h_px;
  int32_t right_px;
  int32_t bottom_px;

  if (obj == (const game_map_object_t *)0)
  {
    return;
  }

  left_px = (int32_t)obj->x_px;
  top_px = (int32_t)obj->y_px;
  w_px = (int32_t)obj->w_px;
  h_px = (int32_t)obj->h_px;
  if (w_px <= 0)
  {
    w_px = 1;
  }
  if (h_px <= 0)
  {
    h_px = 1;
  }
  right_px = left_px + w_px - 1;
  bottom_px = top_px + h_px - 1;

  if (left_px_out != (int32_t *)0)
  {
    *left_px_out = left_px;
  }
  if (top_px_out != (int32_t *)0)
  {
    *top_px_out = top_px;
  }
  if (right_px_out != (int32_t *)0)
  {
    *right_px_out = right_px;
  }
  if (bottom_px_out != (int32_t *)0)
  {
    *bottom_px_out = bottom_px;
  }
}

static uint8_t TopdownRectIntersects(int32_t a_left,
                                     int32_t a_top,
                                     int32_t a_right,
                                     int32_t a_bottom,
                                     int32_t b_left,
                                     int32_t b_top,
                                     int32_t b_right,
                                     int32_t b_bottom)
{
  if (a_right < b_left)
  {
    return 0U;
  }
  if (b_right < a_left)
  {
    return 0U;
  }
  if (a_bottom < b_top)
  {
    return 0U;
  }
  if (b_bottom < a_top)
  {
    return 0U;
  }
  return 1U;
}

static uint8_t TopdownObjectOverlapsPlayerHitbox(const game_map_object_t *obj)
{
  int32_t p_left;
  int32_t p_top;
  int32_t p_right;
  int32_t p_bottom;
  int32_t o_left;
  int32_t o_top;
  int32_t o_right;
  int32_t o_bottom;

  if (obj == (const game_map_object_t *)0)
  {
    return 0U;
  }

  TopdownPlayerHitboxRectPx(&p_left, &p_top, &p_right, &p_bottom);
  TopdownObjectRectPx(obj, &o_left, &o_top, &o_right, &o_bottom);
  return TopdownRectIntersects(p_left, p_top, p_right, p_bottom,
                               o_left, o_top, o_right, o_bottom);
}

static void TopdownSpawnPointPxFromObject(const game_map_object_t *obj, int32_t *x_px_out, int32_t *y_px_out)
{
  int32_t left_px;
  int32_t top_px;
  int32_t right_px;
  int32_t bottom_px;

  if (obj == (const game_map_object_t *)0)
  {
    return;
  }

  TopdownObjectRectPx(obj, &left_px, &top_px, &right_px, &bottom_px);
  if (x_px_out != (int32_t *)0)
  {
    *x_px_out = left_px + ((right_px - left_px) / 2);
  }
  if (y_px_out != (int32_t *)0)
  {
    *y_px_out = top_px + ((bottom_px - top_px) / 2);
  }
}

static uint8_t TopdownFindSpawnPx(const game_map_view_t *map_view,
                                  uint32_t requested_spawn_hash,
                                  uint8_t allow_fallback,
                                  int32_t *spawn_x_px_out,
                                  int32_t *spawn_y_px_out,
                                  uint32_t *resolved_spawn_hash_out)
{
  uint32_t i;
  const uint32_t kind_player_hash = TopdownFnv1a32("player");
  const game_map_object_t *fallback_player = (const game_map_object_t *)0;
  const game_map_object_t *fallback_any = (const game_map_object_t *)0;

  if (TopdownMapViewValid(map_view) == 0U)
  {
    return 0U;
  }

  for (i = 0UL; i < map_view->object_count; i++)
  {
    const game_map_object_t *obj = &map_view->objects[i];
    if ((uint32_t)obj->type != (uint32_t)GAME_MAP_OBJECT_SPAWN)
    {
      continue;
    }

    if ((obj->arg0 == kind_player_hash) && (fallback_player == (const game_map_object_t *)0))
    {
      fallback_player = obj;
    }
    if (fallback_any == (const game_map_object_t *)0)
    {
      fallback_any = obj;
    }

    if ((requested_spawn_hash != 0UL) &&
        (obj->arg1 == requested_spawn_hash))
    {
      TopdownSpawnPointPxFromObject(obj, spawn_x_px_out, spawn_y_px_out);
      if (resolved_spawn_hash_out != (uint32_t *)0)
      {
        *resolved_spawn_hash_out = obj->arg1;
      }
      return 1U;
    }
  }

  if ((allow_fallback != 0U) && (fallback_player != (const game_map_object_t *)0))
  {
    TopdownSpawnPointPxFromObject(fallback_player, spawn_x_px_out, spawn_y_px_out);
    if (resolved_spawn_hash_out != (uint32_t *)0)
    {
      *resolved_spawn_hash_out = fallback_player->arg1;
    }
    return 1U;
  }

  if ((allow_fallback != 0U) && (fallback_any != (const game_map_object_t *)0))
  {
    TopdownSpawnPointPxFromObject(fallback_any, spawn_x_px_out, spawn_y_px_out);
    if (resolved_spawn_hash_out != (uint32_t *)0)
    {
      *resolved_spawn_hash_out = fallback_any->arg1;
    }
    return 1U;
  }

  return 0U;
}

static void TopdownApplySpawnPosition(const game_map_view_t *map_view, int32_t spawn_x_px, int32_t spawn_y_px)
{
  uint32_t map_w_px = 0UL;
  uint32_t map_h_px = 0UL;

  if (TopdownMapMetrics(map_view, &map_w_px, &map_h_px, (uint16_t *)0, (uint16_t *)0) != 0U)
  {
    spawn_x_px = TopdownClampI32(spawn_x_px, 0, (map_w_px > 0UL) ? ((int32_t)map_w_px - 1) : 0);
    spawn_y_px = TopdownClampI32(spawn_y_px, 0, (map_h_px > 0UL) ? ((int32_t)map_h_px - 1) : 0);
  }

  s_topdown.player_x_px = spawn_x_px;
  s_topdown.player_y_px = spawn_y_px;
  s_topdown.player_vx_px_s = 0.0f;
  s_topdown.player_vy_px_s = 0.0f;
  s_topdown.player_rem_x_px = 0.0f;
  s_topdown.player_rem_y_px = 0.0f;
  s_topdown.camera_initialized = 0U;
  s_topdown.cam_x_px = 0.0f;
  s_topdown.cam_y_px = 0.0f;
  s_topdown.spawn_locked = 1U;
  s_topdown.spawn_require_exact = 0U;
}

static uint8_t TopdownTryTeleportToSpawnHash(const game_map_view_t *map_view, uint32_t spawn_hash)
{
  int32_t spawn_x_px;
  int32_t spawn_y_px;
  uint32_t resolved_spawn_hash = 0UL;

  if (spawn_hash == 0UL)
  {
    return 0U;
  }
  if (TopdownFindSpawnPx(map_view,
                         spawn_hash,
                         0U,
                         &spawn_x_px,
                         &spawn_y_px,
                         &resolved_spawn_hash) == 0U)
  {
    return 0U;
  }
  if (TopdownCanOccupyPx(map_view, spawn_x_px, spawn_y_px) == 0U)
  {
    return 0U;
  }

  TopdownApplySpawnPosition(map_view, spawn_x_px, spawn_y_px);
  s_topdown.spawn_request_hash = resolved_spawn_hash;
  return 1U;
}

static void TopdownHandleExitOverlap(const game_map_view_t *map_view)
{
  uint32_t i;
  const game_map_object_t *exit_obj = (const game_map_object_t *)0;

  if (TopdownMapViewValid(map_view) == 0U)
  {
    s_topdown.exit_zone_latched = 0U;
    return;
  }

  for (i = 0UL; i < map_view->object_count; i++)
  {
    const game_map_object_t *obj = &map_view->objects[i];
    if ((uint32_t)obj->type != (uint32_t)GAME_MAP_OBJECT_EXIT)
    {
      continue;
    }
    if (TopdownObjectOverlapsPlayerHitbox(obj) != 0U)
    {
      exit_obj = obj;
      break;
    }
  }

  if (exit_obj == (const game_map_object_t *)0)
  {
    s_topdown.exit_zone_latched = 0U;
    return;
  }

  if (s_topdown.exit_zone_latched != 0U)
  {
    return;
  }

  s_topdown.exit_zone_latched = 1U;
  if (exit_obj->arg0 != 0UL)
  {
    GameRuntime_RequestSceneTransition(exit_obj->arg0, exit_obj->arg1);
    return;
  }
  if (exit_obj->arg1 != 0UL)
  {
    (void)TopdownTryTeleportToSpawnHash(map_view, exit_obj->arg1);
  }
}

static const game_map_object_t *TopdownFindOverlappingInteract(const game_map_view_t *map_view)
{
  uint32_t i;

  if (TopdownMapViewValid(map_view) == 0U)
  {
    return (const game_map_object_t *)0;
  }

  for (i = 0UL; i < map_view->object_count; i++)
  {
    const game_map_object_t *obj = &map_view->objects[i];
    if ((uint32_t)obj->type != (uint32_t)GAME_MAP_OBJECT_INTERACT)
    {
      continue;
    }
    if (TopdownObjectOverlapsPlayerHitbox(obj) != 0U)
    {
      return obj;
    }
  }

  return (const game_map_object_t *)0;
}

static const game_package_runtime_config_t *TopdownCfg(void)
{
  return GameRuntime_GetActiveModeConfig();
}

static uint8_t TopdownRenderScale(void)
{
  const game_package_runtime_config_t *cfg = TopdownCfg();
  ULONG scale = (cfg != (const game_package_runtime_config_t *)0)
                    ? (ULONG)cfg->topdown_render_scale
                    : 2UL;
  if (scale < 1UL)
  {
    scale = 1UL;
  }
  if (scale > 4UL)
  {
    scale = 4UL;
  }
  return (uint8_t)scale;
}

static render_2bpp_present_t TopdownPresentMode(uint8_t scale)
{
  const game_package_runtime_config_t *cfg = TopdownCfg();
  ULONG mode = (cfg != (const game_package_runtime_config_t *)0)
                   ? (ULONG)cfg->topdown_tile_present_mode
                   : (ULONG)GAME_PACKAGE_TOPDOWN_PRESENT_AUTO;

  if (mode == (ULONG)GAME_PACKAGE_TOPDOWN_PRESENT_BINARY)
  {
    return RENDER_2BPP_PRESENT_BINARY_CLAMP;
  }
  if (mode == (ULONG)GAME_PACKAGE_TOPDOWN_PRESENT_BAYER)
  {
    return RENDER_2BPP_PRESENT_BAYER2X2;
  }
  if (scale <= 1U)
  {
    return RENDER_2BPP_PRESENT_BINARY_CLAMP;
  }
  return RENDER_2BPP_PRESENT_BAYER2X2;
}

static uint32_t TopdownBitForSource(ULONG source)
{
  if ((source < 1UL) || (source > 31UL))
  {
    return 0UL;
  }

  return (1UL << (source - 1UL));
}

static uint8_t TopdownHeld(ULONG source)
{
  uint32_t bit = TopdownBitForSource(source);
  if (bit == 0UL)
  {
    return 0U;
  }
  return ((s_topdown.held_mask & bit) != 0UL) ? 1U : 0U;
}

static int32_t TopdownAbsI32(int32_t value)
{
  return (value >= 0) ? value : -value;
}

static int32_t TopdownClampI32(int32_t value, int32_t min_v, int32_t max_v)
{
  if (value < min_v)
  {
    return min_v;
  }
  if (value > max_v)
  {
    return max_v;
  }
  return value;
}

static float TopdownAbsF(float value)
{
  return (value >= 0.0f) ? value : -value;
}

static uint8_t TopdownPlayerIdleClipIndex(void)
{
  if (GameSpriteAnim_HasClip(&player_sprite_set, PLAYER_CLIP_IDLE_INDEX) != 0U)
  {
    return PLAYER_CLIP_IDLE_INDEX;
  }
  if (player_sprite_set.clip_count > 0U)
  {
    return 0U;
  }
  return 0xFFU;
}

static uint8_t TopdownPlayerWalkClipIndex(void)
{
  if (GameSpriteAnim_HasClip(&player_sprite_set, PLAYER_CLIP_WALK_INDEX) != 0U)
  {
    return PLAYER_CLIP_WALK_INDEX;
  }
  return TopdownPlayerIdleClipIndex();
}

static uint8_t TopdownClipHasDirection(uint8_t clip_index, uint8_t dir)
{
  const game_sprite_clip_t *clip;
  const game_sprite_dir_track_t *track;
  if (dir >= (uint8_t)GAME_SPRITE_DIR_COUNT)
  {
    return 0U;
  }
  if (GameSpriteAnim_HasClip(&player_sprite_set, clip_index) == 0U)
  {
    return 0U;
  }
  clip = &player_sprite_set.clips[clip_index];
  if (clip->dir_tracks == (const game_sprite_dir_track_t *)0)
  {
    return 0U;
  }
  track = &clip->dir_tracks[dir];
  return ((track->frames != (const game_sprite_frame_t *)0) && (track->frame_count > 0U)) ? 1U : 0U;
}

static uint8_t TopdownFacingIsHorizontal(uint8_t facing)
{
  return ((facing == (uint8_t)GAME_SPRITE_DIR_LEFT) ||
          (facing == (uint8_t)GAME_SPRITE_DIR_RIGHT))
             ? 1U
             : 0U;
}

static uint8_t TopdownFacingFromDelta(int32_t dx, int32_t dy, uint8_t fallback_facing)
{
  const int32_t switch_margin_px = TOPDOWN_PLAYER_FACING_SWITCH_MARGIN_PX;
  int32_t abs_x = TopdownAbsI32(dx);
  int32_t abs_y = TopdownAbsI32(dy);
  uint8_t facing = fallback_facing;

  if (facing >= (uint8_t)GAME_SPRITE_DIR_COUNT)
  {
    facing = (uint8_t)GAME_SPRITE_DIR_DOWN;
  }

  if ((abs_x == 0) && (abs_y == 0))
  {
    return facing;
  }

  if (abs_x == 0)
  {
    return (dy >= 0) ? (uint8_t)GAME_SPRITE_DIR_DOWN : (uint8_t)GAME_SPRITE_DIR_UP;
  }

  if (abs_y == 0)
  {
    return (dx >= 0) ? (uint8_t)GAME_SPRITE_DIR_RIGHT : (uint8_t)GAME_SPRITE_DIR_LEFT;
  }

  {
    uint8_t walk_clip = TopdownPlayerWalkClipIndex();
    uint8_t diag_dir = (dy >= 0)
                           ? ((dx >= 0) ? (uint8_t)GAME_SPRITE_DIR_DOWN_RIGHT : (uint8_t)GAME_SPRITE_DIR_DOWN_LEFT)
                           : ((dx >= 0) ? (uint8_t)GAME_SPRITE_DIR_UP_RIGHT : (uint8_t)GAME_SPRITE_DIR_UP_LEFT);
    if ((walk_clip != 0xFFU) &&
        (TopdownClipHasDirection(walk_clip, diag_dir) != 0U) &&
        (abs_x >= (abs_y - switch_margin_px)) &&
        (abs_y >= (abs_x - switch_margin_px)))
    {
      return diag_dir;
    }
  }

  if (TopdownFacingIsHorizontal(facing) != 0U)
  {
    if (abs_y > (abs_x + switch_margin_px))
    {
      return (dy >= 0) ? (uint8_t)GAME_SPRITE_DIR_DOWN : (uint8_t)GAME_SPRITE_DIR_UP;
    }
    return (dx >= 0) ? (uint8_t)GAME_SPRITE_DIR_RIGHT : (uint8_t)GAME_SPRITE_DIR_LEFT;
  }

  if (abs_x > (abs_y + switch_margin_px))
  {
    return (dx >= 0) ? (uint8_t)GAME_SPRITE_DIR_RIGHT : (uint8_t)GAME_SPRITE_DIR_LEFT;
  }
  return (dy >= 0) ? (uint8_t)GAME_SPRITE_DIR_DOWN : (uint8_t)GAME_SPRITE_DIR_UP;
}

static const game_sprite_frame_t *TopdownCurrentPlayerFrame(void)
{
  return GameSpriteAnim_GetFrame(&s_topdown.player_anim, &player_sprite_set);
}

static void TopdownUpdatePlayerAnim(int32_t moved_x, int32_t moved_y, uint32_t dt_ms)
{
  uint8_t moving = 0U;
  uint8_t next_facing = s_topdown.player_anim.dir;
  uint8_t idle_clip = TopdownPlayerIdleClipIndex();
  uint8_t walk_clip = TopdownPlayerWalkClipIndex();
  uint8_t next_clip = idle_clip;

  if (idle_clip == 0xFFU)
  {
    return;
  }

  if ((moved_x != 0) || (moved_y != 0))
  {
    moving = 1U;
    next_facing = TopdownFacingFromDelta(moved_x, moved_y, next_facing);
  }
  else
  {
    int32_t vx_dir = 0;
    int32_t vy_dir = 0;
    if (s_topdown.player_vx_px_s > 0.5f)
    {
      vx_dir = 1;
    }
    else if (s_topdown.player_vx_px_s < -0.5f)
    {
      vx_dir = -1;
    }
    if (s_topdown.player_vy_px_s > 0.5f)
    {
      vy_dir = 1;
    }
    else if (s_topdown.player_vy_px_s < -0.5f)
    {
      vy_dir = -1;
    }
    if ((vx_dir != 0) || (vy_dir != 0))
    {
      moving = 1U;
      next_facing = TopdownFacingFromDelta(vx_dir, vy_dir, next_facing);
    }
  }

  if (moving != 0U)
  {
    next_clip = walk_clip;
  }

  if (next_clip == 0xFFU)
  {
    next_clip = idle_clip;
  }

  GameSpriteAnim_SetDirection(&s_topdown.player_anim, next_facing, 1U);
  GameSpriteAnim_SetClip(&s_topdown.player_anim, next_clip);
  GameSpriteAnim_Tick(&s_topdown.player_anim, &player_sprite_set, dt_ms);
}

static float TopdownClampF(float value, float min_v, float max_v)
{
  if (value < min_v)
  {
    return min_v;
  }
  if (value > max_v)
  {
    return max_v;
  }
  return value;
}

static uint32_t TopdownControllerProfile(void)
{
  const game_package_runtime_config_t *cfg = TopdownCfg();
  uint32_t profile = (cfg != (const game_package_runtime_config_t *)0)
                         ? cfg->controller_profile_id
                         : (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_TOPDOWN_ANALOG;

  if ((profile != (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_TOPDOWN_DIGITAL_8DIR) &&
      (profile != (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_TOPDOWN_ANALOG) &&
      (profile != (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_SIDESCROLL_PLATFORMER) &&
      (profile != (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_MINIGAME_CURSOR))
  {
    profile = (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_TOPDOWN_ANALOG;
  }
  return profile;
}

static uint32_t TopdownInputFlags(void)
{
  const game_package_runtime_config_t *cfg = TopdownCfg();
  return (cfg != (const game_package_runtime_config_t *)0)
             ? cfg->input_flags
             : (uint32_t)(GAME_PACKAGE_INPUT_FLAG_NORMALIZE_DIAGONAL |
                          GAME_PACKAGE_INPUT_FLAG_ANALOG_PREFERRED);
}

static uint32_t TopdownMoveSpeedPxPerSec(void)
{
  const game_package_runtime_config_t *cfg = TopdownCfg();
  uint32_t speed = (cfg != (const game_package_runtime_config_t *)0)
                       ? cfg->move_speed_px_s
                       : 72UL;
  if (speed < 8UL)
  {
    speed = 8UL;
  }
  if (speed > 512UL)
  {
    speed = 512UL;
  }
  return speed;
}

static uint32_t TopdownMoveAccelPxPerSec2(void)
{
  const game_package_runtime_config_t *cfg = TopdownCfg();
  uint32_t accel = (cfg != (const game_package_runtime_config_t *)0)
                       ? cfg->move_accel_px_s2
                       : 480UL;
  if (accel < 16UL)
  {
    accel = 16UL;
  }
  if (accel > 4096UL)
  {
    accel = 4096UL;
  }
  return accel;
}

static uint32_t TopdownMoveDecelPxPerSec2(void)
{
  const game_package_runtime_config_t *cfg = TopdownCfg();
  uint32_t decel = (cfg != (const game_package_runtime_config_t *)0)
                       ? cfg->move_decel_px_s2
                       : 640UL;
  if (decel < 16UL)
  {
    decel = 16UL;
  }
  if (decel > 4096UL)
  {
    decel = 4096UL;
  }
  return decel;
}

static uint32_t TopdownCameraProfile(void)
{
  const game_package_runtime_config_t *cfg = TopdownCfg();
  uint32_t profile = (cfg != (const game_package_runtime_config_t *)0)
                         ? cfg->camera_profile_id
                         : (uint32_t)GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_DEADZONE;

  if ((profile != (uint32_t)GAME_PACKAGE_CAMERA_PROFILE_LOCKED) &&
      (profile != (uint32_t)GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_X) &&
      (profile != (uint32_t)GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_XY) &&
      (profile != (uint32_t)GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_DEADZONE))
  {
    profile = (uint32_t)GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_DEADZONE;
  }
  return profile;
}

static uint32_t TopdownCameraDeadzoneWPx(void)
{
  const game_package_runtime_config_t *cfg = TopdownCfg();
  return (cfg != (const game_package_runtime_config_t *)0) ? cfg->camera_deadzone_w_px : 24UL;
}

static uint32_t TopdownCameraDeadzoneHPx(void)
{
  const game_package_runtime_config_t *cfg = TopdownCfg();
  return (cfg != (const game_package_runtime_config_t *)0) ? cfg->camera_deadzone_h_px : 24UL;
}

static uint32_t TopdownCameraFollowPermille(void)
{
  const game_package_runtime_config_t *cfg = TopdownCfg();
  uint32_t value = (cfg != (const game_package_runtime_config_t *)0) ? cfg->camera_follow_permille : 280UL;
  if (value > 1000UL)
  {
    value = 1000UL;
  }
  return value;
}

static uint32_t TopdownCameraMaxSpeedPxPerSec(void)
{
  const game_package_runtime_config_t *cfg = TopdownCfg();
  return (cfg != (const game_package_runtime_config_t *)0) ? cfg->camera_max_speed_px_s : 240UL;
}

static int32_t TopdownCameraLookaheadXPx(void)
{
  const game_package_runtime_config_t *cfg = TopdownCfg();
  return (cfg != (const game_package_runtime_config_t *)0) ? cfg->camera_lookahead_x_px : 0;
}

static int32_t TopdownCameraLookaheadYPx(void)
{
  const game_package_runtime_config_t *cfg = TopdownCfg();
  return (cfg != (const game_package_runtime_config_t *)0) ? cfg->camera_lookahead_y_px : 0;
}

static float TopdownSignedStepFromFloat(float value)
{
  if (value > 0.0f)
  {
    return 1.0f;
  }
  if (value < 0.0f)
  {
    return -1.0f;
  }
  return 0.0f;
}

static void TopdownBuildMoveIntent(const app_sensor_snapshot_t *sensor_snapshot,
                                   float *out_x,
                                   float *out_y)
{
  float digital_x;
  float digital_y;
  float analog_x = 0.0f;
  float analog_y = 0.0f;
  uint8_t analog_active = 0U;
  int32_t dir_x = 0;
  int32_t dir_y = 0;
  uint32_t profile;
  uint32_t flags;

  if ((out_x == (float *)0) || (out_y == (float *)0))
  {
    return;
  }

  if (TopdownHeld((ULONG)GAME_RT_INPUT_SRC_JOY_LEFT) != 0U)
  {
    dir_x--;
  }
  if (TopdownHeld((ULONG)GAME_RT_INPUT_SRC_JOY_RIGHT) != 0U)
  {
    dir_x++;
  }
  if (TopdownHeld((ULONG)GAME_RT_INPUT_SRC_JOY_UP) != 0U)
  {
    dir_y--;
  }
  if (TopdownHeld((ULONG)GAME_RT_INPUT_SRC_JOY_DOWN) != 0U)
  {
    dir_y++;
  }

  digital_x = (float)dir_x;
  digital_y = (float)dir_y;
  flags = TopdownInputFlags();
  if (((flags & (uint32_t)GAME_PACKAGE_INPUT_FLAG_NORMALIZE_DIAGONAL) != 0UL) &&
      (digital_x != 0.0f) &&
      (digital_y != 0.0f))
  {
    digital_x *= 0.70710678f;
    digital_y *= 0.70710678f;
  }

  if ((sensor_snapshot != (const app_sensor_snapshot_t *)0) &&
      ((sensor_snapshot->valid_mask & APP_SENSOR_SNAPSHOT_VALID_JOY) != 0UL))
  {
    if ((sensor_snapshot->joy_deadzone_enabled != 0UL) &&
        (sensor_snapshot->joy_r_abs_mT <= (sensor_snapshot->joy_deadzone_mT + 0.05f)))
    {
      analog_x = 0.0f;
      analog_y = 0.0f;
    }
    else
    {
      analog_x = TopdownClampF(sensor_snapshot->joy_nx, -1.0f, 1.0f);
      analog_y = -TopdownClampF(sensor_snapshot->joy_ny, -1.0f, 1.0f);
    }
    if ((TopdownAbsF(analog_x) >= 0.001f) || (TopdownAbsF(analog_y) >= 0.001f))
    {
      analog_active = 1U;
    }
  }

  profile = TopdownControllerProfile();
  if (profile == (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_TOPDOWN_DIGITAL_8DIR)
  {
    *out_x = digital_x;
    *out_y = digital_y;
    return;
  }

  if (profile == (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_TOPDOWN_ANALOG)
  {
    if (analog_active != 0U)
    {
      *out_x = analog_x;
      *out_y = analog_y;
    }
    else
    {
      *out_x = 0.0f;
      *out_y = 0.0f;
    }
    return;
  }

  if (analog_active != 0U)
  {
    if ((flags & (uint32_t)GAME_PACKAGE_INPUT_FLAG_ANALOG_PREFERRED) != 0UL)
    {
      *out_x = analog_x;
      *out_y = analog_y;
    }
    else
    {
      *out_x = TopdownClampF(analog_x + digital_x, -1.0f, 1.0f);
      *out_y = TopdownClampF(analog_y + digital_y, -1.0f, 1.0f);
    }
  }
  else
  {
    *out_x = digital_x;
    *out_y = digital_y;
  }

  if (TopdownAbsF(*out_x) < 0.001f)
  {
    *out_x = 0.0f;
  }
  if (TopdownAbsF(*out_y) < 0.001f)
  {
    *out_y = 0.0f;
  }
}

static uint8_t TopdownCanOccupyPx(const game_map_view_t *map_view, int32_t cx_px, int32_t cy_px)
{
  int32_t map_w_px;
  int32_t map_h_px;
  int32_t left_px;
  int32_t right_px;
  int32_t top_px;
  int32_t bottom_px;
  uint16_t tx0;
  uint16_t tx1;
  uint16_t ty0;
  uint16_t ty1;
  uint16_t tx;
  uint16_t ty;
  uint8_t flags;

  if ((map_view == (const game_map_view_t *)0) ||
      (map_view->header == (const game_map_blob_header_t *)0))
  {
    return 1U;
  }

  map_w_px = (int32_t)map_view->header->map_width * (int32_t)map_view->header->tile_width;
  map_h_px = (int32_t)map_view->header->map_height * (int32_t)map_view->header->tile_height;

  left_px = cx_px - (TOPDOWN_PLAYER_HITBOX_W_PX / 2);
  right_px = cx_px + ((TOPDOWN_PLAYER_HITBOX_W_PX - 1) / 2);
  top_px = cy_px - (TOPDOWN_PLAYER_HITBOX_H_PX / 2);
  bottom_px = cy_px + ((TOPDOWN_PLAYER_HITBOX_H_PX - 1) / 2);

  if ((left_px < 0) || (top_px < 0) || (right_px >= map_w_px) || (bottom_px >= map_h_px))
  {
    return 0U;
  }

  tx0 = (uint16_t)(left_px / (int32_t)map_view->header->tile_width);
  tx1 = (uint16_t)(right_px / (int32_t)map_view->header->tile_width);
  ty0 = (uint16_t)(top_px / (int32_t)map_view->header->tile_height);
  ty1 = (uint16_t)(bottom_px / (int32_t)map_view->header->tile_height);

  for (ty = ty0; ty <= ty1; ty++)
  {
    for (tx = tx0; tx <= tx1; tx++)
    {
      flags = GameMap_GetTileFlags(map_view, tx, ty);
      if ((flags & (uint8_t)GAME_MAP_TILE_FLAG_SOLID) != 0U)
      {
        return 0U;
      }
    }
  }

  return 1U;
}

static uint8_t TopdownReadMaskBitMsb(const uint8_t *row, uint16_t x)
{
  return (uint8_t)((row[x >> 3U] >> (7U - (x & 7U))) & 0x1U);
}

static uint8_t TopdownReadLevel2bppMsb(const uint8_t *row, uint16_t x)
{
  uint8_t group = (uint8_t)(x & 3U);
  uint8_t shift = (uint8_t)(6U - (group * 2U));
  return (uint8_t)((row[x >> 2U] >> shift) & 0x3U);
}

static uint8_t TopdownBayer2x2Threshold(uint16_t x, uint16_t y)
{
  if ((y & 1U) == 0U)
  {
    return ((x & 1U) == 0U) ? 0U : 2U;
  }
  return ((x & 1U) == 0U) ? 3U : 1U;
}

typedef struct
{
  uint8_t visible;
  uint8_t black_limit;
} topdown_decoded_2bpp_t;

/*
 * True 5-shade decode over 2bpp+mask:
 * - mask=1,color=0 -> white
 * - mask=1,color=1 -> 1/4 black
 * - mask=1,color=2 -> 2/4 black
 * - mask=0,color=1 -> 3/4 black (extended shade)
 * - mask=1,color=3 -> 4/4 black
 * - mask=0,color=0 -> transparent
 */
static topdown_decoded_2bpp_t TopdownDecodeMasked2bpp(uint8_t level, uint8_t mask_bit)
{
  topdown_decoded_2bpp_t out;
  uint8_t level2 = (uint8_t)(level & 0x03U);

  out.visible = 1U;
  out.black_limit = 0U;

  if (mask_bit != 0U)
  {
    switch (level2)
    {
      case 0U:
        out.black_limit = 0U;
        break;
      case 1U:
        out.black_limit = 1U;
        break;
      case 2U:
        out.black_limit = 2U;
        break;
      default:
        out.black_limit = 4U;
        break;
    }
    return out;
  }

  if (level2 == 1U)
  {
    out.black_limit = 3U;
    return out;
  }

  out.visible = 0U;
  out.black_limit = 0U;
  return out;
}

static render_color_t TopdownBinaryColorFromBlackLimit(uint8_t black_limit)
{
  return (black_limit >= 2U) ? RENDER_COLOR_BLACK : RENDER_COLOR_WHITE;
}

static uint8_t TopdownBlitScaleForPresent(uint8_t requested_scale, render_2bpp_present_t present_mode)
{
  uint8_t blit_scale = requested_scale;
  if (blit_scale == 0U)
  {
    blit_scale = 1U;
  }
  if (present_mode == RENDER_2BPP_PRESENT_BAYER2X2)
  {
    blit_scale = (uint8_t)(blit_scale / 2U);
    if (blit_scale == 0U)
    {
      blit_scale = 1U;
    }
  }
  if (blit_scale > 8U)
  {
    blit_scale = 8U;
  }
  return blit_scale;
}

static uint8_t TopdownEffectiveScaleForPresent(uint8_t blit_scale, render_2bpp_present_t present_mode)
{
  uint16_t effective_scale = (blit_scale == 0U) ? 1U : (uint16_t)blit_scale;
  if (present_mode == RENDER_2BPP_PRESENT_BAYER2X2)
  {
    effective_scale = (uint16_t)(effective_scale * 2U);
  }
  if (effective_scale > 8U)
  {
    effective_scale = 8U;
  }
  return (uint8_t)effective_scale;
}

static void TopdownWriteBitMsb(uint8_t *row, uint16_t x, uint8_t value)
{
  uint16_t byte_ix;
  uint8_t mask;

  if (row == (uint8_t *)0)
  {
    return;
  }

  byte_ix = (uint16_t)(x >> 3U);
  mask = (uint8_t)(1U << (7U - (x & 7U)));
  if (value != 0U)
  {
    row[byte_ix] = (uint8_t)(row[byte_ix] | mask);
  }
  else
  {
    row[byte_ix] = (uint8_t)(row[byte_ix] & (uint8_t)(~mask));
  }
}

static void TopdownBlitMasked1bppClipped(int32_t dst_x,
                                         int32_t dst_y,
                                         uint16_t w,
                                         uint16_t h,
                                         const uint8_t *src_on,
                                         const uint8_t *src_mask,
                                         uint16_t src_stride_bytes,
                                         render_layer_t layer)
{
  uint16_t sy;

  if ((src_on == (const uint8_t *)0) ||
      (src_mask == (const uint8_t *)0) ||
      (w == 0U) ||
      (h == 0U) ||
      (src_stride_bytes == 0U))
  {
    return;
  }

  for (sy = 0U; sy < h; sy++)
  {
    const uint8_t *row_mask = src_mask + ((uint32_t)sy * (uint32_t)src_stride_bytes);
    const uint8_t *row_on = src_on + ((uint32_t)sy * (uint32_t)src_stride_bytes);
    int32_t py = dst_y + (int32_t)sy;
    uint16_t sx;

    if ((py < 0) || (py >= (int32_t)RENDER_HEIGHT))
    {
      continue;
    }

    for (sx = 0U; sx < w; sx++)
    {
      int32_t px = dst_x + (int32_t)sx;
      if ((px < 0) || (px >= (int32_t)RENDER_WIDTH))
      {
        continue;
      }
      if (TopdownReadMaskBitMsb(row_mask, sx) == 0U)
      {
        continue;
      }
      renderSetPixel((uint16_t)px,
                     (uint16_t)py,
                     layer,
                     (TopdownReadMaskBitMsb(row_on, sx) != 0U) ? RENDER_COLOR_WHITE : RENDER_COLOR_BLACK);
    }
  }
}

static void TopdownTileCacheInvalidate(void)
{
  uint16_t i;
  s_topdown_tile_cache.context_valid = 0U;
  s_topdown_tile_cache.next_victim = 0U;
  for (i = 0U; i < (uint16_t)TOPDOWN_TILE_CACHE_MAX_ENTRIES; i++)
  {
    s_topdown_tile_cache.entries[i].valid = 0U;
  }
}

static uint8_t TopdownTileCacheBuildEntry(topdown_tile_cache_entry_t *entry,
                                          uint16_t gid,
                                          uint16_t tile_w,
                                          uint16_t tile_h,
                                          const uint8_t *src_color2bpp,
                                          const uint8_t *src_mask1bpp,
                                          uint32_t src_color_stride_bytes,
                                          uint32_t src_mask_stride_bytes,
                                          uint8_t blit_scale,
                                          render_2bpp_present_t present_mode)
{
  uint16_t dst_w;
  uint16_t dst_h;
  uint16_t dst_stride;
  uint8_t pixel_scale;
  uint16_t py;

  if ((entry == (topdown_tile_cache_entry_t *)0) ||
      (src_color2bpp == (const uint8_t *)0) ||
      (tile_w == 0U) ||
      (tile_h == 0U) ||
      (src_color_stride_bytes == 0UL))
  {
    return 0U;
  }
  if ((src_mask1bpp != (const uint8_t *)0) && (src_mask_stride_bytes == 0UL))
  {
    return 0U;
  }

  pixel_scale = TopdownEffectiveScaleForPresent(blit_scale, present_mode);
  if (pixel_scale == 0U)
  {
    return 0U;
  }

  if (((uint32_t)tile_w * (uint32_t)pixel_scale) > (uint32_t)UINT16_MAX)
  {
    return 0U;
  }
  if (((uint32_t)tile_h * (uint32_t)pixel_scale) > (uint32_t)UINT16_MAX)
  {
    return 0U;
  }

  dst_w = (uint16_t)((uint32_t)tile_w * (uint32_t)pixel_scale);
  dst_h = (uint16_t)((uint32_t)tile_h * (uint32_t)pixel_scale);
  if ((dst_w == 0U) || (dst_h == 0U))
  {
    return 0U;
  }
  if ((dst_w > (uint16_t)TOPDOWN_TILE_CACHE_MAX_DIM_PX) ||
      (dst_h > (uint16_t)TOPDOWN_TILE_CACHE_MAX_DIM_PX))
  {
    return 0U;
  }

  dst_stride = (uint16_t)((dst_w + 7U) >> 3U);
  if ((dst_stride == 0U) || (dst_stride > (uint16_t)TOPDOWN_TILE_CACHE_MAX_ROW_BYTES))
  {
    return 0U;
  }

  (void)memset(entry->on_plane, 0, sizeof(entry->on_plane));
  (void)memset(entry->mask_plane, 0, sizeof(entry->mask_plane));

  for (py = 0U; py < tile_h; py++)
  {
    const uint8_t *color_row = src_color2bpp + ((uint32_t)py * src_color_stride_bytes);
    const uint8_t *mask_row = (src_mask1bpp != (const uint8_t *)0)
                                  ? (src_mask1bpp + ((uint32_t)py * src_mask_stride_bytes))
                                  : (const uint8_t *)0;
    uint16_t px;
    for (px = 0U; px < tile_w; px++)
    {
      uint8_t level;
      uint8_t mask_bit;
      topdown_decoded_2bpp_t decoded;
      uint8_t dy;

      level = TopdownReadLevel2bppMsb(color_row, px);
      mask_bit = (mask_row != (const uint8_t *)0) ? TopdownReadMaskBitMsb(mask_row, px) : 1U;
      decoded = TopdownDecodeMasked2bpp(level, mask_bit);
      if (decoded.visible == 0U)
      {
        continue;
      }

      for (dy = 0U; dy < pixel_scale; dy++)
      {
        uint8_t dx;
        uint16_t out_y = (uint16_t)(((uint32_t)py * (uint32_t)pixel_scale) + (uint32_t)dy);
        uint8_t *dst_mask_row = &entry->mask_plane[(uint32_t)out_y * (uint32_t)dst_stride];
        uint8_t *dst_on_row = &entry->on_plane[(uint32_t)out_y * (uint32_t)dst_stride];
        for (dx = 0U; dx < pixel_scale; dx++)
        {
          uint8_t is_black = 0U;
          uint16_t out_x = (uint16_t)(((uint32_t)px * (uint32_t)pixel_scale) + (uint32_t)dx);
          if (present_mode == RENDER_2BPP_PRESENT_BINARY_CLAMP)
          {
            is_black = (decoded.black_limit >= 2U) ? 1U : 0U;
          }
          else
          {
            uint8_t black_limit = decoded.black_limit;
            if (black_limit >= 4U)
            {
              is_black = 1U;
            }
            else if (black_limit != 0U)
            {
              uint8_t thr = TopdownBayer2x2Threshold((uint16_t)dx, (uint16_t)dy);
              is_black = (thr < black_limit) ? 1U : 0U;
            }
          }

          TopdownWriteBitMsb(dst_mask_row, out_x, 1U);
          TopdownWriteBitMsb(dst_on_row, out_x, (is_black == 0U) ? 1U : 0U);
        }
      }
    }
  }

  entry->gid = gid;
  entry->width_px = (uint8_t)dst_w;
  entry->height_px = (uint8_t)dst_h;
  entry->stride_bytes = (uint8_t)dst_stride;
  entry->valid = 1U;
  return 1U;
}

static topdown_tile_cache_entry_t *TopdownTileCacheFind(uint16_t gid)
{
  uint16_t i;
  for (i = 0U; i < (uint16_t)TOPDOWN_TILE_CACHE_MAX_ENTRIES; i++)
  {
    if ((s_topdown_tile_cache.entries[i].valid != 0U) &&
        (s_topdown_tile_cache.entries[i].gid == gid))
    {
      return &s_topdown_tile_cache.entries[i];
    }
  }
  return (topdown_tile_cache_entry_t *)0;
}

static topdown_tile_cache_entry_t *TopdownTileCacheAllocSlot(void)
{
  uint16_t i;
  for (i = 0U; i < (uint16_t)TOPDOWN_TILE_CACHE_MAX_ENTRIES; i++)
  {
    if (s_topdown_tile_cache.entries[i].valid == 0U)
    {
      return &s_topdown_tile_cache.entries[i];
    }
  }

  if (s_topdown_tile_cache.next_victim >= (uint16_t)TOPDOWN_TILE_CACHE_MAX_ENTRIES)
  {
    s_topdown_tile_cache.next_victim = 0U;
  }
  i = s_topdown_tile_cache.next_victim++;
  return &s_topdown_tile_cache.entries[i];
}

static void TopdownTileCacheEnsureContext(const game_tileset_blob_header_t *tileset_header,
                                          uint16_t tile_w,
                                          uint16_t tile_h,
                                          uint8_t blit_scale,
                                          render_2bpp_present_t present_mode)
{
  uint8_t pixel_scale;

  if (tileset_header == (const game_tileset_blob_header_t *)0)
  {
    TopdownTileCacheInvalidate();
    return;
  }

  pixel_scale = TopdownEffectiveScaleForPresent(blit_scale, present_mode);
  if ((s_topdown_tile_cache.context_valid != 0U) &&
      (s_topdown_tile_cache.tileset_header == tileset_header) &&
      (s_topdown_tile_cache.tileset_crc32 == tileset_header->crc32) &&
      (s_topdown_tile_cache.tileset_tile_count == tileset_header->tile_count) &&
      (s_topdown_tile_cache.tile_w == tile_w) &&
      (s_topdown_tile_cache.tile_h == tile_h) &&
      (s_topdown_tile_cache.blit_scale == blit_scale) &&
      (s_topdown_tile_cache.present_mode == (uint8_t)present_mode) &&
      (s_topdown_tile_cache.pixel_scale == pixel_scale))
  {
    return;
  }

  TopdownTileCacheInvalidate();
  s_topdown_tile_cache.context_valid = 1U;
  s_topdown_tile_cache.tileset_header = tileset_header;
  s_topdown_tile_cache.tileset_crc32 = tileset_header->crc32;
  s_topdown_tile_cache.tileset_tile_count = tileset_header->tile_count;
  s_topdown_tile_cache.tile_w = tile_w;
  s_topdown_tile_cache.tile_h = tile_h;
  s_topdown_tile_cache.blit_scale = blit_scale;
  s_topdown_tile_cache.present_mode = (uint8_t)present_mode;
  s_topdown_tile_cache.pixel_scale = pixel_scale;
}

static uint8_t TopdownTileCacheTryDrawTile(uint16_t gid,
                                           int32_t dst_x,
                                           int32_t dst_y,
                                           uint16_t tile_w,
                                           uint16_t tile_h,
                                           const uint8_t *src_color2bpp,
                                           const uint8_t *src_mask1bpp,
                                           uint32_t src_color_stride_bytes,
                                           uint32_t src_mask_stride_bytes,
                                           uint8_t blit_scale,
                                           render_2bpp_present_t present_mode,
                                           render_layer_t layer,
                                           uint8_t fast_no_clip_ok)
{
  topdown_tile_cache_entry_t *entry;
  uint16_t stride;
  int32_t dst_x1;
  int32_t dst_y1;

  if (s_topdown_tile_cache.context_valid == 0U)
  {
    return 0U;
  }

  entry = TopdownTileCacheFind(gid);
  if (entry == (topdown_tile_cache_entry_t *)0)
  {
    entry = TopdownTileCacheAllocSlot();
    if (TopdownTileCacheBuildEntry(entry,
                                   gid,
                                   tile_w,
                                   tile_h,
                                   src_color2bpp,
                                   src_mask1bpp,
                                   src_color_stride_bytes,
                                   src_mask_stride_bytes,
                                   blit_scale,
                                   present_mode) == 0U)
    {
      if (entry != (topdown_tile_cache_entry_t *)0)
      {
        entry->valid = 0U;
      }
      return 0U;
    }
  }

  stride = (uint16_t)entry->stride_bytes;
  dst_x1 = dst_x + (int32_t)entry->width_px;
  dst_y1 = dst_y + (int32_t)entry->height_px;
  if ((dst_x >= 0) &&
      (dst_y >= 0) &&
      (dst_x1 <= (int32_t)RENDER_WIDTH) &&
      (dst_y1 <= (int32_t)RENDER_HEIGHT))
  {
    if (fast_no_clip_ok != 0U)
    {
      renderBlitMasked1bppFastNoClip((uint16_t)dst_x,
                                     (uint16_t)dst_y,
                                     (uint16_t)entry->width_px,
                                     (uint16_t)entry->height_px,
                                     entry->on_plane,
                                     entry->mask_plane,
                                     stride,
                                     true,
                                     layer);
    }
    else
    {
      renderBlitMasked1bpp((uint16_t)dst_x,
                           (uint16_t)dst_y,
                           (uint16_t)entry->width_px,
                           (uint16_t)entry->height_px,
                           entry->on_plane,
                           entry->mask_plane,
                           stride,
                           true,
                           layer);
    }
  }
  else
  {
    TopdownBlitMasked1bppClipped(dst_x,
                                 dst_y,
                                 (uint16_t)entry->width_px,
                                 (uint16_t)entry->height_px,
                                 entry->on_plane,
                                 entry->mask_plane,
                                 stride,
                                 layer);
  }
  return 1U;
}

static void TopdownPlayerCacheInvalidate(void)
{
  uint16_t i;
  s_topdown_player_cache.next_victim = 0U;
  for (i = 0U; i < (uint16_t)TOPDOWN_PLAYER_CACHE_MAX_ENTRIES; i++)
  {
    s_topdown_player_cache.entries[i].valid = 0U;
  }
}

static topdown_player_cache_entry_t *TopdownPlayerCacheFind(const uint8_t *color2bpp,
                                                            const uint8_t *mask1bpp,
                                                            uint8_t blit_scale,
                                                            render_2bpp_present_t present_mode)
{
  uint16_t i;
  for (i = 0U; i < (uint16_t)TOPDOWN_PLAYER_CACHE_MAX_ENTRIES; i++)
  {
    topdown_player_cache_entry_t *entry = &s_topdown_player_cache.entries[i];
    if ((entry->valid != 0U) &&
        (entry->color_ptr == color2bpp) &&
        (entry->mask_ptr == mask1bpp) &&
        (entry->blit_scale == blit_scale) &&
        (entry->present_mode == (uint8_t)present_mode))
    {
      return entry;
    }
  }
  return (topdown_player_cache_entry_t *)0;
}

static topdown_player_cache_entry_t *TopdownPlayerCacheAllocSlot(void)
{
  uint16_t i;
  for (i = 0U; i < (uint16_t)TOPDOWN_PLAYER_CACHE_MAX_ENTRIES; i++)
  {
    if (s_topdown_player_cache.entries[i].valid == 0U)
    {
      return &s_topdown_player_cache.entries[i];
    }
  }

  if (s_topdown_player_cache.next_victim >= (uint16_t)TOPDOWN_PLAYER_CACHE_MAX_ENTRIES)
  {
    s_topdown_player_cache.next_victim = 0U;
  }
  i = s_topdown_player_cache.next_victim++;
  return &s_topdown_player_cache.entries[i];
}

static uint8_t TopdownPlayerCacheBuildEntry(topdown_player_cache_entry_t *entry,
                                            const uint8_t *color2bpp,
                                            const uint8_t *mask1bpp,
                                            uint8_t blit_scale,
                                            render_2bpp_present_t present_mode)
{
  uint8_t pixel_scale;
  uint16_t dst_w;
  uint16_t dst_h;
  uint16_t dst_stride;
  uint16_t py;

  if ((entry == (topdown_player_cache_entry_t *)0) ||
      (color2bpp == (const uint8_t *)0) ||
      (mask1bpp == (const uint8_t *)0))
  {
    return 0U;
  }

  pixel_scale = TopdownEffectiveScaleForPresent(blit_scale, present_mode);
  if (pixel_scale == 0U)
  {
    return 0U;
  }

  dst_w = (uint16_t)((uint32_t)TOPDOWN_PLAYER_SPR_W_PX * (uint32_t)pixel_scale);
  dst_h = (uint16_t)((uint32_t)TOPDOWN_PLAYER_SPR_H_PX * (uint32_t)pixel_scale);
  if ((dst_w == 0U) || (dst_h == 0U))
  {
    return 0U;
  }
  if ((dst_w > (uint16_t)TOPDOWN_PLAYER_CACHE_MAX_DIM_PX) ||
      (dst_h > (uint16_t)TOPDOWN_PLAYER_CACHE_MAX_DIM_PX))
  {
    return 0U;
  }
  dst_stride = (uint16_t)((dst_w + 7U) >> 3U);
  if ((dst_stride == 0U) || (dst_stride > (uint16_t)TOPDOWN_PLAYER_CACHE_MAX_ROW_BYTES))
  {
    return 0U;
  }

  (void)memset(entry->on_plane, 0, sizeof(entry->on_plane));
  (void)memset(entry->mask_plane, 0, sizeof(entry->mask_plane));

  for (py = 0U; py < TOPDOWN_PLAYER_SPR_H_PX; py++)
  {
    const uint8_t *color_row = color2bpp + ((uint32_t)py * (uint32_t)TOPDOWN_PLAYER_SPR_COLOR_ROW_BYTES);
    const uint8_t *mask_row = mask1bpp + ((uint32_t)py * (uint32_t)TOPDOWN_PLAYER_SPR_MASK_ROW_BYTES);
    uint16_t px;
    for (px = 0U; px < TOPDOWN_PLAYER_SPR_W_PX; px++)
    {
      uint8_t level;
      uint8_t mask_bit;
      topdown_decoded_2bpp_t decoded;
      uint8_t dy;

      level = TopdownReadLevel2bppMsb(color_row, px);
      mask_bit = TopdownReadMaskBitMsb(mask_row, px);
      decoded = TopdownDecodeMasked2bpp(level, mask_bit);
      if (decoded.visible == 0U)
      {
        continue;
      }

      for (dy = 0U; dy < pixel_scale; dy++)
      {
        uint8_t dx;
        uint16_t out_y = (uint16_t)(((uint32_t)py * (uint32_t)pixel_scale) + (uint32_t)dy);
        uint8_t *dst_mask_row = &entry->mask_plane[(uint32_t)out_y * (uint32_t)dst_stride];
        uint8_t *dst_on_row = &entry->on_plane[(uint32_t)out_y * (uint32_t)dst_stride];
        for (dx = 0U; dx < pixel_scale; dx++)
        {
          uint8_t is_black = 0U;
          uint16_t out_x = (uint16_t)(((uint32_t)px * (uint32_t)pixel_scale) + (uint32_t)dx);
          if (present_mode == RENDER_2BPP_PRESENT_BINARY_CLAMP)
          {
            is_black = (decoded.black_limit >= 2U) ? 1U : 0U;
          }
          else
          {
            uint8_t black_limit = decoded.black_limit;
            if (black_limit >= 4U)
            {
              is_black = 1U;
            }
            else if (black_limit != 0U)
            {
              uint8_t thr = TopdownBayer2x2Threshold((uint16_t)dx, (uint16_t)dy);
              is_black = (thr < black_limit) ? 1U : 0U;
            }
          }

          TopdownWriteBitMsb(dst_mask_row, out_x, 1U);
          TopdownWriteBitMsb(dst_on_row, out_x, (is_black == 0U) ? 1U : 0U);
        }
      }
    }
  }

  entry->color_ptr = color2bpp;
  entry->mask_ptr = mask1bpp;
  entry->present_mode = (uint8_t)present_mode;
  entry->blit_scale = blit_scale;
  entry->width_px = (uint8_t)dst_w;
  entry->height_px = (uint8_t)dst_h;
  entry->stride_bytes = (uint8_t)dst_stride;
  entry->valid = 1U;
  return 1U;
}

static uint8_t TopdownPlayerCacheTryDraw(const uint8_t *color2bpp,
                                         const uint8_t *mask1bpp,
                                         int32_t dst_x,
                                         int32_t dst_y,
                                         uint8_t blit_scale,
                                         render_2bpp_present_t present_mode,
                                         render_layer_t layer)
{
  topdown_player_cache_entry_t *entry;
  uint16_t stride;
  int32_t dst_x1;
  int32_t dst_y1;

  if ((color2bpp == (const uint8_t *)0) || (mask1bpp == (const uint8_t *)0))
  {
    return 0U;
  }

  entry = TopdownPlayerCacheFind(color2bpp, mask1bpp, blit_scale, present_mode);
  if (entry == (topdown_player_cache_entry_t *)0)
  {
    entry = TopdownPlayerCacheAllocSlot();
    if (TopdownPlayerCacheBuildEntry(entry, color2bpp, mask1bpp, blit_scale, present_mode) == 0U)
    {
      if (entry != (topdown_player_cache_entry_t *)0)
      {
        entry->valid = 0U;
      }
      return 0U;
    }
  }

  stride = (uint16_t)entry->stride_bytes;
  dst_x1 = dst_x + (int32_t)entry->width_px;
  dst_y1 = dst_y + (int32_t)entry->height_px;
  if ((dst_x >= 0) &&
      (dst_y >= 0) &&
      (dst_x1 <= (int32_t)RENDER_WIDTH) &&
      (dst_y1 <= (int32_t)RENDER_HEIGHT))
  {
    renderBlitMasked1bpp((uint16_t)dst_x,
                         (uint16_t)dst_y,
                         (uint16_t)entry->width_px,
                         (uint16_t)entry->height_px,
                         entry->on_plane,
                         entry->mask_plane,
                         stride,
                         true,
                         layer);
  }
  else
  {
    TopdownBlitMasked1bppClipped(dst_x,
                                 dst_y,
                                 (uint16_t)entry->width_px,
                                 (uint16_t)entry->height_px,
                                 entry->on_plane,
                                 entry->mask_plane,
                                 stride,
                                 layer);
  }
  return 1U;
}

static void TopdownDrawTile2bppScaledClipped(int32_t dst_x,
                                             int32_t dst_y,
                                             uint16_t w,
                                             uint16_t h,
                                             const uint8_t *src_color2bpp,
                                             const uint8_t *src_mask,
                                             uint16_t src_color_stride_bytes,
                                             uint16_t src_mask_stride_bytes,
                                             uint8_t blit_scale,
                                             render_2bpp_present_t present_mode,
                                             render_layer_t layer)
{
  uint8_t pixel_scale = blit_scale;
  uint16_t py;
  if ((src_color2bpp == (const uint8_t *)0) || (w == 0U) || (h == 0U) || (pixel_scale == 0U))
  {
    return;
  }
  if (present_mode == RENDER_2BPP_PRESENT_BAYER2X2)
  {
    pixel_scale = (uint8_t)(pixel_scale * 2U);
    if (pixel_scale == 0U)
    {
      return;
    }
  }

  for (py = 0U; py < h; py++)
  {
    const uint8_t *color_row = src_color2bpp + ((uint32_t)py * (uint32_t)src_color_stride_bytes);
    const uint8_t *mask_row = (src_mask != (const uint8_t *)0)
                                  ? (src_mask + ((uint32_t)py * (uint32_t)src_mask_stride_bytes))
                                  : (const uint8_t *)0;
    uint16_t px;
    for (px = 0U; px < w; px++)
    {
      uint8_t level;
      uint8_t mask_bit;
      topdown_decoded_2bpp_t decoded;
      uint8_t dy;
      int32_t dst_x0;
      int32_t dst_y0;

      level = TopdownReadLevel2bppMsb(color_row, px);
      mask_bit = (mask_row != (const uint8_t *)0) ? TopdownReadMaskBitMsb(mask_row, px) : 1U;
      decoded = TopdownDecodeMasked2bpp(level, mask_bit);
      if (decoded.visible == 0U)
      {
        continue;
      }

      dst_x0 = dst_x + ((int32_t)px * (int32_t)pixel_scale);
      dst_y0 = dst_y + ((int32_t)py * (int32_t)pixel_scale);
      for (dy = 0U; dy < pixel_scale; dy++)
      {
        uint8_t dx;
        int32_t write_y = dst_y0 + (int32_t)dy;
        if ((write_y < 0) || (write_y >= (int32_t)RENDER_HEIGHT))
        {
          continue;
        }
        for (dx = 0U; dx < pixel_scale; dx++)
        {
          int32_t write_x = dst_x0 + (int32_t)dx;
          render_color_t color;
          if ((write_x < 0) || (write_x >= (int32_t)RENDER_WIDTH))
          {
            continue;
          }
          if (present_mode == RENDER_2BPP_PRESENT_BINARY_CLAMP)
          {
            color = TopdownBinaryColorFromBlackLimit(decoded.black_limit);
          }
          else
          {
            uint8_t black_limit = decoded.black_limit;
            if (black_limit == 0U)
            {
              color = RENDER_COLOR_WHITE;
            }
            else if (black_limit >= 4U)
            {
              color = RENDER_COLOR_BLACK;
            }
            else
            {
              uint8_t thr = TopdownBayer2x2Threshold((uint16_t)dx, (uint16_t)dy);
              color = (thr < black_limit) ? RENDER_COLOR_BLACK : RENDER_COLOR_WHITE;
            }
          }
          renderSetPixel((uint16_t)write_x, (uint16_t)write_y, layer, color);
        }
      }
    }
  }
}

static uint8_t TopdownAppendU32(char *dst, uint8_t pos, uint8_t max_len, uint32_t value)
{
  char rev[10];
  uint8_t n = 0U;
  do
  {
    rev[n++] = (char)('0' + (value % 10UL));
    value /= 10UL;
  } while ((value > 0UL) && (n < (uint8_t)sizeof(rev)));

  while (n > 0U)
  {
    if (pos >= (uint8_t)(max_len - 1U))
    {
      break;
    }
    dst[pos++] = rev[n - 1U];
    n--;
  }
  dst[pos] = '\0';
  return pos;
}

static void TopdownUpdateFpsHud(void)
{
  uint32_t now_ms = HAL_GetTick();
  uint32_t elapsed_ms;

  if (s_topdown.fps_window_start_ms == 0UL)
  {
    s_topdown.fps_window_start_ms = now_ms;
    s_topdown.fps_frames_in_window = 0U;
    s_topdown.fps_last = 0U;
  }

  s_topdown.fps_frames_in_window++;
  elapsed_ms = now_ms - s_topdown.fps_window_start_ms;
  if (elapsed_ms >= 1000UL)
  {
    uint32_t fps = ((uint32_t)s_topdown.fps_frames_in_window * 1000UL) / elapsed_ms;
    if (fps > 999UL)
    {
      fps = 999UL;
    }
    s_topdown.fps_last = (uint16_t)fps;
    s_topdown.fps_frames_in_window = 0U;
    s_topdown.fps_window_start_ms = now_ms;
  }
}

static void TopdownClearLayerRectTransparent(render_layer_t layer, int32_t x, int32_t y, int32_t w, int32_t h)
{
  int32_t x0;
  int32_t y0;
  int32_t x1;
  int32_t y1;
  uint16_t draw_x;
  uint16_t draw_y;
  uint16_t draw_w;
  uint16_t draw_h;

  if ((w <= 0) || (h <= 0))
  {
    return;
  }

  x0 = x;
  y0 = y;
  x1 = x + w;
  y1 = y + h;

  if (x0 < 0)
  {
    x0 = 0;
  }
  if (y0 < 0)
  {
    y0 = 0;
  }
  if (x1 > (int32_t)RENDER_WIDTH)
  {
    x1 = (int32_t)RENDER_WIDTH;
  }
  if (y1 > (int32_t)RENDER_HEIGHT)
  {
    y1 = (int32_t)RENDER_HEIGHT;
  }
  if ((x1 <= x0) || (y1 <= y0))
  {
    return;
  }

  draw_x = (uint16_t)x0;
  draw_y = (uint16_t)y0;
  draw_w = (uint16_t)(x1 - x0);
  draw_h = (uint16_t)(y1 - y0);
  renderClearRectTransparent(draw_x, draw_y, draw_w, draw_h, layer);
}

static void TopdownDrawPerfHud(void)
{
  char line_fps[16] = "FPS:";
  char line_clk[16] = "CLK:";
  uint8_t pos;
  uint32_t clk_mhz = HAL_RCC_GetSysClockFreq() / 1000000UL;

  if ((s_topdown.hud_valid != 0U) &&
      (s_topdown.last_hud_fps == s_topdown.fps_last) &&
      (s_topdown.last_clk_mhz == clk_mhz))
  {
    return;
  }

  pos = 4U;
  pos = TopdownAppendU32(line_fps, pos, (uint8_t)sizeof(line_fps), (uint32_t)s_topdown.fps_last);
  (void)pos;

  pos = 4U;
  pos = TopdownAppendU32(line_clk, pos, (uint8_t)sizeof(line_clk), clk_mhz);
  if (pos < (uint8_t)(sizeof(line_clk) - 1U))
  {
    line_clk[pos++] = 'M';
  }
  if (pos < (uint8_t)(sizeof(line_clk) - 1U))
  {
    line_clk[pos++] = 'H';
  }
  if (pos < (uint8_t)(sizeof(line_clk) - 1U))
  {
    line_clk[pos++] = 'z';
  }
  line_clk[pos] = '\0';

  renderFillRect(2U, 2U, 62U, 14U, RENDER_LAYER_UI, RENDER_COLOR_WHITE);
  renderDrawText(4U, 3U, line_fps, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  renderDrawText(4U, 9U, line_clk, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  s_topdown.last_hud_fps = s_topdown.fps_last;
  s_topdown.last_clk_mhz = clk_mhz;
  s_topdown.hud_valid = 1U;
}

static int32_t TopdownMoveAxis(const game_map_view_t *map_view, int32_t *coord, int32_t fixed, int32_t delta, uint8_t x_axis)
{
  int32_t i;
  int32_t step;
  int32_t limit;
  int32_t moved = 0;

  if ((coord == (int32_t *)0) || (delta == 0))
  {
    return 0;
  }

  step = (delta > 0) ? 1 : -1;
  limit = TopdownAbsI32(delta);
  for (i = 0; i < limit; i++)
  {
    int32_t candidate = *coord + step;
    uint8_t can_move = (x_axis != 0U) ? TopdownCanOccupyPx(map_view, candidate, fixed)
                                      : TopdownCanOccupyPx(map_view, fixed, candidate);
    if (can_move == 0U)
    {
      break;
    }
    *coord = candidate;
    moved += step;
  }

  return moved;
}

static uint8_t TopdownMapMetrics(const game_map_view_t *map_view,
                                 uint32_t *map_w_px_out,
                                 uint32_t *map_h_px_out,
                                 uint16_t *tile_w_out,
                                 uint16_t *tile_h_out)
{
  if ((map_view == (const game_map_view_t *)0) ||
      (map_view->header == (const game_map_blob_header_t *)0))
  {
    return 0U;
  }

  if (map_w_px_out != (uint32_t *)0)
  {
    *map_w_px_out = (uint32_t)map_view->header->map_width * (uint32_t)map_view->header->tile_width;
  }
  if (map_h_px_out != (uint32_t *)0)
  {
    *map_h_px_out = (uint32_t)map_view->header->map_height * (uint32_t)map_view->header->tile_height;
  }
  if (tile_w_out != (uint16_t *)0)
  {
    *tile_w_out = map_view->header->tile_width;
  }
  if (tile_h_out != (uint16_t *)0)
  {
    *tile_h_out = map_view->header->tile_height;
  }
  return 1U;
}

static void TopdownViewWorldSize(uint32_t *view_w_world_out, uint32_t *view_h_world_out, uint8_t *render_scale_out)
{
  uint8_t render_scale_req = TopdownRenderScale();
  render_2bpp_present_t present_mode = TopdownPresentMode(render_scale_req);
  uint8_t blit_scale = TopdownBlitScaleForPresent(render_scale_req, present_mode);
  uint8_t render_scale = TopdownEffectiveScaleForPresent(blit_scale, present_mode);
  uint32_t view_w_world = (uint32_t)RENDER_WIDTH / (uint32_t)render_scale;
  uint32_t view_h_world = (uint32_t)RENDER_HEIGHT / (uint32_t)render_scale;

  if (view_w_world == 0UL)
  {
    view_w_world = 1UL;
  }
  if (view_h_world == 0UL)
  {
    view_h_world = 1UL;
  }

  if (view_w_world_out != (uint32_t *)0)
  {
    *view_w_world_out = view_w_world;
  }
  if (view_h_world_out != (uint32_t *)0)
  {
    *view_h_world_out = view_h_world;
  }
  if (render_scale_out != (uint8_t *)0)
  {
    *render_scale_out = render_scale;
  }
}

static float TopdownApproachF(float current, float target, float max_step)
{
  float delta = target - current;
  if (delta > max_step)
  {
    return current + max_step;
  }
  if (delta < -max_step)
  {
    return current - max_step;
  }
  return target;
}

static int32_t TopdownToPixelDelta(float *remainder, float velocity_px_s, uint32_t dt_ms)
{
  float px = (velocity_px_s * ((float)dt_ms / 1000.0f)) + *remainder;
  int32_t delta = (int32_t)px;
  *remainder = px - (float)delta;
  return delta;
}

static void TopdownTryInitSpawn(void)
{
  const game_map_view_t *map_view = GameRuntime_GetSceneMap();
  uint32_t map_w_px = 0UL;
  uint32_t map_h_px = 0UL;
  int32_t spawn_x_px = 0;
  int32_t spawn_y_px = 0;
  uint32_t resolved_spawn_hash = 0UL;

  if (s_topdown.spawn_locked != 0U)
  {
    return;
  }
  if (TopdownMapMetrics(map_view, &map_w_px, &map_h_px, (uint16_t *)0, (uint16_t *)0) == 0U)
  {
    return;
  }

  if (TopdownFindSpawnPx(map_view,
                         s_topdown.spawn_request_hash,
                         (s_topdown.spawn_require_exact != 0U) ? 0U : 1U,
                         &spawn_x_px,
                         &spawn_y_px,
                         &resolved_spawn_hash) != 0U)
  {
    if (TopdownCanOccupyPx(map_view, spawn_x_px, spawn_y_px) != 0U)
    {
      TopdownApplySpawnPosition(map_view, spawn_x_px, spawn_y_px);
      s_topdown.spawn_request_hash = resolved_spawn_hash;
      s_topdown.spawn_require_exact = 0U;
      return;
    }
  }

  if (s_topdown.spawn_require_exact != 0U)
  {
    return;
  }

  spawn_x_px = TopdownClampI32((int32_t)(map_w_px / 2UL), 0, (map_w_px > 0UL) ? ((int32_t)map_w_px - 1) : 0);
  spawn_y_px = TopdownClampI32((int32_t)(map_h_px / 2UL), 0, (map_h_px > 0UL) ? ((int32_t)map_h_px - 1) : 0);
  TopdownApplySpawnPosition(map_view, spawn_x_px, spawn_y_px);
}

static void TopdownHandleSceneSwap(const game_map_view_t *map_view)
{
  uint32_t map_crc32;

  if ((map_view == (const game_map_view_t *)0) ||
      (map_view->header == (const game_map_blob_header_t *)0))
  {
    return;
  }

  map_crc32 = map_view->header->crc32;
  if (s_topdown.active_scene_crc32 == 0UL)
  {
    s_topdown.active_scene_crc32 = map_crc32;
    return;
  }
  if (s_topdown.active_scene_crc32 == map_crc32)
  {
    return;
  }

  s_topdown.active_scene_crc32 = map_crc32;
  s_topdown.spawn_locked = 0U;
  s_topdown.camera_initialized = 0U;
  s_topdown.exit_zone_latched = 0U;
  s_topdown.player_vx_px_s = 0.0f;
  s_topdown.player_vy_px_s = 0.0f;
  s_topdown.player_rem_x_px = 0.0f;
  s_topdown.player_rem_y_px = 0.0f;
  s_topdown.clear_pending = 1U;
  s_topdown.map_cache_valid = 0U;
  s_topdown.player_rect_valid = 0U;
  s_topdown.hud_valid = 0U;
  s_topdown.map_cache_header = (const game_map_blob_header_t *)0;
  s_topdown.tileset_cache_header = (const game_tileset_blob_header_t *)0;
  s_topdown.player_prev_color2bpp = (const uint8_t *)0;
  s_topdown.player_prev_mask1bpp = (const uint8_t *)0;
  TopdownTileCacheInvalidate();
  TopdownPlayerCacheInvalidate();
}

static void TopdownTryInitCamera(const game_map_view_t *map_view)
{
  uint32_t map_w_px = 0UL;
  uint32_t map_h_px = 0UL;
  uint32_t view_w_world = 1UL;
  uint32_t view_h_world = 1UL;
  int32_t max_cam_x;
  int32_t max_cam_y;
  int32_t cam_x;
  int32_t cam_y;

  if ((s_topdown.spawn_locked == 0U) || (s_topdown.camera_initialized != 0U))
  {
    return;
  }
  if (TopdownMapMetrics(map_view, &map_w_px, &map_h_px, (uint16_t *)0, (uint16_t *)0) == 0U)
  {
    return;
  }

  TopdownViewWorldSize(&view_w_world, &view_h_world, (uint8_t *)0);
  cam_x = s_topdown.player_x_px - ((int32_t)view_w_world / 2);
  cam_y = s_topdown.player_y_px - ((int32_t)view_h_world / 2);
  max_cam_x = (map_w_px > view_w_world) ? (int32_t)(map_w_px - view_w_world) : 0;
  max_cam_y = (map_h_px > view_h_world) ? (int32_t)(map_h_px - view_h_world) : 0;

  cam_x = TopdownClampI32(cam_x, 0, max_cam_x);
  cam_y = TopdownClampI32(cam_y, 0, max_cam_y);
  s_topdown.cam_x_px = (float)cam_x;
  s_topdown.cam_y_px = (float)cam_y;
  s_topdown.camera_initialized = 1U;
}

static void TopdownUpdateCamera(const game_map_view_t *map_view, uint32_t dt_ms)
{
  uint32_t map_w_px = 0UL;
  uint32_t map_h_px = 0UL;
  uint32_t view_w_world = 1UL;
  uint32_t view_h_world = 1UL;
  int32_t max_cam_x;
  int32_t max_cam_y;
  uint32_t profile;
  int32_t look_x_cfg = TopdownCameraLookaheadXPx();
  int32_t look_y_cfg = TopdownCameraLookaheadYPx();
  int32_t look_x = 0;
  int32_t look_y = 0;
  float desired_x = (float)(s_topdown.player_x_px + look_x);
  float desired_y = (float)(s_topdown.player_y_px + look_y);
  float target_cam_x = s_topdown.cam_x_px;
  float target_cam_y = s_topdown.cam_y_px;
  float follow = ((float)TopdownCameraFollowPermille()) / 1000.0f;
  float dt_s = (float)dt_ms / 1000.0f;
  float max_step = ((float)TopdownCameraMaxSpeedPxPerSec()) * dt_s;
  float step_x;
  float step_y;
  uint32_t dz_w;
  uint32_t dz_h;

  if ((s_topdown.camera_initialized == 0U) || (TopdownMapMetrics(map_view, &map_w_px, &map_h_px, (uint16_t *)0, (uint16_t *)0) == 0U))
  {
    return;
  }

  TopdownViewWorldSize(&view_w_world, &view_h_world, (uint8_t *)0);
  max_cam_x = (map_w_px > view_w_world) ? (int32_t)(map_w_px - view_w_world) : 0;
  max_cam_y = (map_h_px > view_h_world) ? (int32_t)(map_h_px - view_h_world) : 0;
  profile = TopdownCameraProfile();

  if (look_x_cfg != 0)
  {
    int32_t look_mag_x = TopdownAbsI32(look_x_cfg);
    if (s_topdown.player_vx_px_s > 0.5f)
    {
      look_x = look_mag_x;
    }
    else if (s_topdown.player_vx_px_s < -0.5f)
    {
      look_x = -look_mag_x;
    }
  }
  if (look_y_cfg != 0)
  {
    int32_t look_mag_y = TopdownAbsI32(look_y_cfg);
    if (s_topdown.player_vy_px_s > 0.5f)
    {
      look_y = look_mag_y;
    }
    else if (s_topdown.player_vy_px_s < -0.5f)
    {
      look_y = -look_mag_y;
    }
  }
  desired_x = (float)(s_topdown.player_x_px + look_x);
  desired_y = (float)(s_topdown.player_y_px + look_y);

  if (profile == (uint32_t)GAME_PACKAGE_CAMERA_PROFILE_LOCKED)
  {
    target_cam_x = s_topdown.cam_x_px;
    target_cam_y = s_topdown.cam_y_px;
  }
  else if (profile == (uint32_t)GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_X)
  {
    target_cam_x = desired_x - ((float)view_w_world * 0.5f);
  }
  else if (profile == (uint32_t)GAME_PACKAGE_CAMERA_PROFILE_FOLLOW_XY)
  {
    target_cam_x = desired_x - ((float)view_w_world * 0.5f);
    target_cam_y = desired_y - ((float)view_h_world * 0.5f);
  }
  else
  {
    float left = s_topdown.cam_x_px;
    float top = s_topdown.cam_y_px;
    float right = left + (float)view_w_world;
    float bottom = top + (float)view_h_world;
    float half_dz_w;
    float half_dz_h;

    dz_w = TopdownCameraDeadzoneWPx();
    dz_h = TopdownCameraDeadzoneHPx();
    if (dz_w > view_w_world)
    {
      dz_w = view_w_world;
    }
    if (dz_h > view_h_world)
    {
      dz_h = view_h_world;
    }
    half_dz_w = (float)dz_w * 0.5f;
    half_dz_h = (float)dz_h * 0.5f;

    if (desired_x < (left + half_dz_w))
    {
      target_cam_x = desired_x - half_dz_w;
    }
    else if (desired_x > (right - half_dz_w))
    {
      target_cam_x = desired_x + half_dz_w - (float)view_w_world;
    }

    if (desired_y < (top + half_dz_h))
    {
      target_cam_y = desired_y - half_dz_h;
    }
    else if (desired_y > (bottom - half_dz_h))
    {
      target_cam_y = desired_y + half_dz_h - (float)view_h_world;
    }
  }

  target_cam_x = TopdownClampF(target_cam_x, 0.0f, (float)max_cam_x);
  target_cam_y = TopdownClampF(target_cam_y, 0.0f, (float)max_cam_y);

  if (follow >= 0.999f)
  {
    s_topdown.cam_x_px = target_cam_x;
    s_topdown.cam_y_px = target_cam_y;
    return;
  }

  step_x = (target_cam_x - s_topdown.cam_x_px) * follow;
  step_y = (target_cam_y - s_topdown.cam_y_px) * follow;
  if (max_step > 0.0f)
  {
    step_x = TopdownClampF(step_x, -max_step, max_step);
    step_y = TopdownClampF(step_y, -max_step, max_step);
  }
  if (TopdownAbsF(target_cam_x - s_topdown.cam_x_px) < 0.5f)
  {
    s_topdown.cam_x_px = target_cam_x;
  }
  else
  {
    s_topdown.cam_x_px += step_x;
  }
  if (TopdownAbsF(target_cam_y - s_topdown.cam_y_px) < 0.5f)
  {
    s_topdown.cam_y_px = target_cam_y;
  }
  else
  {
    s_topdown.cam_y_px += step_y;
  }

  s_topdown.cam_x_px = TopdownClampF(s_topdown.cam_x_px, 0.0f, (float)max_cam_x);
  s_topdown.cam_y_px = TopdownClampF(s_topdown.cam_y_px, 0.0f, (float)max_cam_y);
}

static void TopdownDrawMap(uint32_t cam_x_px,
                           uint32_t cam_y_px,
                           render_2bpp_present_t present_mode,
                           uint8_t blit_scale,
                           uint8_t effective_scale,
                           uint16_t clip_x,
                           uint16_t clip_y,
                           uint16_t clip_w,
                           uint16_t clip_h)
{
  const game_map_view_t *map_view = GameRuntime_GetSceneMap();
  const game_tileset_view_t *tileset_view = GameRuntime_GetSceneTileset();
  uint16_t map_w_tiles;
  uint16_t map_h_tiles;
  uint16_t cell_w;
  uint16_t cell_h;
  uint16_t layer_count;
  uint16_t first_tx;
  uint16_t first_ty;
  uint16_t last_tx;
  uint16_t last_ty;
  uint32_t clip_x0_px;
  uint32_t clip_y0_px;
  uint32_t clip_x1_px;
  uint32_t clip_y1_px;
  uint32_t world_x0_px;
  uint32_t world_y0_px;
  uint32_t world_x1_px;
  uint32_t world_y1_px;
  int32_t draw_cell_w;
  int32_t draw_cell_h;
  uint8_t use_tileset = 0U;
  uint16_t ty;

  if ((map_view == (const game_map_view_t *)0) ||
      (map_view->header == (const game_map_blob_header_t *)0))
  {
    return;
  }
  if ((clip_w == 0U) || (clip_h == 0U))
  {
    return;
  }

  map_w_tiles = map_view->header->map_width;
  map_h_tiles = map_view->header->map_height;
  cell_w = (uint16_t)map_view->header->tile_width;
  cell_h = (uint16_t)map_view->header->tile_height;
  layer_count = GameMap_GetTileLayerCount(map_view);
  if (blit_scale == 0U)
  {
    blit_scale = 1U;
  }
  if (effective_scale == 0U)
  {
    effective_scale = 1U;
  }
  if (layer_count == 0U)
  {
    layer_count = 1U;
  }

  clip_x0_px = (uint32_t)clip_x;
  clip_y0_px = (uint32_t)clip_y;
  clip_x1_px = clip_x0_px + (uint32_t)clip_w;
  clip_y1_px = clip_y0_px + (uint32_t)clip_h;
  if (clip_x1_px > (uint32_t)RENDER_WIDTH)
  {
    clip_x1_px = (uint32_t)RENDER_WIDTH;
  }
  if (clip_y1_px > (uint32_t)RENDER_HEIGHT)
  {
    clip_y1_px = (uint32_t)RENDER_HEIGHT;
  }
  if ((clip_x1_px <= clip_x0_px) || (clip_y1_px <= clip_y0_px))
  {
    return;
  }

  world_x0_px = cam_x_px + (clip_x0_px / (uint32_t)effective_scale);
  world_y0_px = cam_y_px + (clip_y0_px / (uint32_t)effective_scale);
  world_x1_px = cam_x_px + ((clip_x1_px + (uint32_t)effective_scale - 1UL) / (uint32_t)effective_scale);
  world_y1_px = cam_y_px + ((clip_y1_px + (uint32_t)effective_scale - 1UL) / (uint32_t)effective_scale);
  if (world_x1_px <= world_x0_px)
  {
    world_x1_px = world_x0_px + 1UL;
  }
  if (world_y1_px <= world_y0_px)
  {
    world_y1_px = world_y0_px + 1UL;
  }

  draw_cell_w = (int32_t)cell_w * (int32_t)effective_scale;
  draw_cell_h = (int32_t)cell_h * (int32_t)effective_scale;

  first_tx = (uint16_t)(world_x0_px / (uint32_t)cell_w);
  first_ty = (uint16_t)(world_y0_px / (uint32_t)cell_h);
  last_tx = (uint16_t)((world_x1_px + (uint32_t)cell_w - 1UL) / (uint32_t)cell_w);
  last_ty = (uint16_t)((world_y1_px + (uint32_t)cell_h - 1UL) / (uint32_t)cell_h);
  if (last_tx > 0U)
  {
    last_tx--;
  }
  if (last_ty > 0U)
  {
    last_ty--;
  }
  if (last_tx >= map_w_tiles)
  {
    last_tx = (map_w_tiles > 0U) ? (uint16_t)(map_w_tiles - 1U) : 0U;
  }
  if (last_ty >= map_h_tiles)
  {
    last_ty = (map_h_tiles > 0U) ? (uint16_t)(map_h_tiles - 1U) : 0U;
  }

  if ((tileset_view != (const game_tileset_view_t *)0) &&
      (tileset_view->header != (const game_tileset_blob_header_t *)0) &&
      (tileset_view->header->tile_width == cell_w) &&
      (tileset_view->header->tile_height == cell_h))
  {
    use_tileset = 1U;
    TopdownTileCacheEnsureContext(tileset_view->header,
                                  cell_w,
                                  cell_h,
                                  blit_scale,
                                  present_mode);
  }

  Render_SetClipRect(clip_x, clip_y, clip_w, clip_h);
  for (ty = first_ty; (ty < map_h_tiles) && (ty <= last_ty); ty++)
  {
    int32_t world_y = (int32_t)((uint32_t)ty * (uint32_t)cell_h) - (int32_t)cam_y_px;
    int32_t tile_py = world_y * (int32_t)effective_scale;
    uint16_t tx;

    if (tile_py >= (int32_t)RENDER_HEIGHT)
    {
      break;
    }
    if ((tile_py + draw_cell_h) <= 0)
    {
      continue;
    }

    for (tx = first_tx; (tx < map_w_tiles) && (tx <= last_tx); tx++)
    {
      int32_t world_x = (int32_t)((uint32_t)tx * (uint32_t)cell_w) - (int32_t)cam_x_px;
      int32_t tile_px = world_x * (int32_t)effective_scale;
      uint16_t layer_index;

      if (tile_px >= (int32_t)RENDER_WIDTH)
      {
        break;
      }
      if ((tile_px + draw_cell_w) <= 0)
      {
        continue;
      }
      for (layer_index = 0U; layer_index < layer_count; layer_index++)
      {
        uint16_t gid = GameMap_GetTileGidAtLayer(map_view, tx, ty, layer_index);
        if (gid == 0U)
        {
          continue;
        }

        if (use_tileset != 0U)
        {
          const uint8_t *tile_color = (const uint8_t *)0;
          const uint8_t *tile_mask = (const uint8_t *)0;
          uint8_t tile_fast_no_clip_ok = 0U;

          if ((tile_px >= (int32_t)clip_x0_px) &&
              (tile_py >= (int32_t)clip_y0_px) &&
              ((tile_px + draw_cell_w) <= (int32_t)clip_x1_px) &&
              ((tile_py + draw_cell_h) <= (int32_t)clip_y1_px))
          {
            tile_fast_no_clip_ok = 1U;
          }

          if (GameTileset_TryGetTileByGid(tileset_view, gid, &tile_color, &tile_mask) != 0U)
          {
            if (TopdownTileCacheTryDrawTile(gid,
                                            tile_px,
                                            tile_py,
                                            cell_w,
                                            cell_h,
                                            tile_color,
                                            tile_mask,
                                            tileset_view->header->color_stride,
                                            tileset_view->header->mask_stride,
                                            blit_scale,
                                            present_mode,
                                            RENDER_LAYER_BG,
                                            tile_fast_no_clip_ok) == 0U)
            {
              if ((tile_px >= 0) &&
                  (tile_py >= 0) &&
                  ((tile_px + draw_cell_w) <= (int32_t)RENDER_WIDTH) &&
                  ((tile_py + draw_cell_h) <= (int32_t)RENDER_HEIGHT))
              {
                renderBlitMasked2bppScaled((uint16_t)tile_px,
                                           (uint16_t)tile_py,
                                           cell_w,
                                           cell_h,
                                           tile_color,
                                           tile_mask,
                                           tileset_view->header->color_stride,
                                           tileset_view->header->mask_stride,
                                           true,
                                           RENDER_LAYER_BG,
                                           present_mode,
                                           blit_scale);
              }
              else
              {
                TopdownDrawTile2bppScaledClipped(tile_px,
                                                 tile_py,
                                                 cell_w,
                                                 cell_h,
                                                 tile_color,
                                                 tile_mask,
                                                 tileset_view->header->color_stride,
                                                 tileset_view->header->mask_stride,
                                                 blit_scale,
                                                 present_mode,
                                                 RENDER_LAYER_BG);
              }
            }
            continue;
          }
        }

        if ((tile_px >= 0) && (tile_py >= 0))
        {
          uint16_t draw_w = (uint16_t)((uint32_t)cell_w * (uint32_t)effective_scale);
          uint16_t draw_h = (uint16_t)((uint32_t)cell_h * (uint32_t)effective_scale);
          renderFillRect((uint16_t)tile_px,
                         (uint16_t)tile_py,
                         draw_w,
                         draw_h,
                         RENDER_LAYER_BG,
                         ((gid & 1U) != 0U) ? RENDER_COLOR_BLACK : RENDER_COLOR_WHITE);
        }
      }
    }
  }
  Render_ClearClip();
}

static void TopdownDrawPlayer(uint32_t cam_x_px,
                              uint32_t cam_y_px,
                              render_2bpp_present_t present_mode,
                              uint8_t blit_scale,
                              uint8_t effective_scale)
{
  uint8_t sprite_scale;
  uint8_t sprite_effective_scale;
  int32_t sprite_w;
  int32_t sprite_h;
  int32_t world_sx;
  int32_t world_sy;
  int32_t sx;
  int32_t sy;
  const game_sprite_frame_t *frame;
  uint8_t visible = 0U;
  uint8_t need_clear_prev = 0U;
  uint8_t need_draw = 0U;
  const uint8_t *frame_color = (const uint8_t *)0;
  const uint8_t *frame_mask = (const uint8_t *)0;

  sprite_scale = (uint8_t)((uint32_t)TOPDOWN_PLAYER_BASE_SPR_SCALE * (uint32_t)blit_scale);
  if (sprite_scale == 0U)
  {
    sprite_scale = 1U;
  }
  if (sprite_scale > 8U)
  {
    sprite_scale = 8U;
  }
  sprite_effective_scale = sprite_scale;
  if (present_mode == RENDER_2BPP_PRESENT_BAYER2X2)
  {
    uint16_t doubled = (uint16_t)sprite_effective_scale * 2U;
    if (doubled > 255U)
    {
      doubled = 255U;
    }
    sprite_effective_scale = (uint8_t)doubled;
  }

  sprite_w = (int32_t)TOPDOWN_PLAYER_SPR_W_PX * (int32_t)sprite_effective_scale;
  sprite_h = (int32_t)TOPDOWN_PLAYER_SPR_H_PX * (int32_t)sprite_effective_scale;
  world_sx = s_topdown.player_x_px - (int32_t)cam_x_px;
  world_sy = s_topdown.player_y_px - (int32_t)cam_y_px;
  sx = world_sx * (int32_t)effective_scale - (sprite_w / 2);
  sy = world_sy * (int32_t)effective_scale - (sprite_h / 2);

  frame = TopdownCurrentPlayerFrame();
  if ((frame == (const game_sprite_frame_t *)0) ||
      (frame->color2bpp == (const uint8_t *)0) ||
      (frame->mask1bpp == (const uint8_t *)0))
  {
    visible = 0U;
  }
  else
  {
    frame_color = frame->color2bpp;
    frame_mask = frame->mask1bpp;
    if ((sx < (int32_t)RENDER_WIDTH) &&
        (sy < (int32_t)RENDER_HEIGHT) &&
        ((sx + sprite_w) > 0) &&
        ((sy + sprite_h) > 0))
    {
      visible = 1U;
    }
  }

  if (s_topdown.player_rect_valid != 0U)
  {
    if ((visible == 0U) ||
        (s_topdown.player_prev_sx != (int16_t)sx) ||
        (s_topdown.player_prev_sy != (int16_t)sy) ||
        (s_topdown.player_prev_w != (uint16_t)sprite_w) ||
        (s_topdown.player_prev_h != (uint16_t)sprite_h) ||
        (s_topdown.player_prev_color2bpp != frame_color) ||
        (s_topdown.player_prev_mask1bpp != frame_mask))
    {
      need_clear_prev = 1U;
    }
  }

  if (need_clear_prev != 0U)
  {
    TopdownClearLayerRectTransparent(RENDER_LAYER_GAME,
                                     (int32_t)s_topdown.player_prev_sx,
                                     (int32_t)s_topdown.player_prev_sy,
                                     (int32_t)s_topdown.player_prev_w,
                                     (int32_t)s_topdown.player_prev_h);
  }

  if (visible != 0U)
  {
    if ((s_topdown.player_rect_valid == 0U) || (need_clear_prev != 0U))
    {
      need_draw = 1U;
    }
  }

  if (need_draw != 0U)
  {
    if (TopdownPlayerCacheTryDraw(frame_color,
                                  frame_mask,
                                  sx,
                                  sy,
                                  sprite_scale,
                                  present_mode,
                                  RENDER_LAYER_GAME) == 0U)
    {
      if ((sx >= 0) &&
          (sy >= 0) &&
          ((sx + sprite_w) <= (int32_t)RENDER_WIDTH) &&
          ((sy + sprite_h) <= (int32_t)RENDER_HEIGHT))
      {
        renderBlitMasked2bppScaled((uint16_t)sx,
                                   (uint16_t)sy,
                                   TOPDOWN_PLAYER_SPR_W_PX,
                                   TOPDOWN_PLAYER_SPR_H_PX,
                                   frame_color,
                                   frame_mask,
                                   TOPDOWN_PLAYER_SPR_COLOR_ROW_BYTES,
                                   TOPDOWN_PLAYER_SPR_MASK_ROW_BYTES,
                                   true,
                                   RENDER_LAYER_GAME,
                                   present_mode,
                                   sprite_scale);
      }
      else
      {
        TopdownDrawTile2bppScaledClipped(sx,
                                         sy,
                                         TOPDOWN_PLAYER_SPR_W_PX,
                                         TOPDOWN_PLAYER_SPR_H_PX,
                                         frame_color,
                                         frame_mask,
                                         TOPDOWN_PLAYER_SPR_COLOR_ROW_BYTES,
                                         TOPDOWN_PLAYER_SPR_MASK_ROW_BYTES,
                                         sprite_scale,
                                         present_mode,
                                         RENDER_LAYER_GAME);
      }
    }
  }

  if (visible != 0U)
  {
    s_topdown.player_rect_valid = 1U;
    s_topdown.player_prev_sx = (int16_t)sx;
    s_topdown.player_prev_sy = (int16_t)sy;
    s_topdown.player_prev_w = (uint16_t)sprite_w;
    s_topdown.player_prev_h = (uint16_t)sprite_h;
    s_topdown.player_prev_color2bpp = frame_color;
    s_topdown.player_prev_mask1bpp = frame_mask;
  }
  else
  {
    s_topdown.player_rect_valid = 0U;
    s_topdown.player_prev_color2bpp = (const uint8_t *)0;
    s_topdown.player_prev_mask1bpp = (const uint8_t *)0;
  }
}

static uint8_t TopdownTryShiftedMapRedraw(uint32_t cam_x_px,
                                          uint32_t cam_y_px,
                                          uint32_t prev_cam_x_px,
                                          uint32_t prev_cam_y_px,
                                          render_2bpp_present_t present_mode,
                                          uint8_t blit_scale,
                                          uint8_t effective_scale)
{
  /* Shift cached BG pixels, then redraw only the newly exposed strips. */
  int32_t cam_dx = (int32_t)cam_x_px - (int32_t)prev_cam_x_px;
  int32_t cam_dy = (int32_t)cam_y_px - (int32_t)prev_cam_y_px;
  int32_t shift_x = -cam_dx * (int32_t)effective_scale;
  int32_t shift_y = -cam_dy * (int32_t)effective_scale;

  if ((shift_x == 0) && (shift_y == 0))
  {
    return 1U;
  }

  if ((shift_x <= -(int32_t)RENDER_WIDTH) || (shift_x >= (int32_t)RENDER_WIDTH) ||
      (shift_y <= -(int32_t)RENDER_HEIGHT) || (shift_y >= (int32_t)RENDER_HEIGHT))
  {
    return 0U;
  }

  if (Render_ShiftLayer(RENDER_LAYER_BG,
                        (int16_t)shift_x,
                        (int16_t)shift_y,
                        RENDER_COLOR_WHITE) == false)
  {
    return 0U;
  }

  if (shift_x > 0)
  {
    TopdownDrawMap(cam_x_px,
                   cam_y_px,
                   present_mode,
                   blit_scale,
                   effective_scale,
                   0U,
                   0U,
                   (uint16_t)shift_x,
                   (uint16_t)RENDER_HEIGHT);
  }
  else if (shift_x < 0)
  {
    uint16_t strip_w = (uint16_t)(-shift_x);
    uint16_t strip_x = (uint16_t)((uint16_t)RENDER_WIDTH - strip_w);
    TopdownDrawMap(cam_x_px,
                   cam_y_px,
                   present_mode,
                   blit_scale,
                   effective_scale,
                   strip_x,
                   0U,
                   strip_w,
                   (uint16_t)RENDER_HEIGHT);
  }

  if (shift_y > 0)
  {
    TopdownDrawMap(cam_x_px,
                   cam_y_px,
                   present_mode,
                   blit_scale,
                   effective_scale,
                   0U,
                   0U,
                   (uint16_t)RENDER_WIDTH,
                   (uint16_t)shift_y);
  }
  else if (shift_y < 0)
  {
    uint16_t strip_h = (uint16_t)(-shift_y);
    uint16_t strip_y = (uint16_t)((uint16_t)RENDER_HEIGHT - strip_h);
    TopdownDrawMap(cam_x_px,
                   cam_y_px,
                   present_mode,
                   blit_scale,
                   effective_scale,
                   0U,
                   strip_y,
                   (uint16_t)RENDER_WIDTH,
                   strip_h);
  }

  return 1U;
}

uint8_t GameModeTopdownBasic_SaveSnapshot(game_mode_topdown_basic_snapshot_t *snapshot_out)
{
  if (snapshot_out == (game_mode_topdown_basic_snapshot_t *)0)
  {
    return 0U;
  }
  if (s_topdown.initialized == 0U)
  {
    return 0U;
  }

  (void)memset(snapshot_out, 0, sizeof(*snapshot_out));
  snapshot_out->version = GAME_MODE_TOPDOWN_BASIC_SNAPSHOT_VERSION;
  snapshot_out->valid = 1U;
  snapshot_out->spawn_locked = (s_topdown.spawn_locked != 0U) ? 1U : 0U;
  snapshot_out->camera_initialized = (s_topdown.camera_initialized != 0U) ? 1U : 0U;
  snapshot_out->exit_zone_latched = (s_topdown.exit_zone_latched != 0U) ? 1U : 0U;
  snapshot_out->player_x_px = s_topdown.player_x_px;
  snapshot_out->player_y_px = s_topdown.player_y_px;
  snapshot_out->player_vx_px_s = s_topdown.player_vx_px_s;
  snapshot_out->player_vy_px_s = s_topdown.player_vy_px_s;
  snapshot_out->player_rem_x_px = s_topdown.player_rem_x_px;
  snapshot_out->player_rem_y_px = s_topdown.player_rem_y_px;
  snapshot_out->cam_x_px = s_topdown.cam_x_px;
  snapshot_out->cam_y_px = s_topdown.cam_y_px;
  snapshot_out->spawn_request_hash = s_topdown.spawn_request_hash;
  snapshot_out->player_anim = s_topdown.player_anim;
  return 1U;
}

uint8_t GameModeTopdownBasic_LoadSnapshot(const game_mode_topdown_basic_snapshot_t *snapshot)
{
  uint8_t idle_clip;
  const game_map_view_t *map_view = GameRuntime_GetSceneMap();
  uint32_t map_w_px = 0UL;
  uint32_t map_h_px = 0UL;

  if (snapshot == (const game_mode_topdown_basic_snapshot_t *)0)
  {
    return 0U;
  }
  if ((snapshot->valid == 0U) ||
      (snapshot->version != GAME_MODE_TOPDOWN_BASIC_SNAPSHOT_VERSION))
  {
    return 0U;
  }
  if (TopdownMapMetrics(map_view, &map_w_px, &map_h_px, (uint16_t *)0, (uint16_t *)0) == 0U)
  {
    return 0U;
  }
  if ((snapshot->player_x_px < 0) ||
      (snapshot->player_y_px < 0) ||
      ((uint32_t)snapshot->player_x_px >= map_w_px) ||
      ((uint32_t)snapshot->player_y_px >= map_h_px))
  {
    return 0U;
  }
  if (TopdownCanOccupyPx(map_view, snapshot->player_x_px, snapshot->player_y_px) == 0U)
  {
    return 0U;
  }

  GameModeTopdownBasic_Reset();
  s_topdown.spawn_locked = (snapshot->spawn_locked != 0U) ? 1U : 0U;
  s_topdown.camera_initialized = (snapshot->camera_initialized != 0U) ? 1U : 0U;
  s_topdown.exit_zone_latched = (snapshot->exit_zone_latched != 0U) ? 1U : 0U;
  s_topdown.player_x_px = snapshot->player_x_px;
  s_topdown.player_y_px = snapshot->player_y_px;
  s_topdown.player_vx_px_s = snapshot->player_vx_px_s;
  s_topdown.player_vy_px_s = snapshot->player_vy_px_s;
  s_topdown.player_rem_x_px = snapshot->player_rem_x_px;
  s_topdown.player_rem_y_px = snapshot->player_rem_y_px;
  s_topdown.cam_x_px = snapshot->cam_x_px;
  s_topdown.cam_y_px = snapshot->cam_y_px;
  s_topdown.spawn_request_hash = snapshot->spawn_request_hash;
  if (s_topdown.spawn_request_hash == 0UL)
  {
    s_topdown.spawn_request_hash = TopdownFnv1a32("player_start");
  }

  s_topdown.player_anim = snapshot->player_anim;
  if (s_topdown.player_anim.dir >= (uint8_t)GAME_SPRITE_DIR_COUNT)
  {
    s_topdown.player_anim.dir = (uint8_t)GAME_SPRITE_DIR_DOWN;
  }
  if (GameSpriteAnim_HasClip(&player_sprite_set, s_topdown.player_anim.clip_index) == 0U)
  {
    idle_clip = TopdownPlayerIdleClipIndex();
    if (idle_clip == 0xFFU)
    {
      idle_clip = 0U;
    }
    GameSpriteAnim_Init(&s_topdown.player_anim, idle_clip, (uint8_t)GAME_SPRITE_DIR_DOWN);
  }

  /* Force a fresh redraw/caches on the next frame after restore. */
  s_topdown.clear_pending = 1U;
  s_topdown.map_cache_valid = 0U;
  s_topdown.player_rect_valid = 0U;
  s_topdown.hud_valid = 0U;
  s_topdown.last_cam_x_px = 0UL;
  s_topdown.last_cam_y_px = 0UL;
  s_topdown.map_cache_header = (const game_map_blob_header_t *)0;
  s_topdown.tileset_cache_header = (const game_tileset_blob_header_t *)0;
  s_topdown.player_prev_color2bpp = (const uint8_t *)0;
  s_topdown.player_prev_mask1bpp = (const uint8_t *)0;
  TopdownTileCacheInvalidate();
  TopdownPlayerCacheInvalidate();
  return 1U;
}

void GameModeTopdownBasic_Reset(void)
{
  uint8_t idle_clip;
  (void)memset(&s_topdown, 0, sizeof(s_topdown));
  (void)memset(&s_topdown_tile_cache, 0, sizeof(s_topdown_tile_cache));
  (void)memset(&s_topdown_player_cache, 0, sizeof(s_topdown_player_cache));
  TopdownPlayerCacheInvalidate();
  s_topdown.initialized = 1U;
  s_topdown.clear_pending = 1U;
  s_topdown.spawn_request_hash = TopdownFnv1a32("player_start");
  idle_clip = TopdownPlayerIdleClipIndex();
  if (idle_clip == 0xFFU)
  {
    idle_clip = 0U;
  }
  GameSpriteAnim_Init(&s_topdown.player_anim, idle_clip, (uint8_t)GAME_SPRITE_DIR_DOWN);
}

void GameModeTopdownBasic_RequestSpawn(uint32_t spawn_hash, uint8_t require_exact)
{
  if (s_topdown.initialized == 0U)
  {
    GameModeTopdownBasic_Reset();
  }

  s_topdown.spawn_request_hash =
      (spawn_hash != 0UL) ? spawn_hash : TopdownFnv1a32("player_start");
  s_topdown.spawn_require_exact = (require_exact != 0U) ? 1U : 0U;
  s_topdown.spawn_locked = 0U;
  s_topdown.camera_initialized = 0U;
}

uint32_t GameModeTopdownBasic_LastInteractScriptHash(void)
{
  return s_topdown.last_interact_script_hash;
}

uint32_t GameModeTopdownBasic_LastInteractDialogueHash(void)
{
  return s_topdown.last_interact_dialogue_hash;
}

uint8_t GameModeTopdownBasic_HandleControl(const game_runtime_input_t *input,
                                           uint8_t *request_exit_to_static,
                                           game_runtime_audio_cue_t *audio_cue_out)
{
  if (request_exit_to_static != (uint8_t *)0)
  {
    *request_exit_to_static = 0U;
  }
  if (audio_cue_out != (game_runtime_audio_cue_t *)0)
  {
    *audio_cue_out = GAME_RT_AUDIO_CUE_NONE;
  }

  if (input == (const game_runtime_input_t *)0)
  {
    return 0U;
  }

  s_topdown.held_mask = (uint32_t)input->pressed_mask;

  if ((input->event == (ULONG)GAME_RT_INPUT_EVENT_PRESS) ||
      (input->event == (ULONG)GAME_RT_INPUT_EVENT_LONG))
  {
    if (input->source == (ULONG)GAME_RT_INPUT_SRC_BTN_B)
    {
      if (audio_cue_out != (game_runtime_audio_cue_t *)0)
      {
        *audio_cue_out = GAME_RT_AUDIO_CUE_SECONDARY;
      }
      return 1U;
    }

    if (input->source == (ULONG)GAME_RT_INPUT_SRC_BTN_A)
    {
      const game_map_view_t *map_view = GameRuntime_GetSceneMap();
      const game_map_object_t *interact_obj = TopdownFindOverlappingInteract(map_view);
      if (interact_obj != (const game_map_object_t *)0)
      {
        s_topdown.last_interact_script_hash = interact_obj->arg0;
        s_topdown.last_interact_dialogue_hash = interact_obj->arg1;
      }
      if (audio_cue_out != (game_runtime_audio_cue_t *)0)
      {
        *audio_cue_out = (interact_obj != (const game_map_object_t *)0)
                             ? GAME_RT_AUDIO_CUE_GAME_ACTION
                             : GAME_RT_AUDIO_CUE_PRIMARY;
      }
      return 1U;
    }
  }

  return 0U;
}

void GameModeTopdownBasic_Update(const app_sensor_snapshot_t *sensor_snapshot, uint32_t dt_ms)
{
  const game_map_view_t *map_view = GameRuntime_GetSceneMap();
  uint32_t profile = TopdownControllerProfile();
  float speed_px_s = (float)TopdownMoveSpeedPxPerSec();
  float accel_px_s2 = (float)TopdownMoveAccelPxPerSec2();
  float decel_px_s2 = (float)TopdownMoveDecelPxPerSec2();
  float dt_s = (float)dt_ms / 1000.0f;
  float move_x = 0.0f;
  float move_y = 0.0f;
  float target_vx;
  float target_vy;
  float max_v_step_x;
  float max_v_step_y;
  int32_t dx;
  int32_t dy;
  int32_t moved_x;
  int32_t moved_y;

  if (s_topdown.initialized == 0U)
  {
    GameModeTopdownBasic_Reset();
  }

  TopdownHandleSceneSwap(map_view);
  TopdownTryInitSpawn();
  TopdownTryInitCamera(map_view);
  if (s_topdown.spawn_locked == 0U)
  {
    return;
  }

  TopdownBuildMoveIntent(sensor_snapshot, &move_x, &move_y);
  target_vx = move_x * speed_px_s;
  target_vy = move_y * speed_px_s;

  if (dt_s < 0.0001f)
  {
    TopdownUpdateCamera(map_view, dt_ms);
    TopdownHandleExitOverlap(map_view);
    return;
  }

  max_v_step_x = ((TopdownAbsF(target_vx) > TopdownAbsF(s_topdown.player_vx_px_s)) || (move_x != 0.0f))
                     ? (accel_px_s2 * dt_s)
                     : (decel_px_s2 * dt_s);
  max_v_step_y = ((TopdownAbsF(target_vy) > TopdownAbsF(s_topdown.player_vy_px_s)) || (move_y != 0.0f))
                     ? (accel_px_s2 * dt_s)
                     : (decel_px_s2 * dt_s);
  s_topdown.player_vx_px_s = TopdownApproachF(s_topdown.player_vx_px_s, target_vx, max_v_step_x);
  s_topdown.player_vy_px_s = TopdownApproachF(s_topdown.player_vy_px_s, target_vy, max_v_step_y);

  if ((move_x == 0.0f) && (TopdownAbsF(s_topdown.player_vx_px_s) < 0.25f))
  {
    s_topdown.player_vx_px_s = 0.0f;
    s_topdown.player_rem_x_px = 0.0f;
  }
  if ((move_y == 0.0f) && (TopdownAbsF(s_topdown.player_vy_px_s) < 0.25f))
  {
    s_topdown.player_vy_px_s = 0.0f;
    s_topdown.player_rem_y_px = 0.0f;
  }

  dx = TopdownToPixelDelta(&s_topdown.player_rem_x_px, s_topdown.player_vx_px_s, dt_ms);
  dy = TopdownToPixelDelta(&s_topdown.player_rem_y_px, s_topdown.player_vy_px_s, dt_ms);
  if ((profile == (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_TOPDOWN_DIGITAL_8DIR) &&
      (dx == 0) &&
      (move_x != 0.0f))
  {
    dx = (int32_t)TopdownSignedStepFromFloat(move_x);
  }
  if ((profile == (uint32_t)GAME_PACKAGE_CONTROLLER_PROFILE_TOPDOWN_DIGITAL_8DIR) &&
      (dy == 0) &&
      (move_y != 0.0f))
  {
    dy = (int32_t)TopdownSignedStepFromFloat(move_y);
  }

  moved_x = TopdownMoveAxis(map_view, &s_topdown.player_x_px, s_topdown.player_y_px, dx, 1U);
  moved_y = TopdownMoveAxis(map_view, &s_topdown.player_y_px, s_topdown.player_x_px, dy, 0U);
  if (moved_x != dx)
  {
    s_topdown.player_vx_px_s = 0.0f;
    s_topdown.player_rem_x_px = 0.0f;
  }
  if (moved_y != dy)
  {
    s_topdown.player_vy_px_s = 0.0f;
    s_topdown.player_rem_y_px = 0.0f;
  }

  TopdownUpdatePlayerAnim(moved_x, moved_y, dt_ms);
  TopdownUpdateCamera(map_view, dt_ms);
  TopdownHandleExitOverlap(map_view);
}

void GameModeTopdownBasic_DrawFrame(const app_sensor_snapshot_t *sensor_snapshot)
{
  const game_map_view_t *map_view = GameRuntime_GetSceneMap();
  const game_tileset_view_t *tileset_view = GameRuntime_GetSceneTileset();
  uint32_t cam_x_px = 0UL;
  uint32_t cam_y_px = 0UL;
  uint8_t render_scale_req = TopdownRenderScale();
  render_2bpp_present_t present_mode = TopdownPresentMode(render_scale_req);
  uint8_t blit_scale = TopdownBlitScaleForPresent(render_scale_req, present_mode);
  uint8_t render_scale = TopdownEffectiveScaleForPresent(blit_scale, present_mode);
  uint8_t redraw_map = 0U;
  uint8_t map_config_changed = 0U;
  uint8_t cam_changed = 0U;

  (void)sensor_snapshot;

  if (s_topdown.clear_pending != 0U)
  {
    renderClear(RENDER_COLOR_WHITE);
    s_topdown.clear_pending = 0U;
    s_topdown.map_cache_valid = 0U;
    s_topdown.player_rect_valid = 0U;
    s_topdown.hud_valid = 0U;
    s_topdown.map_cache_header = (const game_map_blob_header_t *)0;
    s_topdown.tileset_cache_header = (const game_tileset_blob_header_t *)0;
  }

  if ((map_view == (const game_map_view_t *)0) ||
      (map_view->header == (const game_map_blob_header_t *)0))
  {
    if ((s_topdown.map_cache_valid == 0U) ||
        (s_topdown.map_cache_header != (const game_map_blob_header_t *)0))
    {
      renderClear(RENDER_COLOR_WHITE);
      renderDrawText(4U, 4U, "NO MAP", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      s_topdown.player_rect_valid = 0U;
      s_topdown.hud_valid = 0U;
      s_topdown.map_cache_valid = 1U;
      s_topdown.map_cache_header = (const game_map_blob_header_t *)0;
      s_topdown.tileset_cache_header = (const game_tileset_blob_header_t *)0;
    }
    return;
  }

  TopdownTryInitCamera(map_view);
  {
    uint32_t map_w_px = 0UL;
    uint32_t map_h_px = 0UL;
    uint32_t view_w_world = (uint32_t)RENDER_WIDTH / (uint32_t)render_scale;
    uint32_t view_h_world = (uint32_t)RENDER_HEIGHT / (uint32_t)render_scale;
    int32_t max_cam_x;
    int32_t max_cam_y;
    int32_t cam_x;
    int32_t cam_y;

    if (view_w_world == 0UL)
    {
      view_w_world = 1UL;
    }
    if (view_h_world == 0UL)
    {
      view_h_world = 1UL;
    }

    (void)TopdownMapMetrics(map_view, &map_w_px, &map_h_px, (uint16_t *)0, (uint16_t *)0);
    max_cam_x = (map_w_px > view_w_world) ? (int32_t)(map_w_px - view_w_world) : 0;
    max_cam_y = (map_h_px > view_h_world) ? (int32_t)(map_h_px - view_h_world) : 0;
    cam_x = TopdownClampI32((int32_t)(s_topdown.cam_x_px + 0.5f), 0, max_cam_x);
    cam_y = TopdownClampI32((int32_t)(s_topdown.cam_y_px + 0.5f), 0, max_cam_y);

    cam_x_px = (uint32_t)cam_x;
    cam_y_px = (uint32_t)cam_y;
  }

  map_config_changed =
      ((s_topdown.map_cache_header != map_view->header) ||
       (s_topdown.tileset_cache_header !=
        ((tileset_view != (const game_tileset_view_t *)0) ? tileset_view->header : (const game_tileset_blob_header_t *)0)) ||
       (s_topdown.last_present_mode != (uint8_t)present_mode) ||
       (s_topdown.last_blit_scale != blit_scale) ||
       (s_topdown.last_effective_scale != render_scale))
          ? 1U
          : 0U;
  cam_changed =
      ((s_topdown.last_cam_x_px != cam_x_px) ||
       (s_topdown.last_cam_y_px != cam_y_px))
          ? 1U
          : 0U;

  if ((s_topdown.map_cache_valid == 0U) || (map_config_changed != 0U))
  {
    redraw_map = 1U;
  }
  else if (cam_changed != 0U)
  {
    if (TopdownTryShiftedMapRedraw(cam_x_px,
                                   cam_y_px,
                                   s_topdown.last_cam_x_px,
                                   s_topdown.last_cam_y_px,
                                   present_mode,
                                   blit_scale,
                                   render_scale) == 0U)
    {
      redraw_map = 1U;
    }
  }

  if (redraw_map != 0U)
  {
    renderFillRect(0U,
                   0U,
                   (uint16_t)RENDER_WIDTH,
                   (uint16_t)RENDER_HEIGHT,
                   RENDER_LAYER_BG,
                   RENDER_COLOR_WHITE);
    TopdownDrawMap(cam_x_px,
                   cam_y_px,
                   present_mode,
                   blit_scale,
                   render_scale,
                   0U,
                   0U,
                   (uint16_t)RENDER_WIDTH,
                   (uint16_t)RENDER_HEIGHT);
    s_topdown.map_cache_valid = 1U;
    s_topdown.map_cache_header = map_view->header;
    s_topdown.tileset_cache_header =
        (tileset_view != (const game_tileset_view_t *)0) ? tileset_view->header : (const game_tileset_blob_header_t *)0;
  }

  s_topdown.last_cam_x_px = cam_x_px;
  s_topdown.last_cam_y_px = cam_y_px;
  s_topdown.last_present_mode = (uint8_t)present_mode;
  s_topdown.last_blit_scale = blit_scale;
  s_topdown.last_effective_scale = render_scale;

  TopdownDrawPlayer(cam_x_px, cam_y_px, present_mode, blit_scale, render_scale);
  TopdownUpdateFpsHud();
  TopdownDrawPerfHud();
}
