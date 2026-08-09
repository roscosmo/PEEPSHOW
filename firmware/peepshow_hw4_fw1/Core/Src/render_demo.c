#include "render_demo.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
#include "knobs_autogen.h"
#include "main.h"
#include "th_mode.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define UI_BAR_H_PIXELS       (14U)
#define CUBE_EDGE_THICKNESS   (4U)
#define BG_PATTERN_W_PIXELS   (20U)
#define BG_PATTERN_H_PIXELS   (34U)
#define BG_PATTERN_STRIDE     ((BG_PATTERN_W_PIXELS + 7U) >> 3U)
#define RENDER_DEMO_TILE_PRESENT_MODE (RENDER_2BPP_PRESENT_BAYER2X2)

typedef struct
{
  float x;
  float y;
  float z;
} vec3_t;

typedef struct
{
  int16_t x;
  int16_t y;
} pt2_t;

typedef struct
{
  uint8_t initialized;
  uint8_t bg_enabled;
  uint8_t cube_enabled;
  uint16_t width;
  uint16_t height;
  uint16_t ui_bar_h;
  uint16_t game_y0;
  uint16_t game_y1;
  float ay;
  float ax;
  uint32_t frame_id;
  uint32_t scroll_x;
  uint32_t scroll_y;
  uint32_t fps;
  uint32_t fps_ms_acc;
  uint32_t fps_frames;
  uint32_t uptime_ms;
  uint32_t boot_ms;
  uint32_t last_frame_ms;
} render_demo_state_t;

static render_demo_state_t s_demo;
static uint8_t s_bg_shifted[(uint32_t)BG_PATTERN_STRIDE * BG_PATTERN_H_PIXELS];

typedef enum
{
  RENDER_DEMO_ROLE_IGNORE = 0U,
  RENDER_DEMO_ROLE_PRIMARY = 1U,
  RENDER_DEMO_ROLE_SECONDARY = 2U,
  RENDER_DEMO_ROLE_BACK = 3U
} render_demo_role_t;

static const uint8_t kBgPattern1bpp[] =
{
  0xD5U, 0x62U, 0x20U, 0xAAU, 0xE8U, 0x80U, 0xD5U, 0x62U, 0x20U, 0xAAU, 0xE8U, 0x80U, 0xD5U, 0x62U, 0x20U, 0xAAU,
  0xE8U, 0x80U, 0xD5U, 0xF2U, 0x20U, 0xABU, 0xFCU, 0x80U, 0xDFU, 0x0FU, 0x20U, 0xBCU, 0x03U, 0xC0U, 0xF0U, 0x00U,
  0xF0U, 0xC0U, 0x00U, 0x30U, 0xB0U, 0x00U, 0xF0U, 0x8CU, 0x03U, 0x50U, 0xA3U, 0x0EU, 0xB0U, 0x88U, 0xF5U, 0x50U,
  0xA2U, 0x2AU, 0xB0U, 0x88U, 0xB5U, 0x50U, 0xA2U, 0x2AU, 0xB0U, 0x88U, 0xB5U, 0x50U, 0xA2U, 0x2AU, 0xB0U, 0x88U,
  0xB5U, 0x50U, 0xA2U, 0x2AU, 0xB0U, 0xC8U, 0xB5U, 0x70U, 0xF2U, 0x2AU, 0xF0U, 0x3CU, 0xB7U, 0xC0U, 0x0FU, 0x2FU,
  0x00U, 0x03U, 0xFCU, 0x00U, 0x00U, 0xF0U, 0x00U, 0x03U, 0xECU, 0x00U, 0x0DU, 0x63U, 0x00U, 0x3AU, 0xE8U, 0xC0U,
  0xD5U, 0x62U, 0x30U, 0xAAU, 0xE8U, 0x80U
};

static const vec3_t kCubeVerts[8] =
{
  {-0.6f, -0.6f, -0.6f}, {+0.6f, -0.6f, -0.6f},
  {+0.6f, +0.6f, -0.6f}, {-0.6f, +0.6f, -0.6f},
  {-0.6f, -0.6f, +0.6f}, {+0.6f, -0.6f, +0.6f},
  {+0.6f, +0.6f, +0.6f}, {-0.6f, +0.6f, +0.6f}
};

static const uint8_t kCubeEdges[12][2] =
{
  {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U},
  {4U, 5U}, {5U, 6U}, {6U, 7U}, {7U, 4U},
  {0U, 4U}, {1U, 5U}, {2U, 6U}, {3U, 7U}
};

static int32_t RenderDemo_Round(float x)
{
  return (int32_t)(x >= 0.0f ? (x + 0.5f) : (x - 0.5f));
}

static float RenderDemo_AbsF(float x)
{
  return (x >= 0.0f) ? x : -x;
}

static float RenderDemo_ClampF(float x, float lo, float hi)
{
  if (x < lo)
  {
    return lo;
  }
  if (x > hi)
  {
    return hi;
  }
  return x;
}

static uint32_t RenderDemo_AddSignedU32(uint32_t base, int32_t delta)
{
  if (delta >= 0)
  {
    uint32_t add = (uint32_t)delta;
    if ((UINT32_MAX - base) < add)
    {
      return UINT32_MAX;
    }
    return (uint32_t)(base + add);
  }

  {
    uint32_t sub = (uint32_t)(-delta);
    if (sub > base)
    {
      return 0U;
    }
    return (uint32_t)(base - sub);
  }
}

static render_demo_role_t RenderDemo_RoleForSource(ULONG source)
{
  ULONG role = 0UL;

  switch (source)
  {
    case GAME_RT_INPUT_SRC_BTN_A:
      role = (ULONG)KNOB_GAME_RT_DEMO_BTN_A_ROLE;
      break;

    case GAME_RT_INPUT_SRC_BTN_B:
      role = (ULONG)KNOB_GAME_RT_DEMO_BTN_B_ROLE;
      break;

    case GAME_RT_INPUT_SRC_BTN_L:
      role = (ULONG)KNOB_GAME_RT_DEMO_BTN_L_ROLE;
      break;

    case GAME_RT_INPUT_SRC_BTN_R:
      role = (ULONG)KNOB_GAME_RT_DEMO_BTN_R_ROLE;
      break;

    case GAME_RT_INPUT_SRC_JOY_UP:
      role = (ULONG)KNOB_GAME_RT_DEMO_JOY_UP_ROLE;
      break;

    case GAME_RT_INPUT_SRC_JOY_RIGHT:
      role = (ULONG)KNOB_GAME_RT_DEMO_JOY_RIGHT_ROLE;
      break;

    case GAME_RT_INPUT_SRC_JOY_DOWN:
      role = (ULONG)KNOB_GAME_RT_DEMO_JOY_DOWN_ROLE;
      break;

    case GAME_RT_INPUT_SRC_JOY_LEFT:
      role = (ULONG)KNOB_GAME_RT_DEMO_JOY_LEFT_ROLE;
      break;

    default:
      role = (ULONG)RENDER_DEMO_ROLE_IGNORE;
      break;
  }

  if (role > (ULONG)RENDER_DEMO_ROLE_BACK)
  {
    role = (ULONG)RENDER_DEMO_ROLE_IGNORE;
  }

  return (render_demo_role_t)role;
}

static int32_t RenderDemo_LisRawToScrollDelta(int32_t raw)
{
  const int32_t deadband = 1200;
  const int32_t quantum = 3000;
  const int32_t max_step = 4;
  int32_t mag;
  int32_t step;

  if ((raw > -deadband) && (raw < deadband))
  {
    return 0;
  }

  if (raw >= 0)
  {
    mag = raw;
    step = mag / quantum;
    if (step < 1)
    {
      step = 1;
    }
    if (step > max_step)
    {
      step = max_step;
    }
    return step;
  }

  mag = -raw;
  step = mag / quantum;
  if (step < 1)
  {
    step = 1;
  }
  if (step > max_step)
  {
    step = max_step;
  }
  return -step;
}

static void RenderDemo_UpdateBackgroundFromSensor(const app_sensor_snapshot_t *sensor_snapshot)
{
  uint32_t now_ms;

  if ((sensor_snapshot != NULL) &&
      ((sensor_snapshot->valid_mask & APP_SENSOR_SNAPSHOT_VALID_LIS) != 0UL) &&
      (sensor_snapshot->lis_sample_count > 0UL) &&
      (sensor_snapshot->lis_last_sample_tick > 0UL))
  {
    const int32_t dx = RenderDemo_LisRawToScrollDelta((int32_t)sensor_snapshot->lis_x_raw);
    const int32_t dy = RenderDemo_LisRawToScrollDelta((int32_t)sensor_snapshot->lis_y_raw);
    now_ms = HAL_GetTick();
    if ((uint32_t)(now_ms - sensor_snapshot->lis_last_sample_tick) <= 600UL)
    {
      s_demo.scroll_x = RenderDemo_AddSignedU32(s_demo.scroll_x, dx);
      s_demo.scroll_y = RenderDemo_AddSignedU32(s_demo.scroll_y, dy);
      return;
    }
  }

  s_demo.scroll_x++;
  s_demo.scroll_y++;
}

static void RenderDemo_UpdateMapCameraFromSensor(const app_sensor_snapshot_t *sensor_snapshot)
{
  if ((sensor_snapshot != NULL) &&
      ((sensor_snapshot->valid_mask & APP_SENSOR_SNAPSHOT_VALID_LIS) != 0UL) &&
      (sensor_snapshot->lis_sample_count > 0UL) &&
      (sensor_snapshot->lis_last_sample_tick > 0UL))
  {
    uint32_t now_ms = HAL_GetTick();
    if ((uint32_t)(now_ms - sensor_snapshot->lis_last_sample_tick) <= 600UL)
    {
      const int32_t dx = RenderDemo_LisRawToScrollDelta((int32_t)sensor_snapshot->lis_x_raw);
      const int32_t dy = RenderDemo_LisRawToScrollDelta((int32_t)sensor_snapshot->lis_y_raw);
      s_demo.scroll_x = RenderDemo_AddSignedU32(s_demo.scroll_x, dx);
      s_demo.scroll_y = RenderDemo_AddSignedU32(s_demo.scroll_y, dy);
    }
  }
}

static void RenderDemo_UpdateCubeFromSensor(const app_sensor_snapshot_t *sensor_snapshot)
{
  const float joy_deadzone = 0.06f;
  const float joy_rate_gain = 0.09f;

  if ((sensor_snapshot != NULL) &&
      ((sensor_snapshot->valid_mask & APP_SENSOR_SNAPSHOT_VALID_JOY) != 0UL))
  {
    const float nx = RenderDemo_ClampF(sensor_snapshot->joy_nx, -1.0f, 1.0f);
    const float ny = RenderDemo_ClampF(sensor_snapshot->joy_ny, -1.0f, 1.0f);

    if ((RenderDemo_AbsF(nx) >= joy_deadzone) || (RenderDemo_AbsF(ny) >= joy_deadzone))
    {
      s_demo.ay += (nx * joy_rate_gain);
      s_demo.ax += (ny * joy_rate_gain);
    }
  }
}

static char *RenderDemo_U32ToDec(char *dst, uint32_t v)
{
  char tmp[11];
  int32_t n = 0;

  do
  {
    tmp[n++] = (char)('0' + (v % 10U));
    v /= 10U;
  } while (v != 0U);

  while (n-- > 0)
  {
    *dst++ = tmp[n];
  }

  *dst = '\0';
  return dst;
}

static uint8_t RenderDemo_GetBitMsb(const uint8_t *row, uint16_t x)
{
  const uint8_t byte = row[x >> 3U];
  const uint8_t bit = (uint8_t)(0x80U >> (x & 7U));
  return ((byte & bit) != 0U) ? 1U : 0U;
}

static void RenderDemo_SetBitMsb(uint8_t *row, uint16_t x)
{
  row[x >> 3U] |= (uint8_t)(0x80U >> (x & 7U));
}

static void RenderDemo_BuildShiftedTile(void)
{
  const uint16_t oxm = (uint16_t)(s_demo.scroll_x % BG_PATTERN_W_PIXELS);
  const uint16_t oym = (uint16_t)(s_demo.scroll_y % BG_PATTERN_H_PIXELS);
  uint16_t y = 0U;

  (void)memset(s_bg_shifted, 0, sizeof(s_bg_shifted));

  for (y = 0U; y < BG_PATTERN_H_PIXELS; ++y)
  {
    const uint16_t sy = (uint16_t)((y + oym) % BG_PATTERN_H_PIXELS);
    const uint8_t *src_row = &kBgPattern1bpp[(uint32_t)sy * BG_PATTERN_STRIDE];
    uint8_t *dst_row = &s_bg_shifted[(uint32_t)y * BG_PATTERN_STRIDE];
    uint16_t x = 0U;

    for (x = 0U; x < BG_PATTERN_W_PIXELS; ++x)
    {
      const uint16_t sx = (uint16_t)((x + oxm) % BG_PATTERN_W_PIXELS);
      if (RenderDemo_GetBitMsb(src_row, sx) != 0U)
      {
        RenderDemo_SetBitMsb(dst_row, x);
      }
    }
  }
}

static void RenderDemo_DrawBgPattern(void)
{
  const uint16_t clip_h = (uint16_t)(s_demo.game_y1 - s_demo.game_y0 + 1U);
  uint16_t y = s_demo.game_y0;

  RenderDemo_BuildShiftedTile();
  Render_SetClipRect(0U, s_demo.game_y0, s_demo.width, clip_h);

  while (y <= s_demo.game_y1)
  {
    uint16_t x = 0U;

    while (x < s_demo.width)
    {
      renderBlit1bpp(x, y,
                     BG_PATTERN_W_PIXELS, BG_PATTERN_H_PIXELS,
                     s_bg_shifted, BG_PATTERN_STRIDE,
                     true,
                     RENDER_LAYER_BG, RENDER_COLOR_BLACK);
      x = (uint16_t)(x + BG_PATTERN_W_PIXELS);
    }

    if ((uint16_t)(s_demo.game_y1 - y) < BG_PATTERN_H_PIXELS)
    {
      break;
    }
    y = (uint16_t)(y + BG_PATTERN_H_PIXELS);
  }

  Render_ClearClip();
}

static uint8_t RenderDemo_MapPatternBit(uint16_t gid, uint8_t x, uint8_t y)
{
  switch ((uint8_t)(((gid * 13U) ^ (gid >> 2U)) & 0x3U))
  {
    case 0U:
      return (uint8_t)(((x + y) & 1U) == 0U);
    case 1U:
      return (uint8_t)(((x & 1U) == 0U) && ((y & 1U) == 0U));
    case 2U:
      return (uint8_t)(((x + y) & 0x3U) == 0U);
    default:
      return (uint8_t)((((x * 3U) + (y * 5U) + (uint8_t)gid) & 0x3U) == 0U);
  }
}

static uint8_t RenderDemo_ReadMaskBitMsb(const uint8_t *row, uint16_t x)
{
  return (uint8_t)((row[x >> 3U] >> (7U - (x & 7U))) & 0x1U);
}

static uint8_t RenderDemo_ReadLevel2bppMsb(const uint8_t *row, uint16_t x)
{
  uint8_t group = (uint8_t)(x & 3U);
  uint8_t shift = (uint8_t)(6U - (group * 2U));
  return (uint8_t)((row[x >> 2U] >> shift) & 0x3U);
}

static uint8_t RenderDemo_Bayer2x2Threshold(uint16_t x, uint16_t y)
{
  if ((y & 1U) == 0U)
  {
    return ((x & 1U) == 0U) ? 0U : 2U;
  }
  return ((x & 1U) == 0U) ? 3U : 1U;
}

static render_color_t RenderDemo_ColorFrom2bppLevel(uint8_t level, uint16_t x, uint16_t y)
{
  const render_2bpp_present_t present_mode = (render_2bpp_present_t)RENDER_DEMO_TILE_PRESENT_MODE;
  uint8_t black = 0U;

  if (present_mode == RENDER_2BPP_PRESENT_BAYER2X2)
  {
    uint8_t thr = RenderDemo_Bayer2x2Threshold(x, y);
    black = (level > thr) ? 1U : 0U;
  }
  else
  {
    black = (level >= 2U) ? 1U : 0U;
  }

  return (black != 0U) ? RENDER_COLOR_BLACK : RENDER_COLOR_WHITE;
}

static void RenderDemo_DrawSceneMap(const game_map_view_t *map_view,
                                    const game_tileset_view_t *tileset_view)
{
  const game_map_blob_header_t *header;
  const uint16_t game_h = (uint16_t)(s_demo.game_y1 - s_demo.game_y0 + 1U);
  uint8_t use_tileset = 0U;
  uint16_t cell_w;
  uint16_t cell_h;
  uint16_t map_w_tiles;
  uint16_t map_h_tiles;
  uint32_t map_w_px;
  uint32_t map_h_px;
  uint32_t cam_x = 0U;
  uint32_t cam_y = 0U;
  int32_t base_x;
  int32_t base_y;
  uint16_t first_tx = 0U;
  uint16_t first_ty = 0U;
  uint16_t ty;
  uint16_t tx;

  if ((map_view == NULL) || (map_view->header == NULL))
  {
    return;
  }

  header = map_view->header;
  cell_w = (uint16_t)header->tile_width;
  cell_h = (uint16_t)header->tile_height;
  map_w_tiles = header->map_width;
  map_h_tiles = header->map_height;

  if ((cell_w == 0U) || (cell_h == 0U) || (map_w_tiles == 0U) || (map_h_tiles == 0U))
  {
    return;
  }

  if ((tileset_view != NULL) &&
      (tileset_view->header != NULL) &&
      (tileset_view->color_plane != NULL) &&
      (tileset_view->header->tile_width == cell_w) &&
      (tileset_view->header->tile_height == cell_h))
  {
    use_tileset = 1U;
  }

  map_w_px = (uint32_t)map_w_tiles * (uint32_t)cell_w;
  map_h_px = (uint32_t)map_h_tiles * (uint32_t)cell_h;

  if (map_w_px <= (uint32_t)s_demo.width)
  {
    base_x = (int32_t)((uint32_t)(s_demo.width - (uint16_t)map_w_px) / 2U);
  }
  else
  {
    uint32_t max_cam_x = map_w_px - (uint32_t)s_demo.width;
    cam_x = (s_demo.scroll_x > max_cam_x) ? max_cam_x : s_demo.scroll_x;
    first_tx = (uint16_t)(cam_x / (uint32_t)cell_w);
    base_x = -(int32_t)(cam_x % (uint32_t)cell_w);
  }

  if (map_h_px <= (uint32_t)game_h)
  {
    base_y = (int32_t)(((uint32_t)game_h - map_h_px) / 2U);
  }
  else
  {
    uint32_t max_cam_y = map_h_px - (uint32_t)game_h;
    cam_y = (s_demo.scroll_y > max_cam_y) ? max_cam_y : s_demo.scroll_y;
    first_ty = (uint16_t)(cam_y / (uint32_t)cell_h);
    base_y = -(int32_t)(cam_y % (uint32_t)cell_h);
  }

  Render_SetClipRect(0U, s_demo.game_y0, s_demo.width, game_h);
  renderFillRect(0U, s_demo.game_y0, s_demo.width, game_h, RENDER_LAYER_BG, RENDER_COLOR_WHITE);

  for (ty = first_ty; ty < map_h_tiles; ty++)
  {
    int32_t tile_py = base_y + (int32_t)((uint32_t)(ty - first_ty) * (uint32_t)cell_h);
    if (tile_py >= (int32_t)game_h)
    {
      break;
    }
    if ((tile_py + (int32_t)cell_h) <= 0)
    {
      continue;
    }

    for (tx = first_tx; tx < map_w_tiles; tx++)
    {
      int32_t tile_px = base_x + (int32_t)((uint32_t)(tx - first_tx) * (uint32_t)cell_w);
      uint16_t gid;
      uint8_t flags;
      uint16_t py;
      uint16_t px;
      const uint8_t *tile_color = (const uint8_t *)0;
      const uint8_t *tile_mask = (const uint8_t *)0;
      uint8_t have_tile_pixels = 0U;

      if (tile_px >= (int32_t)s_demo.width)
      {
        break;
      }
      if ((tile_px + (int32_t)cell_w) <= 0)
      {
        continue;
      }

      gid = GameMap_GetTileGid(map_view, tx, ty);
      if (gid == 0U)
      {
        continue;
      }
      flags = GameMap_GetTileFlags(map_view, tx, ty);
      if (use_tileset != 0U)
      {
        if (GameTileset_TryGetTileByGid(tileset_view, gid, &tile_color, &tile_mask) != 0U)
        {
          have_tile_pixels = 1U;
        }
      }

      for (py = 0U; py < cell_h; py++)
      {
        int32_t sy = tile_py + (int32_t)py;
        if ((sy < 0) || (sy >= (int32_t)game_h))
        {
          continue;
        }

        for (px = 0U; px < cell_w; px++)
        {
          int32_t sx = tile_px + (int32_t)px;
          uint16_t screen_y;
          if ((sx < 0) || (sx >= (int32_t)s_demo.width))
          {
            continue;
          }

          screen_y = (uint16_t)((int32_t)s_demo.game_y0 + sy);
          if (have_tile_pixels != 0U)
          {
            const uint8_t *color_row = tile_color + ((uint32_t)py * tileset_view->header->color_stride);
            const uint8_t *mask_row = (tile_mask != (const uint8_t *)0)
                                          ? (tile_mask + ((uint32_t)py * tileset_view->header->mask_stride))
                                          : (const uint8_t *)0;
            if ((mask_row != (const uint8_t *)0) && (RenderDemo_ReadMaskBitMsb(mask_row, px) == 0U))
            {
              continue;
            }
            renderSetPixel((uint16_t)sx,
                           screen_y,
                           RENDER_LAYER_GAME,
                           RenderDemo_ColorFrom2bppLevel(RenderDemo_ReadLevel2bppMsb(color_row, px),
                                                         (uint16_t)sx,
                                                         screen_y));
            continue;
          }

          renderSetPixel((uint16_t)sx,
                         screen_y,
                         RENDER_LAYER_GAME,
                         (RenderDemo_MapPatternBit(gid, (uint8_t)px, (uint8_t)py) != 0U)
                             ? RENDER_COLOR_WHITE
                             : RENDER_COLOR_BLACK);
        }
      }

      if ((flags & (uint8_t)GAME_MAP_TILE_FLAG_SOLID) != 0U)
      {
        int32_t left = tile_px;
        int32_t right = tile_px + (int32_t)cell_w - 1;
        int32_t top = tile_py;
        int32_t bottom = tile_py + (int32_t)cell_h - 1;
        int32_t x;
        int32_t y;

        for (x = left; x <= right; x++)
        {
          if ((x < 0) || (x >= (int32_t)s_demo.width))
          {
            continue;
          }
          if ((top >= 0) && (top < (int32_t)game_h))
          {
            renderSetPixel((uint16_t)x,
                           (uint16_t)((int32_t)s_demo.game_y0 + top),
                           RENDER_LAYER_UI,
                           RENDER_COLOR_BLACK);
          }
          if ((bottom >= 0) && (bottom < (int32_t)game_h))
          {
            renderSetPixel((uint16_t)x,
                           (uint16_t)((int32_t)s_demo.game_y0 + bottom),
                           RENDER_LAYER_UI,
                           RENDER_COLOR_BLACK);
          }
        }

        for (y = top; y <= bottom; y++)
        {
          if ((y < 0) || (y >= (int32_t)game_h))
          {
            continue;
          }
          if ((left >= 0) && (left < (int32_t)s_demo.width))
          {
            renderSetPixel((uint16_t)left,
                           (uint16_t)((int32_t)s_demo.game_y0 + y),
                           RENDER_LAYER_UI,
                           RENDER_COLOR_BLACK);
          }
          if ((right >= 0) && (right < (int32_t)s_demo.width))
          {
            renderSetPixel((uint16_t)right,
                           (uint16_t)((int32_t)s_demo.game_y0 + y),
                           RENDER_LAYER_UI,
                           RENDER_COLOR_BLACK);
          }
        }
      }
    }
  }

  Render_ClearClip();
}

static void RenderDemo_RotateYX(const vec3_t *v, float cy, float sy, float cx, float sx, vec3_t *o)
{
  const float xx = (v->x * cy) + (v->z * sy);
  const float zz = (-v->x * sy) + (v->z * cy);

  o->x = xx;
  o->y = (v->y * cx) - (zz * sx);
  o->z = (v->y * sx) + (zz * cx);
}

static void RenderDemo_ProjectPoints(const vec3_t *in, pt2_t *out)
{
  const float z_off = 2.3f;
  const float near_z = 0.25f;
  const float f = 84.0f;
  const float cx = (float)(s_demo.width / 2U);
  const float cy = (float)((s_demo.game_y0 + s_demo.game_y1) / 2U);
  uint8_t i;

  for (i = 0U; i < 8U; ++i)
  {
    float z = in[i].z + z_off;
    float px;
    float py;

    if (z < near_z)
    {
      z = near_z;
    }

    px = cx + ((f * in[i].x) / z);
    py = cy - ((f * in[i].y) / z);

    out[i].x = (int16_t)RenderDemo_Round(px);
    out[i].y = (int16_t)RenderDemo_Round(py);
  }
}

static void RenderDemo_DrawCube(void)
{
  vec3_t rotated[8];
  pt2_t proj[8];
  const float cy = cosf(s_demo.ay);
  const float sy = sinf(s_demo.ay);
  const float cx = cosf(s_demo.ax);
  const float sx = sinf(s_demo.ax);
  const int32_t width = (int32_t)s_demo.width;
  const int32_t min_y = (int32_t)s_demo.game_y0;
  const int32_t max_y = (int32_t)s_demo.game_y1;
  uint8_t i;

  for (i = 0U; i < 8U; ++i)
  {
    RenderDemo_RotateYX(&kCubeVerts[i], cy, sy, cx, sx, &rotated[i]);
  }
  RenderDemo_ProjectPoints(rotated, proj);

  for (i = 0U; i < 12U; ++i)
  {
    int32_t x0 = (int32_t)proj[kCubeEdges[i][0U]].x;
    int32_t y0 = (int32_t)proj[kCubeEdges[i][0U]].y;
    int32_t x1 = (int32_t)proj[kCubeEdges[i][1U]].x;
    int32_t y1 = (int32_t)proj[kCubeEdges[i][1U]].y;

    if (((x0 < 0) && (x1 < 0)) || ((x0 >= width) && (x1 >= width)))
    {
      continue;
    }
    if (((y0 < min_y) && (y1 < min_y)) || ((y0 > max_y) && (y1 > max_y)))
    {
      continue;
    }

    if (x0 < 0)
    {
      x0 = 0;
    }
    else if (x0 >= width)
    {
      x0 = width - 1;
    }
    if (x1 < 0)
    {
      x1 = 0;
    }
    else if (x1 >= width)
    {
      x1 = width - 1;
    }

    if (y0 < min_y)
    {
      y0 = min_y;
    }
    else if (y0 > max_y)
    {
      y0 = max_y;
    }
    if (y1 < min_y)
    {
      y1 = min_y;
    }
    else if (y1 > max_y)
    {
      y1 = max_y;
    }

    renderDrawLine((uint16_t)x0, (uint16_t)y0,
                   (uint16_t)x1, (uint16_t)y1,
                   RENDER_LAYER_GAME, RENDER_COLOR_BLACK, CUBE_EDGE_THICKNESS);
  }
}

static void RenderDemo_DrawTopBar(void)
{
  char fps_buf[24];
  char *p = fps_buf;
  uint16_t fps_x = 2U;

  renderFillRect(0U, 0U, s_demo.width, s_demo.ui_bar_h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  renderDrawText(4U, 3U, "REALTIME DEMO", RENDER_LAYER_UI, RENDER_COLOR_WHITE);

  *p++ = 'F';
  *p++ = 'P';
  *p++ = 'S';
  *p++ = ':';
  *p++ = ' ';
  p = RenderDemo_U32ToDec(p, s_demo.fps);
  (void)p;

  {
    uint16_t fps_w = 0U;
    const char *s = fps_buf;
    while (*s != '\0')
    {
      fps_w = (uint16_t)(fps_w + FONT8X8_WIDTH + 1U);
      s++;
    }
    if (fps_w > 0U)
    {
      fps_w = (uint16_t)(fps_w - 1U);
    }
    if (s_demo.width > (uint16_t)(fps_w + 2U))
    {
      fps_x = (uint16_t)(s_demo.width - fps_w - 2U);
    }
  }

  renderDrawText(fps_x, 3U, fps_buf, RENDER_LAYER_UI, RENDER_COLOR_WHITE);
}

static void RenderDemo_DrawBottomBar(void)
{
  char stat_buf[28];
  char *p = stat_buf;
  uint32_t sim_sec;
  uint32_t wall_sec = 0U;
  uint16_t y0;

  sim_sec = s_demo.uptime_ms / 1000U;
  if (s_demo.last_frame_ms >= s_demo.boot_ms)
  {
    wall_sec = (s_demo.last_frame_ms - s_demo.boot_ms) / 1000U;
  }

  y0 = (uint16_t)(s_demo.height - s_demo.ui_bar_h);
  renderFillRect(0U, y0, s_demo.width, s_demo.ui_bar_h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);

  *p++ = 'S';
  *p++ = 'I';
  *p++ = 'M';
  *p++ = ':';
  *p++ = ' ';
  p = RenderDemo_U32ToDec(p, sim_sec);
  *p++ = 's';
  *p++ = ' ';
  *p++ = 'W';
  *p++ = 'A';
  *p++ = 'L';
  *p++ = 'L';
  *p++ = ':';
  *p++ = ' ';
  p = RenderDemo_U32ToDec(p, wall_sec);
  *p++ = 's';
  *p = '\0';

  renderDrawText(4U, (uint16_t)(y0 + 3U), stat_buf, RENDER_LAYER_UI, RENDER_COLOR_WHITE);
}

void RenderDemo_Reset(void)
{
  (void)memset(&s_demo, 0, sizeof(s_demo));
}

static void RenderDemo_EnsureInitialized(void)
{
  uint32_t now_ms;

  if (s_demo.initialized != 0U)
  {
    return;
  }

  now_ms = HAL_GetTick();
  s_demo.width = RENDER_WIDTH;
  s_demo.height = RENDER_HEIGHT;
  s_demo.ui_bar_h = (s_demo.height > ((UI_BAR_H_PIXELS * 2U) + 1U)) ? UI_BAR_H_PIXELS : 0U;
  s_demo.game_y0 = s_demo.ui_bar_h;
  s_demo.game_y1 = (s_demo.height > s_demo.ui_bar_h) ? (uint16_t)(s_demo.height - s_demo.ui_bar_h - 1U) : 0U;
  if (s_demo.game_y1 < s_demo.game_y0)
  {
    s_demo.game_y0 = 0U;
    s_demo.game_y1 = (s_demo.height > 0U) ? (uint16_t)(s_demo.height - 1U) : 0U;
  }
  s_demo.bg_enabled = 1U;
  s_demo.cube_enabled = 1U;
  s_demo.uptime_ms = 0U;
  s_demo.boot_ms = now_ms;
  s_demo.last_frame_ms = now_ms;
  s_demo.initialized = 1U;
}

void RenderDemo_ToggleBackground(void)
{
  RenderDemo_EnsureInitialized();
  s_demo.bg_enabled = (s_demo.bg_enabled == 0U) ? 1U : 0U;
}

void RenderDemo_ToggleCube(void)
{
  RenderDemo_EnsureInitialized();
  s_demo.cube_enabled = (s_demo.cube_enabled == 0U) ? 1U : 0U;
}

uint8_t RenderDemo_HandleControl(const game_runtime_input_t *input,
                                 uint8_t *request_exit_to_static,
                                 game_runtime_audio_cue_t *audio_cue_out)
{
  render_demo_role_t role;

  if (input == NULL)
  {
    return 0U;
  }

  if (request_exit_to_static != NULL)
  {
    *request_exit_to_static = 0U;
  }
  if (audio_cue_out != NULL)
  {
    *audio_cue_out = GAME_RT_AUDIO_CUE_NONE;
  }

  if (input->event != (ULONG)GAME_RT_INPUT_EVENT_PRESS)
  {
    return 0U;
  }

  role = RenderDemo_RoleForSource(input->source);

  switch (role)
  {
    case RENDER_DEMO_ROLE_PRIMARY:
      RenderDemo_ToggleBackground();
      if (audio_cue_out != NULL)
      {
        *audio_cue_out = GAME_RT_AUDIO_CUE_PRIMARY;
      }
      return 1U;

    case RENDER_DEMO_ROLE_SECONDARY:
      RenderDemo_ToggleCube();
      if (audio_cue_out != NULL)
      {
        *audio_cue_out = GAME_RT_AUDIO_CUE_SECONDARY;
      }
      return 1U;

    case RENDER_DEMO_ROLE_BACK:
      if (request_exit_to_static != NULL)
      {
        *request_exit_to_static = 1U;
      }
      if (audio_cue_out != NULL)
      {
        *audio_cue_out = GAME_RT_AUDIO_CUE_BACK;
      }
      return 1U;

    default:
      return 0U;
  }
}

void RenderDemo_Update(const app_sensor_snapshot_t *sensor_snapshot, uint32_t dt_ms)
{
  const game_map_view_t *map_view;
  RenderDemo_EnsureInitialized();
  s_demo.uptime_ms += dt_ms;

  map_view = GameRuntime_GetSceneMap();

  if (map_view != NULL)
  {
    RenderDemo_UpdateMapCameraFromSensor(sensor_snapshot);
  }
  else if (s_demo.bg_enabled != 0U)
  {
    RenderDemo_UpdateBackgroundFromSensor(sensor_snapshot);
  }

  if (s_demo.cube_enabled != 0U)
  {
    RenderDemo_UpdateCubeFromSensor(sensor_snapshot);
  }
}

void RenderDemo_DrawFrame(const app_sensor_snapshot_t *sensor_snapshot)
{
  const game_map_view_t *map_view;
  const game_tileset_view_t *tileset_view;
  uint32_t now_ms;
  uint32_t dt_ms;
  (void)sensor_snapshot;

  RenderDemo_EnsureInitialized();

  now_ms = HAL_GetTick();
  dt_ms = (uint32_t)(now_ms - s_demo.last_frame_ms);
  s_demo.last_frame_ms = now_ms;

  s_demo.fps_ms_acc += dt_ms;
  s_demo.fps_frames++;
  if (s_demo.fps_ms_acc >= 1000U)
  {
    if (s_demo.fps_ms_acc > 0U)
    {
      s_demo.fps = (uint32_t)((s_demo.fps_frames * 1000U) / s_demo.fps_ms_acc);
    }
    s_demo.fps_ms_acc = 0U;
    s_demo.fps_frames = 0U;
  }

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(TH_MODE_REALTIME);
  map_view = GameRuntime_GetSceneMap();
  tileset_view = GameRuntime_GetSceneTileset();

  if (map_view != NULL)
  {
    RenderDemo_DrawSceneMap(map_view, tileset_view);
  }
  else if (s_demo.bg_enabled != 0U)
  {
    RenderDemo_DrawBgPattern();
  }

  if (s_demo.cube_enabled != 0U)
  {
    RenderDemo_DrawCube();
  }

  if (s_demo.ui_bar_h > 0U)
  {
    RenderDemo_DrawTopBar();
    RenderDemo_DrawBottomBar();
  }
  else
  {
    renderDrawText(2U, 2U, "REALTIME DEMO", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    renderDrawText(2U, (uint16_t)(RENDER_HEIGHT - (FONT8X8_HEIGHT + 2U)),
                   "SIM/WALL STATS", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }

  s_demo.frame_id++;
  Render_MarkDirtyAll();
}
