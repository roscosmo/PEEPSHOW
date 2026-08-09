#include "ui/ui_page_native.h"
#include "app_threadx.h"
#include "ui/ui_runtime_context.h"
#include "display_renderer.h"
#include "th_mode.h"
#include <string.h>

static ui_lis_live_status_t s_ui_lis_last;
static uint8_t s_ui_lis_last_valid = 0U;

static th_mode_t UiPageLis2_ModeFromFlags(ULONG mode_flags)
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

static const char *UiPageLis2_FsmLabel(ULONG state)
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

static const char *UiPageLis2_ProfileLabel(ULONG profile)
{
  return (profile == (ULONG)APP_SENSOR_LIS_PROFILE_LIVE) ? "LIVE" : "LOW";
}

static char *UiPageLis2_AppendU32(char *dst, ULONG v)
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

static char *UiPageLis2_AppendS32(char *dst, LONG v)
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

  return UiPageLis2_AppendU32(dst, mag);
}

static char UiPageLis2_HexNibble(uint8_t nibble)
{
  static const char k_hex[] = "0123456789ABCDEF";
  return k_hex[nibble & 0x0FU];
}

static void UiPageLis2_AppendHex8(char *dst, ULONG v)
{
  uint8_t b = (uint8_t)(v & 0xFFUL);
  uint8_t hi = (uint8_t)((b >> 4U) & 0x0FU);
  uint8_t lo = (uint8_t)(b & 0x0FU);

  dst[0] = '0';
  dst[1] = 'x';
  dst[2] = UiPageLis2_HexNibble(hi);
  dst[3] = UiPageLis2_HexNibble(lo);
  dst[4] = '\0';
}

static void UiPageLis2_Enter(ui_router_t *ui, const void *arg)
{
  (void)ui;
  (void)arg;
  s_ui_lis_last_valid = 0U;
  (void)memset(&s_ui_lis_last, 0, sizeof(s_ui_lis_last));
}

static uint32_t UiPageLis2_Event(ui_router_t *ui, const ui_input_evt_t *evt)
{
  const ui_router_state_t *state = UiRuntimeContext_GetState();

  (void)ui;

  if ((evt == 0) || (state == TX_NULL))
  {
    return UI_EVT_RESULT_NONE;
  }

  if (evt->evt == UI_EVT_SELECT)
  {
    if (state->lis_live.stream_enabled != 0UL)
    {
      (void)App_SensorReq_LisStreamStop();
    }
    else
    {
      (void)App_SensorReq_LisSetLowPower();
      (void)App_SensorReq_LisStreamStart();
      (void)App_SensorReq_Poll(APP_SENSOR_TARGET_LIS);
    }
    return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
  }

  if ((evt->evt == UI_EVT_LEFT) || (evt->evt == UI_EVT_RIGHT))
  {
    (void)App_SensorReq_Poll(APP_SENSOR_TARGET_LIS);
    return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
  }

  if (evt->evt != UI_EVT_TICK)
  {
    return UI_EVT_RESULT_NONE;
  }

  if ((s_ui_lis_last_valid == 0U) ||
      (memcmp(&s_ui_lis_last, &state->lis_live, sizeof(s_ui_lis_last)) != 0))
  {
    s_ui_lis_last = state->lis_live;
    s_ui_lis_last_valid = 1U;
    return UI_EVT_RESULT_DIRTY;
  }

  return UI_EVT_RESULT_NONE;
}

static void UiPageLis2_Render(ui_router_t *ui)
{
  const ui_router_state_t *state = UiRuntimeContext_GetState();
  const ui_lis_live_status_t *lis;
  char line[48];
  char hex_a[5];
  char hex_b[5];
  uint16_t y = 22U;

  (void)ui;

  if (state == TX_NULL)
  {
    return;
  }
  lis = &state->lis_live;

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(UiPageLis2_ModeFromFlags(state->mode_flags));
  renderDrawText(4U, 8U, "LIS2", RENDER_LAYER_UI, RENDER_COLOR_BLACK);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "FSM:");
  (void)strcat(line, UiPageLis2_FsmLabel(lis->fsm_state));
  (void)strcat(line, " STR:");
  (void)strcat(line, (lis->stream_enabled != 0UL) ? "ON" : "OFF");
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "REQ:");
  (void)strcat(line, UiPageLis2_ProfileLabel(lis->profile_requested));
  (void)strcat(line, " AP:");
  (void)strcat(line, UiPageLis2_ProfileLabel(lis->profile_applied));
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  UiPageLis2_AppendHex8(hex_a, lis->whoami);
  UiPageLis2_AppendHex8(hex_b, lis->addr);
  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "WHO:");
  (void)strcat(line, hex_a);
  (void)strcat(line, " ADR:");
  (void)strcat(line, hex_b);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  UiPageLis2_AppendHex8(hex_a, lis->status);
  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "ST:");
  (void)strcat(line, hex_a);
  (void)strcat(line, " S:");
  (void)UiPageLis2_AppendU32(line + strlen(line), lis->sample_count);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "X:");
  (void)UiPageLis2_AppendS32(line + strlen(line), lis->x_raw);
  (void)strcat(line, " Y:");
  (void)UiPageLis2_AppendS32(line + strlen(line), lis->y_raw);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "Z:");
  (void)UiPageLis2_AppendS32(line + strlen(line), lis->z_raw);
  (void)strcat(line, " LF:");
  (void)UiPageLis2_AppendU32(line + strlen(line), lis->fail_count);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "LE:");
  (void)UiPageLis2_AppendS32(line + strlen(line), lis->last_error);
  (void)strcat(line, " FE:");
  (void)UiPageLis2_AppendS32(line + strlen(line), lis->fsm_last_error);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "T:");
  (void)UiPageLis2_AppendU32(line + strlen(line), lis->last_sample_tick);
  (void)strcat(line, " R:");
  (void)UiPageLis2_AppendU32(line + strlen(line), lis->fsm_recovery_attempts);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);

  UiRuntimeContext_RenderFooterHints("A:STREAM  L/R:POLL", "B: BACK");
  Render_MarkDirtyAll();
}

static void UiPageLis2_Exit(ui_router_t *ui)
{
  (void)ui;
}

const ui_page_t UI_PAGE_LIS2_NATIVE =
{
  .name = "lis2",
  .footer = UI_FOOTER_A_SELECT_B_BACK,
  .enter = UiPageLis2_Enter,
  .event = UiPageLis2_Event,
  .render = UiPageLis2_Render,
  .exit = UiPageLis2_Exit
};


