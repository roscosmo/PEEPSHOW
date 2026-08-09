#include "ui/ui_page_native.h"
#include "app_threadx.h"
#include "ui/ui_runtime_context.h"
#include "display_renderer.h"
#include "th_mode.h"
#include <string.h>

static ui_pmic_live_status_t s_ui_batt_last_pmic;
static uint8_t s_ui_batt_last_valid = 0U;

static th_mode_t UiPageBattStats_ModeFromFlags(ULONG mode_flags)
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

static const char *UiPageBattStats_FsmLabel(ULONG state)
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

static const char *UiPageBattStats_ChargeStateLabel(ULONG state)
{
  switch (state)
  {
    case 0UL:
      return "OFF";
    case 1UL:
      return "TRICKLE";
    case 2UL:
      return "FAST_CC";
    case 3UL:
      return "FAST_CV";
    case 4UL:
      return "DONE";
    case 5UL:
      return "LDO";
    case 6UL:
      return "TIMER";
    case 7UL:
      return "DETECT";
    default:
      return "UNK";
  }
}

static const char *UiPageBattStats_BatteryHealthLabel(ULONG state)
{
  switch (state)
  {
    case 0UL:
      return "UNKNOWN";
    case 1UL:
      return "OK";
    case 2UL:
      return "WARN";
    case 3UL:
      return "CRIT";
    default:
      return "UNK";
  }
}

static char *UiPageBattStats_AppendU32(char *dst, ULONG v)
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

static char *UiPageBattStats_AppendS32(char *dst, LONG v)
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

  return UiPageBattStats_AppendU32(dst, mag);
}

static char UiPageBattStats_HexNibble(uint8_t nibble)
{
  static const char k_hex[] = "0123456789ABCDEF";
  return k_hex[nibble & 0x0FU];
}

static void UiPageBattStats_AppendHex8(char *dst, ULONG v)
{
  uint8_t b = (uint8_t)(v & 0xFFUL);
  uint8_t hi = (uint8_t)((b >> 4U) & 0x0FU);
  uint8_t lo = (uint8_t)(b & 0x0FU);

  dst[0] = '0';
  dst[1] = 'x';
  dst[2] = UiPageBattStats_HexNibble(hi);
  dst[3] = UiPageBattStats_HexNibble(lo);
  dst[4] = '\0';
}

static void UiPageBattStats_Enter(ui_router_t *ui, const void *arg)
{
  (void)ui;
  (void)arg;
  s_ui_batt_last_valid = 0U;
  (void)memset(&s_ui_batt_last_pmic, 0, sizeof(s_ui_batt_last_pmic));
}

static uint32_t UiPageBattStats_Event(ui_router_t *ui, const ui_input_evt_t *evt)
{
  const ui_router_state_t *state;

  (void)ui;

  if (evt == 0)
  {
    return UI_EVT_RESULT_NONE;
  }

  if (evt->evt == UI_EVT_SELECT)
  {
    (void)App_SensorReq_Poll(APP_SENSOR_TARGET_PMIC);
    return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
  }

  if (evt->evt != UI_EVT_TICK)
  {
    return UI_EVT_RESULT_NONE;
  }

  state = UiRuntimeContext_GetState();
  if (state == TX_NULL)
  {
    return UI_EVT_RESULT_NONE;
  }

  if ((s_ui_batt_last_valid == 0U) ||
      (memcmp(&s_ui_batt_last_pmic, &state->pmic_live, sizeof(s_ui_batt_last_pmic)) != 0))
  {
    s_ui_batt_last_pmic = state->pmic_live;
    s_ui_batt_last_valid = 1U;
    return UI_EVT_RESULT_DIRTY;
  }

  return UI_EVT_RESULT_NONE;
}

static void UiPageBattStats_Render(ui_router_t *ui)
{
  const ui_router_state_t *state = UiRuntimeContext_GetState();
  const ui_pmic_live_status_t *pmic;
  char line[48];
  char hex[5];
  uint16_t y = 22U;

  (void)ui;

  if (state == TX_NULL)
  {
    return;
  }
  pmic = &state->pmic_live;

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(UiPageBattStats_ModeFromFlags(state->mode_flags));
  renderDrawText(4U, 8U, "BATT STATS", RENDER_LAYER_UI, RENDER_COLOR_BLACK);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "FSM:");
  (void)strcat(line, UiPageBattStats_FsmLabel(pmic->fsm_state));
  (void)strcat(line, " CHG:");
  (void)strcat(line, (pmic->charging_active != 0UL) ? "ON" : "OFF");
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "STATE:");
  (void)strcat(line, UiPageBattStats_ChargeStateLabel(pmic->charger_state));
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "SOC:");
  (void)UiPageBattStats_AppendU32(line + strlen(line), pmic->battery_soc_percent);
  (void)strcat(line, "% RAW:");
  (void)UiPageBattStats_AppendU32(line + strlen(line), pmic->battery_soc_raw);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "VBAT:");
  (void)UiPageBattStats_AppendU32(line + strlen(line), pmic->vbat_mV);
  (void)strcat(line, "mV");
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  UiPageBattStats_AppendHex8(hex, pmic->battery_health_reason);
  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "BAT:");
  (void)strcat(line, UiPageBattStats_BatteryHealthLabel(pmic->battery_health_state));
  (void)strcat(line, " R:");
  (void)strcat(line, hex);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "CFG_EN:");
  (void)UiPageBattStats_AppendU32(line + strlen(line), pmic->charging_enabled_cfg);
  (void)strcat(line, " UV:");
  (void)UiPageBattStats_AppendU32(line + strlen(line), pmic->battery_uv);
  (void)strcat(line, " OV:");
  (void)UiPageBattStats_AppendU32(line + strlen(line), pmic->battery_ov);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  UiPageBattStats_AppendHex8(hex, pmic->fault_raw);
  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "FLT:");
  (void)strcat(line, hex);
  (void)strcat(line, " ERR:");
  (void)UiPageBattStats_AppendS32(line + strlen(line), pmic->last_error);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);

  (void)memset(line, 0, sizeof(line));
  (void)strcpy(line, "SMP:");
  (void)UiPageBattStats_AppendU32(line + strlen(line), pmic->sample_count);
  (void)strcat(line, " T:");
  (void)UiPageBattStats_AppendU32(line + strlen(line), pmic->last_sample_tick);
  renderDrawText(4U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);

  UiRuntimeContext_RenderFooterHints("A:REFRESH SNAPSHOT", "B: BACK");
  Render_MarkDirtyAll();
}

static void UiPageBattStats_Exit(ui_router_t *ui)
{
  (void)ui;
}

const ui_page_t UI_PAGE_BATT_STATS_NATIVE =
{
  .name = "batt_stats",
  .footer = UI_FOOTER_A_SELECT_B_BACK,
  .enter = UiPageBattStats_Enter,
  .event = UiPageBattStats_Event,
  .render = UiPageBattStats_Render,
  .exit = UiPageBattStats_Exit
};


