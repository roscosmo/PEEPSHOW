/* Extracted from app_threadx.c for thread-level organization. */

static VOID AppUiThreadEntry(ULONG thread_input)
{
  UINT status;
  app_input_action_evt_t evt;
  ui_router_api_t router_api;

  (void)thread_input;
  (void)memset(&router_api, 0, sizeof(router_api));
  router_api.joy_cal_start = AppUiRouterJoyCalStart;
  router_api.joy_cal_save = AppUiRouterJoyCalSave;
  router_api.joy_cal_cancel = AppUiRouterJoyCalCancel;
  UiRouter_Init(&router_api);
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
      AppUiHandleTick();
    }
    else if (status == TX_QUEUE_EMPTY)
    {
      AppUiHandleTick();
    }
    else if (status != TX_QUEUE_EMPTY)
    {
      /* Queue error in UI input consumer: continue receiving future events. */
      g_ui_event_queue_error_count++;
      AppUiHandleTick();
    }
  }
}
