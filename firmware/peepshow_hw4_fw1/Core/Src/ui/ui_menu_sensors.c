#include "ui/ui_menu_tree.h"
#include "ui/ui_page_native.h"

static const ui_menu_item_t s_menu_sensors_items[] =
{
  {
    .label = "LIS2",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_PAGE,
    .arg = 0,
    .target.page = &UI_PAGE_LIS2_NATIVE
  },
  {
    .label = "LIS2 STEPS",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_PAGE,
    .arg = 0,
    .target.page = &UI_PAGE_LIS2_STEPS_NATIVE
  }
};

const ui_menu_t UI_MENU_SENSORS =
{
  .title = "SENSORS",
  .items = s_menu_sensors_items,
  .count = (uint16_t)(sizeof(s_menu_sensors_items) / sizeof(s_menu_sensors_items[0])),
  .footer = UI_FOOTER_A_SELECT_B_BACK
};

