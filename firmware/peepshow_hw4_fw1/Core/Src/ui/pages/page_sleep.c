#include "ui/pages/page_sleep.h"
#include "ui/pages/page_stub_common.h"

static uint8_t UiPageSleep_Action(const ui_action_evt_t *evt)
{
  return UiPageStub_ActionBackTo(UI_PAGE_MENU_POWER_TIME_SUB, evt);
}

static void UiPageSleep_Render(void)
{
  UiPageStub_Render("SLEEP", "TODO: SLEEP PAGE", "POWER SETTINGS");
}

static const ui_page_vtable_t s_page_sleep =
{
  .name = "sleep",
  .on_enter = UiPageStub_Enter,
  .on_action = UiPageSleep_Action,
  .on_tick = TX_NULL,
  .on_render = UiPageSleep_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageSleep_GetVTable(void)
{
  return &s_page_sleep;
}
