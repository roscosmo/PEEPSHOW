#include "ui/pages/page_home.h"
#include "ui/ui_menu_tree.h"
#include "app_threadx.h"
#include "display_renderer.h"
#include "th_mode.h"

static ULONG s_selected = 0UL;
static const ULONG kHomeIdxStartGame = 0UL;

static th_mode_t UiHomeModeFromFlags(ULONG mode_flags)
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

static void UiPageHome_Enter(void)
{
  s_selected = 0UL;
  UiRouter_MarkDirty();
}

static uint8_t UiPageHome_Action(const ui_action_evt_t *evt)
{
  const ui_menu_item_t *items;
  ULONG item_count = 0UL;

  if (evt == TX_NULL)
  {
    return 0U;
  }

  items = UiMenuTree_GetHomeItems(&item_count);
  if ((items == TX_NULL) || (item_count == 0UL))
  {
    return 0U;
  }

  if (((evt->action == UI_ACTION_UP) || (evt->action == UI_ACTION_LEFT)) &&
      ((evt->event == UI_EVENT_PRESS) || (evt->event == UI_EVENT_REPEAT)))
  {
    if (s_selected == 0UL)
    {
      s_selected = (item_count - 1UL);
    }
    else
    {
      s_selected--;
    }
    UiRouter_MarkDirty();
    return 1U;
  }

  if (((evt->action == UI_ACTION_DOWN) || (evt->action == UI_ACTION_RIGHT)) &&
      ((evt->event == UI_EVENT_PRESS) || (evt->event == UI_EVENT_REPEAT)))
  {
    s_selected = (s_selected + 1UL) % item_count;
    UiRouter_MarkDirty();
    return 1U;
  }

  if ((evt->action == UI_ACTION_CONFIRM) && (evt->event == UI_EVENT_PRESS))
  {
    if (s_selected == kHomeIdxStartGame)
    {
      (void)App_SysEvent_ModeSet(APP_MODE_REALTIME);
    }
    else
    {
      UiRouter_RequestPage(items[s_selected].target_page);
    }
    return 1U;
  }

  if ((evt->action == UI_ACTION_CANCEL) && (evt->event == UI_EVENT_PRESS))
  {
    UiRouter_RequestPage(UI_PAGE_HOME);
    return 1U;
  }

  return 0U;
}

static void UiPageHome_Render(void)
{
  const ui_router_state_t *state = UiRouter_GetState();
  const ui_menu_item_t *items;
  ULONG item_count = 0UL;
  uint16_t y = 44U;
  ULONG i;

  if (state == TX_NULL)
  {
    return;
  }

  items = UiMenuTree_GetHomeItems(&item_count);

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(UiHomeModeFromFlags(state->mode_flags));
  renderDrawTextScaled(12U, 16U, "PEEPSHOW", RENDER_LAYER_UI, RENDER_COLOR_BLACK, 2U);

  for (i = 0UL; i < item_count; i++)
  {
    if (i == s_selected)
    {
      renderDrawText(8U, y, ">", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    }
    renderDrawText(16U, y, items[i].label, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    y = (uint16_t)(y + 12U);
  }

  renderDrawText(8U, 84U, "A STARTS REALTIME", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  UiRouter_RenderFooterHints("JOY: NAV  A:SELECT", "B: HOME");
  Render_MarkDirtyAll();
}

static const ui_page_vtable_t s_page_home =
{
  .name = "home",
  .on_enter = UiPageHome_Enter,
  .on_action = UiPageHome_Action,
  .on_tick = TX_NULL,
  .on_render = UiPageHome_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageHome_GetVTable(void)
{
  return &s_page_home;
}
