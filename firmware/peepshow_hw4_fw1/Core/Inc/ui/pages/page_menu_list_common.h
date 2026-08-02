#ifndef UI_PAGE_MENU_LIST_COMMON_H
#define UI_PAGE_MENU_LIST_COMMON_H

#include "ui/ui_menu_tree.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_PAGE_MENU_LIST_VISIBLE_ROWS 8U

typedef struct
{
  ULONG selected;
  ULONG top;
} ui_page_menu_list_state_t;

void UiPageMenuList_Enter(ui_page_menu_list_state_t *state);
uint8_t UiPageMenuList_Action(ui_page_menu_list_state_t *state,
                              ui_page_id_t back_page,
                              const ui_menu_item_t *items,
                              ULONG item_count,
                              const ui_action_evt_t *evt);
void UiPageMenuList_Render(ui_page_menu_list_state_t *state,
                           const char *title,
                           const ui_menu_item_t *items,
                           ULONG item_count);

#ifdef __cplusplus
}
#endif

#endif
