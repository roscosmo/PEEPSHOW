#ifndef UI_TREE_CONTRACT_H
#define UI_TREE_CONTRACT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t ui_menu_tree_id_t;
typedef uint16_t ui_menu_action_id_t;

enum
{
  UI_TREE_ID_SYSTEM_ROOT = 0U,
  UI_TREE_ID_PET_FEED = 1U,
  UI_TREE_ID_USB_FLASH_PROMPT = 2U
};

enum
{
  UI_MENU_ACTION_NONE = 0U,
  UI_MENU_ACTION_PET_FEED_SELECT = 0x1000U,
  UI_MENU_ACTION_USB_FLASH_ENTER = 0x1100U,
  UI_MENU_ACTION_USB_FLASH_DECLINE = 0x1101U,
  UI_MENU_ACTION_USB_IMPORT_MANIFEST = 0x1102U,
  UI_MENU_ACTION_USB_IMPORT_SCENE = 0x1103U
};

#ifdef __cplusplus
}
#endif

#endif
