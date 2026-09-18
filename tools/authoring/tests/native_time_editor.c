#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ps_ui_router.h"
#include "ps_system_font.h"

#define DISPLAY_RENDERER_WIDTH 168U
#define DISPLAY_RENDERER_HEIGHT 144U
static uint8_t pixels[DISPLAY_RENDERER_HEIGHT][DISPLAY_RENDERER_WIDTH];
static uint32_t DisplayRenderer_SetBlack(uint16_t x, uint16_t y)
{
  assert(x < DISPLAY_RENDERER_WIDTH && y < DISPLAY_RENDERER_HEIGHT);
  assert(pixels[y][x] == 0U);
  pixels[y][x] = 1U;
  return 1UL;
}
#include "time_renderer.inc"

void PS_HW6_TraceUiDispatch(uint32_t event, uint32_t from, uint32_t to, uint32_t status)
{ (void)event; (void)from; (void)to; (void)status; }

static void event(uint32_t value) { assert(PS_UIRouter_Dispatch(value) == PS_STATUS_OK); }
static uint32_t open_editor(ps_system_datetime_t local, uint32_t status)
{
  uint32_t session;
  ps_system_datetime_t draft;
  event(PS_UI_ROUTER_EVENT_NAV_TIME);
  assert(g_ps_ui_time_probe.status == PS_UI_TIME_LOADING);
  assert(PS_UIRouter_TakeTimeRequest(&session, &draft) == 1);
  assert(PS_UIRouter_TakeTimeRequest(&session, &draft) == 0);
  assert(PS_UIRouter_CompleteTimeRequest(session, 1, status, &local) == 1);
  return session;
}
static void select_field(uint32_t field)
{
  for (uint32_t i = 0; i < 8 && g_ps_ui_router_probe.focus_index != field; ++i)
  { event(PS_UI_ROUTER_EVENT_INPUT_JOY_RIGHT); }
  assert(g_ps_ui_router_probe.focus_index == field);
}
int main(void)
{
  uint32_t session, current, seconds;
  ps_system_datetime_t draft, local = {2024, 1, 31, 23, 59, 59};
  PS_UIRouter_Init();
  event(PS_UI_ROUTER_EVENT_BOOT_COMPLETE);
  event(PS_UI_ROUTER_EVENT_INPUT_BTN_L);
  event(PS_UI_ROUTER_EVENT_INPUT_BTN_A);
  assert(g_ps_ui_router_probe.current_page == PS_UI_ROUTER_PAGE_TIME);
  event(PS_UI_ROUTER_EVENT_INPUT_BTN_B);
  assert(PS_UIRouter_TakeTimeRequest(&session, &draft) == 0);
  session = open_editor(local, PS_SYSTEM_TIME_OK);
  assert(g_ps_ui_time_probe.status == PS_UI_TIME_EDIT);
  select_field(1);
  event(PS_UI_ROUTER_EVENT_INPUT_JOY_UP);
  assert(g_ps_ui_time_probe.draft.month == 2 && g_ps_ui_time_probe.draft.day == 29);
  select_field(0);
  event(PS_UI_ROUTER_EVENT_INPUT_JOY_UP);
  assert(g_ps_ui_time_probe.draft.year == 2025 && g_ps_ui_time_probe.draft.day == 28);
  select_field(2);
  event(PS_UI_ROUTER_EVENT_INPUT_JOY_UP);
  assert(g_ps_ui_time_probe.draft.day == 1);
  event(PS_UI_ROUTER_EVENT_INPUT_JOY_DOWN);
  assert(g_ps_ui_time_probe.draft.day == 28);
  select_field(3);
  event(PS_UI_ROUTER_EVENT_INPUT_JOY_UP);
  assert(g_ps_ui_time_probe.draft.hour == 0);
  select_field(4);
  event(PS_UI_ROUTER_EVENT_INPUT_JOY_UP);
  assert(g_ps_ui_time_probe.draft.minute == 0);
  select_field(5);
  event(PS_UI_ROUTER_EVENT_INPUT_JOY_UP);
  assert(g_ps_ui_time_probe.draft.second == 0);
  assert(PS_UIRouter_TakeTimeRequest(&current, &draft) == 0);
  select_field(6);
  event(PS_UI_ROUTER_EVENT_INPUT_BTN_A);
  assert(g_ps_ui_time_probe.status == PS_UI_TIME_SAVING);
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_INPUT_BTN_A) == PS_STATUS_BUSY);
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_INPUT_BTN_B) == PS_STATUS_BUSY);
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_NAV_MENU) != PS_STATUS_OK);
  assert(PS_UIRouter_TakeTimeRequest(&current, &draft) == 2 && current == session);
  assert(PS_SystemTime_Encode(&draft, &seconds) == PS_SYSTEM_TIME_OK);
  assert(PS_UIRouter_CompleteTimeRequest(session, 2, UINT32_MAX, &draft) == 1);
  assert(g_ps_ui_time_probe.status == PS_UI_TIME_SAVE_ERROR);
  event(PS_UI_ROUTER_EVENT_INPUT_BTN_A);
  assert(PS_UIRouter_TakeTimeRequest(&current, &draft) == 2);
  assert(PS_UIRouter_CompleteTimeRequest(session, 2, PS_SYSTEM_TIME_OK, &draft) == 1);
  assert(g_ps_ui_time_probe.status == PS_UI_TIME_SAVED);
  assert(PS_UIRouter_CompleteTimeRequest(session, 2, PS_SYSTEM_TIME_OK, &local) == 0);
  select_field(7);
  event(PS_UI_ROUTER_EVENT_INPUT_BTN_A);
  assert(g_ps_ui_router_probe.current_page == PS_UI_ROUTER_PAGE_MENU);
  assert(PS_UIRouter_TakeTimeRequest(&current, &draft) == 0);

  session = open_editor(local, PS_SYSTEM_TIME_UNSET);
  assert(g_ps_ui_time_probe.status == PS_UI_TIME_UNSET && g_ps_ui_time_probe.draft.year == 2000);
  event(PS_UI_ROUTER_EVENT_INPUT_JOY_DOWN);
  assert(g_ps_ui_time_probe.draft.year == 2099);
  event(PS_UI_ROUTER_EVENT_INPUT_JOY_UP);
  assert(g_ps_ui_time_probe.draft.year == 2000);
  event(PS_UI_ROUTER_EVENT_INPUT_BTN_B);
  event(PS_UI_ROUTER_EVENT_NAV_TIME);
  assert(PS_UIRouter_TakeTimeRequest(&session, &draft) == 1);
  event(PS_UI_ROUTER_EVENT_INPUT_BTN_B);
  assert(PS_UIRouter_CompleteTimeRequest(session, 1, PS_SYSTEM_TIME_OK, &local) == 0);
  assert(g_ps_ui_router_probe.current_page == PS_UI_ROUTER_PAGE_MENU);
  event(PS_UI_ROUTER_EVENT_NAV_TIME);
  assert(PS_UIRouter_TakeTimeRequest(&current, &draft) == 1 && current != session);
  assert(PS_UIRouter_CompleteTimeRequest(session, 1, PS_SYSTEM_TIME_OK, &local) == 0);
  assert(g_ps_ui_time_probe.status == PS_UI_TIME_LOADING);
  assert(PS_UIRouter_CompleteTimeRequest(current, 1, UINT32_MAX, &local) == 1);
  assert(g_ps_ui_time_probe.status == PS_UI_TIME_READ_ERROR);
  event(PS_UI_ROUTER_EVENT_INPUT_BTN_A);
  assert(PS_UIRouter_TakeTimeRequest(&current, &draft) == 1);
  assert(PS_UIRouter_CompleteTimeRequest(current, 1, PS_SYSTEM_TIME_OK, &local) == 1);

  /* A power overlay can interrupt a submitted save; completion never reopens it. */
  select_field(6);
  event(PS_UI_ROUTER_EVENT_INPUT_BTN_A);
  assert(PS_UIRouter_TakeTimeRequest(&current, &draft) == 2);
  event(PS_UI_ROUTER_EVENT_SHUTDOWN_PREP);
  assert(PS_UIRouter_CompleteTimeRequest(current, 2, PS_SYSTEM_TIME_OK, &draft) == 0);
  event(PS_UI_ROUTER_EVENT_SHUTDOWN_CANCEL);
  assert(g_ps_ui_router_probe.current_page == PS_UI_ROUTER_PAGE_TIME);
  assert(g_ps_ui_time_probe.status == PS_UI_TIME_SAVED);
  select_field(6);
  event(PS_UI_ROUTER_EVENT_INPUT_BTN_A);
  event(PS_UI_ROUTER_EVENT_SHUTDOWN_PREP);
  event(PS_UI_ROUTER_EVENT_SHUTDOWN_CANCEL);
  assert(PS_UIRouter_TakeTimeRequest(&current, &draft) == 0);
  assert(g_ps_ui_time_probe.status == PS_UI_TIME_READ_ERROR);
  assert(PS_SystemTime_Encode(&local, &seconds) == PS_SYSTEM_TIME_OK);
  for (uint32_t focus = 0; focus < 8; ++focus)
  {
    for (uint32_t status = 0; status <= PS_UI_TIME_SAVE_ERROR; ++status)
    {
      memset(pixels, 0, sizeof(pixels));
      assert(DisplayRenderer_DrawTimeEditor(seconds, focus, status) > 200);
    }
  }
  puts("time editor checks passed");
  return 0;
}
