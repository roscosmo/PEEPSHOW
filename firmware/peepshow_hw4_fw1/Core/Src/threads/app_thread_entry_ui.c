/* Thread entry implementation for App_ThreadX runtime. */

/* UI router bridge/helpers. */
static UINT AppUiJoyCalStartReq(void);
static UINT AppUiJoyCalSaveReq(void);
static UINT AppUiJoyCalCancelReq(void);
static ULONG AppUiMapInputEdgeToRouter(ULONG edge);
static ULONG AppUiMapInputEventToRouter(ULONG event);
static ULONG AppUiMapInputActionToUiAction(ULONG action, ULONG source);
static VOID AppUiCopyJoyState(ui_router_state_t *state_out);
static VOID AppUiCopyAudioState(ui_router_state_t *state_out);
static VOID AppUiCopyPmicState(ui_router_state_t *state_out);
static VOID AppUiCopyLisState(ui_router_state_t *state_out);
static VOID AppUiCopyPetState(ui_router_state_t *state_out);
static uint8_t AppUiBuildInputEvent(const app_input_action_evt_t *src, ui_input_evt_t *dst);
static uint8_t AppUiAtRootMenu(void);
static VOID AppUiBuildRouterState(ui_router_state_t *state_out);
static uint8_t AppUiShouldTick(void);
static uint8_t AppUiIsExpectedMenuBoundaryNoop(const ui_input_evt_t *evt);
static uint8_t AppUiAudioEventForAction(const ui_action_evt_t *ui_evt, app_audio_event_t *event_out);
static uint8_t AppUiUsbFlashPromptTreeActive(const ui_router_t *ui);
static VOID AppUiUsbFlashPromptHandleDismiss(void);
static VOID AppUiUsbFlashPromptMaybeShow(ULONG mode_flags);
static VOID AppUiEnsurePageForMode(ULONG mode_flags);
static void AppUiHandleTick(void);
static void AppUiEnterPage(app_ui_page_t page);
static uint8_t AppUiHandleAction(const app_input_action_evt_t *evt);
static uint32_t AppUiRouterMenuAction(ui_router_t *ui, ui_menu_action_id_t action_id, uint16_t arg0);
static VOID AppUiDiagInc(volatile ULONG *counter);
extern uint8_t UiPagePet_IsSandActive(void);

volatile ULONG g_ui_ignore_null_evt_count __attribute__((used));
volatile ULONG g_ui_ignore_not_ui_mode_count __attribute__((used));
volatile ULONG g_ui_ignore_stop_joy_count __attribute__((used));
volatile ULONG g_ui_ignore_unmapped_action_count __attribute__((used));
volatile ULONG g_ui_ignore_unmapped_event_count __attribute__((used));
volatile ULONG g_ui_ignore_router_map_fail_count __attribute__((used));
volatile ULONG g_ui_ignore_router_unhandled_count __attribute__((used));
volatile ULONG g_ui_ignore_router_no_evt_count __attribute__((used));
volatile ULONG g_ui_ignore_router_long_reject_count __attribute__((used));
volatile ULONG g_ui_ignore_router_repeat_reject_count __attribute__((used));
volatile ULONG g_ui_ignore_router_release_reject_count __attribute__((used));
volatile ULONG g_ui_ignore_router_policy_reject_count __attribute__((used));
volatile ULONG g_ui_ignore_router_menu_boundary_noop_count __attribute__((used));
volatile ULONG g_ui_denied_audio_emit_count __attribute__((used));

static VOID AppUiThreadEntry(ULONG thread_input)
{
  UINT status;
  app_input_action_evt_t evt;
  ui_router_api_t router_api;

  (void)thread_input;
  (void)memset(&router_api, 0, sizeof(router_api));
  router_api.joy_cal_start = AppUiJoyCalStartReq;
  router_api.joy_cal_save = AppUiJoyCalSaveReq;
  router_api.joy_cal_cancel = AppUiJoyCalCancelReq;
  UiRuntimeContext_Init(&router_api);
  UiRouter_Init(&g_ui_router, &UI_MENU_ROOT);
  UiRouter_SetActionHandler(&g_ui_router, AppUiRouterMenuAction);
  AppUiEnterPage(APP_UI_PAGE_HOME);
  AppUiHandleTick();

  for (;;)
  {
    status = tx_queue_receive(&g_q_ui_events, &evt, KNOB_RTOS_INPUT_WAIT_TICKS);
    if (status == TX_SUCCESS)
    {
      g_ui_event_recv_count++;
      g_ui_event_last_action = evt.action;
      if (AppUiHandleAction(&evt) != 0U)
      {
        g_ui_event_handled_count++;
      }
      else
      {
        g_ui_event_ignored_count++;
      }
      if (AppUiShouldTick() != 0U)
      {
        AppUiHandleTick();
      }
    }
    else if (status == TX_QUEUE_EMPTY)
    {
      if (AppUiShouldTick() != 0U)
      {
        AppUiHandleTick();
      }
    }
    else if (status != TX_QUEUE_EMPTY)
    {
      /* Queue error in UI input consumer: continue receiving future events. */
      g_ui_event_queue_error_count++;
      if (AppUiShouldTick() != 0U)
      {
        AppUiHandleTick();
      }
    }
  }
}

/* UI router bridge and runtime state snapshot helpers. */
static UINT AppUiJoyCalStartReq(void)
{
  return App_SensorReq_JoyCalStart();
}

static UINT AppUiJoyCalSaveReq(void)
{
  return App_SensorReq_JoyCalSave();
}

static UINT AppUiJoyCalCancelReq(void)
{
  return App_SensorReq_JoyCalCancel();
}

static uint32_t AppUiRouterMenuAction(ui_router_t *ui, ui_menu_action_id_t action_id, uint16_t arg0)
{
  UINT status;

  switch (action_id)
  {
    case (ui_menu_action_id_t)UI_MENU_ACTION_PET_FEED_SELECT:
    {
      ULONG hold_ms = 1200UL;
      if (arg0 == 0U)
      {
        hold_ms = 900UL;
      }
      else if (arg0 >= 2U)
      {
        hold_ms = 1500UL;
      }
      (void)App_PetReq_ActionWithHold(APP_PET_ACTION_FEED, hold_ms);
      (void)UiRouter_PopTree(ui);
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
    }

    case (ui_menu_action_id_t)UI_MENU_ACTION_USB_FLASH_ENTER:
    {
      status = App_UsbFlash_RequestEnter();
      if (status != TX_SUCCESS)
      {
        (void)App_AudioReq_PlayEvent(APP_AUDIO_EVENT_UI_DENIED);
        AppUiDiagInc(&g_ui_denied_audio_emit_count);
        return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
      }

      if (AppUiUsbFlashPromptTreeActive(ui) != 0U)
      {
        (void)UiRouter_PopTree(ui);
      }
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
    }

    case (ui_menu_action_id_t)UI_MENU_ACTION_USB_FLASH_DECLINE:
      (void)App_UsbFlashPrompt_Clear();
      if (AppUiUsbFlashPromptTreeActive(ui) != 0U)
      {
        (void)UiRouter_PopTree(ui);
      }
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);

    case (ui_menu_action_id_t)UI_MENU_ACTION_USB_IMPORT_MANIFEST:
      status = App_StorageReq_GamePackageManifestImportFat();
      if (status != TX_SUCCESS)
      {
        (void)App_AudioReq_PlayEvent(APP_AUDIO_EVENT_UI_DENIED);
        AppUiDiagInc(&g_ui_denied_audio_emit_count);
      }
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);

    case (ui_menu_action_id_t)UI_MENU_ACTION_USB_IMPORT_SCENE:
      status = App_StorageReq_GamePackageSceneImportFat();
      if (status != TX_SUCCESS)
      {
        (void)App_AudioReq_PlayEvent(APP_AUDIO_EVENT_UI_DENIED);
        AppUiDiagInc(&g_ui_denied_audio_emit_count);
      }
      return (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);

    default:
      return UI_EVT_RESULT_NONE;
  }
}

static VOID AppUiDiagInc(volatile ULONG *counter)
{
  if ((counter != TX_NULL) && (*counter < 0xFFFFFFFFUL))
  {
    (*counter)++;
  }
}

static ULONG AppUiMapInputEdgeToRouter(ULONG edge)
{
  switch (edge)
  {
    case APP_INPUT_EDGE_LOW:
      return UI_EVENT_PRESS;
    case APP_INPUT_EDGE_HIGH:
      return UI_EVENT_RELEASE;
    case APP_INPUT_EDGE_REPEAT:
      return UI_EVENT_REPEAT;
    default:
      return 0UL;
  }
}

static ULONG AppUiMapInputEventToRouter(ULONG event)
{
  switch ((app_input_event_t)event)
  {
    case APP_INPUT_EVENT_PRESS:
      return UI_EVENT_PRESS;
    case APP_INPUT_EVENT_RELEASE:
      return UI_EVENT_RELEASE;
    case APP_INPUT_EVENT_REPEAT:
      return UI_EVENT_REPEAT;
    case APP_INPUT_EVENT_LONG:
      return UI_EVENT_LONG;
    default:
      return 0UL;
  }
}

static ULONG AppUiMapInputActionToUiAction(ULONG action, ULONG source)
{
  (void)source;

  switch ((app_input_action_t)action)
  {
    case APP_INPUT_ACTION_BTN_A:
      return (ULONG)UI_ACTION_BTN_A;
    case APP_INPUT_ACTION_BTN_B:
      return (ULONG)UI_ACTION_BTN_B;
    case APP_INPUT_ACTION_BTN_L:
      return (ULONG)UI_ACTION_BTN_L;
    case APP_INPUT_ACTION_BTN_R:
      return (ULONG)UI_ACTION_BTN_R;
    case APP_INPUT_ACTION_BTN_BOOT:
      return (ULONG)UI_ACTION_BTN_BOOT;
    case APP_INPUT_ACTION_JOY_UP:
      return (ULONG)UI_ACTION_JOY_UP;
    case APP_INPUT_ACTION_JOY_RIGHT:
      return (ULONG)UI_ACTION_JOY_RIGHT;
    case APP_INPUT_ACTION_JOY_DOWN:
      return (ULONG)UI_ACTION_JOY_DOWN;
    case APP_INPUT_ACTION_JOY_LEFT:
      return (ULONG)UI_ACTION_JOY_LEFT;
    default:
      return (ULONG)UI_ACTION_NONE;
  }
}

static VOID AppUiCopyJoyState(ui_router_state_t *state_out)
{
  state_out->joy_cal_status.stage = g_sensor_joy_cal_status.stage;
  state_out->joy_cal_status.progress = g_sensor_joy_cal_status.progress;
  state_out->joy_cal_status.last_error = g_sensor_joy_cal_status.last_error;
  state_out->joy_cal_status.save_pending = g_sensor_joy_cal_status.save_pending;
  state_out->joy_cal_active = g_sensor_joy_cal_active;
  state_out->joy_cal_quality.valid = g_sensor_joy_cal_quality.valid;
  state_out->joy_cal_quality.quality_ok = g_sensor_joy_cal_quality.quality_ok;
  state_out->joy_cal_quality.span_ratio = g_sensor_joy_cal_quality.span_ratio;
  state_out->joy_cal_quality.axis_error = g_sensor_joy_cal_quality.axis_error;
  state_out->joy_cal_quality.dir_norm_min = g_sensor_joy_cal_quality.dir_norm_min;
  state_out->joy_cal_quality.dir_norm_max = g_sensor_joy_cal_quality.dir_norm_max;
  state_out->joy_live.dir = g_sensor_joy_live_status.dir;
  state_out->joy_live.input_mask = g_sensor_joy_live_status.input_mask;
  state_out->joy_live.deadzone_enabled = g_sensor_joy_live_status.deadzone_enabled;
  state_out->joy_live.invert_x = g_sensor_joy_live_status.invert_x;
  state_out->joy_live.invert_y = g_sensor_joy_live_status.invert_y;
  state_out->joy_live.nx = g_sensor_joy_live_status.nx;
  state_out->joy_live.ny = g_sensor_joy_live_status.ny;
  state_out->joy_live.r_abs_mT = g_sensor_joy_live_status.r_abs_mT;
  state_out->joy_live.center_x_mT = g_sensor_joy_live_status.center_x_mT;
  state_out->joy_live.center_y_mT = g_sensor_joy_live_status.center_y_mT;
  state_out->joy_live.span_x_mT = g_sensor_joy_live_status.span_x_mT;
  state_out->joy_live.span_y_mT = g_sensor_joy_live_status.span_y_mT;
  state_out->joy_live.rotation_deg = g_sensor_joy_live_status.rotation_deg;
  state_out->joy_live.threshold_x_mT = g_sensor_joy_live_status.threshold_x_mT;
  state_out->joy_live.threshold_y_mT = g_sensor_joy_live_status.threshold_y_mT;
  state_out->joy_live.deadzone_mT = g_sensor_joy_live_status.deadzone_mT;
}

static VOID AppUiCopyPmicState(ui_router_state_t *state_out)
{
  state_out->pmic_live.fsm_state = g_sensor_pmic.state;
  state_out->pmic_live.sample_count = g_sensor_pmic_live.sample_count;
  state_out->pmic_live.last_sample_tick = g_sensor_pmic_live.last_sample_tick;
  state_out->pmic_live.last_error = g_sensor_pmic_live.last_error;
  state_out->pmic_live.charging_enabled_cfg = g_sensor_pmic_live.charging_enabled_cfg;
  state_out->pmic_live.charging_active = g_sensor_pmic_live.charging_active;
  state_out->pmic_live.battery_soc_percent = g_sensor_pmic_live.battery_soc_percent;
  state_out->pmic_live.battery_soc_raw = g_sensor_pmic_live.battery_soc_raw;
  state_out->pmic_live.battery_health_state = g_sensor_pmic_live.battery_health_state;
  state_out->pmic_live.battery_health_reason = g_sensor_pmic_live.battery_health_reason;
  state_out->pmic_live.charger_state = g_sensor_pmic_live.charger_state;
  state_out->pmic_live.battery_uv = g_sensor_pmic_live.battery_uv;
  state_out->pmic_live.battery_ov = g_sensor_pmic_live.battery_ov;
  state_out->pmic_live.vbat_mV = g_sensor_pmic_live.vbat_mV;
  state_out->pmic_live.fault_raw = g_sensor_pmic_live.fault_raw;
}

static VOID AppUiCopyAudioState(ui_router_state_t *state_out)
{
  state_out->audio_live.user_master_pct = g_audio_user_gain_master_pct;
  state_out->audio_live.user_music_pct = g_audio_user_gain_music_pct;
  state_out->audio_live.user_sfx_pct = g_audio_user_gain_sfx_pct;
  state_out->audio_live.user_ui_pct = g_audio_user_gain_ui_pct;
}

static VOID AppUiCopyLisState(ui_router_state_t *state_out)
{
  state_out->lis_live.fsm_state = g_sensor_lis.state;
  state_out->lis_live.fsm_recovery_attempts = g_sensor_lis.recovery_attempts;
  state_out->lis_live.fsm_last_error = g_sensor_lis.last_error;
  state_out->lis_live.stream_enabled = g_sensor_lis_stream_enabled;
  state_out->lis_live.profile_requested = (ULONG)g_sensor_lis_profile_requested;
  state_out->lis_live.profile_applied = (ULONG)g_sensor_lis_profile_applied;
  state_out->lis_live.addr = g_sensor_lis_live.addr;
  state_out->lis_live.whoami = g_sensor_lis_live.whoami;
  state_out->lis_live.status = g_sensor_lis_live.status;
  state_out->lis_live.sample_count = g_sensor_lis_live.sample_count;
  state_out->lis_live.fail_count = g_sensor_lis_live.fail_count;
  state_out->lis_live.last_sample_tick = g_sensor_lis_live.last_sample_tick;
  state_out->lis_live.last_error = g_sensor_lis_live.last_error;
  state_out->lis_live.x_raw = (LONG)g_sensor_lis_live.x_raw;
  state_out->lis_live.y_raw = (LONG)g_sensor_lis_live.y_raw;
  state_out->lis_live.z_raw = (LONG)g_sensor_lis_live.z_raw;
  state_out->lis_live.step_enabled = g_sensor_lis_live.step_enabled;
  state_out->lis_live.step_count = g_sensor_lis_live.step_count;
  state_out->lis_live.step_detected = g_sensor_lis_live.step_detected;
  state_out->lis_live.tilt_detected = g_sensor_lis_live.tilt_detected;
  state_out->lis_live.sigmot_detected = g_sensor_lis_live.sigmot_detected;
}

static VOID AppUiCopyPetState(ui_router_state_t *state_out)
{
  state_out->pet_state = g_pet_state;
  state_out->pet_tick_count = g_pet_tick_count;
  state_out->pet_wake_count = g_pet_wake_count;
  state_out->pet_last_action = g_pet_last_action;
  state_out->pet_hunger_pct = g_pet_hunger_pct;
  state_out->pet_energy_pct = g_pet_energy_pct;
  state_out->pet_mood_pct = g_pet_mood_pct;
  state_out->stop_select_active = g_power_stop_select_active;
  state_out->stop_select_last_input_tick = g_power_stop_select_last_input_tick;
}

static VOID AppUiBuildRouterState(ui_router_state_t *state_out)
{
  if (state_out == TX_NULL)
  {
    return;
  }

  (void)memset(state_out, 0, sizeof(*state_out));
  state_out->mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  state_out->last_input_source = g_input_last_source;
  state_out->last_input_action = g_input_action_last;
  state_out->last_input_edge = AppUiMapInputEdgeToRouter(g_input_last_edge);
  AppUiCopyJoyState(state_out);
  AppUiCopyAudioState(state_out);
  AppUiCopyPmicState(state_out);
  AppUiCopyLisState(state_out);
  AppUiCopyPetState(state_out);
}

static uint8_t AppUiShouldTick(void)
{
  ULONG mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);

  if ((mode_flags & (APP_MODE_FLAG_STOP | APP_MODE_FLAG_STATIC | APP_MODE_FLAG_FLASHING)) != 0UL)
  {
    return 1U;
  }

  if (g_ui_bootstrap_present_pending != 0UL)
  {
    return 1U;
  }

  return 0U;
}

static VOID AppUiEnsurePageForMode(ULONG mode_flags)
{
  const ui_page_t *current_page = UiRouter_CurrentPage(&g_ui_router);

  if ((mode_flags & APP_MODE_FLAG_FLASHING) != 0UL)
  {
    if (current_page != &UI_PAGE_FLASHING_NATIVE)
    {
      UiRouter_OpenPage(&g_ui_router, &UI_PAGE_FLASHING_NATIVE, TX_NULL);
    }
    return;
  }

  if (current_page == &UI_PAGE_FLASHING_NATIVE)
  {
    AppUiEnterPage(APP_UI_PAGE_HOME);
  }
}

static uint8_t AppUiUsbFlashPromptTreeActive(const ui_router_t *ui)
{
  if (ui == TX_NULL)
  {
    return 0U;
  }

  return (UiRouter_CurrentTree(ui) == (ui_menu_tree_id_t)UI_TREE_ID_USB_FLASH_PROMPT) ? 1U : 0U;
}

static VOID AppUiUsbFlashPromptHandleDismiss(void)
{
  static uint8_t prev_prompt_active = 0U;
  uint8_t prompt_active = AppUiUsbFlashPromptTreeActive(&g_ui_router);

  if ((prev_prompt_active != 0U) && (prompt_active == 0U))
  {
    /*
     * Back/cancel dismissal path for the temporary consent UI.
     * Keep policy state in one place so a future modal dialog backend
     * can swap in without touching power/USB logic.
     */
    (void)App_UsbFlashPrompt_Clear();
  }

  prev_prompt_active = prompt_active;
}

static VOID AppUiUsbFlashPromptMaybeShow(ULONG mode_flags)
{
  ULONG pending = 0UL;
  ULONG vbus_present = 0UL;

  if ((mode_flags & (APP_MODE_FLAG_STOP | APP_MODE_FLAG_STATIC)) == 0UL)
  {
    return;
  }

  if ((mode_flags & APP_MODE_FLAG_FLASHING) != 0UL)
  {
    return;
  }

  if (AppUiUsbFlashPromptTreeActive(&g_ui_router) != 0U)
  {
    return;
  }

  if (App_UsbFlashPrompt_GetPending(&pending) != TX_SUCCESS)
  {
    return;
  }
  if (pending == 0UL)
  {
    return;
  }

  if ((App_UsbVbusPresent_Get(&vbus_present) != TX_SUCCESS) || (vbus_present == 0UL))
  {
    (void)App_UsbFlashPrompt_Clear();
    return;
  }

  (void)UiRouter_PushTree(&g_ui_router, (ui_menu_tree_id_t)UI_TREE_ID_USB_FLASH_PROMPT, &UI_MENU_USB_FLASH_PROMPT);
}

static uint8_t AppUiIsExpectedMenuBoundaryNoop(const ui_input_evt_t *evt)
{
  const ui_menu_stack_frame_t *frame;
  ULONG selected_index;
  ULONG menu_count;

  if (evt == TX_NULL)
  {
    return 0U;
  }

  if ((evt->evt != UI_EVT_UP) &&
      (evt->evt != UI_EVT_LEFT) &&
      (evt->evt != UI_EVT_DOWN) &&
      (evt->evt != UI_EVT_RIGHT))
  {
    return 0U;
  }

  if (UiRouter_CurrentPage(&g_ui_router) != TX_NULL)
  {
    return 0U;
  }

  frame = UiRouter_CurrentMenu(&g_ui_router);
  if ((frame == TX_NULL) || (frame->menu == TX_NULL) || (frame->menu->count == 0U))
  {
    return 0U;
  }

  selected_index = (ULONG)frame->selected_index;
  menu_count = (ULONG)frame->menu->count;

  if (((evt->evt == UI_EVT_UP) || (evt->evt == UI_EVT_LEFT)) &&
      (selected_index == 0UL))
  {
    return 1U;
  }

  if (((evt->evt == UI_EVT_DOWN) || (evt->evt == UI_EVT_RIGHT)) &&
      (selected_index + 1UL >= menu_count))
  {
    return 1U;
  }

  return 0U;
}

static void AppUiHandleTick(void)
{
  ui_router_state_t state;
  ULONG mode_flags;
  const ui_page_t *current_page;
  uint8_t stop_now;
  uint8_t stop_prev;
  uint8_t static_now;
  uint8_t static_prev;
  uint8_t allow_bootstrap_present;
  UINT present_status;

  AppUiBuildRouterState(&state);
  UiRuntimeContext_UpdateState(&state);

  current_page = UiRouter_CurrentPage(&g_ui_router);
  mode_flags = (state.mode_flags & APP_MODE_FLAGS_ALL);
  AppUiEnsurePageForMode(mode_flags);
  current_page = UiRouter_CurrentPage(&g_ui_router);
  stop_now = ((mode_flags & APP_MODE_FLAG_STOP) != 0UL) ? 1U : 0U;
  stop_prev = ((g_ui_mode_flags_last & APP_MODE_FLAG_STOP) != 0UL) ? 1U : 0U;
  static_now = ((mode_flags & APP_MODE_FLAG_STATIC) != 0UL) ? 1U : 0U;
  static_prev = ((g_ui_mode_flags_last & APP_MODE_FLAG_STATIC) != 0UL) ? 1U : 0U;

  if ((stop_now != 0U) && (stop_prev == 0U))
  {
    AppUiEnterPage(APP_UI_PAGE_PET);
  }
  else if ((static_now != 0U) && (static_prev == 0U))
  {
    if (current_page == &UI_PAGE_PET_NATIVE)
    {
      if (UiPagePet_IsSandActive() == 0U)
      {
        app_ui_page_t entry_page = APP_UI_PAGE_HOME;

        if ((ULONG)KNOB_UI_STATIC_ENTRY_POINT == APP_UI_STATIC_ENTRY_JOY_CAL)
        {
          entry_page = APP_UI_PAGE_JOY_CAL;
        }
        else if ((ULONG)KNOB_UI_STATIC_ENTRY_POINT == APP_UI_STATIC_ENTRY_HOME)
        {
          entry_page = APP_UI_PAGE_HOME;
        }
        else
        {
          if ((g_sensor_tmag.state == (ULONG)APP_SENSOR_STATE_READY) &&
              (g_sensor_joy_input_gate_valid == 0UL) &&
              (g_sensor_joy_cal_active == 0UL))
          {
            entry_page = APP_UI_PAGE_JOY_CAL;
          }
        }

        AppUiEnterPage(entry_page);
      }
    }
  }

  AppUiUsbFlashPromptHandleDismiss();
  AppUiUsbFlashPromptMaybeShow(mode_flags);

  g_ui_mode_flags_last = mode_flags;

  allow_bootstrap_present = ((g_ui_bootstrap_present_pending != 0UL) && (UiRouter_IsDirty(&g_ui_router) != 0U)) ? 1U : 0U;
  if (((mode_flags & (APP_MODE_FLAG_STOP | APP_MODE_FLAG_STATIC | APP_MODE_FLAG_FLASHING)) == 0UL) &&
      (allow_bootstrap_present == 0U))
  {
    return;
  }

  {
    ui_input_evt_t tick_evt;
    (void)memset(&tick_evt, 0, sizeof(tick_evt));
    tick_evt.evt = UI_EVT_TICK;
    (void)UiRouter_HandleEvent(&g_ui_router, &tick_evt);
  }

  if (UiRouter_IsDirty(&g_ui_router) != 0U)
  {
    if (AppRendererLock() == TX_SUCCESS)
    {
      UiRouter_Render(&g_ui_router);
      AppRendererUnlock();
      present_status = App_Display_Present();
      if ((present_status == TX_SUCCESS) && (g_display_ready != 0U))
      {
        UiRouter_ClearDirty(&g_ui_router);
        g_ui_bootstrap_present_pending = 0UL;
        g_ui_boot_ready = 1UL;
      }
    }
  }
}

static void AppUiEnterPage(app_ui_page_t page)
{
  UiRouter_Reset(&g_ui_router, &UI_MENU_ROOT);
  UiRouter_SetActionHandler(&g_ui_router, AppUiRouterMenuAction);

  if (page == APP_UI_PAGE_PET)
  {
    UiRouter_OpenPage(&g_ui_router, &UI_PAGE_PET_NATIVE, TX_NULL);
  }
  else if (page == APP_UI_PAGE_JOY_CAL)
  {
    UiRouter_OpenPage(&g_ui_router, &UI_PAGE_JOY_CAL_NATIVE, TX_NULL);
  }
  else
  {
    UiRouter_OpenPage(&g_ui_router, &UI_PAGE_HOME_NATIVE, TX_NULL);
  }
}

static uint8_t AppUiAudioEventForAction(const ui_action_evt_t *ui_evt, app_audio_event_t *event_out)
{
  if ((ui_evt == TX_NULL) || (event_out == TX_NULL))
  {
    return 0U;
  }

  if ((ui_evt->event == (ULONG)UI_EVENT_PRESS) || (ui_evt->event == (ULONG)UI_EVENT_REPEAT))
  {
    if ((ui_evt->action == (ULONG)UI_ACTION_BTN_L) ||
        (ui_evt->action == (ULONG)UI_ACTION_BTN_R) ||
        (ui_evt->action == (ULONG)UI_ACTION_JOY_UP) ||
        (ui_evt->action == (ULONG)UI_ACTION_JOY_RIGHT) ||
        (ui_evt->action == (ULONG)UI_ACTION_JOY_DOWN) ||
        (ui_evt->action == (ULONG)UI_ACTION_JOY_LEFT))
    {
      *event_out = APP_AUDIO_EVENT_UI_NAV;
      return 1U;
    }
  }

  if (ui_evt->event == (ULONG)UI_EVENT_PRESS)
  {
    if (ui_evt->action == (ULONG)UI_ACTION_BTN_A)
    {
      *event_out = APP_AUDIO_EVENT_UI_CONFIRM;
      return 1U;
    }

    if ((ui_evt->action == (ULONG)UI_ACTION_BTN_B) ||
        (ui_evt->action == (ULONG)UI_ACTION_BTN_BOOT))
    {
      *event_out = APP_AUDIO_EVENT_UI_CANCEL;
      return 1U;
    }
  }

  return 0U;
}

static uint8_t AppUiHandleAction(const app_input_action_evt_t *evt)
{
  ULONG mode_flags;
  ULONG ui_action;
  uint32_t router_result = UI_EVT_RESULT_NONE;
  ui_action_evt_t ui_evt;
  ui_input_evt_t ui_evt_msg;
  uint8_t handled = 0U;
  uint8_t router_input_valid = 0U;
  uint8_t router_policy_reject = 0U;
  uint8_t router_menu_boundary_noop = 0U;

  if (evt == NULL)
  {
    AppUiDiagInc(&g_ui_ignore_null_evt_count);
    return 0U;
  }

  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  if ((mode_flags & (APP_MODE_FLAG_STOP | APP_MODE_FLAG_STATIC)) == 0UL)
  {
    AppUiDiagInc(&g_ui_ignore_not_ui_mode_count);
    return 0U;
  }

  if ((mode_flags & APP_MODE_FLAG_STOP) != 0UL)
  {
    if ((evt->source == APP_INPUT_SOURCE_JOY_UP) ||
        (evt->source == APP_INPUT_SOURCE_JOY_RIGHT) ||
        (evt->source == APP_INPUT_SOURCE_JOY_DOWN) ||
        (evt->source == APP_INPUT_SOURCE_JOY_LEFT))
    {
      AppUiDiagInc(&g_ui_ignore_stop_joy_count);
      return 0U;
    }
  }

  ui_action = AppUiMapInputActionToUiAction(evt->action, evt->source);
  if (ui_action == (ULONG)UI_ACTION_NONE)
  {
    AppUiDiagInc(&g_ui_ignore_unmapped_action_count);
    return 0U;
  }

  ui_evt.action = ui_action;
  ui_evt.source = evt->source;
  ui_evt.event = AppUiMapInputEventToRouter(evt->event);
  ui_evt.tick = evt->tick;
  ui_evt.pressed_mask = evt->pressed_mask;
  if (ui_evt.event == 0UL)
  {
    AppUiDiagInc(&g_ui_ignore_unmapped_event_count);
    return 0U;
  }

  if (AppUiBuildInputEvent(evt, &ui_evt_msg) != 0U)
  {
    router_input_valid = 1U;
    const ui_page_t *current_page = UiRouter_CurrentPage(&g_ui_router);

    if (((ui_evt_msg.evt == UI_EVT_BACK) || (ui_evt_msg.evt == UI_EVT_LONG_BACK)) &&
        (current_page == &UI_PAGE_HOME_NATIVE))
    {
      (void)App_AudioReq_PlayEvent(APP_AUDIO_EVENT_UI_DENIED);
      AppUiDiagInc(&g_ui_denied_audio_emit_count);
      return 1U;
    }

    if ((current_page != TX_NULL) &&
        (current_page->input_policy != TX_NULL) &&
        (current_page->input_policy(&ui_evt_msg) == 0U))
    {
      router_policy_reject = 1U;
    }
    else
    {
      router_menu_boundary_noop = AppUiIsExpectedMenuBoundaryNoop(&ui_evt_msg);
      router_result = UiRouter_HandleEvent(&g_ui_router, &ui_evt_msg);
      handled = ((router_result & UI_EVT_RESULT_HANDLED) != 0U) ? 1U : 0U;
    }

    if ((handled != 0U) &&
        ((ui_evt_msg.evt == UI_EVT_BACK) || (ui_evt_msg.evt == UI_EVT_LONG_BACK)) &&
        (UiRouter_CurrentPage(&g_ui_router) == &UI_PAGE_PET_NATIVE) &&
        ((g_ui_mode_flags_last & APP_MODE_FLAG_STATIC) != 0UL))
    {
      (void)App_SysEvent_ModeSet(APP_MODE_STOP);
    }

    if ((handled == 0U) &&
        ((ui_evt_msg.evt == UI_EVT_BACK) || (ui_evt_msg.evt == UI_EVT_LONG_BACK)) &&
        (AppUiAtRootMenu() != 0U))
    {
      UiRouter_OpenPage(&g_ui_router, &UI_PAGE_HOME_NATIVE, TX_NULL);
      handled = 1U;
    }
  }

  if (handled != 0U)
  {
    app_audio_event_t audio_event = APP_AUDIO_EVENT_NONE;
    if (AppUiAudioEventForAction(&ui_evt, &audio_event) != 0U)
    {
      (void)App_AudioReq_PlayEvent(audio_event);
    }
    return 1U;
  }

  if (router_input_valid != 0U)
  {
    if (router_policy_reject != 0U)
    {
      AppUiDiagInc(&g_ui_ignore_router_policy_reject_count);
    }
    else if ((router_result == UI_EVT_RESULT_NONE) && (router_menu_boundary_noop != 0U))
    {
      AppUiDiagInc(&g_ui_ignore_router_menu_boundary_noop_count);
    }
    else
    {
      AppUiDiagInc(&g_ui_ignore_router_unhandled_count);
    }
  }
  else
  {
    AppUiDiagInc(&g_ui_ignore_router_map_fail_count);
  }

  if ((ui_evt.event == (ULONG)UI_EVENT_PRESS) &&
      ((ui_evt.action == (ULONG)UI_ACTION_BTN_A) ||
       (ui_evt.action == (ULONG)UI_ACTION_BTN_B) ||
       (ui_evt.action == (ULONG)UI_ACTION_BTN_BOOT)))
  {
    (void)App_AudioReq_PlayEvent(APP_AUDIO_EVENT_UI_DENIED);
    AppUiDiagInc(&g_ui_denied_audio_emit_count);
  }
  return 0U;
}

static uint8_t AppUiBuildInputEvent(const app_input_action_evt_t *src, ui_input_evt_t *dst)
{
  uint8_t is_nav_action = 0U;
  ULONG ui_action;

  if ((src == TX_NULL) || (dst == TX_NULL))
  {
    return 0U;
  }

  dst->source = (uint32_t)src->source;
  dst->tick = (uint32_t)src->tick;
  dst->pressed_mask = (uint32_t)src->pressed_mask;
  dst->action = (uint32_t)UI_INPUT_ACTION_NONE;
  dst->evt = UI_EVT_NONE;

  ui_action = AppUiMapInputActionToUiAction(src->action, src->source);
  dst->action = (uint32_t)ui_action;
  switch ((ui_action_id_t)ui_action)
  {
    case UI_ACTION_JOY_UP:
      dst->evt = UI_EVT_UP;
      is_nav_action = 1U;
      break;
    case UI_ACTION_JOY_DOWN:
      dst->evt = UI_EVT_DOWN;
      is_nav_action = 1U;
      break;
    case UI_ACTION_BTN_L:
    case UI_ACTION_JOY_LEFT:
      dst->evt = UI_EVT_LEFT;
      is_nav_action = 1U;
      break;
    case UI_ACTION_BTN_R:
    case UI_ACTION_JOY_RIGHT:
      dst->evt = UI_EVT_RIGHT;
      is_nav_action = 1U;
      break;
    case UI_ACTION_BTN_A:
      dst->evt = UI_EVT_SELECT;
      break;
    case UI_ACTION_BTN_B:
    case UI_ACTION_BTN_BOOT:
      dst->evt = UI_EVT_BACK;
      break;
    default:
      AppUiDiagInc(&g_ui_ignore_router_no_evt_count);
      return 0U;
  }

  if ((app_input_event_t)src->event == APP_INPUT_EVENT_LONG)
  {
    if (dst->evt == UI_EVT_SELECT)
    {
      dst->evt = UI_EVT_LONG_SELECT;
    }
    else if (dst->evt == UI_EVT_BACK)
    {
      dst->evt = UI_EVT_LONG_BACK;
    }
    else
    {
      AppUiDiagInc(&g_ui_ignore_router_long_reject_count);
      return 0U;
    }
  }
  else if ((app_input_event_t)src->event == APP_INPUT_EVENT_REPEAT)
  {
    /* In router mode, only directional navigation actions should repeat. */
    if (is_nav_action == 0U)
    {
      AppUiDiagInc(&g_ui_ignore_router_repeat_reject_count);
      return 0U;
    }
  }
  else if ((app_input_event_t)src->event != APP_INPUT_EVENT_PRESS)
  {
    /* Release does not map to router logical events. */
    AppUiDiagInc(&g_ui_ignore_router_release_reject_count);
    return 0U;
  }

  return 1U;
}

static uint8_t AppUiAtRootMenu(void)
{
  const ui_menu_stack_frame_t *frame = UiRouter_CurrentMenu(&g_ui_router);
  const ui_page_t *page = UiRouter_CurrentPage(&g_ui_router);

  if (page != TX_NULL)
  {
    return 0U;
  }
  if ((frame == TX_NULL) || (frame->menu != &UI_MENU_ROOT))
  {
    return 0U;
  }

  return 1U;
}

