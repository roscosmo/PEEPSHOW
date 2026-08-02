#include "ui/pages/page_lis2_steps.h"
#include "app_threadx.h"
#include "display_renderer.h"
#include "th_mode.h"
#include <string.h>

static ui_lis_live_status_t s_last_lis;
static uint8_t s_last_valid = 0U;
static uint8_t s_step_enable_selected = 1U;

static th_mode_t UiPageLis2Steps_ModeFromFlags(ULONG mode_flags)
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

static const char *UiPageLis2Steps_FsmLabel(ULONG state)
{
  switch (state)
  {
    case 0UL:
      return "OFF";
    case 1UL:
      return "INIT";
    case 2UL:
      return "READY";
    case 3UL:
      return "FAULT";
    case 4UL:
      return "RECV";
    case 5UL:
      return "SUSP";
    default:
      return "UNK";
  }
}

static char *UiPageLis2Steps_AppendU32(char *dst, ULONG v)
{
  char tmp[11];
  UINT n = 0U;

  do
  {
    tmp[n++] = (char)('0' + (v % 10UL));
    v /= 10UL;
  } while (v != 0UL);

  while (n > 0U)
  {
    *dst++ = tmp[--n];
  }
  *dst = '\0';
  return dst;
}

static char *UiPageLis2Steps_AppendS32(char *dst, LONG v)
{
  ULONG mag;

  if (v < 0L)
  {
    *dst++ = '-';
    mag = ((ULONG)(-(v + 1L))) + 1UL;
  }
  else
  {
    mag = (ULONG)v;
  }

  return UiPageLis2Steps_AppendU32(dst, mag);
}

static void UiPageLis2Steps_Enter(void)
{
  s_last_valid = 0U;
  (void)memset(&s_last_lis, 0, sizeof(s_last_lis));
  s_step_enable_selected = 1U;
  {
    const ui_router_state_t *state = UiRouter_GetState();
    if ((state != TX_NULL) && (state->lis_live.step_enabled == 0UL))
    {
      s_step_enable_selected = 0U;
    }
  }
  UiRouter_MarkDirty();
}

static uint8_t UiPageLis2Steps_Action(const ui_action_evt_t *evt)
{
  if (evt == TX_NULL)
  {
    return 0U;
  }

  if ((evt->action == UI_ACTION_CONFIRM) && (evt->event == UI_EVENT_PRESS))
  {
    if (s_step_enable_selected != 0U)
    {
      (void)App_SensorReq_LisStepEnable();
    }
    else
    {
      (void)App_SensorReq_LisStepDisable();
    }
    (void)App_SensorReq_Poll(APP_SENSOR_TARGET_LIS);
    UiRouter_MarkDirty();
    return 1U;
  }

  if (((evt->action == UI_ACTION_LEFT) || (evt->action == UI_ACTION_RIGHT)) &&
      (evt->event == UI_EVENT_PRESS))
  {
    if (evt->action == UI_ACTION_LEFT)
    {
      s_step_enable_selected = 0U;
    }
    else
    {
      s_step_enable_selected = 1U;
    }
    (void)App_SensorReq_Poll(APP_SENSOR_TARGET_LIS);
    UiRouter_MarkDirty();
    return 1U;
  }

  if ((evt->action == UI_ACTION_CANCEL) && (evt->event == UI_EVENT_PRESS))
  {
    UiRouter_RequestPage(UI_PAGE_MENU_SENSORS_SUB);
    return 1U;
  }

  return 0U;
}

static void UiPageLis2Steps_Tick(void)
{
  const ui_router_state_t *state = UiRouter_GetState();

  if (state == TX_NULL)
  {
    return;
  }

  if ((s_last_valid == 0U) ||
      (memcmp(&s_last_lis, &state->lis_live, sizeof(s_last_lis)) != 0))
  {
    s_last_lis = state->lis_live;
    s_last_valid = 1U;
    if (state->lis_live.step_enabled != 0UL)
    {
      s_step_enable_selected = 1U;
    }
    UiRouter_MarkDirty();
  }
}

static void UiPageLis2Steps_Render(void)
{
  const ui_router_state_t *state = UiRouter_GetState();
  const ui_lis_live_status_t *lis;
  char line[48];
  uint16_t y = 22U;

  if (state == TX_NULL)
  {
    return;
  }
  lis = &state->lis_live;

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(UiPageLis2Steps_ModeFromFlags(state->mode_flags));
  renderDrawText(4U, 8U, "LIS2 STEPS", RENDER_LAYER_UI, RENDER_COLOR_BLACK);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "EN:");
  (void)strcat(line, (lis->step_enabled != 0UL) ? "ON" : "OFF");
  (void)strcat(line, " CNT:");
  (void)UiPageLis2Steps_AppendU32(line + strlen(line), lis->step_count);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "STEP:");
  (void)strcat(line, (lis->step_detected != 0UL) ? "1" : "0");
  (void)strcat(line, " TILT:");
  (void)strcat(line, (lis->tilt_detected != 0UL) ? "1" : "0");
  (void)strcat(line, " SIG:");
  (void)strcat(line, (lis->sigmot_detected != 0UL) ? "1" : "0");
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "FSM:");
  (void)strcat(line, UiPageLis2Steps_FsmLabel(lis->fsm_state));
  (void)strcat(line, " STR:");
  (void)strcat(line, (lis->stream_enabled != 0UL) ? "ON" : "OFF");
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "SMP:");
  (void)UiPageLis2Steps_AppendU32(line + strlen(line), lis->sample_count);
  (void)strcat(line, " T:");
  (void)UiPageLis2Steps_AppendU32(line + strlen(line), lis->last_sample_tick);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "SEL:");
  (void)strcat(line, (s_step_enable_selected != 0U) ? "ON" : "OFF");
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "ERR:");
  (void)UiPageLis2Steps_AppendS32(line + strlen(line), lis->last_error);
  (void)strcat(line, " F:");
  (void)UiPageLis2Steps_AppendU32(line + strlen(line), lis->fail_count);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);

  UiRouter_RenderFooterHints("L:OFF R:ON A:APPLY", "B: BACK");
  Render_MarkDirtyAll();
}

static const ui_page_vtable_t s_page_lis2_steps =
{
  .name = "lis2_steps",
  .on_enter = UiPageLis2Steps_Enter,
  .on_action = UiPageLis2Steps_Action,
  .on_tick = UiPageLis2Steps_Tick,
  .on_render = UiPageLis2Steps_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageLis2Steps_GetVTable(void)
{
  return &s_page_lis2_steps;
}
