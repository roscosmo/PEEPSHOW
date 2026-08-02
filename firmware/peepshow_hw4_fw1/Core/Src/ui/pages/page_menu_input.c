#include "ui/pages/page_menu_input.h"
#include "ui/pages/page_stub_common.h"

static uint8_t UiPageMenuInput_Action(const ui_action_evt_t *evt)
{
  return UiPageStub_ActionBackTo(UI_PAGE_MENU_INPUT_SUB, evt);
}

static void UiPageMenuInput_Render(void)
{
  UiPageStub_Render("MENU INPUT", "TODO: INPUT PAGE", "INPUT METRICS HERE");
}

static const ui_page_vtable_t s_page_menu_input =
{
  .name = "menu_input",
  .on_enter = UiPageStub_Enter,
  .on_action = UiPageMenuInput_Action,
  .on_tick = TX_NULL,
  .on_render = UiPageMenuInput_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageMenuInput_GetVTable(void)
{
  return &s_page_menu_input;
}
