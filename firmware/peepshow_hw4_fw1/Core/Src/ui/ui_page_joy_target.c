#include "ui/ui_page_native.h"
#include "app_threadx.h"
#include "ui/ui_page_joy_common.h"
#include "ui/ui_router.h"
#include "ui/ui_runtime_context.h"
#include "display_renderer.h"
#include "knobs_autogen.h"
#include "th_mode.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static ui_joy_live_status_t s_ui_joy_last_live;
static ULONG s_ui_joy_last_input_source = 0UL;
static ULONG s_ui_joy_last_input_action = 0UL;
static ULONG s_ui_joy_last_input_edge = 0UL;
static uint8_t s_ui_joy_last_valid = 0U;
static uint8_t s_ui_joy_deadzone_dirty = 0U;
static const ULONG k_ui_joy_deadzone_step_x10 = 2UL; /* 0.2 mT */

static th_mode_t UiPageJoyTarget_ModeFromFlags(ULONG mode_flags)
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

static float UiPageJoyAbs(float v)
{
  return (v < 0.0f) ? -v : v;
}

static uint8_t UiPageJoyFloatChanged(float a, float b, float eps)
{
  return (UiPageJoyAbs(a - b) > eps) ? 1U : 0U;
}

static void UiPageJoyDrawEllipse(uint16_t cx, uint16_t cy, uint16_t rx, uint16_t ry)
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

static uint8_t UiPageJoyTargetLiveChanged(const ui_router_state_t *state)
{
  const ui_joy_live_status_t *live;

  if (state == TX_NULL)
  {
    return 0U;
  }
  live = &state->joy_live;

  if (s_ui_joy_last_valid == 0U)
  {
    return 1U;
  }

  if ((live->dir != s_ui_joy_last_live.dir) ||
      (live->input_mask != s_ui_joy_last_live.input_mask) ||
      (live->deadzone_enabled != s_ui_joy_last_live.deadzone_enabled) ||
      (live->invert_x != s_ui_joy_last_live.invert_x) ||
      (live->invert_y != s_ui_joy_last_live.invert_y))
  {
    return 1U;
  }

  if ((state->last_input_source != s_ui_joy_last_input_source) ||
      (state->last_input_action != s_ui_joy_last_input_action) ||
      (state->last_input_edge != s_ui_joy_last_input_edge))
  {
    return 1U;
  }

  if (UiPageJoyFloatChanged(live->nx, s_ui_joy_last_live.nx, 0.015f) != 0U)
  {
    return 1U;
  }
  if (UiPageJoyFloatChanged(live->ny, s_ui_joy_last_live.ny, 0.015f) != 0U)
  {
    return 1U;
  }
  if (UiPageJoyFloatChanged(live->r_abs_mT, s_ui_joy_last_live.r_abs_mT, 0.2f) != 0U)
  {
    return 1U;
  }
  if (UiPageJoyFloatChanged(live->span_x_mT, s_ui_joy_last_live.span_x_mT, 0.05f) != 0U)
  {
    return 1U;
  }
  if (UiPageJoyFloatChanged(live->span_y_mT, s_ui_joy_last_live.span_y_mT, 0.05f) != 0U)
  {
    return 1U;
  }
  if (UiPageJoyFloatChanged(live->threshold_x_mT, s_ui_joy_last_live.threshold_x_mT, 0.05f) != 0U)
  {
    return 1U;
  }
  if (UiPageJoyFloatChanged(live->threshold_y_mT, s_ui_joy_last_live.threshold_y_mT, 0.05f) != 0U)
  {
    return 1U;
  }
  if (UiPageJoyFloatChanged(live->deadzone_mT, s_ui_joy_last_live.deadzone_mT, 0.05f) != 0U)
  {
    return 1U;
  }

  return 0U;
}

static ULONG UiPageJoyTargetDeadzoneClampX10(LONG value_x10)
{
  LONG min_x10 = (LONG)KNOB_SENSOR_JOY_NEUTRAL_DEADZONE_MIN_MT_X10;
  LONG max_x10 = (LONG)KNOB_SENSOR_JOY_NEUTRAL_DEADZONE_MAX_MT_X10;

  if (max_x10 < min_x10)
  {
    max_x10 = min_x10;
  }
  if (value_x10 < min_x10)
  {
    value_x10 = min_x10;
  }
  if (value_x10 > max_x10)
  {
    value_x10 = max_x10;
  }
  return (ULONG)value_x10;
}

static ULONG UiPageJoyTargetCurrentDeadzoneX10(const ui_router_state_t *state)
{
  LONG value_x10 = (LONG)((state->joy_live.deadzone_mT * 10.0f) + 0.5f);
  if (value_x10 < 0L)
  {
    value_x10 = 0L;
  }
  return UiPageJoyTargetDeadzoneClampX10(value_x10);
}

static uint8_t UiPageJoyTarget_InputPolicy(const ui_input_evt_t *evt)
{
  if (evt == TX_NULL)
  {
    return 0U;
  }

  if ((evt->evt == UI_EVT_LEFT) || (evt->evt == UI_EVT_RIGHT))
  {
    if ((evt->action == (uint32_t)UI_INPUT_ACTION_BTN_L) ||
        (evt->action == (uint32_t)UI_INPUT_ACTION_BTN_R))
    {
      return 1U;
    }
    return 0U;
  }

  return 1U;
}

static void UiPageJoyTarget_Enter(ui_router_t *ui, const void *arg)
{
  (void)ui;
  (void)arg;
  s_ui_joy_last_valid = 0U;
  (void)memset(&s_ui_joy_last_live, 0, sizeof(s_ui_joy_last_live));
  s_ui_joy_last_input_source = 0UL;
  s_ui_joy_last_input_action = 0UL;
  s_ui_joy_last_input_edge = 0UL;
  s_ui_joy_deadzone_dirty = 0U;
  UiPageJoyCommon_SetDoneRecalMode((UiPageJoyCommon_GetDoneRecalNextEnter() != 0U) ? 1U : 0U);
}

static uint32_t UiPageJoyTarget_Event(ui_router_t *ui, const ui_input_evt_t *evt)
{
  const ui_router_state_t *state = UiRuntimeContext_GetState();
  const ui_router_api_t *api = UiRuntimeContext_GetApi();
  uint32_t result = UI_EVT_RESULT_NONE;

  if ((ui == 0) || (evt == 0))
  {
    return result;
  }

  if (evt->evt == UI_EVT_SELECT)
  {
    if ((state != TX_NULL) &&
        (state->joy_cal_status.stage == (ULONG)UI_JOY_CAL_STAGE_DONE))
    {
      if (state->joy_cal_status.save_pending != 0UL)
      {
        return UI_EVT_RESULT_HANDLED;
      }

      if (UiPageJoyCommon_GetDoneRecalMode() == 0U)
      {
        if ((api != TX_NULL) && (api->joy_cal_save != TX_NULL))
        {
          (void)api->joy_cal_save();
        }
        UiPageJoyCommon_SetDoneRecalMode(1U);
        UiPageJoyCommon_SetDoneRecalNextEnter(1U);
        return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
      }

      if ((api != TX_NULL) && (api->joy_cal_start != TX_NULL))
      {
        (void)api->joy_cal_start();
      }
      UiPageJoyCommon_SetDoneRecalMode(0U);
      UiPageJoyCommon_SetDoneRecalNextEnter(0U);
      UiRouter_OpenPage(ui, &UI_PAGE_JOY_CAL_NATIVE, 0);
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
    }

    return UI_EVT_RESULT_HANDLED;
  }

  if ((evt->evt == UI_EVT_LEFT) || (evt->evt == UI_EVT_RIGHT))
  {
    if ((state != TX_NULL) &&
        (state->joy_cal_status.save_pending == 0UL))
    {
      LONG current_x10 = (LONG)UiPageJoyTargetCurrentDeadzoneX10(state);
      LONG next_x10 = current_x10;
      UINT status;

      if (evt->evt == UI_EVT_LEFT)
      {
        next_x10 -= (LONG)k_ui_joy_deadzone_step_x10;
      }
      else
      {
        next_x10 += (LONG)k_ui_joy_deadzone_step_x10;
      }
      next_x10 = (LONG)UiPageJoyTargetDeadzoneClampX10(next_x10);
      if (next_x10 != current_x10)
      {
        status = App_SensorReq_JoyDeadzoneSet((ULONG)next_x10);
        if (status == TX_SUCCESS)
        {
          s_ui_joy_deadzone_dirty = 1U;
          return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
        }
      }
    }
    return UI_EVT_RESULT_HANDLED;
  }

  if ((evt->evt == UI_EVT_BACK) || (evt->evt == UI_EVT_LONG_BACK))
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
      UiPageJoyCommon_SetDoneRecalMode(1U);
      UiPageJoyCommon_SetDoneRecalNextEnter(1U);
    }

    return UI_EVT_RESULT_HANDLED;
  }

  if (evt->evt != UI_EVT_TICK)
  {
    return UI_EVT_RESULT_NONE;
  }

  if ((state != TX_NULL) && (state->joy_cal_status.stage != (ULONG)UI_JOY_CAL_STAGE_DONE))
  {
    if ((UiPageJoyCommon_GetDoneRecalMode() != 0U) || (UiPageJoyCommon_GetDoneRecalNextEnter() != 0U))
    {
      UiPageJoyCommon_SetDoneRecalMode(0U);
      UiPageJoyCommon_SetDoneRecalNextEnter(0U);
      result |= UI_EVT_RESULT_DIRTY;
    }
  }

  if (UiPageJoyTargetLiveChanged(state) != 0U)
  {
    if (state != TX_NULL)
    {
      s_ui_joy_last_live = state->joy_live;
      s_ui_joy_last_input_source = state->last_input_source;
      s_ui_joy_last_input_action = state->last_input_action;
      s_ui_joy_last_input_edge = state->last_input_edge;
      s_ui_joy_last_valid = 1U;
    }
    result |= UI_EVT_RESULT_DIRTY;
  }

  return result;
}

static void UiPageJoyTarget_Render(ui_router_t *ui)
{
  const ui_router_state_t *state = UiRuntimeContext_GetState();
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

  (void)ui;

  if (state == TX_NULL)
  {
    return;
  }
  live = &state->joy_live;
  quality = &state->joy_cal_quality;

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(UiPageJoyTarget_ModeFromFlags(state->mode_flags));
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

  UiPageJoyDrawEllipse(cx, cy, rx, ry);

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
    char deadzone_line[24];
    ULONG dz_x10 = UiPageJoyTargetCurrentDeadzoneX10(state);
    (void)snprintf(deadzone_line, sizeof(deadzone_line), "DZ %lu.%01lu mT", dz_x10 / 10UL, dz_x10 % 10UL);
    renderDrawText(4U, (uint16_t)(RENDER_HEIGHT - 32U), deadzone_line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);

    if (UiPageJoyCommon_GetDoneRecalMode() == 0U)
    {
      renderDrawText(4U, (uint16_t)(RENDER_HEIGHT - 8U), "L/R:DZ A:SAVE B:EXIT", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    }
    else
    {
      renderDrawText(4U, (uint16_t)(RENDER_HEIGHT - 8U), "L/R:DZ A:RECAL B:EXIT", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    }
  }
  else
  {
    renderDrawText(4U, (uint16_t)(RENDER_HEIGHT - 8U), "B:EXIT", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }
  Render_MarkDirtyAll();
}

static void UiPageJoyTarget_Exit(ui_router_t *ui)
{
  (void)ui;
  if (s_ui_joy_deadzone_dirty != 0U)
  {
    (void)App_SensorReq_SettingsSave();
    s_ui_joy_deadzone_dirty = 0U;
  }
}

const ui_page_t UI_PAGE_JOY_TARGET_NATIVE =
{
  .name = "joy_target",
  .footer = UI_FOOTER_A_SELECT_B_BACK,
  .input_policy = UiPageJoyTarget_InputPolicy,
  .enter = UiPageJoyTarget_Enter,
  .event = UiPageJoyTarget_Event,
  .render = UiPageJoyTarget_Render,
  .exit = UiPageJoyTarget_Exit
};


