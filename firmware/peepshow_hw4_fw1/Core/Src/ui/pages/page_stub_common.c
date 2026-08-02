#include "ui/pages/page_stub_common.h"
#include "display_renderer.h"
#include "th_mode.h"

static th_mode_t UiPageStub_ModeFromFlags(ULONG mode_flags)
{
  if ((mode_flags & (1UL << 3)) != 0UL)
  {
    return TH_MODE_FLASHING;
  }
  if ((mode_flags & (1UL << 2)) != 0UL)
  {
    return TH_MODE_REALTIME;
  }
  if ((mode_flags & (1UL << 1)) != 0UL)
  {
    return TH_MODE_STATIC;
  }
  return TH_MODE_STOP;
}

void UiPageStub_Enter(void)
{
  UiRouter_MarkDirty();
}

uint8_t UiPageStub_ActionBackTo(ui_page_id_t back_page, const ui_action_evt_t *evt)
{
  if (evt == TX_NULL)
  {
    return 0U;
  }

  if ((evt->action == UI_ACTION_CANCEL) && (evt->event == UI_EVENT_PRESS))
  {
    UiRouter_RequestPage(back_page);
    return 1U;
  }

  return 0U;
}

void UiPageStub_Render(const char *title, const char *line1, const char *line2)
{
  const ui_router_state_t *state = UiRouter_GetState();

  if (state == TX_NULL)
  {
    return;
  }

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(UiPageStub_ModeFromFlags(state->mode_flags));
  renderDrawText(4U, 8U, title, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  renderDrawText(4U, 28U, line1, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  renderDrawText(4U, 40U, line2, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  UiRouter_RenderFooterHints("B: BACK", TX_NULL);
  Render_MarkDirtyAll();
}
