#ifndef UI_PAGE_REGISTRY_H
#define UI_PAGE_REGISTRY_H

#include <stdint.h>

#include "ui/ui_router.h"
#include "ui/ui_menu.h"
#include "ui/ui_page.h"
#include "ui/ui_tree_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  UI_PAGE_ROUTE_NONE = 0U,
  UI_PAGE_ROUTE_NATIVE_PAGE = 1U,
  UI_PAGE_ROUTE_NATIVE_TREE = 2U
} ui_page_registry_route_kind_t;

typedef struct
{
  uint16_t page_id;
  uint8_t route_kind;
  const ui_page_t *native_page;
  ui_menu_tree_id_t native_tree_id;
  const ui_menu_t *native_tree_root;
} ui_page_registry_entry_t;

uint8_t UiPageRegistry_OpenById(ui_router_t *ui, uint16_t page_id);

#ifdef __cplusplus
}
#endif

#endif /* UI_PAGE_REGISTRY_H */
