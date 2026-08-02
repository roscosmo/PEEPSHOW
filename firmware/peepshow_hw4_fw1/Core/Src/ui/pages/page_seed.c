#include "ui/pages/page_seed.h"
#include "ui/pages/page_stub_common.h"

static uint8_t UiPageSeed_Action(const ui_action_evt_t *evt)
{
  return UiPageStub_ActionBackTo(UI_PAGE_MENU_POWER_TIME_SUB, evt);
}

static void UiPageSeed_Render(void)
{
  UiPageStub_Render("SEED", "TODO: SEED PAGE", "RANDOM SEED TOOLS");
}

static const ui_page_vtable_t s_page_seed =
{
  .name = "seed",
  .on_enter = UiPageStub_Enter,
  .on_action = UiPageSeed_Action,
  .on_tick = TX_NULL,
  .on_render = UiPageSeed_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageSeed_GetVTable(void)
{
  return &s_page_seed;
}
