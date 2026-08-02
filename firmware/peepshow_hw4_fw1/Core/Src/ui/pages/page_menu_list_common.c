#include "ui/pages/page_menu_list_common.h"
#include "display_renderer.h"
#include "th_mode.h"

static th_mode_t UiPageMenuList_ModeFromFlags(ULONG mode_flags)
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

static void UiPageMenuList_EnsureVisible(ui_page_menu_list_state_t *state, ULONG item_count)
{
  if ((state == TX_NULL) || (item_count == 0UL))
  {
    return;
  }

  if (state->selected >= item_count)
  {
    state->selected = (item_count - 1UL);
  }

  if (state->selected < state->top)
  {
    state->top = state->selected;
  }
  else if (state->selected >= (state->top + UI_PAGE_MENU_LIST_VISIBLE_ROWS))
  {
    state->top = (state->selected - UI_PAGE_MENU_LIST_VISIBLE_ROWS + 1UL);
  }
}

void UiPageMenuList_Enter(ui_page_menu_list_state_t *state)
{
  if (state == TX_NULL)
  {
    return;
  }
  state->selected = 0UL;
  state->top = 0UL;
  UiRouter_MarkDirty();
}

uint8_t UiPageMenuList_Action(ui_page_menu_list_state_t *state,
                              ui_page_id_t back_page,
                              const ui_menu_item_t *items,
                              ULONG item_count,
                              const ui_action_evt_t *evt)
{
  if ((state == TX_NULL) || (items == TX_NULL) || (item_count == 0UL) || (evt == TX_NULL))
  {
    return 0U;
  }

  if (((evt->action == UI_ACTION_UP) || (evt->action == UI_ACTION_LEFT)) &&
      ((evt->event == UI_EVENT_PRESS) || (evt->event == UI_EVENT_REPEAT)))
  {
    if (state->selected == 0UL)
    {
      state->selected = (item_count - 1UL);
    }
    else
    {
      state->selected--;
    }
    UiPageMenuList_EnsureVisible(state, item_count);
    UiRouter_MarkDirty();
    return 1U;
  }

  if (((evt->action == UI_ACTION_DOWN) || (evt->action == UI_ACTION_RIGHT)) &&
      ((evt->event == UI_EVENT_PRESS) || (evt->event == UI_EVENT_REPEAT)))
  {
    state->selected = (state->selected + 1UL) % item_count;
    UiPageMenuList_EnsureVisible(state, item_count);
    UiRouter_MarkDirty();
    return 1U;
  }

  if ((evt->action == UI_ACTION_CONFIRM) && (evt->event == UI_EVENT_PRESS))
  {
    UiRouter_RequestPage(items[state->selected].target_page);
    return 1U;
  }

  if ((evt->action == UI_ACTION_CANCEL) && (evt->event == UI_EVENT_PRESS))
  {
    UiRouter_RequestPage(back_page);
    return 1U;
  }

  return 0U;
}

void UiPageMenuList_Render(ui_page_menu_list_state_t *state,
                           const char *title,
                           const ui_menu_item_t *items,
                           ULONG item_count)
{
  const ui_router_state_t *router_state = UiRouter_GetState();
  ULONG i;
  uint16_t y = 24U;

  if ((state == TX_NULL) || (items == TX_NULL) || (router_state == TX_NULL))
  {
    return;
  }

  UiPageMenuList_EnsureVisible(state, item_count);

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(UiPageMenuList_ModeFromFlags(router_state->mode_flags));
  renderDrawText(4U, 8U, title, RENDER_LAYER_UI, RENDER_COLOR_BLACK);

  for (i = 0UL; (i < UI_PAGE_MENU_LIST_VISIBLE_ROWS) && ((state->top + i) < item_count); i++)
  {
    ULONG item_idx = (state->top + i);
    if (item_idx == state->selected)
    {
      renderDrawText(4U, y, ">", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    }
    renderDrawText(12U, y, items[item_idx].label, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    y = (uint16_t)(y + 10U);
  }

  if (state->top > 0UL)
  {
    renderDrawText((uint16_t)(RENDER_WIDTH - 12U), 24U, "^", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }
  if ((state->top + UI_PAGE_MENU_LIST_VISIBLE_ROWS) < item_count)
  {
    renderDrawText((uint16_t)(RENDER_WIDTH - 12U), 94U, "v", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }

  UiRouter_RenderFooterHints("JOY: NAV  A:OPEN", "B: BACK");
  Render_MarkDirtyAll();
}
