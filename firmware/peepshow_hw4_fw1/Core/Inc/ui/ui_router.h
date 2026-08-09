#ifndef UI_ROUTER_H
#define UI_ROUTER_H

#include <stdint.h>
#include "ui/ui_events.h"
#include "ui/ui_menu.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UI_MENU_STACK_MAX
#define UI_MENU_STACK_MAX (8U)
#endif

#ifndef UI_TREE_STACK_MAX
#define UI_TREE_STACK_MAX (4U)
#endif

typedef enum
{
  UI_MODE_MENU = 0U,
  UI_MODE_PAGE = 1U
} ui_mode_t;

typedef struct ui_router_tree_frame_s
{
  ui_mode_t mode;
  uint8_t reserved0;
  uint16_t menu_depth;
  ui_menu_tree_id_t tree_id;
  const ui_page_t *current_page;
  const void *current_page_arg;
  ui_menu_stack_frame_t menu_stack[UI_MENU_STACK_MAX];
} ui_router_tree_frame_t;

struct ui_router_s;
typedef struct ui_router_s ui_router_t;

typedef uint32_t (*ui_router_action_handler_t)(ui_router_t *ui, ui_menu_action_id_t action_id, uint16_t arg0);

struct ui_router_s
{
  ui_mode_t mode;
  uint8_t dirty;
  uint8_t tree_depth;
  uint16_t menu_depth;
  ui_menu_tree_id_t current_tree_id;
  uint16_t reserved0;
  uint32_t state_version;
  const ui_page_t *current_page;
  const void *current_page_arg;
  ui_router_action_handler_t action_handler;
  ui_menu_stack_frame_t menu_stack[UI_MENU_STACK_MAX];
  ui_router_tree_frame_t tree_stack[UI_TREE_STACK_MAX];
};

void UiRouter_Init(ui_router_t *ui, const ui_menu_t *root_menu);
void UiRouter_Reset(ui_router_t *ui, const ui_menu_t *root_menu);
void UiRouter_SetActionHandler(ui_router_t *ui, ui_router_action_handler_t handler);
uint32_t UiRouter_HandleEvent(ui_router_t *ui, const ui_input_evt_t *evt);
void UiRouter_OpenPage(ui_router_t *ui, const ui_page_t *page, const void *arg);
uint8_t UiRouter_OpenTree(ui_router_t *ui, ui_menu_tree_id_t tree_id, const ui_menu_t *root_menu);
uint8_t UiRouter_PushTree(ui_router_t *ui, ui_menu_tree_id_t tree_id, const ui_menu_t *root_menu);
uint8_t UiRouter_PopTree(ui_router_t *ui);
void UiRouter_Render(ui_router_t *ui);

uint8_t UiRouter_IsDirty(const ui_router_t *ui);
void UiRouter_ClearDirty(ui_router_t *ui);
uint32_t UiRouter_StateVersion(const ui_router_t *ui);

const ui_menu_stack_frame_t *UiRouter_CurrentMenu(const ui_router_t *ui);
const ui_page_t *UiRouter_CurrentPage(const ui_router_t *ui);
ui_menu_tree_id_t UiRouter_CurrentTree(const ui_router_t *ui);

#ifdef __cplusplus
}
#endif

#endif

