#ifndef UI_MENU_H
#define UI_MENU_H

#include <stdint.h>
#include "ui/ui_events.h"
#include "ui/ui_page.h"
#include "ui/ui_tree_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  UI_MENU_ITEM_SUBMENU = 1U,
  UI_MENU_ITEM_PAGE = 2U,
  UI_MENU_ITEM_ACTION = 3U
} ui_menu_item_type_t;

struct ui_menu_s;
typedef struct ui_menu_s ui_menu_t;

typedef const char *(*ui_menu_label_fn_t)(const void *arg);

typedef struct
{
  ui_menu_action_id_t action_id;
  uint16_t arg0;
} ui_menu_action_t;

typedef struct
{
  const char *label;
  ui_menu_label_fn_t label_fn;
  ui_footer_t footer;
  ui_menu_item_type_t type;
  const void *arg;
  union
  {
    const ui_menu_t *submenu;
    const ui_page_t *page;
    ui_menu_action_t action;
  } target;
} ui_menu_item_t;

struct ui_menu_s
{
  const char *title;
  const ui_menu_item_t *items;
  uint16_t count;
  ui_footer_t footer;
};

typedef struct
{
  const ui_menu_t *menu;
  uint16_t selected_index;
  uint16_t scroll_offset;
} ui_menu_stack_frame_t;

#ifdef __cplusplus
}
#endif

#endif

