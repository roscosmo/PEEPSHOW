/* Extracted from app_threadx.c for thread-level organization. */

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
