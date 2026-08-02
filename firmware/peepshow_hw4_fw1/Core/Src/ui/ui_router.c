#include "ui/ui_router.h"
#include "ui/pages/page_home.h"
#include "ui/pages/page_menu.h"
#include "ui/pages/page_menu_input_sub.h"
#include "ui/pages/page_menu_sensors_sub.h"
#include "ui/pages/page_menu_power_time_sub.h"
#include "ui/pages/page_menu_system_sub.h"
#include "ui/pages/page_joy_cal.h"
#include "ui/pages/page_batt_stats.h"
#include "ui/pages/page_communications.h"
#include "ui/pages/page_joy_cursor.h"
#include "ui/pages/page_joy_target.h"
#include "ui/pages/page_lis2.h"
#include "ui/pages/page_lis2_steps.h"
#include "ui/pages/page_menu_input.h"
#include "ui/pages/page_rtc_set.h"
#include "ui/pages/page_seed.h"
#include "ui/pages/page_sleep.h"
#include "ui/pages/page_sound.h"
#include "ui/pages/page_storage.h"
#include "ui/pages/page_display_stress.h"
#include "display_renderer.h"
#include <string.h>

typedef struct
{
  ui_router_api_t api;
  ui_router_state_t state;
  ui_router_state_t prev_state;
  uint8_t state_valid;
  ui_page_id_t current_page;
  uint8_t dirty;
} ui_router_ctx_t;

static ui_router_ctx_t s_ui;

static const char *UiRouter_InputSourceName(ULONG source)
{
  switch (source)
  {
    case 1UL:
      return "BTN_A";
    case 2UL:
      return "BTN_B";
    case 3UL:
      return "BTN_L";
    case 4UL:
      return "BTN_R";
    case 5UL:
      return "BOOT";
    case 6UL:
      return "JOY_UP";
    case 7UL:
      return "JOY_RIGHT";
    case 8UL:
      return "JOY_DOWN";
    case 9UL:
      return "JOY_LEFT";
    default:
      return "-";
  }
}

static const char *UiRouter_InputActionName(ULONG action)
{
  switch (action)
  {
    case UI_ACTION_CONFIRM:
      return "CONFIRM";
    case UI_ACTION_CANCEL:
      return "CANCEL";
    case UI_ACTION_LEFT:
      return "LEFT";
    case UI_ACTION_RIGHT:
      return "RIGHT";
    case UI_ACTION_MENU:
      return "MENU";
    case UI_ACTION_UP:
      return "UP";
    case UI_ACTION_DOWN:
      return "DOWN";
    case UI_ACTION_NONE:
    default:
      return "-";
  }
}

static const char *UiRouter_InputEdgeName(ULONG edge)
{
  switch (edge)
  {
    case UI_EVENT_PRESS:
      return "PRESS";
    case UI_EVENT_RELEASE:
      return "RELEASE";
    case UI_EVENT_REPEAT:
      return "REPEAT";
    case UI_EVENT_LONG:
      return "LONG";
    default:
      return "-";
  }
}

static const ui_page_vtable_t *UiRouter_PageVTable(ui_page_id_t page)
{
  switch (page)
  {
    case UI_PAGE_MENU:
      return UiPageMenu_GetVTable();

    case UI_PAGE_JOY_CAL:
      return UiPageJoyCal_GetVTable();

    case UI_PAGE_MENU_INPUT_SUB:
      return UiPageMenuInputSub_GetVTable();

    case UI_PAGE_MENU_SENSORS_SUB:
      return UiPageMenuSensorsSub_GetVTable();

    case UI_PAGE_MENU_POWER_TIME_SUB:
      return UiPageMenuPowerTimeSub_GetVTable();

    case UI_PAGE_MENU_SYSTEM_SUB:
      return UiPageMenuSystemSub_GetVTable();

    case UI_PAGE_BATT_STATS:
      return UiPageBattStats_GetVTable();

    case UI_PAGE_COMMUNICATIONS:
      return UiPageCommunications_GetVTable();

    case UI_PAGE_JOY_CURSOR:
      return UiPageJoyCursor_GetVTable();

    case UI_PAGE_JOY_TARGET:
      return UiPageJoyTarget_GetVTable();

    case UI_PAGE_LIS2:
      return UiPageLis2_GetVTable();

    case UI_PAGE_LIS2_STEPS:
      return UiPageLis2Steps_GetVTable();

    case UI_PAGE_MENU_INPUT:
      return UiPageMenuInput_GetVTable();

    case UI_PAGE_RTC_SET:
      return UiPageRtcSet_GetVTable();

    case UI_PAGE_SEED:
      return UiPageSeed_GetVTable();

    case UI_PAGE_SLEEP:
      return UiPageSleep_GetVTable();

    case UI_PAGE_SOUND:
      return UiPageSound_GetVTable();

    case UI_PAGE_STORAGE:
      return UiPageStorage_GetVTable();

    case UI_PAGE_DISPLAY_STRESS:
      return UiPageDisplayStress_GetVTable();

    case UI_PAGE_HOME:
    default:
      return UiPageHome_GetVTable();
  }
}

void UiRouter_Init(const ui_router_api_t *api)
{
  const ui_page_vtable_t *page;

  (void)memset(&s_ui, 0, sizeof(s_ui));
  if (api != TX_NULL)
  {
    s_ui.api = *api;
  }

  s_ui.current_page = UI_PAGE_HOME;
  s_ui.dirty = 1U;
  page = UiRouter_PageVTable(s_ui.current_page);
  if ((page != TX_NULL) && (page->on_enter != TX_NULL))
  {
    page->on_enter();
  }
}

void UiRouter_UpdateState(const ui_router_state_t *state)
{
  ui_router_state_t prev_cmp;
  ui_router_state_t new_cmp;

  if (state == TX_NULL)
  {
    return;
  }

  s_ui.state = *state;
  prev_cmp = s_ui.prev_state;
  new_cmp = s_ui.state;
  /* Joy telemetry is high-rate; page-level tick logic decides when to redraw it. */
  prev_cmp.joy_live = new_cmp.joy_live;
  prev_cmp.pmic_live = new_cmp.pmic_live;
  prev_cmp.lis_live = new_cmp.lis_live;

  if ((s_ui.state_valid == 0U) ||
      (memcmp(&prev_cmp, &new_cmp, sizeof(new_cmp)) != 0))
  {
    s_ui.prev_state = s_ui.state;
    s_ui.state_valid = 1U;
    s_ui.dirty = 1U;
  }
}

void UiRouter_Tick(void)
{
  const ui_page_vtable_t *page = UiRouter_PageVTable(s_ui.current_page);
  if ((page != TX_NULL) && (page->on_tick != TX_NULL))
  {
    page->on_tick();
  }
}

uint8_t UiRouter_HandleAction(const ui_action_evt_t *evt)
{
  const ui_page_vtable_t *page = UiRouter_PageVTable(s_ui.current_page);
  if ((page == TX_NULL) || (page->on_action == TX_NULL))
  {
    return 0U;
  }
  return page->on_action(evt);
}

void UiRouter_Render(void)
{
  const ui_page_vtable_t *page = UiRouter_PageVTable(s_ui.current_page);
  if ((page != TX_NULL) && (page->on_render != TX_NULL))
  {
    page->on_render();
  }
}

void UiRouter_RequestPage(ui_page_id_t page)
{
  const ui_page_vtable_t *old_page;
  const ui_page_vtable_t *new_page;

  if (page >= UI_PAGE_COUNT)
  {
    return;
  }

  if (page == s_ui.current_page)
  {
    s_ui.dirty = 1U;
    return;
  }

  old_page = UiRouter_PageVTable(s_ui.current_page);
  if ((old_page != TX_NULL) && (old_page->on_exit != TX_NULL))
  {
    old_page->on_exit();
  }

  s_ui.current_page = page;
  new_page = UiRouter_PageVTable(s_ui.current_page);
  if ((new_page != TX_NULL) && (new_page->on_enter != TX_NULL))
  {
    new_page->on_enter();
  }
  s_ui.dirty = 1U;
}

ui_page_id_t UiRouter_GetCurrentPage(void)
{
  return s_ui.current_page;
}

uint8_t UiRouter_IsDirty(void)
{
  return s_ui.dirty;
}

void UiRouter_ClearDirty(void)
{
  s_ui.dirty = 0U;
}

void UiRouter_MarkDirty(void)
{
  s_ui.dirty = 1U;
}

const ui_router_state_t *UiRouter_GetState(void)
{
  return &s_ui.state;
}

const ui_router_api_t *UiRouter_GetApi(void)
{
  return &s_ui.api;
}

void UiRouter_RenderFooterHints(const char *line1, const char *line2)
{
  uint16_t y = (uint16_t)(RENDER_HEIGHT - 20U);

  if (line1 != TX_NULL)
  {
    renderDrawText(4U, y, line1, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }

  if (line2 != TX_NULL)
  {
    y = (uint16_t)(RENDER_HEIGHT - 10U);
    renderDrawText(4U, y, line2, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }
}

void UiRouter_RenderInputMonitor(uint16_t x, uint16_t y)
{
  renderDrawText(x, y, "SRC:", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  renderDrawText((uint16_t)(x + 28U), y, UiRouter_InputSourceName(s_ui.state.last_input_source), RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);
  renderDrawText(x, y, "ACT:", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  renderDrawText((uint16_t)(x + 28U), y, UiRouter_InputActionName(s_ui.state.last_input_action), RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);
  renderDrawText(x, y, "EDG:", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  renderDrawText((uint16_t)(x + 28U), y, UiRouter_InputEdgeName(s_ui.state.last_input_edge), RENDER_LAYER_UI, RENDER_COLOR_BLACK);
}
