#ifndef UI_MENU_TREE_H
#define UI_MENU_TREE_H

#include "ui/ui_router.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  const char *label;
  ui_page_id_t target_page;
} ui_menu_item_t;

const ui_menu_item_t *UiMenuTree_GetHomeItems(ULONG *count_out);
const ui_menu_item_t *UiMenuTree_GetSystemMenuItems(ULONG *count_out);
const ui_menu_item_t *UiMenuTree_GetInputSubmenuItems(ULONG *count_out);
const ui_menu_item_t *UiMenuTree_GetSensorsSubmenuItems(ULONG *count_out);
const ui_menu_item_t *UiMenuTree_GetPowerTimeSubmenuItems(ULONG *count_out);
const ui_menu_item_t *UiMenuTree_GetSystemSubmenuItems(ULONG *count_out);

#ifdef __cplusplus
}
#endif

#endif
