#include "ui/ui_menu_tree.h"
#include "ui/ui_page_native.h"

static const ui_menu_item_t s_menu_power_time_items[] =
{
  {
    .label = "BATT STATS",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_PAGE,
    .arg = 0,
    .target.page = &UI_PAGE_BATT_STATS_NATIVE
  }
};

const ui_menu_t UI_MENU_POWER_TIME =
{
  .title = "POWER/TIME",
  .items = s_menu_power_time_items,
  .count = (uint16_t)(sizeof(s_menu_power_time_items) / sizeof(s_menu_power_time_items[0])),
  .footer = UI_FOOTER_A_SELECT_B_BACK
};

