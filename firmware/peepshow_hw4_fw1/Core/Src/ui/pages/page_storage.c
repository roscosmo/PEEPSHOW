#include "ui/pages/page_storage.h"
#include "ui/pages/page_stub_common.h"

static uint8_t UiPageStorage_Action(const ui_action_evt_t *evt)
{
  return UiPageStub_ActionBackTo(UI_PAGE_MENU_SYSTEM_SUB, evt);
}

static void UiPageStorage_Render(void)
{
  UiPageStub_Render("STORAGE", "TODO: STORAGE PAGE", "FLASH/FILEX STATUS");
}

static const ui_page_vtable_t s_page_storage =
{
  .name = "storage",
  .on_enter = UiPageStub_Enter,
  .on_action = UiPageStorage_Action,
  .on_tick = TX_NULL,
  .on_render = UiPageStorage_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageStorage_GetVTable(void)
{
  return &s_page_storage;
}
