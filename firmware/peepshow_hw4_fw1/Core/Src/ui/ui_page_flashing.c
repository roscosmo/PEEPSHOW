#include "ui/ui_page_native.h"
#include "app_threadx.h"
#include "ui/ui_router.h"
#include "ui/ui_runtime_context.h"
#include "display_renderer.h"
#include "th_mode.h"

static void UiPageFlashing_Enter(ui_router_t *ui, const void *arg)
{
  (void)ui;
  (void)arg;
}

static uint32_t UiPageFlashing_Event(ui_router_t *ui, const ui_input_evt_t *evt)
{
  (void)ui;

  if (evt == TX_NULL)
  {
    return UI_EVT_RESULT_NONE;
  }

  if ((evt->evt == UI_EVT_BACK) || (evt->evt == UI_EVT_LONG_BACK))
  {
    (void)App_SysEvent_ModeSet(APP_MODE_STATIC);
    return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_CONSUME_BACK);
  }

  if (evt->evt == UI_EVT_TICK)
  {
    return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
  }

  return UI_EVT_RESULT_HANDLED;
}

static void UiPageFlashing_Render(ui_router_t *ui)
{
  (void)ui;

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(TH_MODE_FLASHING);
  renderDrawTextScaled(8U, 16U, "FLASHING", RENDER_LAYER_UI, RENDER_COLOR_BLACK, 2U);
  renderDrawText(8U, 44U, "USB STORAGE ACTIVE", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  renderDrawText(8U, 58U, "CONNECT TO PC", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  UiRuntimeContext_RenderFooterHints("MSC ACTIVE", "B: EXIT");
  Render_MarkDirtyAll();
}

static void UiPageFlashing_Exit(ui_router_t *ui)
{
  (void)ui;
}

const ui_page_t UI_PAGE_FLASHING_NATIVE =
{
  .name = "flashing",
  .footer = UI_FOOTER_A_SELECT_B_BACK,
  .input_policy = TX_NULL,
  .enter = UiPageFlashing_Enter,
  .event = UiPageFlashing_Event,
  .render = UiPageFlashing_Render,
  .exit = UiPageFlashing_Exit
};

