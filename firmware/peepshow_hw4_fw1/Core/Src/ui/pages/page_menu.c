#include "ui/pages/page_menu.h"
#include "ui/pages/page_menu_list_common.h"
#include "ui/ui_menu_tree.h"
static ui_page_menu_list_state_t s_list;

static void UiPageMenu_Enter(void)
{
  UiPageMenuList_Enter(&s_list);
}

static uint8_t UiPageMenu_Action(const ui_action_evt_t *evt)
{
  ULONG item_count = 0UL;
  const ui_menu_item_t *items = UiMenuTree_GetSystemMenuItems(&item_count);

  return UiPageMenuList_Action(&s_list, UI_PAGE_HOME, items, item_count, evt);
}

static void UiPageMenu_Render(void)
{
  ULONG item_count = 0UL;
  const ui_menu_item_t *items = UiMenuTree_GetSystemMenuItems(&item_count);
  UiPageMenuList_Render(&s_list, "OPTIONS", items, item_count);
}

static const ui_page_vtable_t s_page_menu =
{
  .name = "menu",
  .on_enter = UiPageMenu_Enter,
  .on_action = UiPageMenu_Action,
  .on_tick = TX_NULL,
  .on_render = UiPageMenu_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageMenu_GetVTable(void)
{
  return &s_page_menu;
}
