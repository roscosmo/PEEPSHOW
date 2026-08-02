#include "ui/pages/page_joy_target.h"
#include "display_renderer.h"
#include "th_mode.h"
#include <math.h>
#include <string.h>

static ui_joy_live_status_t s_last_live;
static ULONG s_last_input_source = 0UL;
static ULONG s_last_input_action = 0UL;
static ULONG s_last_input_edge = 0UL;
static uint8_t s_last_valid = 0U;
static uint8_t s_done_recal_mode = 0U;
static uint8_t s_done_recal_next_enter = 0U;

static th_mode_t UiJoyTargetModeFromFlags(ULONG mode_flags)
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

static float UiJoyTargetAbs(float v)
{
  return (v < 0.0f) ? -v : v;
}

static uint8_t UiJoyTargetFloatChanged(float a, float b, float eps)
{
  return (UiJoyTargetAbs(a - b) > eps) ? 1U : 0U;
}

static void UiJoyTargetDrawEllipse(uint16_t cx, uint16_t cy, uint16_t rx, uint16_t ry)
{
  uint32_t i;
  float prev_x = (float)cx + (float)rx;
  float prev_y = (float)cy;
  const uint32_t seg_count = 24U;

  for (i = 1U; i <= seg_count; i++)
  {
    float a = (6.28318530718f * (float)i) / (float)seg_count;
    float x = (float)cx + (cosf(a) * (float)rx);
    float y = (float)cy + (sinf(a) * (float)ry);
    renderDrawLine((uint16_t)prev_x, (uint16_t)prev_y,
                   (uint16_t)x, (uint16_t)y,
                   RENDER_LAYER_UI, RENDER_COLOR_BLACK, 1U);
    prev_x = x;
    prev_y = y;
  }
}

static uint8_t UiJoyTargetLiveChanged(const ui_router_state_t *state)
{
  const ui_joy_live_status_t *live;

  if (state == TX_NULL)
  {
    return 0U;
  }
  live = &state->joy_live;

  if (s_last_valid == 0U)
  {
    return 1U;
  }

  if ((live->dir != s_last_live.dir) ||
      (live->input_mask != s_last_live.input_mask) ||
      (live->deadzone_enabled != s_last_live.deadzone_enabled) ||
      (live->invert_x != s_last_live.invert_x) ||
      (live->invert_y != s_last_live.invert_y))
  {
    return 1U;
  }

  if ((state->last_input_source != s_last_input_source) ||
      (state->last_input_action != s_last_input_action) ||
      (state->last_input_edge != s_last_input_edge))
  {
    return 1U;
  }

  if (UiJoyTargetFloatChanged(live->nx, s_last_live.nx, 0.015f) != 0U)
  {
    return 1U;
  }
  if (UiJoyTargetFloatChanged(live->ny, s_last_live.ny, 0.015f) != 0U)
  {
    return 1U;
  }
  if (UiJoyTargetFloatChanged(live->r_abs_mT, s_last_live.r_abs_mT, 0.2f) != 0U)
  {
    return 1U;
  }
  if (UiJoyTargetFloatChanged(live->span_x_mT, s_last_live.span_x_mT, 0.05f) != 0U)
  {
    return 1U;
  }
  if (UiJoyTargetFloatChanged(live->span_y_mT, s_last_live.span_y_mT, 0.05f) != 0U)
  {
    return 1U;
  }
  if (UiJoyTargetFloatChanged(live->threshold_x_mT, s_last_live.threshold_x_mT, 0.05f) != 0U)
  {
    return 1U;
  }
  if (UiJoyTargetFloatChanged(live->threshold_y_mT, s_last_live.threshold_y_mT, 0.05f) != 0U)
  {
    return 1U;
  }
  if (UiJoyTargetFloatChanged(live->deadzone_mT, s_last_live.deadzone_mT, 0.05f) != 0U)
  {
    return 1U;
  }

  return 0U;
}

static void UiPageJoyTarget_Enter(void)
{
  s_last_valid = 0U;
  (void)memset(&s_last_live, 0, sizeof(s_last_live));
  s_last_input_source = 0UL;
  s_last_input_action = 0UL;
  s_last_input_edge = 0UL;
  s_done_recal_mode = (s_done_recal_next_enter != 0U) ? 1U : 0U;
  UiRouter_MarkDirty();
}

static uint8_t UiPageJoyTarget_Action(const ui_action_evt_t *evt)
{
  const ui_router_state_t *state = UiRouter_GetState();
  const ui_router_api_t *api = UiRouter_GetApi();

  if (evt == TX_NULL)
  {
    return 0U;
  }

  if ((evt->action == UI_ACTION_CONFIRM) && (evt->event == UI_EVENT_PRESS))
  {
    if ((state != TX_NULL) &&
        (state->joy_cal_status.stage == (ULONG)UI_JOY_CAL_STAGE_DONE))
    {
      if (state->joy_cal_status.save_pending != 0UL)
      {
        return 1U;
      }

      if (s_done_recal_mode == 0U)
      {
        if ((api != TX_NULL) && (api->joy_cal_save != TX_NULL))
        {
          (void)api->joy_cal_save();
        }
        s_done_recal_mode = 1U;
        s_done_recal_next_enter = 1U;
        UiRouter_MarkDirty();
      }
      else
      {
        if ((api != TX_NULL) && (api->joy_cal_start != TX_NULL))
        {
          (void)api->joy_cal_start();
        }
        s_done_recal_mode = 0U;
        s_done_recal_next_enter = 0U;
        UiRouter_RequestPage(UI_PAGE_JOY_CAL);
      }
    }
    return 1U;
  }

  if ((evt->action == UI_ACTION_CANCEL) && (evt->event == UI_EVENT_PRESS))
  {
    if ((state != TX_NULL) &&
        (state->joy_cal_status.save_pending == 0UL) &&
        (state->joy_cal_status.stage >= (ULONG)UI_JOY_CAL_STAGE_NEUTRAL) &&
        (state->joy_cal_status.stage <= (ULONG)UI_JOY_CAL_STAGE_ERROR) &&
        (api != TX_NULL) &&
        (api->joy_cal_cancel != TX_NULL))
    {
      (void)api->joy_cal_cancel();
    }

    if ((state != TX_NULL) &&
        (state->joy_cal_status.stage == (ULONG)UI_JOY_CAL_STAGE_DONE) &&
        (state->joy_cal_status.save_pending == 0UL))
    {
      s_done_recal_mode = 1U;
      s_done_recal_next_enter = 1U;
    }
    UiRouter_RequestPage(UI_PAGE_MENU_INPUT_SUB);
    return 1U;
  }

  return 0U;
}

static void UiPageJoyTarget_Tick(void)
{
  const ui_router_state_t *state = UiRouter_GetState();
  if ((state != TX_NULL) && (state->joy_cal_status.stage != (ULONG)UI_JOY_CAL_STAGE_DONE))
  {
    if ((s_done_recal_mode != 0U) || (s_done_recal_next_enter != 0U))
    {
      s_done_recal_mode = 0U;
      s_done_recal_next_enter = 0U;
      UiRouter_MarkDirty();
    }
  }

  if (UiJoyTargetLiveChanged(state) != 0U)
  {
    if (state != TX_NULL)
    {
      s_last_live = state->joy_live;
      s_last_input_source = state->last_input_source;
      s_last_input_action = state->last_input_action;
      s_last_input_edge = state->last_input_edge;
      s_last_valid = 1U;
    }
    UiRouter_MarkDirty();
  }
}

static void UiPageJoyTarget_Render(void)
{
  const ui_router_state_t *state = UiRouter_GetState();
  const ui_joy_live_status_t *live;
  const ui_joy_cal_quality_t *quality;
  uint16_t plot_top = 20U;
  uint16_t plot_size;
  uint16_t plot_left;
  uint16_t cx;
  uint16_t cy;
  uint16_t radius;
  float span_max;
  uint16_t rx;
  uint16_t ry;
  float thr_mT = 0.0f;

  if (state == TX_NULL)
  {
    return;
  }
  live = &state->joy_live;
  quality = &state->joy_cal_quality;

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(UiJoyTargetModeFromFlags(state->mode_flags));
  renderDrawText(4U, 8U, "JOY TARGET", RENDER_LAYER_UI, RENDER_COLOR_BLACK);

  plot_size = (RENDER_WIDTH > 16U) ? (uint16_t)(RENDER_WIDTH - 16U) : RENDER_WIDTH;
  if (plot_size > (RENDER_HEIGHT - 32U))
  {
    plot_size = (uint16_t)(RENDER_HEIGHT - 32U);
  }
  if (plot_size < 24U)
  {
    plot_size = 24U;
  }

  plot_left = (uint16_t)((RENDER_WIDTH - plot_size) / 2U);
  cx = (uint16_t)(plot_left + (plot_size / 2U));
  cy = (uint16_t)(plot_top + (plot_size / 2U));
  radius = (uint16_t)((plot_size / 2U) - 2U);

  renderDrawRectOutline(plot_left, plot_top, plot_size, plot_size,
                        RENDER_LAYER_UI, RENDER_COLOR_BLACK, 1U);
  renderDrawLine((uint16_t)(cx - radius), cy, (uint16_t)(cx + radius), cy,
                 RENDER_LAYER_UI, RENDER_COLOR_BLACK, 1U);
  renderDrawLine(cx, (uint16_t)(cy - radius), cx, (uint16_t)(cy + radius),
                 RENDER_LAYER_UI, RENDER_COLOR_BLACK, 1U);

  span_max = live->span_x_mT;
  if (live->span_y_mT > span_max)
  {
    span_max = live->span_y_mT;
  }
  if (span_max < 0.001f)
  {
    span_max = 1.0f;
  }

  rx = (uint16_t)(((live->span_x_mT / span_max) * (float)radius) + 0.5f);
  ry = (uint16_t)(((live->span_y_mT / span_max) * (float)radius) + 0.5f);
  if (rx < 1U)
  {
    rx = 1U;
  }
  if (ry < 1U)
  {
    ry = 1U;
  }

  UiJoyTargetDrawEllipse(cx, cy, rx, ry);

  if ((live->threshold_x_mT > 0.0f) && (live->threshold_y_mT > 0.0f))
  {
    thr_mT = (live->threshold_x_mT < live->threshold_y_mT) ? live->threshold_x_mT : live->threshold_y_mT;
  }
  else if (live->threshold_x_mT > 0.0f)
  {
    thr_mT = live->threshold_x_mT;
  }
  else if (live->threshold_y_mT > 0.0f)
  {
    thr_mT = live->threshold_y_mT;
  }

  if (thr_mT > 0.0f)
  {
    uint16_t thr_px = (uint16_t)(((thr_mT / span_max) * (float)radius) + 0.5f);
    if (thr_px > radius)
    {
      thr_px = radius;
    }
    if (thr_px > 0U)
    {
      renderDrawCircle(cx, cy, thr_px, RENDER_LAYER_UI, RENDER_COLOR_BLACK, false, 1U);
    }
  }

  if ((live->deadzone_enabled != 0UL) && (live->deadzone_mT > 0.0f))
  {
    uint16_t dz_px = (uint16_t)(((live->deadzone_mT / span_max) * (float)radius) + 0.5f);
    if (dz_px > radius)
    {
      dz_px = radius;
    }
    if (dz_px > 0U)
    {
      renderDrawCircle(cx, cy, dz_px, RENDER_LAYER_UI, RENDER_COLOR_BLACK, false, 1U);
    }
  }

  {
    int16_t px = (int16_t)((int32_t)cx + (int32_t)(live->nx * (float)rx));
    int16_t py = (int16_t)((int32_t)cy - (int32_t)(live->ny * (float)ry));
    if (px < 0)
    {
      px = 0;
    }
    else if (px >= (int16_t)RENDER_WIDTH)
    {
      px = (int16_t)(RENDER_WIDTH - 1U);
    }
    if (py < 0)
    {
      py = 0;
    }
    else if (py >= (int16_t)RENDER_HEIGHT)
    {
      py = (int16_t)(RENDER_HEIGHT - 1U);
    }
    renderDrawCircle((uint16_t)px, (uint16_t)py, 2U, RENDER_LAYER_GAME, RENDER_COLOR_BLACK, true, 1U);
    renderDrawCircle(cx, cy, 1U, RENDER_LAYER_UI, RENDER_COLOR_BLACK, true, 1U);
  }

  if (state->joy_cal_status.save_pending != 0UL)
  {
    renderDrawText(4U, (uint16_t)(RENDER_HEIGHT - 20U), "SAVING...", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }
  else if (state->joy_cal_status.last_error != 0L)
  {
    renderDrawText(4U, (uint16_t)(RENDER_HEIGHT - 20U), "SAVE ERROR", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }
  else if ((state->joy_cal_status.stage == (ULONG)UI_JOY_CAL_STAGE_DONE) &&
           (quality->valid != 0UL) &&
           (quality->quality_ok == 0UL))
  {
    renderDrawText(4U, (uint16_t)(RENDER_HEIGHT - 20U), "WARN: CHECK CAL", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }

  if (state->joy_cal_status.stage == (ULONG)UI_JOY_CAL_STAGE_DONE)
  {
    if (s_done_recal_mode == 0U)
    {
      renderDrawText(4U, (uint16_t)(RENDER_HEIGHT - 8U), "A:SAVE  B:EXIT", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    }
    else
    {
      renderDrawText(4U, (uint16_t)(RENDER_HEIGHT - 8U), "A:RECAL B:EXIT", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    }
  }
  else
  {
    renderDrawText(4U, (uint16_t)(RENDER_HEIGHT - 8U), "B:EXIT", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }
  Render_MarkDirtyAll();
}

static const ui_page_vtable_t s_page_joy_target =
{
  .name = "joy_target",
  .on_enter = UiPageJoyTarget_Enter,
  .on_action = UiPageJoyTarget_Action,
  .on_tick = UiPageJoyTarget_Tick,
  .on_render = UiPageJoyTarget_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageJoyTarget_GetVTable(void)
{
  return &s_page_joy_target;
}

void UiPageJoyTarget_ResetDoneActions(void)
{
  s_done_recal_mode = 0U;
  s_done_recal_next_enter = 0U;
}
