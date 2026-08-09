/* Thread entry implementation for App_ThreadX runtime. */

/* Display helpers. */
static void AppDisplaySetTranslator(uint8_t enabled);
static void AppDisplaySetCs(uint8_t active);
static HAL_StatusTypeDef AppDisplayEnsureReady(void);
static uint8_t AppModeToThMode(app_mode_t mode_token, th_mode_t *mode_out);
static void AppDisplayPrepareBootstrapFrame(void);
static void AppDebugDisplayStackSample(void);
static UINT AppRendererLock(void);
static VOID AppRendererUnlock(void);
static HAL_StatusTypeDef AppDisplayPresent(uint16_t *dirty_rows_out, uint8_t *full_flush_out);

static VOID AppDisplayThreadEntry(ULONG thread_input)
{
  UINT status;
  app_display_cmd_t cmd;
  ULONG t0;
  ULONG t1;
  ULONG draw_ticks;
  uint16_t dirty_rows;
  uint8_t full_flush;

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
          if (g_ui_boot_ready != 0UL)
          {
            if (AppRendererLock() == TX_SUCCESS)
            {
              Render_MarkDirtyAll();
              AppRendererUnlock();
            }
            dirty_rows = 0U;
            full_flush = 0U;
            t0 = tx_time_get();
            (void)AppDisplayPresent(&dirty_rows, &full_flush);
            t1 = tx_time_get();
            draw_ticks = ((g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAG_REALTIME) != 0UL) ?
                         g_power_perf_last_draw_ticks : 0UL;
            AppPowerPerfHintPost((ULONG)(t1 - t0), draw_ticks, (ULONG)dirty_rows, (ULONG)full_flush);
          }
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
          dirty_rows = 0U;
          full_flush = 0U;
          t0 = tx_time_get();
          (void)AppDisplayPresent(&dirty_rows, &full_flush);
          t1 = tx_time_get();
          draw_ticks = ((g_eg_mode.tx_event_flags_group_current & APP_MODE_FLAG_REALTIME) != 0UL) ?
                       g_power_perf_last_draw_ticks : 0UL;
          AppPowerPerfHintPost((ULONG)(t1 - t0), draw_ticks, (ULONG)dirty_rows, (ULONG)full_flush);
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
