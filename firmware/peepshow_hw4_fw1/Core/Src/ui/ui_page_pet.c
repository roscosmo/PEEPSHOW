#include "ui/ui_page_native.h"
#include "app_threadx.h"
#include "game_package.h"
#include "ui/ui_menu_tree.h"
#include "ui/ui_router.h"
#include "ui/ui_runtime_context.h"
#include "ui/ui_page_registry.h"
#include "ui/ui_pet_assets_autogen.h"
#include "display_renderer.h"
#include "th_mode.h"
#include "knobs_autogen.h"

static ULONG s_ui_pet_selected_row = 0UL;
static ULONG s_ui_pet_selected_col = 0UL;
static ULONG s_ui_pet_last_tick_seen = 0UL;
static ULONG s_ui_pet_last_state_seen = 0UL;
static ULONG s_ui_pet_last_select_seen = 0UL;
static ULONG s_ui_pet_anim_last_wake_count_seen = 0UL;
static ULONG s_ui_pet_anim_last_select_wake_count_seen = 0UL;
static ULONG s_ui_pet_anim_last_active_tx_tick_seen = 0UL;
static ULONG s_ui_pet_anim_clip_id = 0xFFFFFFFFUL;
static ULONG s_ui_pet_anim_frame_idx = 0UL;
static ULONG s_ui_pet_anim_ms_left = 1UL;
static ULONG s_ui_pet_battery_icon_last_sample_tick_seen = 0UL;
static ULONG s_ui_pet_battery_icon_last_apply_tick = 0UL;
static uint8_t s_ui_pet_battery_icon_level4_cached = 0U;
static uint8_t s_ui_pet_battery_icon_bool_cached = 0U;
static uint8_t s_ui_pet_battery_icon_cache_valid = 0U;
static uint8_t s_ui_pet_sand_active = 0U;
static uint8_t s_ui_pet_sand_prev_mode_stop = 0U;
static uint16_t s_ui_pet_sand_cell_px = 2U;
static uint16_t s_ui_pet_sand_grid_w = 0U;
static uint16_t s_ui_pet_sand_grid_h = 0U;
static uint16_t s_ui_pet_sand_origin_x = 0U;
static uint16_t s_ui_pet_sand_origin_y = 0U;
static ULONG s_ui_pet_sand_last_tick_seen = 0UL;
static ULONG s_ui_pet_sand_last_mode_keepalive_tick = 0UL;
static ULONG s_ui_pet_sand_last_stream_rearm_tick = 0UL;
static ULONG s_ui_pet_sand_last_perf_hint_tick = 0UL;
static LONG s_ui_pet_sand_last_z_raw = 0L;

#define UI_MODE_FLAG_STOP (1UL << 0)

enum
{
  UI_PET_ROW_COUNT = 2
};

#define UI_PET_ICON_SCALE      (1U)
#define UI_PET_ICON_DRAW_SIZE  (16U)
#define UI_PET_TOP_ROW_Y       (0U)
#define UI_PET_BOTTOM_ROW_Y    (128U)
#define UI_PET_COL_STEP        (30U)
#define UI_PET_STATE_SCALE     (3U)
#define UI_PET_STATE_PRESENT_MODE (RENDER_2BPP_PRESENT_BAYER2X2)
#define UI_PET_STATE_BOX_X     (36U)
#define UI_PET_STATE_BOX_Y     (16U)
#define UI_PET_STATE_BOX_W     (96U)
#define UI_PET_STATE_BOX_H     (112U)
#define UI_PET_ACTION_HOLD_MARGIN_MS (80UL)
#define UI_PET_BATTERY_ICON_REFRESH_PMIC_SAMPLES (8UL)

static uint8_t s_ui_pet_sand_cells[RENDER_WIDTH * RENDER_HEIGHT];

typedef struct
{
  uint16_t y;
} ui_pet_row_layout_t;

typedef struct
{
  uint16_t x;
  uint16_t y;
  const sprite1_t *spr;
} ui_pet_icon_slot_t;

static const ui_pet_row_layout_t k_ui_pet_rows[] =
{
  {UI_PET_TOP_ROW_Y},
  {UI_PET_BOTTOM_ROW_Y}
};

static const ui_menu_item_t s_menu_pet_feed_items[] =
{
  {
    .label = "KIBBLE",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_ACTION,
    .arg = 0,
    .target.action = {.action_id = UI_MENU_ACTION_PET_FEED_SELECT, .arg0 = 0U}
  },
  {
    .label = "SNACK",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_ACTION,
    .arg = 0,
    .target.action = {.action_id = UI_MENU_ACTION_PET_FEED_SELECT, .arg0 = 1U}
  },
  {
    .label = "MEAL",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_ACTION,
    .arg = 0,
    .target.action = {.action_id = UI_MENU_ACTION_PET_FEED_SELECT, .arg0 = 2U}
  }
};

const ui_menu_t UI_MENU_PET_FEED =
{
  .title = "FEED",
  .items = s_menu_pet_feed_items,
  .count = (uint16_t)(sizeof(s_menu_pet_feed_items) / sizeof(s_menu_pet_feed_items[0])),
  .footer = UI_FOOTER_A_SELECT_B_BACK
};

static void UiPagePet_AnimSyncForState(const ui_router_state_t *state);
static const sprite2_t *UiPagePet_AnimCurrentFrame(void);

static uint16_t UiPagePet_RowStartX(ULONG row_count)
{
  uint16_t row_w;

  if (row_count == 0UL)
  {
    return 0U;
  }

  row_w = (uint16_t)(UI_PET_ICON_DRAW_SIZE + (uint16_t)((row_count - 1UL) * UI_PET_COL_STEP));
  if (row_w >= RENDER_WIDTH)
  {
    return 0U;
  }

  return (uint16_t)((RENDER_WIDTH - row_w) / 2U);
}

static uint16_t UiPagePet_RowSlotX(ULONG row_count, ULONG col)
{
  return (uint16_t)(UiPagePet_RowStartX(row_count) + (uint16_t)(col * UI_PET_COL_STEP));
}

static uint16_t UiPagePet_StatePixelScale(void)
{
  uint16_t pixel_scale = UI_PET_STATE_SCALE;

  if (UI_PET_STATE_PRESENT_MODE == RENDER_2BPP_PRESENT_BAYER2X2)
  {
    pixel_scale = (uint16_t)(pixel_scale * 2U);
  }

  return pixel_scale;
}

static uint16_t UiPagePet_SandCellPx(void)
{
  uint16_t cell_px = (uint16_t)KNOB_UI_PET_SAND_CELL_PX;
  if (cell_px == 0U)
  {
    cell_px = 1U;
  }
  if (cell_px > 8U)
  {
    cell_px = 8U;
  }
  return cell_px;
}

static uint32_t UiPagePet_SandIndex(uint16_t gx, uint16_t gy)
{
  return (uint32_t)gy * (uint32_t)s_ui_pet_sand_grid_w + (uint32_t)gx;
}

static void UiPagePet_SandConfigureRegion(uint16_t origin_x,
                                          uint16_t origin_y,
                                          uint16_t region_w_px,
                                          uint16_t region_h_px)
{
  uint32_t capacity;
  uint32_t max_cells_h;

  if (origin_x >= RENDER_WIDTH)
  {
    origin_x = (uint16_t)(RENDER_WIDTH - 1U);
  }
  if (origin_y >= RENDER_HEIGHT)
  {
    origin_y = (uint16_t)(RENDER_HEIGHT - 1U);
  }

  if (region_w_px == 0U)
  {
    region_w_px = 1U;
  }
  if (region_h_px == 0U)
  {
    region_h_px = 1U;
  }

  if ((uint16_t)(origin_x + region_w_px) > RENDER_WIDTH)
  {
    region_w_px = (uint16_t)(RENDER_WIDTH - origin_x);
  }
  if ((uint16_t)(origin_y + region_h_px) > RENDER_HEIGHT)
  {
    region_h_px = (uint16_t)(RENDER_HEIGHT - origin_y);
  }

  s_ui_pet_sand_origin_x = origin_x;
  s_ui_pet_sand_origin_y = origin_y;
  s_ui_pet_sand_grid_w = (uint16_t)(region_w_px / s_ui_pet_sand_cell_px);
  s_ui_pet_sand_grid_h = (uint16_t)(region_h_px / s_ui_pet_sand_cell_px);
  if (s_ui_pet_sand_grid_w == 0U)
  {
    s_ui_pet_sand_grid_w = 1U;
  }
  if (s_ui_pet_sand_grid_h == 0U)
  {
    s_ui_pet_sand_grid_h = 1U;
  }

  capacity = (uint32_t)sizeof(s_ui_pet_sand_cells);
  if ((uint32_t)s_ui_pet_sand_grid_w > capacity)
  {
    s_ui_pet_sand_grid_w = (uint16_t)capacity;
    s_ui_pet_sand_grid_h = 1U;
    return;
  }

  max_cells_h = capacity / (uint32_t)s_ui_pet_sand_grid_w;
  if ((uint32_t)s_ui_pet_sand_grid_h > max_cells_h)
  {
    s_ui_pet_sand_grid_h = (uint16_t)max_cells_h;
  }
}

static void UiPagePet_SandReset(void)
{
  (void)memset(s_ui_pet_sand_cells, 0, sizeof(s_ui_pet_sand_cells));
  s_ui_pet_sand_active = 0U;
  s_ui_pet_sand_cell_px = UiPagePet_SandCellPx();
  UiPagePet_SandConfigureRegion(0U, UI_PET_STATE_BOX_Y, RENDER_WIDTH, UI_PET_STATE_BOX_H);
  s_ui_pet_sand_last_tick_seen = tx_time_get();
  s_ui_pet_sand_last_mode_keepalive_tick = 0UL;
  s_ui_pet_sand_last_stream_rearm_tick = 0UL;
  s_ui_pet_sand_last_perf_hint_tick = 0UL;
  s_ui_pet_sand_last_z_raw = 0L;
}

uint8_t UiPagePet_IsSandActive(void)
{
  return (s_ui_pet_sand_active != 0U) ? 1U : 0U;
}

static void UiPagePet_SandEnterLiveImu(const ui_router_state_t *state)
{
  if ((state != TX_NULL) && ((state->mode_flags & UI_MODE_FLAG_STOP) != 0UL))
  {
    s_ui_pet_sand_prev_mode_stop = 1U;
  }
  else
  {
    s_ui_pet_sand_prev_mode_stop = 0U;
  }

  (void)App_SysEvent_ModeSet(APP_MODE_STATIC);
  (void)App_SensorReq_LisSetLive();
  (void)App_SensorReq_LisStreamStart();
}

static void UiPagePet_SandExitLiveImu(uint8_t restore_mode)
{
  (void)App_SensorReq_LisStreamStop();
  (void)App_SensorReq_LisSetLowPower();

  if (restore_mode != 0U)
  {
    if (s_ui_pet_sand_prev_mode_stop != 0U)
    {
      (void)App_SysEvent_ModeSet(APP_MODE_STOP);
    }
    else
    {
      (void)App_SysEvent_ModeSet(APP_MODE_STATIC);
    }
  }
}

static uint8_t UiPagePet_ResolveSpriteDrawRect(const sprite2_t *pet_sprite,
                                               uint16_t *draw_x_out,
                                               uint16_t *draw_y_out,
                                               uint16_t *draw_w_out,
                                               uint16_t *draw_h_out,
                                               uint16_t *draw_scale_px_out)
{
  uint16_t draw_scale_px;
  uint16_t draw_w;
  uint16_t draw_h;
  uint16_t draw_x;
  uint16_t draw_y;

  if ((pet_sprite == TX_NULL) ||
      (draw_x_out == TX_NULL) ||
      (draw_y_out == TX_NULL) ||
      (draw_w_out == TX_NULL) ||
      (draw_h_out == TX_NULL) ||
      (draw_scale_px_out == TX_NULL))
  {
    return 0U;
  }

  draw_scale_px = UiPagePet_StatePixelScale();
  draw_w = (uint16_t)(pet_sprite->w * draw_scale_px);
  draw_h = (uint16_t)(pet_sprite->h * draw_scale_px);
  draw_x = (uint16_t)(UI_PET_STATE_BOX_X + (uint16_t)((UI_PET_STATE_BOX_W - draw_w) / 2U));
  draw_y = (uint16_t)(UI_PET_STATE_BOX_Y + (uint16_t)((UI_PET_STATE_BOX_H - draw_h) / 2U));

  *draw_x_out = draw_x;
  *draw_y_out = draw_y;
  *draw_w_out = draw_w;
  *draw_h_out = draw_h;
  *draw_scale_px_out = draw_scale_px;
  return 1U;
}

static uint8_t UiPagePet_Sprite2OpaqueShadeAt(const sprite2_t *spr,
                                              uint16_t sx,
                                              uint16_t sy,
                                              uint8_t *shade_out)
{
  const uint8_t *row_color;
  uint8_t byte_color;
  uint8_t shift_color;
  uint8_t opaque = 1U;

  if ((spr == TX_NULL) || (shade_out == TX_NULL))
  {
    return 0U;
  }
  if ((sx >= spr->w) || (sy >= spr->h))
  {
    return 0U;
  }

  row_color = spr->color2bpp + ((uint32_t)sy * (uint32_t)spr->color_stride);
  byte_color = row_color[sx >> 2];
  if (spr->leftmost_is_msb)
  {
    shift_color = (uint8_t)((3U - (sx & 0x3U)) * 2U);
  }
  else
  {
    shift_color = (uint8_t)((sx & 0x3U) * 2U);
  }
  *shade_out = (uint8_t)((byte_color >> shift_color) & 0x3U);

  if ((spr->mask != TX_NULL) && (spr->mask_stride > 0U))
  {
    const uint8_t *row_mask = spr->mask + ((uint32_t)sy * (uint32_t)spr->mask_stride);
    uint8_t byte_mask = row_mask[sx >> 3];
    uint8_t bit_mask = spr->leftmost_is_msb ? (uint8_t)(7U - (sx & 0x7U)) : (uint8_t)(sx & 0x7U);
    opaque = (uint8_t)((byte_mask >> bit_mask) & 0x1U);
  }

  return opaque;
}

static uint8_t UiPagePet_SpriteShadeToBlack(uint8_t shade,
                                            uint16_t local_px,
                                            uint16_t local_py)
{
  if (shade == 0U)
  {
    return 0U;
  }

  if (UI_PET_STATE_PRESENT_MODE != RENDER_2BPP_PRESENT_BAYER2X2)
  {
    return (shade >= 2U) ? 1U : 0U;
  }

  if (shade >= 3U)
  {
    return 1U;
  }

  {
    static const uint8_t bayer2x2[2][2] =
    {
      {0U, 2U},
      {3U, 1U}
    };
    uint8_t threshold = bayer2x2[local_py & 1U][local_px & 1U];
    return (threshold < shade) ? 1U : 0U;
  }
}

static void UiPagePet_SandPopulateFromSprite(const sprite2_t *pet_sprite,
                                             uint16_t draw_x,
                                             uint16_t draw_y,
                                             uint16_t draw_h,
                                             uint16_t draw_scale_px)
{
  uint16_t sx;
  uint16_t sy;
  uint16_t cell_px;
  uint16_t region_top;
  uint16_t floor_y;
  uint16_t region_h;
  uint16_t region_max_x;
  uint16_t region_max_y;

  UiPagePet_SandReset();
  cell_px = UiPagePet_SandCellPx();
  s_ui_pet_sand_cell_px = cell_px;
  region_top = UI_PET_STATE_BOX_Y;
  floor_y = (uint16_t)(draw_y + draw_h);
  if (floor_y <= region_top)
  {
    floor_y = (uint16_t)(region_top + cell_px);
  }
  if (floor_y > RENDER_HEIGHT)
  {
    floor_y = RENDER_HEIGHT;
  }
  region_h = (uint16_t)(floor_y - region_top);
  UiPagePet_SandConfigureRegion(0U, region_top, RENDER_WIDTH, region_h);
  region_max_x = (uint16_t)(s_ui_pet_sand_origin_x + (uint16_t)(s_ui_pet_sand_grid_w * s_ui_pet_sand_cell_px));
  region_max_y = (uint16_t)(s_ui_pet_sand_origin_y + (uint16_t)(s_ui_pet_sand_grid_h * s_ui_pet_sand_cell_px));

  if ((pet_sprite == TX_NULL) || (draw_scale_px == 0U))
  {
    return;
  }

  for (sy = 0U; sy < pet_sprite->h; sy++)
  {
    for (sx = 0U; sx < pet_sprite->w; sx++)
    {
      uint8_t shade = 0U;
      uint16_t px0;
      uint16_t py0;
      uint16_t px1;
      uint16_t py1;
      uint16_t gx0;
      uint16_t gy0;
      uint16_t gx1;
      uint16_t gy1;
      uint16_t gx;
      uint16_t gy;

      if (UiPagePet_Sprite2OpaqueShadeAt(pet_sprite, sx, sy, &shade) == 0U)
      {
        continue;
      }

      px0 = (uint16_t)(draw_x + (uint16_t)(sx * draw_scale_px));
      py0 = (uint16_t)(draw_y + (uint16_t)(sy * draw_scale_px));
      if ((px0 >= region_max_x) || (py0 >= region_max_y))
      {
        continue;
      }

      if (px0 > s_ui_pet_sand_origin_x)
      {
        gx0 = (uint16_t)((px0 - s_ui_pet_sand_origin_x) / s_ui_pet_sand_cell_px);
      }
      else
      {
        gx0 = 0U;
      }
      if (py0 > s_ui_pet_sand_origin_y)
      {
        gy0 = (uint16_t)((py0 - s_ui_pet_sand_origin_y) / s_ui_pet_sand_cell_px);
      }
      else
      {
        gy0 = 0U;
      }
      px1 = (uint16_t)(px0 + draw_scale_px - 1U);
      py1 = (uint16_t)(py0 + draw_scale_px - 1U);
      if (px1 >= region_max_x)
      {
        px1 = (uint16_t)(region_max_x - 1U);
      }
      if (py1 >= region_max_y)
      {
        py1 = (uint16_t)(region_max_y - 1U);
      }
      gx1 = (uint16_t)((px1 - s_ui_pet_sand_origin_x) / s_ui_pet_sand_cell_px);
      gy1 = (uint16_t)((py1 - s_ui_pet_sand_origin_y) / s_ui_pet_sand_cell_px);

      if (gx1 >= s_ui_pet_sand_grid_w)
      {
        gx1 = (uint16_t)(s_ui_pet_sand_grid_w - 1U);
      }
      if (gy1 >= s_ui_pet_sand_grid_h)
      {
        gy1 = (uint16_t)(s_ui_pet_sand_grid_h - 1U);
      }

      for (gy = gy0; gy <= gy1; gy++)
      {
        for (gx = gx0; gx <= gx1; gx++)
        {
          uint32_t idx;
          uint16_t sample_x;
          uint16_t sample_y;
          uint16_t local_x;
          uint16_t local_y;

          idx = UiPagePet_SandIndex(gx, gy);
          if (idx >= (uint32_t)(sizeof(s_ui_pet_sand_cells)))
          {
            continue;
          }

          sample_x = (uint16_t)(s_ui_pet_sand_origin_x + (uint16_t)(gx * s_ui_pet_sand_cell_px) +
                                (uint16_t)(s_ui_pet_sand_cell_px / 2U));
          sample_y = (uint16_t)(s_ui_pet_sand_origin_y + (uint16_t)(gy * s_ui_pet_sand_cell_px) +
                                (uint16_t)(s_ui_pet_sand_cell_px / 2U));
          if ((sample_x < px0) || (sample_x > px1) || (sample_y < py0) || (sample_y > py1))
          {
            continue;
          }

          local_x = (uint16_t)(sample_x - px0);
          local_y = (uint16_t)(sample_y - py0);
          if (UiPagePet_SpriteShadeToBlack(shade, local_x, local_y) != 0U)
          {
            s_ui_pet_sand_cells[idx] = 1U;
          }
        }
      }
    }
  }

  s_ui_pet_sand_last_tick_seen = tx_time_get();
  s_ui_pet_sand_active = 1U;
}

static void UiPagePet_SandStart(const ui_router_state_t *state)
{
  const sprite2_t *pet_sprite;
  uint16_t draw_x;
  uint16_t draw_y;
  uint16_t draw_w;
  uint16_t draw_h;
  uint16_t draw_scale_px;

  UiPagePet_AnimSyncForState(state);
  pet_sprite = UiPagePet_AnimCurrentFrame();
  if ((pet_sprite == TX_NULL) ||
      (UiPagePet_ResolveSpriteDrawRect(pet_sprite,
                                       &draw_x,
                                       &draw_y,
                                       &draw_w,
                                       &draw_h,
                                       &draw_scale_px) == 0U))
  {
    UiPagePet_SandReset();
    return;
  }

  UiPagePet_SandPopulateFromSprite(pet_sprite, draw_x, draw_y, draw_h, draw_scale_px);
  if (s_ui_pet_sand_active != 0U)
  {
    UiPagePet_SandEnterLiveImu(state);
  }
}

static uint8_t UiPagePet_SandStep(const ui_router_state_t *state)
{
  LONG tilt_x;
  LONG tilt_x_scaled;
  LONG tilt_y;
  LONG tilt_z;
  LONG threshold;
  LONG threshold_shake;
  LONG z_delta;
  LONG lr_gain_pct;
  int8_t wind = 0;
  int8_t gravity_y = 1;
  ULONG steps;
  ULONG step_i;
  ULONG shake_boost = 0UL;
  ULONG moved_any = 0UL;
  ULONG now_tick;

  if ((s_ui_pet_sand_active == 0U) || (state == TX_NULL))
  {
    return 0U;
  }

  {
    ULONG now_rearm_tick = tx_time_get();

    if ((state->mode_flags & UI_MODE_FLAG_STOP) != 0UL)
    {
      if ((s_ui_pet_sand_last_mode_keepalive_tick == 0UL) ||
          ((LONG)(now_rearm_tick - s_ui_pet_sand_last_mode_keepalive_tick) >= 20L))
      {
        (void)App_SysEvent_ModeSet(APP_MODE_STATIC);
        s_ui_pet_sand_last_mode_keepalive_tick = now_rearm_tick;
      }
    }

    if (((state->mode_flags & UI_MODE_FLAG_STOP) == 0UL) &&
        (state->lis_live.stream_enabled == 0UL))
    {
      if ((s_ui_pet_sand_last_stream_rearm_tick == 0UL) ||
          ((LONG)(now_rearm_tick - s_ui_pet_sand_last_stream_rearm_tick) >= 20L))
      {
        (void)App_SensorReq_LisSetLive();
        (void)App_SensorReq_LisStreamStart();
        s_ui_pet_sand_last_stream_rearm_tick = now_rearm_tick;
      }
    }
  }

  now_tick = tx_time_get();
  if (now_tick == s_ui_pet_sand_last_tick_seen)
  {
    return 0U;
  }
  s_ui_pet_sand_last_tick_seen = now_tick;
  if ((s_ui_pet_sand_last_perf_hint_tick == 0UL) ||
      ((LONG)(now_tick - s_ui_pet_sand_last_perf_hint_tick) >= 4L))
  {
    ULONG budget_ticks = (ULONG)KNOB_POWER_PERF_FRAME_BUDGET_TICKS;
    if (budget_ticks == 0UL)
    {
      budget_ticks = 1UL;
    }
    (void)App_SysEvent_PerfHint(budget_ticks + 1UL, budget_ticks + 1UL, (ULONG)RENDER_HEIGHT, 1UL);
    s_ui_pet_sand_last_perf_hint_tick = now_tick;
  }

  threshold = (LONG)KNOB_UI_PET_SAND_TILT_THRESHOLD_RAW;
  if (threshold < 0L)
  {
    threshold = 0L;
  }
  threshold_shake = threshold;
  if (threshold_shake < 1L)
  {
    threshold_shake = 1L;
  }
  tilt_x = state->lis_live.x_raw;
  lr_gain_pct = (LONG)KNOB_UI_PET_SAND_LR_GAIN_PCT;
  if (lr_gain_pct < 10L)
  {
    lr_gain_pct = 10L;
  }
  if (lr_gain_pct > 200L)
  {
    lr_gain_pct = 200L;
  }
  tilt_x_scaled = (tilt_x * lr_gain_pct) / 100L;
  tilt_y = state->lis_live.y_raw;
  tilt_z = state->lis_live.z_raw;
  /* Hardware X orientation is opposite display X in this layout. */
  if (tilt_x_scaled > threshold)
  {
    wind = -1;
  }
  else if (tilt_x_scaled < -threshold)
  {
    wind = 1;
  }
  if (tilt_y > threshold)
  {
    gravity_y = 1;
  }
  else if (tilt_y < -threshold)
  {
    gravity_y = -1;
  }
  else
  {
    gravity_y = 0;
  }
  if ((s_ui_pet_sand_grid_w == 0U) || (s_ui_pet_sand_grid_h == 0U))
  {
    return 0U;
  }
  if ((s_ui_pet_sand_grid_h < 2U) && (gravity_y != 0))
  {
    gravity_y = 0;
  }

  if (s_ui_pet_sand_last_z_raw == 0L)
  {
    s_ui_pet_sand_last_z_raw = tilt_z;
  }
  z_delta = tilt_z - s_ui_pet_sand_last_z_raw;
  if (z_delta < 0L)
  {
    z_delta = -z_delta;
  }
  s_ui_pet_sand_last_z_raw = tilt_z;

  if (z_delta > (threshold_shake * 6L))
  {
    shake_boost = 2UL;
  }
  else if (z_delta > (threshold_shake * 3L))
  {
    shake_boost = 1UL;
  }

  steps = (ULONG)KNOB_UI_PET_SAND_STEPS_PER_TICK;
  if (steps == 0UL)
  {
    steps = 1UL;
  }
  {
    LONG abs_tilt_x = tilt_x_scaled;
    LONG abs_tilt_y = tilt_y;
    LONG tilt_mag;
    if (abs_tilt_x < 0L)
    {
      abs_tilt_x = -abs_tilt_x;
    }
    if (abs_tilt_y < 0L)
    {
      abs_tilt_y = -abs_tilt_y;
    }
    tilt_mag = (abs_tilt_x > abs_tilt_y) ? abs_tilt_x : abs_tilt_y;
    if (tilt_mag > (threshold * 3L))
    {
      steps += 1UL;
    }
  }
  steps += shake_boost;
  if (steps > 4UL)
  {
    steps = 4UL;
  }

  for (step_i = 0UL; step_i < steps; step_i++)
  {
    int32_t y_begin;
    int32_t y_end;
    int32_t y_step;
    int32_t y;
    ULONG moved_step = 0UL;

    if (gravity_y > 0)
    {
      y_begin = (int32_t)s_ui_pet_sand_grid_h - 2;
      y_end = 0;
      y_step = -1;
    }
    else if (gravity_y < 0)
    {
      y_begin = 1;
      y_end = (int32_t)s_ui_pet_sand_grid_h - 1;
      y_step = 1;
    }
    else
    {
      y_begin = 0;
      y_end = (int32_t)s_ui_pet_sand_grid_h - 1;
      y_step = 1;
    }

    for (y = y_begin;; y += y_step)
    {
      int32_t x_begin;
      int32_t x_end;
      int32_t x_step;
      int32_t x;

      if (wind > 0)
      {
        x_begin = (int32_t)s_ui_pet_sand_grid_w - 1;
        x_end = 0;
        x_step = -1;
      }
      else if (wind < 0)
      {
        x_begin = 0;
        x_end = (int32_t)s_ui_pet_sand_grid_w - 1;
        x_step = 1;
      }
      else if ((((ULONG)y + now_tick) & 0x1UL) != 0UL)
      {
        x_begin = (int32_t)s_ui_pet_sand_grid_w - 1;
        x_end = 0;
        x_step = -1;
      }
      else
      {
        x_begin = 0;
        x_end = (int32_t)s_ui_pet_sand_grid_w - 1;
        x_step = 1;
      }

      for (x = x_begin;; x += x_step)
      {
        uint16_t gx = (uint16_t)x;
        uint16_t gy = (uint16_t)y;
        uint32_t idx = UiPagePet_SandIndex(gx, gy);
        uint32_t hash;
        int8_t perp1_x;
        int8_t perp1_y;
        int8_t perp2_x;
        int8_t perp2_y;
        int8_t cand_dx[3];
        int8_t cand_dy[3];
        int32_t cand_sum_x;
        int32_t cand_sum_y;
        uint8_t ci;

        if ((idx >= (uint32_t)sizeof(s_ui_pet_sand_cells)) ||
            (s_ui_pet_sand_cells[idx] == 0U))
        {
          if (x == x_end)
          {
            break;
          }
          continue;
        }

        perp1_x = gravity_y;
        perp1_y = (int8_t)(-wind);
        perp2_x = (int8_t)(-gravity_y);
        perp2_y = wind;

        cand_dx[0] = wind;
        cand_dy[0] = gravity_y;

        cand_sum_x = (int32_t)wind + (int32_t)perp1_x;
        cand_sum_y = (int32_t)gravity_y + (int32_t)perp1_y;
        cand_dx[1] = (cand_sum_x > 0) ? 1 : ((cand_sum_x < 0) ? -1 : 0);
        cand_dy[1] = (cand_sum_y > 0) ? 1 : ((cand_sum_y < 0) ? -1 : 0);

        cand_sum_x = (int32_t)wind + (int32_t)perp2_x;
        cand_sum_y = (int32_t)gravity_y + (int32_t)perp2_y;
        cand_dx[2] = (cand_sum_x > 0) ? 1 : ((cand_sum_x < 0) ? -1 : 0);
        cand_dy[2] = (cand_sum_y > 0) ? 1 : ((cand_sum_y < 0) ? -1 : 0);

        hash = ((uint32_t)gx * 1103515245UL) ^ ((uint32_t)gy * 2246822519UL) ^ (uint32_t)now_tick;
        if ((hash & 0x1UL) != 0UL)
        {
          int8_t tmp_dx = cand_dx[1];
          int8_t tmp_dy = cand_dy[1];
          cand_dx[1] = cand_dx[2];
          cand_dy[1] = cand_dy[2];
          cand_dx[2] = tmp_dx;
          cand_dy[2] = tmp_dy;
        }
        if ((shake_boost != 0UL) && ((hash & 0x2UL) != 0UL))
        {
          int8_t tmp_dx = cand_dx[0];
          int8_t tmp_dy = cand_dy[0];
          cand_dx[0] = cand_dx[1];
          cand_dy[0] = cand_dy[1];
          cand_dx[1] = tmp_dx;
          cand_dy[1] = tmp_dy;
        }

        for (ci = 0U; ci < 3U; ci++)
        {
          int32_t tx = (int32_t)gx + (int32_t)cand_dx[ci];
          int32_t ty = (int32_t)gy + (int32_t)cand_dy[ci];
          uint32_t tidx;

          if ((cand_dx[ci] == 0) && (cand_dy[ci] == 0))
          {
            continue;
          }
          if ((tx < 0) || (tx >= (int32_t)s_ui_pet_sand_grid_w) ||
              (ty < 0) || (ty >= (int32_t)s_ui_pet_sand_grid_h))
          {
            continue;
          }

          tidx = UiPagePet_SandIndex((uint16_t)tx, (uint16_t)ty);
          if ((tidx >= (uint32_t)sizeof(s_ui_pet_sand_cells)) ||
              (s_ui_pet_sand_cells[tidx] != 0U))
          {
            continue;
          }

          s_ui_pet_sand_cells[idx] = 0U;
          s_ui_pet_sand_cells[tidx] = 1U;
          moved_step = 1UL;
          break;
        }

        if (x == x_end)
        {
          break;
        }
      }

      if (y == y_end)
      {
        break;
      }
    }

    moved_any |= moved_step;
    if (moved_step == 0UL)
    {
      break;
    }
  }

  return (moved_any != 0UL) ? 1U : 0U;
}

static void UiPagePet_SandRender(void)
{
  uint16_t gx;
  uint16_t gy;

  if (s_ui_pet_sand_active == 0U)
  {
    return;
  }

  for (gy = 0U; gy < s_ui_pet_sand_grid_h; gy++)
  {
    for (gx = 0U; gx < s_ui_pet_sand_grid_w; gx++)
    {
      uint32_t idx = UiPagePet_SandIndex(gx, gy);
      uint16_t x;
      uint16_t y;

      if ((idx >= (uint32_t)sizeof(s_ui_pet_sand_cells)) ||
          (s_ui_pet_sand_cells[idx] == 0U))
      {
        continue;
      }

      x = (uint16_t)(s_ui_pet_sand_origin_x + (uint16_t)(gx * s_ui_pet_sand_cell_px));
      y = (uint16_t)(s_ui_pet_sand_origin_y + (uint16_t)(gy * s_ui_pet_sand_cell_px));
      renderFillRect(x,
                     y,
                     s_ui_pet_sand_cell_px,
                     s_ui_pet_sand_cell_px,
                     RENDER_LAYER_UI,
                     RENDER_COLOR_BLACK);
    }
  }
}

static ULONG UiPagePet_MsFromThreadxTicks(ULONG ticks)
{
  unsigned long long scaled;
  ULONG ms;

  if (ticks == 0UL)
  {
    return 0UL;
  }

  scaled = ((unsigned long long)ticks * 1000ULL) +
           (unsigned long long)(TX_TIMER_TICKS_PER_SECOND - 1UL);
  ms = (ULONG)(scaled / (unsigned long long)TX_TIMER_TICKS_PER_SECOND);
  return (ms == 0UL) ? 1UL : ms;
}

static ULONG UiPagePet_StopWakePeriodMs(uint8_t stop_select_active)
{
  ULONG wake_ticks = (stop_select_active != 0U)
                         ? (ULONG)KNOB_RTOS_POWER_STOP_SELECT_RTC_WAKE_TICKS
                         : (ULONG)KNOB_RTOS_POWER_STOP_RTC_WAKE_TICKS;
  if (wake_ticks == 0UL)
  {
    wake_ticks = 1UL;
  }
  return UiPagePet_MsFromThreadxTicks(wake_ticks);
}

static void UiPagePet_AnimApplyClip(ULONG clip_id)
{
  const ui_pet_anim_clip_t *clip = UiPetAssets_GetClip((uint32_t)clip_id);
  ULONG frame_ms0 = 1UL;

  s_ui_pet_anim_clip_id = clip_id;
  s_ui_pet_anim_frame_idx = 0UL;
  s_ui_pet_anim_last_wake_count_seen = 0UL;
  s_ui_pet_anim_last_select_wake_count_seen = 0UL;
  s_ui_pet_anim_last_active_tx_tick_seen = 0UL;

  if ((clip != TX_NULL) && (clip->frame_ms != TX_NULL) && (clip->frame_count > 0U))
  {
    frame_ms0 = clip->frame_ms[0];
    if (frame_ms0 == 0UL)
    {
      frame_ms0 = 1UL;
    }
  }
  s_ui_pet_anim_ms_left = frame_ms0;
}

static void UiPagePet_AnimAdvanceFrame(void)
{
  const ui_pet_anim_clip_t *clip;
  ULONG frame_next;
  ULONG next_frame_ms;

  if (s_ui_pet_anim_clip_id == 0xFFFFFFFFUL)
  {
    return;
  }

  clip = UiPetAssets_GetClip((uint32_t)s_ui_pet_anim_clip_id);
  if ((clip == TX_NULL) || (clip->frame_count == 0U) || (clip->frame_ms == TX_NULL))
  {
    return;
  }

  frame_next = s_ui_pet_anim_frame_idx + 1UL;
  if (frame_next >= clip->frame_count)
  {
    if (clip->loop != 0U)
    {
      frame_next = 0UL;
    }
    else
    {
      frame_next = (ULONG)(clip->frame_count - 1U);
    }
  }

  s_ui_pet_anim_frame_idx = frame_next;
  next_frame_ms = clip->frame_ms[frame_next];
  if (next_frame_ms == 0UL)
  {
    next_frame_ms = 1UL;
  }
  s_ui_pet_anim_ms_left = next_frame_ms;
}

static void UiPagePet_AnimConsumeElapsedMs(ULONG elapsed_ms)
{
  ULONG guard = 0UL;

  if (elapsed_ms == 0UL)
  {
    return;
  }

  while ((elapsed_ms > 0UL) && (guard < 32UL))
  {
    guard++;
    if (s_ui_pet_anim_ms_left == 0UL)
    {
      s_ui_pet_anim_ms_left = 1UL;
    }

    if (elapsed_ms < s_ui_pet_anim_ms_left)
    {
      s_ui_pet_anim_ms_left -= elapsed_ms;
      elapsed_ms = 0UL;
      break;
    }

    elapsed_ms -= s_ui_pet_anim_ms_left;
    UiPagePet_AnimAdvanceFrame();
  }
}

static void UiPagePet_AnimSyncForState(const ui_router_state_t *state)
{
  ULONG state_clip_id;

  if (state == TX_NULL)
  {
    return;
  }

  state_clip_id = (ULONG)UiPetAssets_ClipIdForPetState((uint32_t)state->pet_state);
  if (s_ui_pet_anim_clip_id != state_clip_id)
  {
    UiPagePet_AnimApplyClip(state_clip_id);
  }
}

static void UiPagePet_AnimTick(const ui_router_state_t *state)
{
  const ui_pet_anim_clip_t *clip;
  ULONG elapsed_ms = 0UL;
  ULONG now_wake_count;
  ULONG now_tx_tick;
  ULONG delta;

  if (state == TX_NULL)
  {
    return;
  }

  UiPagePet_AnimSyncForState(state);
  clip = UiPetAssets_GetClip((uint32_t)s_ui_pet_anim_clip_id);
  if (clip == TX_NULL)
  {
    return;
  }

  if (clip->tick_domain == UI_PET_ANIM_TICK_STOP_WAKE_1HZ)
  {
    now_wake_count = state->pet_wake_count;
    if (s_ui_pet_anim_last_wake_count_seen == 0UL)
    {
      s_ui_pet_anim_last_wake_count_seen = now_wake_count;
      return;
    }

    delta = now_wake_count - s_ui_pet_anim_last_wake_count_seen;
    s_ui_pet_anim_last_wake_count_seen = now_wake_count;
    if (delta > 8UL)
    {
      delta = 8UL;
    }
    elapsed_ms = delta * UiPagePet_StopWakePeriodMs((state->stop_select_active != 0UL) ? 1U : 0U);
  }
  else if (clip->tick_domain == UI_PET_ANIM_TICK_STOP_SELECT_2HZ)
  {
    now_wake_count = state->pet_wake_count;
    if (s_ui_pet_anim_last_select_wake_count_seen == 0UL)
    {
      s_ui_pet_anim_last_select_wake_count_seen = now_wake_count;
      return;
    }

    delta = now_wake_count - s_ui_pet_anim_last_select_wake_count_seen;
    s_ui_pet_anim_last_select_wake_count_seen = now_wake_count;
    if (state->stop_select_active == 0UL)
    {
      return;
    }
    if (delta > 8UL)
    {
      delta = 8UL;
    }
    elapsed_ms = delta * UiPagePet_StopWakePeriodMs(1U);
  }
  else
  {
    if (((state->mode_flags & UI_MODE_FLAG_STOP) != 0UL) && (state->stop_select_active == 0UL))
    {
      return;
    }
    now_tx_tick = tx_time_get();
    if (s_ui_pet_anim_last_active_tx_tick_seen == 0UL)
    {
      s_ui_pet_anim_last_active_tx_tick_seen = now_tx_tick;
      return;
    }

    delta = now_tx_tick - s_ui_pet_anim_last_active_tx_tick_seen;
    s_ui_pet_anim_last_active_tx_tick_seen = now_tx_tick;
    if (delta > 50UL)
    {
      delta = 50UL;
    }
    elapsed_ms = UiPagePet_MsFromThreadxTicks(delta);
  }

  UiPagePet_AnimConsumeElapsedMs(elapsed_ms);
}

static const sprite2_t *UiPagePet_AnimCurrentFrame(void)
{
  const ui_pet_anim_clip_t *clip;

  if (s_ui_pet_anim_clip_id == 0xFFFFFFFFUL)
  {
    return TX_NULL;
  }

  clip = UiPetAssets_GetClip((uint32_t)s_ui_pet_anim_clip_id);
  if ((clip == TX_NULL) || (clip->frames == TX_NULL) || (clip->frame_count == 0U))
  {
    return TX_NULL;
  }

  if (s_ui_pet_anim_frame_idx >= clip->frame_count)
  {
    return clip->frames[0];
  }
  return clip->frames[s_ui_pet_anim_frame_idx];
}

static ULONG UiPagePet_ClipTotalMs(ULONG clip_id)
{
  const ui_pet_anim_clip_t *clip = UiPetAssets_GetClip((uint32_t)clip_id);
  ULONG total = 0UL;
  uint32_t i;

  if ((clip == TX_NULL) || (clip->frame_ms == TX_NULL) || (clip->frame_count == 0U))
  {
    return 0UL;
  }

  for (i = 0U; i < clip->frame_count; i++)
  {
    ULONG frame_ms = (ULONG)clip->frame_ms[i];
    if (frame_ms == 0UL)
    {
      frame_ms = 1UL;
    }
    if (total > (0xFFFFFFFFUL - frame_ms))
    {
      total = 0xFFFFFFFFUL;
      break;
    }
    total += frame_ms;
  }

  return total;
}

static ULONG UiPagePet_ActionHoldMs(ULONG action_idx)
{
  ULONG total_ms = 0UL;

  if (action_idx == (ULONG)GAME_PET_MENU_SELECT_FEED)
  {
    total_ms = UiPagePet_ClipTotalMs(UI_PET_CLIP_ID_EAT);
  }
  else if (action_idx == (ULONG)GAME_PET_MENU_SELECT_PLAY)
  {
    total_ms = UiPagePet_ClipTotalMs(UI_PET_CLIP_ID_PLAY);
  }
  else
  {
    return 0UL;
  }

  if (total_ms >= (0xFFFFFFFFUL - UI_PET_ACTION_HOLD_MARGIN_MS))
  {
    return 0xFFFFFFFFUL;
  }
  return total_ms + UI_PET_ACTION_HOLD_MARGIN_MS;
}

static const game_package_pet_menu_item_t *UiPagePet_CurrentMenuItem(void)
{
  ULONG row_count;
  ULONG flat_slot;

  if (s_ui_pet_selected_row >= UI_PET_ROW_COUNT)
  {
    return (const game_package_pet_menu_item_t *)0;
  }

  row_count = (ULONG)UiPetAssets_RowCount((uint32_t)s_ui_pet_selected_row);
  if (row_count == 0UL)
  {
    return (const game_package_pet_menu_item_t *)0;
  }

  if (s_ui_pet_selected_col >= row_count)
  {
    return (const game_package_pet_menu_item_t *)0;
  }

  flat_slot = 0UL;
  if (s_ui_pet_selected_row != 0UL)
  {
    flat_slot = (ULONG)UiPetAssets_RowCount(0u);
  }
  flat_slot += s_ui_pet_selected_col;
  return GamePackage_GetPetMenuItemBySlot(flat_slot);
}

static ULONG UiPagePet_CurrentSelectKind(void)
{
  const game_package_pet_menu_item_t *item = UiPagePet_CurrentMenuItem();

  if (item == (const game_package_pet_menu_item_t *)0)
  {
    return (ULONG)GAME_PET_MENU_ACTION_COUNT;
  }
  if ((uint32_t)item->select_kind > (uint32_t)GAME_PET_MENU_SELECT_SAND_FX)
  {
    return (ULONG)GAME_PET_MENU_ACTION_COUNT;
  }

  return (ULONG)item->select_kind;
}

static ULONG UiPagePet_TotalSlotCount(void)
{
  ULONG total = 0UL;
  ULONG row_idx;

  for (row_idx = 0UL; row_idx < UI_PET_ROW_COUNT; row_idx++)
  {
    total += (ULONG)UiPetAssets_RowCount((uint32_t)row_idx);
  }

  return total;
}

static uint8_t UiPagePet_SlotEnabledByFlatIndex(ULONG flat_idx)
{
  return (GamePackage_GetPetMenuItemBySlot(flat_idx) != (const game_package_pet_menu_item_t *)0) ? 1U : 0U;
}

static ULONG UiPagePet_FlatIndexFromSelection(void)
{
  ULONG row_idx;
  ULONG base = 0UL;
  ULONG row_count;

  for (row_idx = 0UL; row_idx < UI_PET_ROW_COUNT; row_idx++)
  {
    row_count = (ULONG)UiPetAssets_RowCount((uint32_t)row_idx);
    if (row_idx == s_ui_pet_selected_row)
    {
      if (row_count == 0UL)
      {
        return base;
      }
      if (s_ui_pet_selected_col >= row_count)
      {
        return base;
      }
      return (base + s_ui_pet_selected_col);
    }
    base += row_count;
  }

  return 0UL;
}

static void UiPagePet_SetSelectionFromFlatIndex(ULONG flat_idx)
{
  ULONG row_idx;
  ULONG row_count;
  ULONG base = 0UL;

  for (row_idx = 0UL; row_idx < UI_PET_ROW_COUNT; row_idx++)
  {
    row_count = (ULONG)UiPetAssets_RowCount((uint32_t)row_idx);
    if (row_count == 0UL)
    {
      continue;
    }

    if (flat_idx < (base + row_count))
    {
      s_ui_pet_selected_row = row_idx;
      s_ui_pet_selected_col = (flat_idx - base);
      return;
    }

    base += row_count;
  }

  s_ui_pet_selected_row = 0UL;
  s_ui_pet_selected_col = 0UL;
}

static uint8_t UiPagePet_SelectFirstEnabled(void)
{
  ULONG total = UiPagePet_TotalSlotCount();
  ULONG i;

  for (i = 0UL; i < total; i++)
  {
    if (UiPagePet_SlotEnabledByFlatIndex(i) != 0U)
    {
      UiPagePet_SetSelectionFromFlatIndex(i);
      return 1U;
    }
  }

  s_ui_pet_selected_row = 0UL;
  s_ui_pet_selected_col = 0UL;
  return 0U;
}

static void UiPagePet_EnsureSelectionEnabled(void)
{
  ULONG flat = UiPagePet_FlatIndexFromSelection();
  if (UiPagePet_SlotEnabledByFlatIndex(flat) == 0U)
  {
    (void)UiPagePet_SelectFirstEnabled();
  }
}

static void UiPagePet_MoveLeft(void)
{
  ULONG total = UiPagePet_TotalSlotCount();
  ULONG flat;
  ULONG i;

  if (total == 0UL)
  {
    s_ui_pet_selected_row = 0UL;
    s_ui_pet_selected_col = 0UL;
    return;
  }

  UiPagePet_EnsureSelectionEnabled();
  flat = UiPagePet_FlatIndexFromSelection();
  for (i = 0UL; i < total; i++)
  {
    flat = (flat == 0UL) ? (total - 1UL) : (flat - 1UL);
    if (UiPagePet_SlotEnabledByFlatIndex(flat) != 0U)
    {
      UiPagePet_SetSelectionFromFlatIndex(flat);
      return;
    }
  }
}

static void UiPagePet_MoveRight(void)
{
  ULONG total = UiPagePet_TotalSlotCount();
  ULONG flat;
  ULONG i;

  if (total == 0UL)
  {
    s_ui_pet_selected_row = 0UL;
    s_ui_pet_selected_col = 0UL;
    return;
  }

  UiPagePet_EnsureSelectionEnabled();
  flat = UiPagePet_FlatIndexFromSelection();
  for (i = 0UL; i < total; i++)
  {
    flat = (flat + 1UL) % total;
    if (UiPagePet_SlotEnabledByFlatIndex(flat) != 0U)
    {
      UiPagePet_SetSelectionFromFlatIndex(flat);
      return;
    }
  }
}

static void UiPagePet_MoveVertical(void)
{
  ULONG target_row_idx;
  ULONG target_row_count;
  ULONG src_col;
  ULONG base;
  ULONG i;

  UiPagePet_EnsureSelectionEnabled();
  target_row_idx = (s_ui_pet_selected_row == 0UL) ? 1UL : 0UL;
  if (target_row_idx >= UI_PET_ROW_COUNT)
  {
    return;
  }

  target_row_count = (ULONG)UiPetAssets_RowCount((uint32_t)target_row_idx);
  if (target_row_count == 0UL)
  {
    return;
  }

  src_col = s_ui_pet_selected_col;
  if (src_col >= target_row_count)
  {
    src_col = (target_row_count - 1UL);
  }

  base = (target_row_idx == 0UL) ? 0UL : (ULONG)UiPetAssets_RowCount(0u);
  if (UiPagePet_SlotEnabledByFlatIndex(base + src_col) != 0U)
  {
    s_ui_pet_selected_row = target_row_idx;
    s_ui_pet_selected_col = src_col;
    return;
  }

  for (i = 0UL; i < target_row_count; i++)
  {
    ULONG col = (src_col + i) % target_row_count;
    if (UiPagePet_SlotEnabledByFlatIndex(base + col) != 0U)
    {
      s_ui_pet_selected_row = target_row_idx;
      s_ui_pet_selected_col = col;
      return;
    }
  }
}

static uint8_t UiPagePet_BatteryLevel4Index(ULONG soc_percent)
{
  if (soc_percent > 100UL)
  {
    soc_percent = 100UL;
  }
  if (soc_percent >= 75UL)
  {
    return 3U;
  }
  if (soc_percent >= 50UL)
  {
    return 2U;
  }
  if (soc_percent >= 25UL)
  {
    return 1U;
  }
  return 0U;
}

static ULONG UiPagePet_BatteryIconRefreshMinMs(void)
{
  ULONG min_ms = (ULONG)KNOB_SENSOR_PMIC_POLL_PERIOD_MS;

  if (min_ms == 0UL)
  {
    min_ms = 250UL;
  }
  min_ms *= UI_PET_BATTERY_ICON_REFRESH_PMIC_SAMPLES;
  if (min_ms < 1000UL)
  {
    min_ms = 1000UL;
  }
  return min_ms;
}

static void UiPagePet_BatteryIconCachePrime(const ui_router_state_t *state)
{
  if (state == TX_NULL)
  {
    return;
  }

  s_ui_pet_battery_icon_last_sample_tick_seen = state->pmic_live.last_sample_tick;
  s_ui_pet_battery_icon_last_apply_tick = state->pmic_live.last_sample_tick;
  s_ui_pet_battery_icon_level4_cached =
      UiPagePet_BatteryLevel4Index(state->pmic_live.battery_soc_percent);
  s_ui_pet_battery_icon_bool_cached =
      (state->pmic_live.battery_soc_percent > 0UL) ? 1U : 0U;
  s_ui_pet_battery_icon_cache_valid = 1U;
}

static void UiPagePet_BatteryIconCacheUpdate(const ui_router_state_t *state)
{
  ULONG sample_tick;

  if (state == TX_NULL)
  {
    return;
  }
  if (s_ui_pet_battery_icon_cache_valid == 0U)
  {
    UiPagePet_BatteryIconCachePrime(state);
    return;
  }

  sample_tick = state->pmic_live.last_sample_tick;
  if ((sample_tick != s_ui_pet_battery_icon_last_sample_tick_seen) &&
      ((ULONG)(sample_tick - s_ui_pet_battery_icon_last_apply_tick) >=
       UiPagePet_BatteryIconRefreshMinMs()))
  {
    s_ui_pet_battery_icon_level4_cached =
        UiPagePet_BatteryLevel4Index(state->pmic_live.battery_soc_percent);
    s_ui_pet_battery_icon_bool_cached =
        (state->pmic_live.battery_soc_percent > 0UL) ? 1U : 0U;
    s_ui_pet_battery_icon_last_apply_tick = sample_tick;
  }
  s_ui_pet_battery_icon_last_sample_tick_seen = sample_tick;
}

static uint8_t UiPagePet_SlotActionId(ULONG row_idx,
                                      ULONG col_idx,
                                      const ui_router_state_t *state,
                                      uint8_t *action_id_out)
{
  ULONG flat;
  const game_package_pet_menu_item_t *item;
  uint8_t icon_id;

  if ((action_id_out == TX_NULL) || (row_idx >= UI_PET_ROW_COUNT))
  {
    return 0U;
  }

  flat = (row_idx == 0UL) ? col_idx : ((ULONG)UiPetAssets_RowCount(0u) + col_idx);
  item = GamePackage_GetPetMenuItemBySlot(flat);
  if (item == (const game_package_pet_menu_item_t *)0)
  {
    return 0U;
  }

  icon_id = item->icon_action_id;
  if ((state != TX_NULL) &&
      (item->status_source_id == (uint16_t)GAME_PET_MENU_STATUS_SOURCE_BATTERY))
  {
    UiPagePet_BatteryIconCacheUpdate(state);
    if (item->status_kind == (uint8_t)GAME_PET_MENU_STATUS_BOOL)
    {
      icon_id = (uint8_t)(item->arg0 + s_ui_pet_battery_icon_bool_cached);
    }
    else if (item->status_kind == (uint8_t)GAME_PET_MENU_STATUS_LEVEL4)
    {
      icon_id = (uint8_t)(item->arg0 + s_ui_pet_battery_icon_level4_cached);
    }
  }

  if ((uint32_t)icon_id >= (uint32_t)GAME_PET_MENU_ACTION_COUNT)
  {
    return 0U;
  }
  *action_id_out = icon_id;
  return 1U;
}

static void UiPagePet_SelectBootstrapIfNeeded(void)
{
  if (UiPagePet_CurrentSelectKind() >= (ULONG)GAME_PET_MENU_ACTION_COUNT)
  {
    (void)UiPagePet_SelectFirstEnabled();
  }
}

static void UiPagePet_Enter(ui_router_t *ui, const void *arg)
{
  const ui_router_state_t *state = UiRuntimeContext_GetState();
  (void)ui;
  (void)arg;

  s_ui_pet_anim_last_wake_count_seen = 0UL;
  s_ui_pet_anim_last_select_wake_count_seen = 0UL;
  s_ui_pet_anim_last_active_tx_tick_seen = 0UL;
  s_ui_pet_anim_clip_id = 0xFFFFFFFFUL;
  s_ui_pet_anim_frame_idx = 0UL;
  s_ui_pet_anim_ms_left = 1UL;
  s_ui_pet_battery_icon_last_sample_tick_seen = 0UL;
  s_ui_pet_battery_icon_last_apply_tick = 0UL;
  s_ui_pet_battery_icon_level4_cached = 0U;
  s_ui_pet_battery_icon_bool_cached = 0U;
  s_ui_pet_battery_icon_cache_valid = 0U;
  UiPagePet_SandReset();
  UiPagePet_SelectBootstrapIfNeeded();
  if (state != TX_NULL)
  {
    UiPagePet_AnimSyncForState(state);
    UiPagePet_BatteryIconCachePrime(state);
    s_ui_pet_anim_last_wake_count_seen = state->pet_wake_count;
    s_ui_pet_anim_last_select_wake_count_seen = state->pet_wake_count;
    s_ui_pet_anim_last_active_tx_tick_seen = tx_time_get();
  }
}

static uint32_t UiPagePet_Event(ui_router_t *ui, const ui_input_evt_t *evt)
{
  const ui_router_state_t *state = UiRuntimeContext_GetState();
  uint8_t stop_mode = 0U;

  if ((ui == 0) || (evt == 0))
  {
    return UI_EVT_RESULT_NONE;
  }

  if (state != TX_NULL)
  {
    stop_mode = ((state->mode_flags & UI_MODE_FLAG_STOP) != 0UL) ? 1U : 0U;
  }

  if ((evt->evt == UI_EVT_UP) || (evt->evt == UI_EVT_LEFT))
  {
    if ((stop_mode != 0U) && (state != TX_NULL) && (state->stop_select_active == 0UL))
    {
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
    }
    if (evt->evt == UI_EVT_UP)
    {
      UiPagePet_MoveVertical();
    }
    else
    {
      UiPagePet_MoveLeft();
    }
    return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
  }

  if ((evt->evt == UI_EVT_DOWN) || (evt->evt == UI_EVT_RIGHT))
  {
    if ((stop_mode != 0U) && (state != TX_NULL) && (state->stop_select_active == 0UL))
    {
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
    }
    if (evt->evt == UI_EVT_DOWN)
    {
      UiPagePet_MoveVertical();
    }
    else
    {
      UiPagePet_MoveRight();
    }
    return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
  }

  if (evt->evt == UI_EVT_SELECT)
  {
    const game_package_pet_menu_item_t *selected_item;
    ULONG action_idx;
    uint32_t mode_id;

    if ((stop_mode != 0U) && (state != TX_NULL) && (state->stop_select_active == 0UL))
    {
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
    }

    selected_item = UiPagePet_CurrentMenuItem();
    action_idx = UiPagePet_CurrentSelectKind();
    if ((s_ui_pet_sand_active != 0U) && (action_idx != (ULONG)GAME_PET_MENU_SELECT_SAND_FX))
    {
      UiPagePet_SandExitLiveImu(0U);
      UiPagePet_SandReset();
    }
    if (action_idx == (ULONG)GAME_PET_MENU_SELECT_FEED)
    {
      if (UiRouter_PushTree(ui, (ui_menu_tree_id_t)UI_TREE_ID_PET_FEED, &UI_MENU_PET_FEED) != 0U)
      {
        (void)App_SysEvent_ModeSet(APP_MODE_STATIC);
      }
      else
      {
        (void)App_PetReq_ActionWithHold(APP_PET_ACTION_FEED, UiPagePet_ActionHoldMs(action_idx));
      }
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
    }
    if (action_idx == (ULONG)GAME_PET_MENU_SELECT_PLAY)
    {
      (void)App_PetReq_ActionWithHold(APP_PET_ACTION_PLAY, UiPagePet_ActionHoldMs(action_idx));
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
    }
    if (action_idx == (ULONG)GAME_PET_MENU_SELECT_SAND_FX)
    {
      if (s_ui_pet_sand_active != 0U)
      {
        UiPagePet_SandExitLiveImu(1U);
        UiPagePet_SandReset();
      }
      else
      {
        UiPagePet_SandStart(state);
      }
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
    }
    if (action_idx == (ULONG)GAME_PET_MENU_SELECT_START_GAME)
    {
      (void)GamePackage_RequestRuntimeModeByPetEntry((uint32_t)GAME_PET_ENTRY_START_GAME);
      (void)App_SysEvent_ModeSet(APP_MODE_REALTIME);
      return UI_EVT_RESULT_HANDLED;
    }
    if (action_idx == (ULONG)GAME_PET_MENU_SELECT_OPTIONS)
    {
      (void)UiRouter_PushTree(ui, (ui_menu_tree_id_t)UI_TREE_ID_SYSTEM_ROOT, &UI_MENU_SYSTEM);
      (void)App_SysEvent_ModeSet(APP_MODE_STATIC);
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
    }
    if (action_idx == (ULONG)GAME_PET_MENU_SELECT_LAUNCH_MODE)
    {
      mode_id = 0UL;
      if (selected_item != (const game_package_pet_menu_item_t *)0)
      {
        mode_id = (uint32_t)selected_item->arg0;
      }
      if ((mode_id != 0UL) && (GamePackage_RequestRuntimeModeById(mode_id) == TX_SUCCESS))
      {
        (void)App_SysEvent_ModeSet(APP_MODE_REALTIME);
        return UI_EVT_RESULT_HANDLED;
      }
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
    }
    if (action_idx == (ULONG)GAME_PET_MENU_SELECT_OPEN_PAGE)
    {
      uint16_t page_id = 0U;
      if (selected_item != (const game_package_pet_menu_item_t *)0)
      {
        page_id = (uint16_t)selected_item->arg0;
      }
      if ((page_id != 0U) && (UiPageRegistry_OpenById(ui, page_id) != 0U))
      {
        (void)App_SysEvent_ModeSet(APP_MODE_STATIC);
      }
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
    }
    return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
  }

  if ((evt->evt == UI_EVT_BACK) || (evt->evt == UI_EVT_LONG_BACK))
  {
    if (s_ui_pet_sand_active != 0U)
    {
      UiPagePet_SandExitLiveImu(0U);
      UiPagePet_SandReset();
    }

    if (stop_mode != 0U)
    {
      (void)App_PetReq_Action(APP_PET_ACTION_NONE);
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY | UI_EVT_RESULT_CONSUME_BACK);
    }
    UiRouter_Reset(ui, &UI_MENU_ROOT);
    (void)App_SysEvent_ModeSet(APP_MODE_STATIC);
    return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY | UI_EVT_RESULT_CONSUME_BACK);
  }

  if (evt->evt == UI_EVT_TICK)
  {
    if (state != TX_NULL)
    {
      ULONG clip_before = s_ui_pet_anim_clip_id;
      ULONG frame_before = s_ui_pet_anim_frame_idx;
      UiPagePet_AnimTick(state);
      if ((state->pet_tick_count != s_ui_pet_last_tick_seen) ||
          (state->pet_state != s_ui_pet_last_state_seen) ||
          (state->stop_select_active != s_ui_pet_last_select_seen) ||
          (clip_before != s_ui_pet_anim_clip_id) ||
          (frame_before != s_ui_pet_anim_frame_idx))
      {
        s_ui_pet_last_tick_seen = state->pet_tick_count;
        s_ui_pet_last_state_seen = state->pet_state;
        s_ui_pet_last_select_seen = state->stop_select_active;
        return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
      }

      if (UiPagePet_SandStep(state) != 0U)
      {
        return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
      }
    }
  }

  return UI_EVT_RESULT_NONE;
}

static void UiPagePet_DrawIconSlot(const ui_pet_icon_slot_t *slot,
                                   uint8_t selected,
                                   uint8_t selected_visible)
{
  const sprite1_t *selection_bg;

  if ((slot == TX_NULL) || (slot->spr == TX_NULL))
  {
    return;
  }

  if ((selected != 0U) && (selected_visible != 0U))
  {
    selection_bg = UiPetAssets_GetSelectionBg();
    if (selection_bg != TX_NULL)
    {
      Render_DrawSprite(selection_bg, slot->x, slot->y, RENDER_LAYER_UI, UI_PET_ICON_SCALE);
    }

    renderBlit1bppScaled(slot->x, slot->y,
                         slot->spr->w, slot->spr->h,
                         slot->spr->mask, slot->spr->stride,
                         slot->spr->leftmost_is_msb,
                         RENDER_LAYER_UI,
                         RENDER_COLOR_WHITE,
                         UI_PET_ICON_SCALE);
    return;
  }

  Render_DrawSprite(slot->spr, slot->x, slot->y, RENDER_LAYER_UI, UI_PET_ICON_SCALE);
}

static void UiPagePet_Render(ui_router_t *ui)
{
  const ui_router_state_t *state = UiRuntimeContext_GetState();
  const sprite2_t *pet_sprite;
  uint8_t select_visible = 0U;
  uint8_t dormant = 1U;
  ui_pet_icon_slot_t slot;
  const ui_pet_row_layout_t *row;
  uint8_t action_id;
  ULONG row_idx;
  ULONG row_count;
  ULONG i;

  (void)ui;

  if (state == TX_NULL)
  {
    return;
  }

  dormant = (state->stop_select_active == 0UL) ? 1U : 0U;
  select_visible = (dormant != 0U) ? 0U : (((state->pet_tick_count & 1UL) != 0UL) ? 1U : 0U);
  UiPagePet_AnimSyncForState(state);
  pet_sprite = UiPagePet_AnimCurrentFrame();

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(TH_MODE_STOP);

  for (row_idx = 0UL; row_idx < UI_PET_ROW_COUNT; row_idx++)
  {
    row = &k_ui_pet_rows[row_idx];
    row_count = (ULONG)UiPetAssets_RowCount((uint32_t)row_idx);
    for (i = 0UL; i < row_count; i++)
    {
      if (UiPagePet_SlotActionId(row_idx, i, state, &action_id) == 0U)
      {
        continue;
      }
      slot.x = UiPagePet_RowSlotX(row_count, i);
      slot.y = row->y;
      slot.spr = UiPetAssets_GetActionIcon((uint32_t)action_id);
      if (slot.spr == TX_NULL)
      {
        continue;
      }
      UiPagePet_DrawIconSlot(&slot,
                             (uint8_t)(((row_idx == s_ui_pet_selected_row) && (i == s_ui_pet_selected_col)) ? 1U : 0U),
                             select_visible);
    }
  }

  if (pet_sprite != TX_NULL)
  {
    uint16_t draw_x = 0U;
    uint16_t draw_y = 0U;
    uint16_t draw_w = 0U;
    uint16_t draw_h = 0U;
    uint16_t draw_scale_px = 0U;
    if (UiPagePet_ResolveSpriteDrawRect(pet_sprite,
                                        &draw_x,
                                        &draw_y,
                                        &draw_w,
                                        &draw_h,
                                        &draw_scale_px) != 0U)
    {
      if (s_ui_pet_sand_active != 0U)
      {
        UiPagePet_SandRender();
      }
      else
      {
        Render_DrawSprite2(pet_sprite,
                           draw_x,
                           draw_y,
                           RENDER_LAYER_UI,
                           UI_PET_STATE_PRESENT_MODE,
                           UI_PET_STATE_SCALE);
      }
    }
  }
  else if (s_ui_pet_sand_active != 0U)
  {
    UiPagePet_SandRender();
  }

  Render_MarkDirtyAll();
}

static void UiPagePet_Exit(ui_router_t *ui)
{
  (void)ui;
  if (s_ui_pet_sand_active != 0U)
  {
    UiPagePet_SandExitLiveImu(0U);
  }
  UiPagePet_SandReset();
}

const ui_page_t UI_PAGE_PET_NATIVE =
{
  .name = "pet",
  .footer = UI_FOOTER_A_SELECT_B_BACK,
  .enter = UiPagePet_Enter,
  .event = UiPagePet_Event,
  .render = UiPagePet_Render,
  .exit = UiPagePet_Exit
};
