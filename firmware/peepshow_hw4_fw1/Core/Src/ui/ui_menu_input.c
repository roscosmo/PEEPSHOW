#include "ui/ui_menu_tree.h"
#include "ui/ui_page_native.h"

static const ui_menu_item_t s_menu_input_items[] =
{
  {
    .label = "JOYSTICK CAL",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_PAGE,
    .arg = 0,
    .target.page = &UI_PAGE_JOY_CAL_NATIVE
  },
  {
    .label = "JOY TARGET",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_PAGE,
    .arg = 0,
    .target.page = &UI_PAGE_JOY_TARGET_NATIVE
  }
};

const ui_menu_t UI_MENU_INPUT =
{
  .title = "INPUT",
  .items = s_menu_input_items,
  .count = (uint16_t)(sizeof(s_menu_input_items) / sizeof(s_menu_input_items[0])),
  .footer = UI_FOOTER_A_SELECT_B_BACK
};

