#include "ui/pages/page_communications.h"
#include "ui/pages/page_stub_common.h"

static uint8_t UiPageCommunications_Action(const ui_action_evt_t *evt)
{
  return UiPageStub_ActionBackTo(UI_PAGE_MENU_SYSTEM_SUB, evt);
}

static void UiPageCommunications_Render(void)
{
  UiPageStub_Render("COMMUNICATIONS", "TODO: COMMS PAGE", "RADIO/USB STATUS");
}

static const ui_page_vtable_t s_page_communications =
{
  .name = "communications",
  .on_enter = UiPageStub_Enter,
  .on_action = UiPageCommunications_Action,
  .on_tick = TX_NULL,
  .on_render = UiPageCommunications_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageCommunications_GetVTable(void)
{
  return &s_page_communications;
}
