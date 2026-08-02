#include "ui/pages/page_menu_system_sub.h"
#include "ui/pages/page_menu_list_common.h"
#include "ui/ui_menu_tree.h"

static ui_page_menu_list_state_t s_list;

static void UiPageMenuSystemSub_Enter(void)
{
  UiPageMenuList_Enter(&s_list);
}

static uint8_t UiPageMenuSystemSub_Action(const ui_action_evt_t *evt)
{
  ULONG item_count = 0UL;
  const ui_menu_item_t *items = UiMenuTree_GetSystemSubmenuItems(&item_count);
  return UiPageMenuList_Action(&s_list, UI_PAGE_MENU, items, item_count, evt);
}

static void UiPageMenuSystemSub_Render(void)
{
  ULONG item_count = 0UL;
  const ui_menu_item_t *items = UiMenuTree_GetSystemSubmenuItems(&item_count);
  UiPageMenuList_Render(&s_list, "SYSTEM", items, item_count);
}

static const ui_page_vtable_t s_page_menu_system_sub =
{
  .name = "menu_system_sub",
  .on_enter = UiPageMenuSystemSub_Enter,
  .on_action = UiPageMenuSystemSub_Action,
  .on_tick = TX_NULL,
  .on_render = UiPageMenuSystemSub_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageMenuSystemSub_GetVTable(void)
{
  return &s_page_menu_system_sub;
}
