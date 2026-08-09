#include "ui/ui_menu_tree.h"

static const ui_menu_item_t s_menu_root_items[] =
{
  {
    .label = "SYSTEM",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_SUBMENU,
    .arg = 0,
    .target.submenu = &UI_MENU_SYSTEM
  }
};

const ui_menu_t UI_MENU_ROOT =
{
  .title = "OPTIONS",
  .items = s_menu_root_items,
  .count = (uint16_t)(sizeof(s_menu_root_items) / sizeof(s_menu_root_items[0])),
  .footer = UI_FOOTER_A_SELECT_B_BACK
};

