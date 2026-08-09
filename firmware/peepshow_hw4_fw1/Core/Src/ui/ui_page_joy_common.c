#include "ui/ui_page_joy_common.h"

static uint8_t s_ui_joy_done_recal_mode = 0U;
static uint8_t s_ui_joy_done_recal_next_enter = 0U;

void UiPageJoyCommon_ResetDoneActions(void)
{
  s_ui_joy_done_recal_mode = 0U;
  s_ui_joy_done_recal_next_enter = 0U;
}

uint8_t UiPageJoyCommon_GetDoneRecalMode(void)
{
  return s_ui_joy_done_recal_mode;
}

void UiPageJoyCommon_SetDoneRecalMode(uint8_t enabled)
{
  s_ui_joy_done_recal_mode = (enabled != 0U) ? 1U : 0U;
}

uint8_t UiPageJoyCommon_GetDoneRecalNextEnter(void)
{
  return s_ui_joy_done_recal_next_enter;
}

void UiPageJoyCommon_SetDoneRecalNextEnter(uint8_t enabled)
{
  s_ui_joy_done_recal_next_enter = (enabled != 0U) ? 1U : 0U;
}


