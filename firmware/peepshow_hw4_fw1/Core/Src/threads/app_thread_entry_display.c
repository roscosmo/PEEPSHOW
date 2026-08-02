/* Extracted from app_threadx.c for thread-level organization. */

static VOID AppDisplayThreadEntry(ULONG thread_input)
{
  UINT status;
  app_display_cmd_t cmd;
  ULONG t0;
  ULONG t1;

  (void)thread_input;
  if (AppRendererLock() == TX_SUCCESS)
  {
    AppDisplayPrepareBootstrapFrame();
    AppRendererUnlock();
  }
  AppDebugDisplayStackSample();

  for (;;)
  {
    AppDebugDisplayStackSample();
    status = tx_queue_receive(&g_q_display_cmd, &cmd, KNOB_RTOS_DISPLAY_WAIT_TICKS);
    if (status == TX_SUCCESS)
    {
      switch ((app_display_cmd_type_t)cmd.type)
      {
        case APP_DISPLAY_CMD_QUIESCE:
          g_display_present_pending = 0UL;
          AppDisplaySetCs(0U);
          AppDisplaySetTranslator(1U);
          (void)App_SysEvent_QuiesceAck(APP_POWER_ACK_SRC_DISPLAY);
          break;

        case APP_DISPLAY_CMD_RESUME:
          g_display_present_pending = 0UL;
          AppDisplaySetTranslator(1U);
          if (AppRendererLock() == TX_SUCCESS)
          {
            Render_MarkDirtyAll();
            AppRendererUnlock();
          }
          t0 = tx_time_get();
          (void)AppDisplayPresent();
          t1 = tx_time_get();
          AppPowerPerfHintPost((ULONG)(t1 - t0));
          break;

        case APP_DISPLAY_CMD_INVALIDATE_ALL:
          if (AppRendererLock() == TX_SUCCESS)
          {
            Render_MarkDirtyAll();
            AppRendererUnlock();
          }
          break;

        case APP_DISPLAY_CMD_PRESENT:
          g_display_present_pending = 0UL;
          t0 = tx_time_get();
          (void)AppDisplayPresent();
          t1 = tx_time_get();
          AppPowerPerfHintPost((ULONG)(t1 - t0));
          break;

        case APP_DISPLAY_CMD_SET_MODE:
        {
          th_mode_t mode = TH_MODE_STOP;

          if (AppModeToThMode((app_mode_t)cmd.arg0, &mode) != 0U)
          {
            if (AppRendererLock() == TX_SUCCESS)
            {
              Render_SetModeIndicator(mode);
              AppRendererUnlock();
            }
          }
          break;
        }

        default:
          break;
      }

      AppDebugDisplayStackSample();
    }
    else if (status == TX_QUEUE_EMPTY)
    {
      if ((g_display_present_pending != 0UL) &&
          (g_q_display_cmd.tx_queue_enqueued == 0UL))
      {
        g_display_present_pending = 0UL;
      }
    }
    else if (status != TX_QUEUE_EMPTY)
    {
      /* Queue error in display thread: continue processing future commands. */
    }
  }
}
