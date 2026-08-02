#include "ui/pages/page_menu_sensors_sub.h"
#include "ui/pages/page_menu_list_common.h"
#include "ui/ui_menu_tree.h"

static ui_page_menu_list_state_t s_list;

static void UiPageMenuSensorsSub_Enter(void)
{
  UiPageMenuList_Enter(&s_list);
}

static uint8_t UiPageMenuSensorsSub_Action(const ui_action_evt_t *evt)
{
  ULONG item_count = 0UL;
  const ui_menu_item_t *items = UiMenuTree_GetSensorsSubmenuItems(&item_count);
  return UiPageMenuList_Action(&s_list, UI_PAGE_MENU, items, item_count, evt);
}

static void UiPageMenuSensorsSub_Render(void)
{
  ULONG item_count = 0UL;
  const ui_menu_item_t *items = UiMenuTree_GetSensorsSubmenuItems(&item_count);
  UiPageMenuList_Render(&s_list, "SENSORS", items, item_count);
}

static const ui_page_vtable_t s_page_menu_sensors_sub =
{
  .name = "menu_sensors_sub",
  .on_enter = UiPageMenuSensorsSub_Enter,
  .on_action = UiPageMenuSensorsSub_Action,
  .on_tick = TX_NULL,
  .on_render = UiPageMenuSensorsSub_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageMenuSensorsSub_GetVTable(void)
{
  return &s_page_menu_sensors_sub;
}
