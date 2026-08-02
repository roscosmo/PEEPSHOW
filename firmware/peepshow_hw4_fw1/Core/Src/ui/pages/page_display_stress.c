#include "ui/pages/page_display_stress.h"

#include "display_renderer.h"
#include "font8x8_basic.h"
#include "knobs_autogen.h"
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
  ULONG frame_id;
  ULONG scroll_x;
  ULONG scroll_y;
  ULONG fps;
  ULONG fps_ticks_acc;
  ULONG fps_frames;
  ULONG boot_tick;
  ULONG last_frame_tick;
} ui_display_stress_state_t;

static ui_display_stress_state_t s_demo;
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

static th_mode_t UiPageDisplayStress_ModeFromFlags(ULONG mode_flags)
{
  if ((mode_flags & (1UL << 3)) != 0UL)
  {
    return TH_MODE_FLASHING;
  }
  if ((mode_flags & (1UL << 2)) != 0UL)
  {
    return TH_MODE_REALTIME;
  }
  if ((mode_flags & (1UL << 1)) != 0UL)
  {
    return TH_MODE_STATIC;
  }
  return TH_MODE_STOP;
}

static int32_t UiPageDisplayStress_Round(float x)
{
  return (int32_t)(x >= 0.0f ? (x + 0.5f) : (x - 0.5f));
}

static char *UiPageDisplayStress_U32ToDec(char *dst, ULONG v)
{
  char tmp[11];
  int32_t n = 0;

  do
  {
    tmp[n++] = (char)('0' + (v % 10UL));
    v /= 10UL;
  } while (v != 0UL);

  while (n-- > 0)
  {
    *dst++ = tmp[n];
  }

  *dst = '\0';
  return dst;
}

static uint8_t UiPageDisplayStress_GetBitMsb(const uint8_t *row, uint16_t x)
{
  const uint8_t byte = row[x >> 3U];
  const uint8_t bit = (uint8_t)(0x80U >> (x & 7U));
  return ((byte & bit) != 0U) ? 1U : 0U;
}

static void UiPageDisplayStress_SetBitMsb(uint8_t *row, uint16_t x)
{
  row[x >> 3U] |= (uint8_t)(0x80U >> (x & 7U));
}

static void UiPageDisplayStress_BuildShiftedTile(void)
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
      if (UiPageDisplayStress_GetBitMsb(src_row, sx) != 0U)
      {
        UiPageDisplayStress_SetBitMsb(dst_row, x);
      }
    }
  }
}

static void UiPageDisplayStress_DrawBgPattern(void)
{
  const uint16_t clip_h = (uint16_t)(s_demo.game_y1 - s_demo.game_y0 + 1U);
  uint16_t y = s_demo.game_y0;

  UiPageDisplayStress_BuildShiftedTile();
  Render_SetClipRect(0U, s_demo.game_y0, s_demo.width, clip_h);

  for (y = s_demo.game_y0; y <= s_demo.game_y1; y = (uint16_t)(y + BG_PATTERN_H_PIXELS))
  {
    uint16_t x = 0U;

    for (x = 0U; x < s_demo.width; x = (uint16_t)(x + BG_PATTERN_W_PIXELS))
    {
      renderBlit1bpp(x, y,
                     BG_PATTERN_W_PIXELS, BG_PATTERN_H_PIXELS,
                     s_bg_shifted, BG_PATTERN_STRIDE,
                     true,
                     RENDER_LAYER_BG, RENDER_COLOR_BLACK);
    }
  }

  Render_ClearClip();
}

static void UiPageDisplayStress_RotateYX(const vec3_t *v, float cy, float sy, float cx, float sx, vec3_t *o)
{
  const float xx = (v->x * cy) + (v->z * sy);
  const float zz = (-v->x * sy) + (v->z * cy);

  o->x = xx;
  o->y = (v->y * cx) - (zz * sx);
  o->z = (v->y * sx) + (zz * cx);
}

static void UiPageDisplayStress_ProjectPoints(const vec3_t *in, pt2_t *out)
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

    out[i].x = (int16_t)UiPageDisplayStress_Round(px);
    out[i].y = (int16_t)UiPageDisplayStress_Round(py);
  }
}

static void UiPageDisplayStress_DrawCube(void)
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
    UiPageDisplayStress_RotateYX(&kCubeVerts[i], cy, sy, cx, sx, &rotated[i]);
  }
  UiPageDisplayStress_ProjectPoints(rotated, proj);

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

static void UiPageDisplayStress_DrawTopBar(void)
{
  char fps_buf[24];
  char *p = fps_buf;

  renderFillRect(0U, 0U, s_demo.width, s_demo.ui_bar_h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  renderDrawText(4U, 3U, "DISPLAY STRESS", RENDER_LAYER_UI, RENDER_COLOR_WHITE);

  *p++ = 'F';
  *p++ = 'P';
  *p++ = 'S';
  *p++ = ':';
  *p++ = ' ';
  p = UiPageDisplayStress_U32ToDec(p, s_demo.fps);
  (void)p;
  renderDrawText((uint16_t)(s_demo.width > 64U ? (s_demo.width - 52U) : 2U),
                 3U, fps_buf, RENDER_LAYER_UI, RENDER_COLOR_WHITE);
}

static void UiPageDisplayStress_DrawBottomBar(void)
{
  char up_buf[24];
  char *p = up_buf;
  ULONG uptime_sec = 0UL;
  uint16_t y0;

  if ((KNOB_RTOS_TICK_HZ > 0U) && (s_demo.last_frame_tick >= s_demo.boot_tick))
  {
    uptime_sec = (s_demo.last_frame_tick - s_demo.boot_tick) / (ULONG)KNOB_RTOS_TICK_HZ;
  }

  y0 = (uint16_t)(s_demo.height - s_demo.ui_bar_h);
  renderFillRect(0U, y0, s_demo.width, s_demo.ui_bar_h, RENDER_LAYER_UI, RENDER_COLOR_BLACK);

  *p++ = 'U';
  *p++ = 'P';
  *p++ = ':';
  *p++ = ' ';
  p = UiPageDisplayStress_U32ToDec(p, uptime_sec);
  *p++ = 's';
  *p = '\0';

  renderDrawText(4U, (uint16_t)(y0 + 3U), up_buf, RENDER_LAYER_UI, RENDER_COLOR_WHITE);
  renderDrawText((uint16_t)(s_demo.width > 82U ? (s_demo.width - 82U) : 2U),
                 (uint16_t)(y0 + 3U),
                 "A:BG L:CUBE B:BACK", RENDER_LAYER_UI, RENDER_COLOR_WHITE);
}

static void UiPageDisplayStress_Init(void)
{
  const ULONG now = tx_time_get();

  (void)memset(&s_demo, 0, sizeof(s_demo));
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
  s_demo.boot_tick = now;
  s_demo.last_frame_tick = now;
  s_demo.initialized = 1U;
}

static void UiPageDisplayStress_Enter(void)
{
  UiPageDisplayStress_Init();
  UiRouter_MarkDirty();
}

static uint8_t UiPageDisplayStress_Action(const ui_action_evt_t *evt)
{
  if (evt == TX_NULL)
  {
    return 0U;
  }

  if ((evt->action == UI_ACTION_CANCEL) && (evt->event == UI_EVENT_PRESS))
  {
    UiRouter_RequestPage(UI_PAGE_MENU_SYSTEM_SUB);
    return 1U;
  }

  if ((evt->action == UI_ACTION_CONFIRM) && (evt->event == UI_EVENT_PRESS))
  {
    s_demo.bg_enabled = (s_demo.bg_enabled == 0U) ? 1U : 0U;
    UiRouter_MarkDirty();
    return 1U;
  }

  if (((evt->action == UI_ACTION_LEFT) || (evt->action == UI_ACTION_RIGHT)) &&
      (evt->event == UI_EVENT_PRESS))
  {
    s_demo.cube_enabled = (s_demo.cube_enabled == 0U) ? 1U : 0U;
    UiRouter_MarkDirty();
    return 1U;
  }

  return 0U;
}

static void UiPageDisplayStress_Tick(void)
{
  UiRouter_MarkDirty();
}

static void UiPageDisplayStress_Render(void)
{
  const ui_router_state_t *state = UiRouter_GetState();
  ULONG now;
  ULONG dt_ticks;

  if (state == TX_NULL)
  {
    return;
  }
  if (s_demo.initialized == 0U)
  {
    UiPageDisplayStress_Init();
  }

  now = tx_time_get();
  dt_ticks = now - s_demo.last_frame_tick;
  s_demo.last_frame_tick = now;

  s_demo.fps_ticks_acc += dt_ticks;
  s_demo.fps_frames++;
  if ((s_demo.fps_ticks_acc >= (ULONG)KNOB_RTOS_TICK_HZ) && (s_demo.fps_ticks_acc > 0UL))
  {
    s_demo.fps = (s_demo.fps_frames * (ULONG)KNOB_RTOS_TICK_HZ) / s_demo.fps_ticks_acc;
    s_demo.fps_ticks_acc = 0UL;
    s_demo.fps_frames = 0UL;
  }

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(UiPageDisplayStress_ModeFromFlags(state->mode_flags));

  if (s_demo.bg_enabled != 0U)
  {
    UiPageDisplayStress_DrawBgPattern();
    s_demo.scroll_x++;
    s_demo.scroll_y++;
  }

  if (s_demo.cube_enabled != 0U)
  {
    s_demo.ay += 0.045f;
    s_demo.ax += 0.027f;
    UiPageDisplayStress_DrawCube();
  }

  if (s_demo.ui_bar_h > 0U)
  {
    UiPageDisplayStress_DrawTopBar();
    UiPageDisplayStress_DrawBottomBar();
  }
  else
  {
    renderDrawText(2U, 2U, "DISPLAY STRESS", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    renderDrawText(2U, (uint16_t)(RENDER_HEIGHT - (FONT8X8_HEIGHT + 2U)),
                   "A:BG L/R:CUBE B:BACK", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }

  s_demo.frame_id++;
  Render_MarkDirtyAll();
}

static const ui_page_vtable_t s_page_display_stress =
{
  .name = "display_stress",
  .on_enter = UiPageDisplayStress_Enter,
  .on_action = UiPageDisplayStress_Action,
  .on_tick = UiPageDisplayStress_Tick,
  .on_render = UiPageDisplayStress_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageDisplayStress_GetVTable(void)
{
  return &s_page_display_stress;
}
