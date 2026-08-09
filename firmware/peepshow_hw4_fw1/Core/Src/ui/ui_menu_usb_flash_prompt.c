#include "ui/ui_menu_tree.h"

static const ui_menu_item_t s_menu_usb_flash_prompt_items[] =
{
  {
    .label = "YES: ENTER FLASHING",
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
    .label = "NO: STAY HERE",
    .label_fn = 0,
    .footer = UI_FOOTER_A_SELECT_B_BACK,
    .type = UI_MENU_ITEM_ACTION,
    .arg = 0,
    .target.action = {
      .action_id = (ui_menu_action_id_t)UI_MENU_ACTION_USB_FLASH_DECLINE,
      .arg0 = 0U
    }
  }
};

const ui_menu_t UI_MENU_USB_FLASH_PROMPT =
{
  .title = "USB DETECTED",
  .items = s_menu_usb_flash_prompt_items,
  .count = (uint16_t)(sizeof(s_menu_usb_flash_prompt_items) / sizeof(s_menu_usb_flash_prompt_items[0])),
  .footer = UI_FOOTER_A_SELECT_B_BACK
};

