#include "ui/pages/page_sound.h"
#include "ui/pages/page_stub_common.h"

static uint8_t UiPageSound_Action(const ui_action_evt_t *evt)
{
  return UiPageStub_ActionBackTo(UI_PAGE_MENU_SYSTEM_SUB, evt);
}

static void UiPageSound_Render(void)
{
  UiPageStub_Render("SOUND", "TODO: SOUND PAGE", "AUDIO SETTINGS");
}

static const ui_page_vtable_t s_page_sound =
{
  .name = "sound",
  .on_enter = UiPageStub_Enter,
  .on_action = UiPageSound_Action,
  .on_tick = TX_NULL,
  .on_render = UiPageSound_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageSound_GetVTable(void)
{
  return &s_page_sound;
}
