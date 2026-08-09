#include "ui/ui_menu_tree.h"
#include "ui/ui_page_native.h"

static const ui_menu_item_t s_menu_system_items[] =
{
  {
    .label = "INPUT",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_SUBMENU,
    .arg = 0,
    .target.submenu = &UI_MENU_INPUT
  },
  {
    .label = "SENSORS",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_SUBMENU,
    .arg = 0,
    .target.submenu = &UI_MENU_SENSORS
  },
  {
    .label = "POWER/TIME",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_SUBMENU,
    .arg = 0,
    .target.submenu = &UI_MENU_POWER_TIME
  },
  {
    .label = "AUDIO LEVELS",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_PAGE,
    .arg = 0,
    .target.page = &UI_PAGE_AUDIO_LEVELS_NATIVE
  },
  {
    .label = "ENTER FLASHING",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_ACTION,
    .arg = 0,
    .target.action = {
      .action_id = (ui_menu_action_id_t)UI_MENU_ACTION_USB_FLASH_ENTER,
      .arg0 = 0U
    }
  },
  {
    .label = "IMPORT MANIFEST",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_ACTION,
    .arg = 0,
    .target.action = {
      .action_id = (ui_menu_action_id_t)UI_MENU_ACTION_USB_IMPORT_MANIFEST,
      .arg0 = 0U
    }
  },
  {
    .label = "IMPORT SCENE",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_ACTION,
    .arg = 0,
    .target.action = {
      .action_id = (ui_menu_action_id_t)UI_MENU_ACTION_USB_IMPORT_SCENE,
      .arg0 = 0U
    }
  }
};

const ui_menu_t UI_MENU_SYSTEM =
{
  .title = "SYSTEM",
  .items = s_menu_system_items,
  .count = (uint16_t)(sizeof(s_menu_system_items) / sizeof(s_menu_system_items[0])),
  .footer = UI_FOOTER_A_SELECT_B_BACK
};
