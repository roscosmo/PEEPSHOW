#include "ui/ui_runtime_context.h"
#include "display_renderer.h"
#include <string.h>

typedef struct
{
  ui_router_api_t api;
  ui_router_state_t state;
  ui_router_state_t prev_state;
  uint8_t state_valid;
} ui_runtime_context_t;

static ui_runtime_context_t s_ui_ctx;

static const char *UiRuntimeContext_InputSourceName(ULONG source)
{
  switch (source)
  {
    case 1UL:
      return "BTN_A";
    case 2UL:
      return "BTN_B";
    case 3UL:
      return "BTN_L";
    case 4UL:
      return "BTN_R";
    case 5UL:
      return "BOOT";
    case 6UL:
      return "JOY_UP";
    case 7UL:
      return "JOY_RIGHT";
    case 8UL:
      return "JOY_DOWN";
    case 9UL:
      return "JOY_LEFT";
    default:
      return "-";
  }
}

static const char *UiRuntimeContext_InputActionName(ULONG action)
{
  switch (action)
  {
    case UI_ACTION_BTN_A:
      return "BTN_A";
    case UI_ACTION_BTN_B:
      return "BTN_B";
    case UI_ACTION_BTN_L:
      return "BTN_L";
    case UI_ACTION_BTN_R:
      return "BTN_R";
    case UI_ACTION_BTN_BOOT:
      return "BOOT";
    case UI_ACTION_JOY_UP:
      return "JOY_UP";
    case UI_ACTION_JOY_RIGHT:
      return "JOY_RIGHT";
    case UI_ACTION_JOY_DOWN:
      return "JOY_DOWN";
    case UI_ACTION_JOY_LEFT:
      return "JOY_LEFT";
    case UI_ACTION_NONE:
    default:
      return "-";
  }
}

static const char *UiRuntimeContext_InputEdgeName(ULONG edge)
{
  switch (edge)
  {
    case UI_EVENT_PRESS:
      return "PRESS";
    case UI_EVENT_RELEASE:
      return "RELEASE";
    case UI_EVENT_REPEAT:
      return "REPEAT";
    case UI_EVENT_LONG:
      return "LONG";
    default:
      return "-";
  }
}

void UiRuntimeContext_Init(const ui_router_api_t *api)
{
  (void)memset(&s_ui_ctx, 0, sizeof(s_ui_ctx));
  if (api != TX_NULL)
  {
    s_ui_ctx.api = *api;
  }
}

void UiRuntimeContext_UpdateState(const ui_router_state_t *state)
{
  ui_router_state_t prev_cmp;
  ui_router_state_t new_cmp;

  if (state == TX_NULL)
  {
    return;
  }

  s_ui_ctx.state = *state;
  prev_cmp = s_ui_ctx.prev_state;
  new_cmp = s_ui_ctx.state;
  /* Joy/sensor telemetry is high-rate; page-level logic decides redraw policy. */
  prev_cmp.joy_live = new_cmp.joy_live;
  prev_cmp.pmic_live = new_cmp.pmic_live;
  prev_cmp.lis_live = new_cmp.lis_live;

  if ((s_ui_ctx.state_valid == 0U) ||
      (memcmp(&prev_cmp, &new_cmp, sizeof(new_cmp)) != 0))
  {
    s_ui_ctx.prev_state = s_ui_ctx.state;
    s_ui_ctx.state_valid = 1U;
  }
}

const ui_router_state_t *UiRuntimeContext_GetState(void)
{
  return &s_ui_ctx.state;
}

const ui_router_api_t *UiRuntimeContext_GetApi(void)
{
  return &s_ui_ctx.api;
}

void UiRuntimeContext_RenderFooterHints(const char *line1, const char *line2)
{
  uint16_t y = (uint16_t)(RENDER_HEIGHT - 20U);

  if (line1 != TX_NULL)
  {
    renderDrawText(4U, y, line1, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }

  if (line2 != TX_NULL)
  {
    y = (uint16_t)(RENDER_HEIGHT - 10U);
    renderDrawText(4U, y, line2, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }
}

void UiRuntimeContext_RenderInputMonitor(uint16_t x, uint16_t y)
{
  renderDrawText(x, y, "SRC:", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  renderDrawText((uint16_t)(x + 28U), y, UiRuntimeContext_InputSourceName(s_ui_ctx.state.last_input_source), RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);
  renderDrawText(x, y, "ACT:", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  renderDrawText((uint16_t)(x + 28U), y, UiRuntimeContext_InputActionName(s_ui_ctx.state.last_input_action), RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  y = (uint16_t)(y + 10U);
  renderDrawText(x, y, "EDG:", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  renderDrawText((uint16_t)(x + 28U), y, UiRuntimeContext_InputEdgeName(s_ui_ctx.state.last_input_edge), RENDER_LAYER_UI, RENDER_COLOR_BLACK);
}
