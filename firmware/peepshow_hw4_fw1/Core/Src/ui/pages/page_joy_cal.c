#include "ui/pages/page_joy_cal.h"
#include "ui/pages/page_joy_target.h"
#include "display_renderer.h"
#include "th_mode.h"
#include <string.h>

static ui_joy_cal_status_t s_last_status;
static ULONG s_last_active = 0UL;
static uint8_t s_last_valid = 0U;

static th_mode_t UiJoyCalModeFromFlags(ULONG mode_flags)
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

static void UiPageJoyCal_Enter(void)
{
  s_last_valid = 0U;
  s_last_active = 0UL;
  (void)memset(&s_last_status, 0, sizeof(s_last_status));
  UiRouter_MarkDirty();
}

static uint8_t UiPageJoyCal_Action(const ui_action_evt_t *evt)
{
  const ui_router_state_t *state = UiRouter_GetState();
  const ui_router_api_t *api = UiRouter_GetApi();

  if ((evt == TX_NULL) || (state == TX_NULL))
  {
    return 0U;
  }

  if (evt->action == UI_ACTION_CONFIRM)
  {
    if (evt->event == UI_EVENT_PRESS)
    {
      UiPageJoyTarget_ResetDoneActions();
      if ((api != TX_NULL) && (api->joy_cal_start != TX_NULL))
      {
        (void)api->joy_cal_start();
      }
    }
    return 1U;
  }

  if ((evt->action == UI_ACTION_CANCEL) && (evt->event == UI_EVENT_PRESS))
  {
    if ((api != TX_NULL) && (api->joy_cal_cancel != TX_NULL))
    {
      (void)api->joy_cal_cancel();
    }
    UiRouter_RequestPage(UI_PAGE_MENU_INPUT_SUB);
    return 1U;
  }

  return 0U;
}

static void UiPageJoyCal_Tick(void)
{
  const ui_router_state_t *state = UiRouter_GetState();
  ULONG current_stage;
  ULONG prev_stage = (ULONG)UI_JOY_CAL_STAGE_IDLE;
  uint8_t had_valid = s_last_valid;

  if (state == TX_NULL)
  {
    return;
  }

  current_stage = state->joy_cal_status.stage;
  if (s_last_valid != 0U)
  {
    prev_stage = s_last_status.stage;
  }

  if ((s_last_valid == 0U) ||
      (s_last_active != state->joy_cal_active) ||
      (memcmp(&s_last_status, &state->joy_cal_status, sizeof(s_last_status)) != 0))
  {
    s_last_status = state->joy_cal_status;
    s_last_active = state->joy_cal_active;
    s_last_valid = 1U;
    UiRouter_MarkDirty();
  }

  if ((had_valid != 0U) &&
      (current_stage == (ULONG)UI_JOY_CAL_STAGE_DONE) &&
      (prev_stage != (ULONG)UI_JOY_CAL_STAGE_DONE))
  {
    UiRouter_RequestPage(UI_PAGE_JOY_TARGET);
  }
}

static void UiPageJoyCal_Render(void)
{
  const ui_router_state_t *state = UiRouter_GetState();
  const ui_joy_cal_status_t *status;
  const char *line1 = "A: START CAL";
  const char *line2 = "B: BACK";
  uint16_t y = 34U;

  if (state == TX_NULL)
  {
    return;
  }
  status = &state->joy_cal_status;

  switch ((ui_joy_cal_stage_t)status->stage)
  {
    case UI_JOY_CAL_STAGE_NEUTRAL:
      line1 = "HOLD NEUTRAL";
      line2 = "MEASURING...";
      break;
    case UI_JOY_CAL_STAGE_UP:
      line1 = "HOLD UP";
      line2 = "PRESS A TO CAPTURE";
      break;
    case UI_JOY_CAL_STAGE_RIGHT:
      line1 = "HOLD RIGHT";
      line2 = "PRESS A TO CAPTURE";
      break;
    case UI_JOY_CAL_STAGE_DOWN:
      line1 = "HOLD DOWN";
      line2 = "PRESS A TO CAPTURE";
      break;
    case UI_JOY_CAL_STAGE_LEFT:
      line1 = "HOLD LEFT";
      line2 = "PRESS A TO CAPTURE";
      break;
    case UI_JOY_CAL_STAGE_SWEEP:
      line1 = "SWEEP FULL RANGE";
      line2 = "BIG CIRCLES";
      break;
    case UI_JOY_CAL_STAGE_DONE:
      line1 = "A: RECAL";
      line2 = "B: BACK";
      break;
    case UI_JOY_CAL_STAGE_ERROR:
      line1 = "CAL ERROR";
      line2 = "A: RETRY";
      break;
    case UI_JOY_CAL_STAGE_IDLE:
    default:
      break;
  }

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(UiJoyCalModeFromFlags(state->mode_flags));
  renderDrawText(4U, 8U, "JOYSTICK CAL", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  renderDrawText(4U, y, line1, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 12U);
  renderDrawText(4U, y, line2, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 14U);

  if (((ui_joy_cal_stage_t)status->stage == UI_JOY_CAL_STAGE_NEUTRAL) ||
      ((ui_joy_cal_stage_t)status->stage == UI_JOY_CAL_STAGE_UP) ||
      ((ui_joy_cal_stage_t)status->stage == UI_JOY_CAL_STAGE_RIGHT) ||
      ((ui_joy_cal_stage_t)status->stage == UI_JOY_CAL_STAGE_DOWN) ||
      ((ui_joy_cal_stage_t)status->stage == UI_JOY_CAL_STAGE_LEFT) ||
      ((ui_joy_cal_stage_t)status->stage == UI_JOY_CAL_STAGE_SWEEP))
  {
    float p01 = status->progress;
    uint16_t bar_x = 8U;
    uint16_t bar_y = y;
    uint16_t bar_w = (RENDER_WIDTH > 16U) ? (uint16_t)(RENDER_WIDTH - 16U) : 8U;
    uint16_t fill_w;

    if (p01 < 0.0f)
    {
      p01 = 0.0f;
    }
    if (p01 > 1.0f)
    {
      p01 = 1.0f;
    }

    renderDrawRectOutline(bar_x, bar_y, bar_w, 10U, RENDER_LAYER_UI, RENDER_COLOR_BLACK, 1U);
    fill_w = (uint16_t)((float)(bar_w - 2U) * p01);
    if (fill_w > 0U)
    {
      renderFillRect((uint16_t)(bar_x + 1U), (uint16_t)(bar_y + 1U), fill_w, 8U, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    }
    y = (uint16_t)(y + 14U);
  }

  if (status->save_pending != 0UL)
  {
    renderDrawText(4U, y, "SAVING...", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    y = (uint16_t)(y + 12U);
  }

  if (status->last_error != 0L)
  {
    renderDrawText(4U, y, "LAST ERROR", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }

  if ((ui_joy_cal_stage_t)status->stage == UI_JOY_CAL_STAGE_DONE)
  {
    UiRouter_RenderFooterHints("A:RECAL", "B: BACK");
  }
  else if ((ui_joy_cal_stage_t)status->stage == UI_JOY_CAL_STAGE_ERROR)
  {
    UiRouter_RenderFooterHints("A:RETRY", "B: BACK");
  }
  else
  {
    UiRouter_RenderFooterHints("A:START", "B: BACK");
  }
  Render_MarkDirtyAll();
}

static const ui_page_vtable_t s_page_joy_cal =
{
  .name = "joy_cal",
  .on_enter = UiPageJoyCal_Enter,
  .on_action = UiPageJoyCal_Action,
  .on_tick = UiPageJoyCal_Tick,
  .on_render = UiPageJoyCal_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageJoyCal_GetVTable(void)
{
  return &s_page_joy_cal;
}
