#include "ui/pages/page_joy_cursor.h"
#include "ui/pages/page_stub_common.h"

static uint8_t UiPageJoyCursor_Action(const ui_action_evt_t *evt)
{
  return UiPageStub_ActionBackTo(UI_PAGE_MENU_INPUT_SUB, evt);
}

static void UiPageJoyCursor_Render(void)
{
  UiPageStub_Render("JOY CURSOR", "TODO: CURSOR PAGE", "JOY MAPPING VIEW");
}

static const ui_page_vtable_t s_page_joy_cursor =
{
  .name = "joy_cursor",
  .on_enter = UiPageStub_Enter,
  .on_action = UiPageJoyCursor_Action,
  .on_tick = TX_NULL,
  .on_render = UiPageJoyCursor_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageJoyCursor_GetVTable(void)
{
  return &s_page_joy_cursor;
}
