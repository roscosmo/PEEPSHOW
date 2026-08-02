/* Extracted from app_threadx.c for thread-level organization. */

static uint8_t AppGameIgnoreJoystickToggleEvent(const app_input_action_evt_t *evt)
{
  if (evt == TX_NULL)
  {
    return 1U;
  }

  if ((evt->source == APP_INPUT_SOURCE_JOY_LEFT) ||
      (evt->source == APP_INPUT_SOURCE_JOY_RIGHT))
  {
    if ((evt->action == APP_INPUT_ACTION_LEFT) ||
        (evt->action == APP_INPUT_ACTION_RIGHT))
    {
      return 1U;
    }
  }

  return 0U;
}

static VOID AppGameThreadEntry(ULONG thread_input)
{
  UINT status;
  app_input_action_evt_t evt;
  ULONG frame_period_ticks = 1UL;
  ULONG next_frame_tick = 0UL;
  uint8_t realtime_active = 0U;
  uint8_t realtime_lis_stream_active = 0U;

  (void)thread_input;

  if ((ULONG)KNOB_UI_FPS > 0UL)
  {
    frame_period_ticks = ((ULONG)TX_TIMER_TICKS_PER_SECOND / (ULONG)KNOB_UI_FPS);
    if (frame_period_ticks == 0UL)
    {
      frame_period_ticks = 1UL;
    }
  }

  for (;;)
  {
    ULONG mode_flags = (g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAGS_ALL);
    if ((mode_flags & APP_MODE_FLAG_REALTIME) != 0UL)
    {
      ULONG now_tick;
      ULONG delta_ticks;
      UINT wait_ticks;
      UINT recv_i;

      if (g_game_exit_to_static_pending != 0UL)
      {
        if (App_SysEvent_ModeSet(APP_MODE_STATIC) == TX_SUCCESS)
        {
          g_game_exit_to_static_pending = 0UL;
        }
      }

      if (realtime_active == 0U)
      {
        realtime_active = 1U;
        if (realtime_lis_stream_active == 0U)
        {
          if (App_SensorReq_LisStreamStart() == TX_SUCCESS)
          {
            realtime_lis_stream_active = 1U;
          }
        }
        RenderDemo_Reset();
        next_frame_tick = tx_time_get() + frame_period_ticks;
      }

      now_tick = tx_time_get();
      if ((LONG)(now_tick - next_frame_tick) < 0L)
      {
        delta_ticks = (ULONG)(next_frame_tick - now_tick);
        wait_ticks = (UINT)delta_ticks;
        if (wait_ticks == 0U)
        {
          wait_ticks = 1U;
        }
      }
      else
      {
        wait_ticks = TX_NO_WAIT;
      }

      status = tx_queue_receive(&g_q_game_events, &evt, wait_ticks);
      if (status == TX_SUCCESS)
      {
        g_game_event_recv_count++;
        g_game_event_last_action = evt.action;
        if (AppGameIgnoreJoystickToggleEvent(&evt) != 0U)
        {
          g_game_event_ignored_count++;
        }
        else
        {
          if (AppGameHandleAction(&evt) != 0U)
          {
            g_game_event_handled_count++;
          }
          else
          {
            g_game_event_ignored_count++;
          }
        }

        for (recv_i = 0U; recv_i < 7U; ++recv_i)
        {
          status = tx_queue_receive(&g_q_game_events, &evt, TX_NO_WAIT);
          if (status != TX_SUCCESS)
          {
            break;
          }

          g_game_event_recv_count++;
          g_game_event_last_action = evt.action;
          if (AppGameIgnoreJoystickToggleEvent(&evt) != 0U)
          {
            g_game_event_ignored_count++;
          }
          else if (AppGameHandleAction(&evt) != 0U)
          {
            g_game_event_handled_count++;
          }
          else
          {
            g_game_event_ignored_count++;
          }
        }
      }
      else if ((status != TX_QUEUE_EMPTY) && (status != TX_NO_EVENTS))
      {
        g_game_event_queue_error_count++;
      }

      now_tick = tx_time_get();
      if ((LONG)(now_tick - next_frame_tick) >= 0L)
      {
        if (AppRendererLock() == TX_SUCCESS)
        {
          app_sensor_snapshot_t sensor_snapshot;
          if (App_SensorSnapshot_Get(&sensor_snapshot) == TX_SUCCESS)
          {
            RenderDemo_DrawFrame(&sensor_snapshot);
          }
          else
          {
            RenderDemo_DrawFrame((const app_sensor_snapshot_t *)0);
          }
          AppRendererUnlock();
          (void)App_Display_Present();
        }

        next_frame_tick += frame_period_ticks;
        if ((LONG)(now_tick - next_frame_tick) >= 0L)
        {
          next_frame_tick = now_tick + frame_period_ticks;
        }
      }
      continue;
    }

    if (realtime_active != 0U)
    {
      realtime_active = 0U;
      if (realtime_lis_stream_active != 0U)
      {
        (void)App_SensorReq_LisStreamStop();
        realtime_lis_stream_active = 0U;
      }
      RenderDemo_Reset();
      g_game_exit_to_static_pending = 0UL;
    }

    status = tx_queue_receive(&g_q_game_events, &evt, KNOB_RTOS_GAME_WAIT_TICKS);
    if (status == TX_SUCCESS)
    {
      g_game_event_recv_count++;
      g_game_event_last_action = evt.action;
      if (AppGameIgnoreJoystickToggleEvent(&evt) != 0U)
      {
        g_game_event_ignored_count++;
      }
      else if (AppGameHandleAction(&evt) != 0U)
      {
        g_game_event_handled_count++;
      }
      else
      {
        g_game_event_ignored_count++;
      }
    }
    else if (status != TX_QUEUE_EMPTY)
    {
      /* Queue error in game input consumer: continue receiving future events. */
      g_game_event_queue_error_count++;
    }
  }
}
