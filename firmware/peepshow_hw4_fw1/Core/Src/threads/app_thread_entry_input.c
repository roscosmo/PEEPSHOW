/* Thread entry implementation for App_ThreadX runtime. */

/* Input helpers. */
static uint8_t AppInputResolveSource(uint16_t gpio_pin, GPIO_TypeDef **port_out, ULONG *source_out);
static uint8_t AppInputTranslateRaw(const app_input_raw_evt_t *raw_evt, app_input_action_evt_t *action_evt);
static VOID AppInputRouteAction(const app_input_action_evt_t *action_evt);
static uint8_t AppInputSourceValid(ULONG source);
static ULONG AppInputSourceBit(ULONG source);
static uint8_t AppInputSourceIsPhysicalButton(ULONG source);
static uint8_t AppInputSourceIsJoystick(ULONG source);
static uint8_t AppInputReadPhysicalLevel(ULONG source, ULONG *level_out);
static VOID AppInputRefreshPhysicalIdleLevels(void);
static uint8_t AppInputPhysicalPressedFromLevel(ULONG source, ULONG level, ULONG *pressed_out);
static uint8_t AppInputSourceRepeatEnabled(ULONG source);
static uint8_t AppInputSourceLongEnabled(ULONG source);
static ULONG AppInputRepeatPeriodTicks(ULONG source, ULONG held_ticks);
static UINT AppInputPushActionToQueue(TX_QUEUE *queue, const app_input_action_evt_t *evt, ULONG *drop_oldest_counter);
static VOID AppInputDropQueuedRepeatsForSource(TX_QUEUE *queue, ULONG source);
static uint8_t AppInputTickDue(ULONG now_tick, ULONG deadline_tick);
static uint8_t AppInputShouldDebounce(ULONG source, ULONG edge, ULONG tick);
static VOID AppInputProcessRepeat(ULONG now_tick);
static UINT AppInputPostSystemEvent(app_sys_event_type_t event_type, ULONG arg0, ULONG arg1, ULONG arg2);
static VOID AppInputPostRawEvent(ULONG source, ULONG edge, ULONG level, ULONG tick);
static uint8_t AppInputStopWakeSourceAllowed(ULONG source);
static VOID AppInputCaptureStopWakeWhileQuiesced(const app_input_raw_evt_t *raw_evt);
static VOID AppInputReplayStopWakePending(void);

static uint8_t AppInputStopWakeSourceAllowed(ULONG source)
{
  switch (source)
  {
    case APP_INPUT_SOURCE_BTN_A:
    case APP_INPUT_SOURCE_BTN_B:
    case APP_INPUT_SOURCE_BTN_L:
    case APP_INPUT_SOURCE_BTN_R:
      return 1U;
    default:
      return 0U;
  }
}

static VOID AppInputCaptureStopWakeWhileQuiesced(const app_input_raw_evt_t *raw_evt)
{
  ULONG mode_flags;
  ULONG bit;

  if (raw_evt == TX_NULL)
  {
    return;
  }

  mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
  if ((mode_flags & APP_MODE_FLAG_STOP) == 0UL)
  {
    return;
  }
  if (raw_evt->edge != APP_INPUT_EDGE_LOW)
  {
    return;
  }
  if (AppInputStopWakeSourceAllowed(raw_evt->source) == 0U)
  {
    return;
  }

  bit = AppInputSourceBit(raw_evt->source);
  if (bit == 0UL)
  {
    return;
  }

  g_input_stop_wake_pending_mask |= bit;
  g_input_stop_wake_pending_tick[raw_evt->source] = raw_evt->tick;
}

static VOID AppInputReplayStopWakePending(void)
{
  ULONG source;
  ULONG pending_mask = g_input_stop_wake_pending_mask;
  ULONG replay_tick = (ULONG)HAL_GetTick();

  if (pending_mask == 0UL)
  {
    return;
  }

  g_input_stop_wake_pending_mask = 0UL;
  for (source = APP_INPUT_SOURCE_BTN_A; source <= APP_INPUT_SOURCE_BTN_R; source++)
  {
    ULONG bit = AppInputSourceBit(source);
    ULONG tick = replay_tick;

    if (bit == 0UL)
    {
      continue;
    }
    if ((pending_mask & bit) == 0UL)
    {
      continue;
    }
    AppInputPostRawEvent(source, APP_INPUT_EDGE_LOW, 0UL, tick);
    /* STOP wake replay is a one-shot tap; never leave sources latched pressed. */
    AppInputPostRawEvent(source, APP_INPUT_EDGE_HIGH, 1UL, tick + 1UL);

    g_input_stop_wake_pending_tick[source] = 0UL;
  }
}

static VOID AppInputThreadEntry(ULONG thread_input)
{
  UINT status;
  app_input_cmd_t cmd;
  app_input_raw_evt_t raw_evt;
  app_input_action_evt_t action_evt;
  ULONG now_tick;

  (void)thread_input;

  for (;;)
  {
    status = tx_queue_receive(&g_q_input_cmd, &cmd, TX_NO_WAIT);
    if (status == TX_SUCCESS)
    {
      switch ((app_input_cmd_type_t)cmd.type)
      {
        case APP_INPUT_CMD_QUIESCE:
          g_input_quiesced = 1UL;
          (void)memset(g_input_button_state, 0, sizeof(g_input_button_state));
          AppInputRefreshPhysicalIdleLevels();
          (void)App_SysEvent_QuiesceAck(APP_POWER_ACK_SRC_INPUT);
          break;

        case APP_INPUT_CMD_RESUME:
          g_input_quiesced = 0UL;
          (void)memset(g_input_button_state, 0, sizeof(g_input_button_state));
          AppInputRefreshPhysicalIdleLevels();
          AppInputReplayStopWakePending();
          break;

        default:
          break;
      }
    }
    else if (status != TX_QUEUE_EMPTY)
    {
      /* Queue error in input stub: continue processing future commands. */
    }

    status = tx_queue_receive(&g_q_input_raw, &raw_evt, KNOB_RTOS_INPUT_WAIT_TICKS);
    if (status == TX_SUCCESS)
    {
      if (g_input_quiesced != 0UL)
      {
        AppInputCaptureStopWakeWhileQuiesced(&raw_evt);
        g_input_raw_suppressed_count++;
        continue;
      }

      if (AppInputShouldDebounce(raw_evt.source, raw_evt.edge, raw_evt.tick) != 0U)
      {
        g_input_debounce_drop_count++;
        continue;
      }

      g_input_raw_recv_count++;
      g_input_last_source = raw_evt.source;
      g_input_last_edge = raw_evt.edge;
      g_input_last_tick = raw_evt.tick;
      g_input_last_level = raw_evt.level;

      if (AppInputSourceValid(raw_evt.source) != 0U)
      {
        app_input_button_state_t *state = &g_input_button_state[raw_evt.source];
        if (raw_evt.edge == APP_INPUT_EDGE_LOW)
        {
          ULONG repeat_delay_ticks;
          ULONG repeat_period_ticks;

          state->pressed = 1UL;
          state->press_tick = raw_evt.tick;
          state->long_sent = 0UL;

          if (AppInputSourceIsJoystick(raw_evt.source) != 0U)
          {
            repeat_delay_ticks = (ULONG)KNOB_INPUT_JOY_REPEAT_DELAY_TICKS;
            repeat_period_ticks = (ULONG)KNOB_INPUT_JOY_REPEAT_PERIOD_TICKS;
          }
          else
          {
            repeat_delay_ticks = (ULONG)KNOB_INPUT_REPEAT_DELAY_TICKS;
            repeat_period_ticks = (ULONG)KNOB_INPUT_REPEAT_PERIOD_TICKS;
          }

          if ((AppInputSourceRepeatEnabled(raw_evt.source) != 0U) &&
              (repeat_delay_ticks > 0UL) &&
              (repeat_period_ticks > 0UL))
          {
            state->next_repeat_tick = (raw_evt.tick + repeat_delay_ticks);
          }
          else
          {
            state->next_repeat_tick = 0UL;
          }
        }
        else if (raw_evt.edge == APP_INPUT_EDGE_HIGH)
        {
          state->pressed = 0UL;
          state->long_sent = 0UL;
          state->next_repeat_tick = 0UL;
          AppInputDropQueuedRepeatsForSource(&g_q_ui_events, raw_evt.source);
          AppInputDropQueuedRepeatsForSource(&g_q_game_events, raw_evt.source);
        }
      }

      if (AppInputTranslateRaw(&raw_evt, &action_evt) != 0U)
      {
        AppInputRouteAction(&action_evt);
      }
    }
    else if (status != TX_QUEUE_EMPTY)
    {
      /* Queue error in input raw path: continue processing future events. */
    }

    if (g_input_quiesced == 0UL)
    {
      now_tick = (ULONG)HAL_GetTick();
      AppInputProcessRepeat(now_tick);
    }
  }
}
