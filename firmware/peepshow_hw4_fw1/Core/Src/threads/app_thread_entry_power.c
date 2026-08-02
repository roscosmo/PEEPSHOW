/* Extracted from app_threadx.c for thread-level organization. */

static VOID AppPowerThreadEntry(ULONG thread_input)
{
  UINT status;
  app_sys_event_t msg;

  (void)thread_input;

  for (;;)
  {
    status = tx_queue_receive(&g_q_sys_events, &msg, KNOB_RTOS_POWER_WAIT_TICKS);
    if (status == TX_SUCCESS)
    {
      switch ((app_sys_event_type_t)msg.type)
      {
        case APP_SYS_EVT_MODE_SET:
          if (AppSetModeFlag((app_mode_t)msg.arg0) == TX_SUCCESS)
          {
            AppPowerPerfOnModeChange((app_mode_t)msg.arg0, tx_time_get());
            (void)AppDisplayCmdPost(APP_DISPLAY_CMD_SET_MODE, msg.arg0);
            (void)AppDisplayCmdPost(APP_DISPLAY_CMD_PRESENT, 0UL);
            (void)AppSensorReqPost(APP_SENSOR_REQ_MODE_CHANGED, msg.arg0, 0UL, 0UL);
          }
          break;

        case APP_SYS_EVT_QUIESCE_REQ:
        {
          UINT display_status;
          UINT storage_status;
          UINT input_status;
          UINT audio_status;
          UINT sensor_status;

          g_power_pending_ack_mask = APP_POWER_ACK_MASK_ALL;
          g_power_quiesce_wait_active = 1UL;
          g_power_quiesce_wait_elapsed_ticks = 0UL;
          (void)AppPowerFlagsUpdate(APP_POWER_FLAG_QUIESCE_REQ,
                                    (APP_POWER_FLAG_QUIESCED | APP_POWER_FLAG_RESUME_REQ | APP_POWER_FLAG_RUNNING | APP_POWER_FLAG_QUIESCE_TIMEOUT));

          display_status = AppDisplayCmdPost(APP_DISPLAY_CMD_QUIESCE, 0UL);
          storage_status = AppStorageReqPost(APP_STORAGE_REQ_QUIESCE, 0UL);
          input_status = AppInputCmdPost(APP_INPUT_CMD_QUIESCE, 0UL);
          audio_status = AppAudioCmdPost(APP_AUDIO_CMD_QUIESCE, 0UL);
          sensor_status = AppSensorReqPost(APP_SENSOR_REQ_QUIESCE, 0UL, 0UL, 0UL);
          if ((display_status != TX_SUCCESS) || (storage_status != TX_SUCCESS) || (input_status != TX_SUCCESS) || (audio_status != TX_SUCCESS) || (sensor_status != TX_SUCCESS))
          {
            g_power_quiesce_wait_active = 0UL;
            g_power_pending_ack_mask = 0UL;
            (void)AppPowerFlagsUpdate(APP_POWER_FLAG_QUIESCE_TIMEOUT, APP_POWER_FLAG_QUIESCED);
          }
          break;
        }

        case APP_SYS_EVT_RESUME_REQ:
          g_power_pending_ack_mask = 0UL;
          g_power_quiesce_wait_active = 0UL;
          g_power_quiesce_wait_elapsed_ticks = 0UL;
          (void)AppPowerFlagsUpdate((APP_POWER_FLAG_RESUME_REQ | APP_POWER_FLAG_RUNNING),
                                    (APP_POWER_FLAG_QUIESCE_REQ | APP_POWER_FLAG_QUIESCED | APP_POWER_FLAG_QUIESCE_TIMEOUT));
          (void)AppDisplayCmdPost(APP_DISPLAY_CMD_RESUME, 0UL);
          (void)AppStorageReqPost(APP_STORAGE_REQ_RESUME, 0UL);
          (void)AppInputCmdPost(APP_INPUT_CMD_RESUME, 0UL);
          (void)AppAudioCmdPost(APP_AUDIO_CMD_RESUME, 0UL);
          (void)AppSensorReqPost(APP_SENSOR_REQ_RESUME, 0UL, 0UL, 0UL);
          break;

        case APP_SYS_EVT_QUIESCE_ACK:
        {
          ULONG ack_mask = (msg.arg0 & APP_POWER_ACK_MASK_ALL);

          if ((g_power_quiesce_wait_active != 0UL) && (ack_mask != 0UL))
          {
            g_power_pending_ack_mask &= ~ack_mask;
          }

          if ((g_power_quiesce_wait_active != 0UL) && (g_power_pending_ack_mask == 0UL))
          {
            g_power_quiesce_wait_active = 0UL;
            g_power_quiesce_wait_elapsed_ticks = 0UL;
            (void)AppPowerFlagsUpdate(APP_POWER_FLAG_QUIESCED, 0UL);
          }
          break;
        }

        case APP_SYS_EVT_INPUT_ACTIVITY:
          g_power_input_activity_count++;
          g_power_last_input_tick = msg.arg2;
          break;

        case APP_SYS_EVT_INPUT_MENU:
          g_power_menu_event_count++;
          break;

        case APP_SYS_EVT_PERF_HINT:
          g_power_perf_hint_inflight = 0UL;
          if (g_power_perf_hint_rx_count < 0xFFFFFFFFUL)
          {
            g_power_perf_hint_rx_count++;
          }
          AppPowerPerfHandleHint(msg.arg0, tx_time_get());
          break;

        default:
          /* Unknown event type: ignore for forward compatibility. */
          break;
      }
    }
    else if (status == TX_QUEUE_EMPTY)
    {
      if ((g_power_quiesce_wait_active != 0UL) && (g_power_pending_ack_mask != 0UL))
      {
        if (g_power_quiesce_wait_elapsed_ticks < KNOB_RTOS_POWER_QUIESCE_TIMEOUT_TICKS)
        {
          ULONG remaining = KNOB_RTOS_POWER_QUIESCE_TIMEOUT_TICKS - g_power_quiesce_wait_elapsed_ticks;
          if (remaining <= KNOB_RTOS_POWER_WAIT_TICKS)
          {
            g_power_quiesce_wait_elapsed_ticks = KNOB_RTOS_POWER_QUIESCE_TIMEOUT_TICKS;
          }
          else
          {
            g_power_quiesce_wait_elapsed_ticks += KNOB_RTOS_POWER_WAIT_TICKS;
          }
        }

        if (g_power_quiesce_wait_elapsed_ticks >= KNOB_RTOS_POWER_QUIESCE_TIMEOUT_TICKS)
        {
          g_power_quiesce_wait_active = 0UL;
          g_power_pending_ack_mask = 0UL;
          (void)AppPowerFlagsUpdate(APP_POWER_FLAG_QUIESCE_TIMEOUT, APP_POWER_FLAG_QUIESCED);
        }
      }
    }
    else if (status != TX_QUEUE_EMPTY)
    {
      /* Fail-safe default mode if queue operation returns an unexpected status. */
      g_power_pending_ack_mask = 0UL;
      g_power_quiesce_wait_active = 0UL;
      g_power_quiesce_wait_elapsed_ticks = 0UL;
      (void)AppSetModeFlag(APP_MODE_STOP);
      (void)AppPowerFlagsUpdate(APP_POWER_FLAG_RUNNING, (APP_POWER_FLAGS_ALL & ~APP_POWER_FLAG_RUNNING));
    }
  }
}
