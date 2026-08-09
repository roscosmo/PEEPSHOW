#include "app_threadx.h"
#include "ui/ui_page.h"
#include "ui/ui_runtime_context.h"
#include "display_renderer.h"
#include "th_mode.h"
#include <stdio.h>

typedef struct
{
  const char *label;
  app_audio_user_gain_id_t gain_id;
} ui_audio_gain_row_t;

static const ui_audio_gain_row_t k_ui_audio_rows[] =
{
  {"MASTER", APP_AUDIO_USER_GAIN_MASTER},
  {"MUSIC", APP_AUDIO_USER_GAIN_MUSIC},
  {"SFX", APP_AUDIO_USER_GAIN_SFX},
  {"UI", APP_AUDIO_USER_GAIN_UI}
};

static ULONG s_ui_audio_selected = 0UL;

static th_mode_t UiPageAudioLevels_ModeFromFlags(ULONG mode_flags)
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

static ULONG UiPageAudioLevels_GetPct(const ui_router_state_t *state, ULONG index)
{
  if ((state == TX_NULL) || (index >= (ULONG)(sizeof(k_ui_audio_rows) / sizeof(k_ui_audio_rows[0]))))
  {
    return 100UL;
  }

  switch (k_ui_audio_rows[index].gain_id)
  {
    case APP_AUDIO_USER_GAIN_MASTER:
      return state->audio_live.user_master_pct;
    case APP_AUDIO_USER_GAIN_MUSIC:
      return state->audio_live.user_music_pct;
    case APP_AUDIO_USER_GAIN_SFX:
      return state->audio_live.user_sfx_pct;
    case APP_AUDIO_USER_GAIN_UI:
      return state->audio_live.user_ui_pct;
    default:
      return 100UL;
  }
}

static void UiPageAudioLevels_Enter(ui_router_t *ui, const void *arg)
{
  (void)ui;
  (void)arg;
  s_ui_audio_selected = 0UL;
}

static uint32_t UiPageAudioLevels_Event(ui_router_t *ui, const ui_input_evt_t *evt)
{
  const ui_router_state_t *state = UiRuntimeContext_GetState();
  ULONG row_count = (ULONG)(sizeof(k_ui_audio_rows) / sizeof(k_ui_audio_rows[0]));

  (void)ui;

  if (evt == TX_NULL)
  {
    return UI_EVT_RESULT_NONE;
  }

  if (evt->evt == UI_EVT_UP)
  {
    if (s_ui_audio_selected == 0UL)
    {
      s_ui_audio_selected = row_count - 1UL;
    }
    else
    {
      s_ui_audio_selected--;
    }
    return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
  }

  if (evt->evt == UI_EVT_DOWN)
  {
    s_ui_audio_selected = (s_ui_audio_selected + 1UL) % row_count;
    return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
  }

  if ((evt->evt == UI_EVT_LEFT) || (evt->evt == UI_EVT_RIGHT))
  {
    LONG next_pct = (LONG)UiPageAudioLevels_GetPct(state, s_ui_audio_selected);
    UINT status;
    ULONG clamped_pct;

    if (evt->evt == UI_EVT_LEFT)
    {
      next_pct -= 5L;
    }
    else
    {
      next_pct += 5L;
    }

    if (next_pct < 0L)
    {
      next_pct = 0L;
    }
    if (next_pct > 300L)
    {
      next_pct = 300L;
    }
    clamped_pct = (ULONG)next_pct;
    status = App_AudioReq_SetUserGain(k_ui_audio_rows[s_ui_audio_selected].gain_id, clamped_pct);
    if (status == TX_SUCCESS)
    {
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
    }
    return UI_EVT_RESULT_HANDLED;
  }

  if (evt->evt == UI_EVT_SELECT)
  {
    (void)App_SensorReq_SettingsSave();
    return UI_EVT_RESULT_HANDLED;
  }

  if (evt->evt == UI_EVT_TICK)
  {
    return UI_EVT_RESULT_DIRTY;
  }

  return UI_EVT_RESULT_NONE;
}

static void UiPageAudioLevels_Render(ui_router_t *ui)
{
  const ui_router_state_t *state = UiRuntimeContext_GetState();
  ULONG i;
  uint16_t y = 20U;

  (void)ui;

  if (state == TX_NULL)
  {
    return;
  }

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(UiPageAudioLevels_ModeFromFlags(state->mode_flags));
  renderDrawText(4U, 8U, "AUDIO LEVELS", RENDER_LAYER_UI, RENDER_COLOR_BLACK);

  for (i = 0UL; i < (ULONG)(sizeof(k_ui_audio_rows) / sizeof(k_ui_audio_rows[0])); i++)
  {
    char line[32];
    ULONG pct = UiPageAudioLevels_GetPct(state, i);
    (void)snprintf(line, sizeof(line), "%-6s %3lu%%", k_ui_audio_rows[i].label, pct);
    if (i == s_ui_audio_selected)
    {
      renderDrawText(4U, y, ">", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    }
    renderDrawText(12U, y, line, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    y = (uint16_t)(y + 12U);
  }

  UiRuntimeContext_RenderFooterHints("U/D:ROW L/R:ADJ", "A:SAVE B:BACK");
  Render_MarkDirtyAll();
}

static void UiPageAudioLevels_Exit(ui_router_t *ui)
{
  (void)ui;
}

const ui_page_t UI_PAGE_AUDIO_LEVELS_NATIVE =
{
  .name = "audio_levels",
  .footer = UI_FOOTER_A_SELECT_B_BACK,
  .enter = UiPageAudioLevels_Enter,
  .event = UiPageAudioLevels_Event,
  .render = UiPageAudioLevels_Render,
  .exit = UiPageAudioLevels_Exit
};
