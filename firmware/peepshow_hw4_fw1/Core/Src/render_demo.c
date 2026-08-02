#include "render_demo.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
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
  uint32_t boot_ms;
  uint32_t last_frame_ms;
} render_demo_state_t;

static render_demo_state_t s_demo;
static uint8_t s_bg_shifted[(uint32_t)BG_PATTERN_STRIDE * BG_PATTERN_H_PIXELS];

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
    return base + (uint32_t)delta;
  }
  return base - (uint32_t)(-delta);
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
  char up_buf[24];
  char *p = up_buf;
  uint32_t uptime_sec = 0U;
  uint16_t y0;

  if (s_demo.last_frame_ms >= s_demo.boot_ms)
  {
    uptime_sec = (s_demo.last_frame_ms - s_demo.boot_ms) / 1000U;
  }

  y0 = (uint16_t)(s_demo.height - s_demo.ui_bar_h);
  renderFillRect(0U, y0, s_demo.width, s_demo.ui_bar_h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);

  *p++ = 'U';
  *p++ = 'P';
  *p++ = ':';
  *p++ = ' ';
  p = RenderDemo_U32ToDec(p, uptime_sec);
  *p++ = 's';
  *p = '\0';

  renderDrawText(4U, (uint16_t)(y0 + 3U), up_buf, RENDER_LAYER_UI, RENDER_COLOR_WHITE);
  renderDrawText((uint16_t)(s_demo.width > 82U ? (s_demo.width - 82U) : 2U),
                 (uint16_t)(y0 + 3U),
                 "A:BG L/R:CUBE B:BACK", RENDER_LAYER_UI, RENDER_COLOR_WHITE);
}

void RenderDemo_Reset(void)
{
  (void)memset(&s_demo, 0, sizeof(s_demo));
}

void RenderDemo_ToggleBackground(void)
{
  if (s_demo.initialized == 0U)
  {
    return;
  }
  s_demo.bg_enabled = (s_demo.bg_enabled == 0U) ? 1U : 0U;
}

void RenderDemo_ToggleCube(void)
{
  if (s_demo.initialized == 0U)
  {
    return;
  }
  s_demo.cube_enabled = (s_demo.cube_enabled == 0U) ? 1U : 0U;
}

void RenderDemo_DrawFrame(const app_sensor_snapshot_t *sensor_snapshot)
{
  uint32_t now_ms;
  uint32_t dt_ms;

  if (s_demo.initialized == 0U)
  {
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
    s_demo.boot_ms = now_ms;
    s_demo.last_frame_ms = now_ms;
    s_demo.initialized = 1U;
  }

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

  if (s_demo.bg_enabled != 0U)
  {
    RenderDemo_DrawBgPattern();
    RenderDemo_UpdateBackgroundFromSensor(sensor_snapshot);
  }

  if (s_demo.cube_enabled != 0U)
  {
    RenderDemo_UpdateCubeFromSensor(sensor_snapshot);
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
                   "A:BG L/R:CUBE B:BACK", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }

  s_demo.frame_id++;
  Render_MarkDirtyAll();
}
