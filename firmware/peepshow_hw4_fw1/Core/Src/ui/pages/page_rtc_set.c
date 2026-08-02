#include "ui/pages/page_rtc_set.h"
#include "ui/pages/page_stub_common.h"

static uint8_t UiPageRtcSet_Action(const ui_action_evt_t *evt)
{
  return UiPageStub_ActionBackTo(UI_PAGE_MENU_POWER_TIME_SUB, evt);
}

static void UiPageRtcSet_Render(void)
{
  UiPageStub_Render("RTC SET", "TODO: RTC PAGE", "TIME/DATE SETUP");
}

static const ui_page_vtable_t s_page_rtc_set =
{
  .name = "rtc_set",
  .on_enter = UiPageStub_Enter,
  .on_action = UiPageRtcSet_Action,
  .on_tick = TX_NULL,
  .on_render = UiPageRtcSet_Render,
  .on_exit = TX_NULL
};

const ui_page_vtable_t *UiPageRtcSet_GetVTable(void)
{
  return &s_page_rtc_set;
}
