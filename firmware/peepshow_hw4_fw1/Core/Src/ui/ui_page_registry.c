#include "ui/ui_page_registry.h"

#include "ui/ui_page_registry_autogen.h"

uint8_t UiPageRegistry_OpenById(ui_router_t *ui, uint16_t page_id)
{
  uint32_t i;

  if ((ui == (ui_router_t *)0) || (page_id == 0U))
  {
    return 0U;
  }

  for (i = 0U; i < (uint32_t)UI_PAGE_REGISTRY_ENTRY_COUNT; i++)
  {
    const ui_page_registry_entry_t *entry = &g_ui_page_registry_entries[i];
    if (entry->page_id != page_id)
    {
      continue;
    }

    if ((entry->route_kind == (uint8_t)UI_PAGE_ROUTE_NATIVE_PAGE) &&
        (entry->native_page != (const ui_page_t *)0))
    {
      UiRouter_OpenPage(ui, entry->native_page, (const void *)0);
      return 1U;
    }
    if ((entry->route_kind == (uint8_t)UI_PAGE_ROUTE_NATIVE_TREE) &&
        (entry->native_tree_root != (const ui_menu_t *)0))
    {
      return (UiRouter_OpenTree(ui, entry->native_tree_id, entry->native_tree_root) != 0U) ? 1U : 0U;
    }
    return 0U;
  }

  return 0U;
}
