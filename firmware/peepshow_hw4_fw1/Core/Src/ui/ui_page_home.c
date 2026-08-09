#include "ui/ui_page_native.h"
#include "app_threadx.h"
#include "game_package.h"
#include "ui/ui_menu_tree.h"
#include "ui/ui_router.h"
#include "ui/ui_runtime_context.h"
#include "ui/ui_test_title_anim_autogen.h"
#include "display_renderer.h"
#include "th_mode.h"

static ULONG s_ui_home_selected = 0UL;
static ULONG s_ui_home_anim_frame_idx = 0UL;
static ULONG s_ui_home_anim_accum_ticks = 0UL;
static ULONG s_ui_home_anim_last_tick = 0UL;
static const ULONG k_ui_home_idx_start_game = 0UL;
static const ULONG k_ui_home_anim_target_fps = 12UL;
static const char *k_ui_home_items[] = {"START GAME", "OPTIONS"};

static ULONG UiPageHome_AnimFrameTicks(void)
{
  ULONG tps = (ULONG)TX_TIMER_TICKS_PER_SECOND;
  ULONG ticks;

  if (tps == 0UL)
  {
    return 1UL;
  }

  ticks = (tps + (k_ui_home_anim_target_fps - 1UL)) / k_ui_home_anim_target_fps;
  if (ticks == 0UL)
  {
    ticks = 1UL;
  }
  return ticks;
}

static void UiPageHome_AnimReset(void)
{
  s_ui_home_anim_frame_idx = 0UL;
  s_ui_home_anim_accum_ticks = 0UL;
  s_ui_home_anim_last_tick = 0UL;
}

static uint8_t UiPageHome_AnimTick(void)
{
  ULONG now_tick = tx_time_get();
  ULONG frame_ticks = UiPageHome_AnimFrameTicks();
  ULONG delta_ticks;
  uint8_t frame_changed = 0U;

  if (s_ui_home_anim_last_tick == 0UL)
  {
    s_ui_home_anim_last_tick = now_tick;
    return 0U;
  }

  delta_ticks = now_tick - s_ui_home_anim_last_tick;
  s_ui_home_anim_last_tick = now_tick;
  if (delta_ticks == 0UL)
  {
    return 0U;
  }

  s_ui_home_anim_accum_ticks += delta_ticks;
  while (s_ui_home_anim_accum_ticks >= frame_ticks)
  {
    s_ui_home_anim_accum_ticks -= frame_ticks;
    s_ui_home_anim_frame_idx++;
    if (s_ui_home_anim_frame_idx >= (ULONG)UI_TEST_TITLE_ANIM_FRAME_COUNT)
    {
      s_ui_home_anim_frame_idx = 0UL;
    }
    frame_changed = 1U;
  }

  return frame_changed;
}

static th_mode_t UiPageHome_ModeFromFlags(ULONG mode_flags)
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

static void UiPageHome_Enter(ui_router_t *ui, const void *arg)
{
  (void)ui;
  (void)arg;
  UiPageHome_AnimReset();
}

static uint32_t UiPageHome_Event(ui_router_t *ui, const ui_input_evt_t *evt)
{
  if ((ui == 0) || (evt == 0))
  {
    return UI_EVT_RESULT_NONE;
  }

  if ((evt->evt == UI_EVT_UP) || (evt->evt == UI_EVT_LEFT))
  {
    if (s_ui_home_selected == 0UL)
    {
      s_ui_home_selected = 1UL;
    }
    else
    {
      s_ui_home_selected = 0UL;
    }
    return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
  }

  if ((evt->evt == UI_EVT_DOWN) || (evt->evt == UI_EVT_RIGHT))
  {
    s_ui_home_selected = (s_ui_home_selected + 1UL) % 2UL;
    return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
  }

  if (evt->evt == UI_EVT_SELECT)
  {
    if (s_ui_home_selected == k_ui_home_idx_start_game)
    {
      (void)GamePackage_RequestRuntimeModeById(1U);
      (void)App_SysEvent_ModeSet(APP_MODE_REALTIME);
      return UI_EVT_RESULT_HANDLED;
    }

    UiRouter_Reset(ui, &UI_MENU_ROOT);
    return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
  }

  if (evt->evt == UI_EVT_TICK)
  {
    if (UiPageHome_AnimTick() != 0U)
    {
      return UI_EVT_RESULT_DIRTY;
    }
    return UI_EVT_RESULT_NONE;
  }

  return UI_EVT_RESULT_NONE;
}

static void UiPageHome_Render(ui_router_t *ui)
{
  const ui_router_state_t *state = UiRuntimeContext_GetState();
  const uint8_t *frame_data = ui_test_title_anim_frames[0];
  uint16_t frame_x = 0U;
  uint16_t frame_y = 0U;
  uint16_t y = (uint16_t)(RENDER_HEIGHT - 24U);
  ULONG i;

  (void)ui;

  if (state == TX_NULL)
  {
    return;
  }

  if ((ULONG)UI_TEST_TITLE_ANIM_FRAME_COUNT > 0UL)
  {
    frame_data = ui_test_title_anim_frames[s_ui_home_anim_frame_idx];
  }
  if (RENDER_WIDTH > UI_TEST_TITLE_ANIM_FRAME_W)
  {
    frame_x = (uint16_t)((RENDER_WIDTH - UI_TEST_TITLE_ANIM_FRAME_W) / 2U);
  }
  if (RENDER_HEIGHT > UI_TEST_TITLE_ANIM_FRAME_H)
  {
    frame_y = (uint16_t)((RENDER_HEIGHT - UI_TEST_TITLE_ANIM_FRAME_H) / 2U);
  }

  renderClear(RENDER_COLOR_BLACK);
  Render_SetModeIndicator(UiPageHome_ModeFromFlags(state->mode_flags));
  renderBlit1bpp(frame_x,
                 frame_y,
                 (uint16_t)UI_TEST_TITLE_ANIM_FRAME_W,
                 (uint16_t)UI_TEST_TITLE_ANIM_FRAME_H,
                 frame_data,
                 (uint16_t)UI_TEST_TITLE_ANIM_FRAME_STRIDE,
                 true,
                 RENDER_LAYER_GAME,
                 RENDER_COLOR_WHITE);
  renderDrawTextScaled(12U, 8U, "PEEPSHOW", RENDER_LAYER_UI, RENDER_COLOR_WHITE, 2U);

  for (i = 0UL; i < 2UL; i++)
  {
    if (i == s_ui_home_selected)
    {
      renderDrawText(8U, y, ">", RENDER_LAYER_UI, RENDER_COLOR_WHITE);
    }
    renderDrawText(16U, y, k_ui_home_items[i], RENDER_LAYER_UI, RENDER_COLOR_WHITE);
    y = (uint16_t)(y + 12U);
  }
  Render_MarkDirtyAll();
}

static void UiPageHome_Exit(ui_router_t *ui)
{
  (void)ui;
}

const ui_page_t UI_PAGE_HOME_NATIVE =
{
  .name = "home",
  .footer = UI_FOOTER_A_START_B_BACK,
  .enter = UiPageHome_Enter,
  .event = UiPageHome_Event,
  .render = UiPageHome_Render,
  .exit = UiPageHome_Exit
};


